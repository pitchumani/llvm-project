//===- Parser.h - Parser for the ImgProc language ------------------------===//
//
// This file implements the Parser for the ImgProc language.
// This is based on the LLVM toy example
//
//===----------------------------------------------------------------------===//

#ifndef IMGPROC_PARSER_H
#define IMGPROC_PARSER_H

#include "imgproc/AST.h"
#include "imgproc/Lexer.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace imgproc {

/// Parser for ImgProc language.
class Parser {
public:
    /// Create a Parser for the supplied lexer.
    Parser(Lexer &lexer) : lexer(lexer) {}

	std::unique_ptr<ModuleAST> parseModule() {
		lexer.getNextToken();

		auto exprList = std::make_unique<ExprASTList>();
		while(auto e = parseExpression()) {
			exprList->push_back(std::move(e));
			if (lexer.getCurToken() == tok_eof)
				break;
		}

		if (lexer.getCurToken() != tok_eof)
			return parseError<ModuleAST>("nothing", "at end of module");

		return std::make_unique<ModuleAST>(std::move(exprList));
	}

private:
	Lexer &lexer;

	/// parenexpr ::= '(' expression ')'
	std::unique_ptr<ExprAST> parseParenExpr() {
		lexer.getNextToken(); // eat (.
		auto v = parseExpression();
		if (!v)
			return nullptr;

		if (lexer.getCurToken() != ')')
			return parseError<ExprAST>(")", "to close expression with parentheses");
		lexer.consume(Token(')'));
		return v;
	}

	/// identifierexpr
	///   ::= identifer
	///   ::= identifier '(' expression ')'
	std::unique_ptr<ExprAST> parseIdentifierExpr() {
		std::string name(lexer.getId());
		//llvm::errs() << "parseIdentifierExpr: name: " << name << "\n";
		auto loc = lexer.getLastLocation();
		lexer.getNextToken(); // eat identifier

		// simple variable is not supported, only function call
		if (lexer.getCurToken() != '(') {
			return parseError<ExprAST>("(", "in expression");
		}
		// this is a function call
		lexer.consume(Token('('));
		std::unique_ptr<ExprAST> arg;
		if (lexer.getCurToken() != ')') {
			if (lexer.getCurToken() != tok_string) {
				return parseError<ExprAST>("<string>", "in function call argument");
			}
			arg = parseStringExpr();

			if (lexer.getCurToken() != ')')
				return parseError<ExprAST>(")", "in expression");
		}
		lexer.consume(Token(')'));
		/*
		llvm::errs() << "parseIdentifierExpr: " << name;
		if (arg) {
			llvm::errs() << "(" <<
				static_cast<StringExprAST*>(arg.get())->getValue()
						 << ")";
		}
		llvm::errs() << "\n";
		*/
		// check the call for builtin functions
		if (name == "load") {
			if (!arg)
				return parseError<ExprAST>("<single arg>", "as argument to load()");

			   auto uptr_arg = std::unique_ptr<StringExprAST>(
				   static_cast<StringExprAST*>(arg.release()));
			   return std::make_unique<LoadExprAST>(std::move(loc),
													std::move(uptr_arg));
		} else if (name == "save") {
			if (!arg)
				return parseError<ExprAST>("<single arg>", "as argument to save()");

			   auto uptr_arg = std::unique_ptr<StringExprAST>(
				   static_cast<StringExprAST*>(arg.release()));
			   return std::make_unique<SaveExprAST>(std::move(loc),
													std::move(uptr_arg));
		} else if (name == "grayscale") {
			if (arg)
				return parseError<ExprAST>("unexpected argument",
										   "no argument is expected for grayscale()");

			return std::make_unique<GrayscaleExprAST>(std::move(loc));
		} else if (name == "convolve") {
			if (arg)
				return parseError<ExprAST>("unexpected argument",
										   "no argument is expected for convolve()");

			return std::make_unique<ConvolveExprAST>(std::move(loc));
		}

		return parseError<ExprAST>("", "unexpected function call");
	}

	/// Parse a string.
	/// stringexpr ::= "string"
	std::unique_ptr<ExprAST> parseStringExpr() {
		auto loc = lexer.getLastLocation();
		auto str = lexer.getString();
		//llvm::errs() << "parseStringExpr: str: " << str << "\n";

		auto result =
			std::make_unique<StringExprAST>(std::move(loc), str);
		lexer.consume(tok_string);
		return std::move(result);
	}

	/// primary
	///   ::= identifierexpr
	///   ::= stringexpr
	///   ::= parenexpr
	std::unique_ptr<ExprAST> parsePrimary() {
		switch (lexer.getCurToken()) {
		default:
			llvm::errs() << "unknown token '" << lexer.getCurToken()
						 << "' when expecting an expression\n";
			return nullptr;
		case tok_identifier:
			return parseIdentifierExpr();
		case tok_string:
			return parseStringExpr();
		case '(':
			return parseParenExpr();
		case ';':
			return nullptr;
		}
	}

	/// expression::= primary ( primary ) ;
	std::unique_ptr<ExprAST> parseExpression() {
		auto e = parsePrimary();
		if (lexer.getCurToken() != ';') {
			return parseError<ExprAST>(";", "after expression");
		}
		while (lexer.getCurToken() == ';')
			lexer.consume(Token(';'));
		return e;
	}

	/// Helper function to signal errors while parsing, it takes an argument
	/// indicating the expected token and another argument giving more context.
	/// Location is retrieved from the lexer to enrich the error message.
	template <typename R, typename T, typename U = const char *>
		std::unique_ptr<R> parseError(T &&expected, U &&context = "") {
		auto curToken = lexer.getCurToken();
		llvm::errs() << "Parse error (" << lexer.getLastLocation().line << ", "
		<< lexer.getLastLocation().col << "): expected '" << expected
		<< "' " << context << " but has Token " << curToken;
		if (isprint(curToken))
			llvm::errs() << " '" << (char)curToken << "'";
		llvm::errs() << "\n";
		return nullptr;
	}
};

} // namespace imgproc

#endif // IMGPROC_PARSER_H
