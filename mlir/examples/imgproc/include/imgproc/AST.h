//===- AST.h - Node definition for the ImgProc AST -------------------------===//
//
// This file implements the AST for the ImgProc language.
//
//===-----------------------------------------------------------------------===//

#ifndef IMGPROC_AST_H
#define IMGPROC_AST_H

#include "imgproc/Lexer.h"

#include "llvm/Support/Casting.h"

#include <optional>
#include <utility>
#include <vector>

namespace imgproc {

/// Base class for all expression nodes.
class ExprAST {
public:
	enum ExprASTKind {
		Expr_String,
		Expr_Load,
		Expr_Save,
		Expr_Grayscale,
		Expr_Convolve,
	};

	ExprAST(ExprASTKind kind, Location location)
		: kind(kind), location(std::move(location)) {}
	virtual ~ExprAST() = default;
	ExprASTKind getKind() const { return kind; }
	const Location &loc() const { return location; }

	static bool classof(const ExprAST *) { return true; }

private:
	const ExprASTKind kind;
	Location location;
};

/// A block-list of expressions
using ExprASTList = std::vector<std::unique_ptr<ExprAST>>;

/// Expression class for string literals like "hello.png"
class StringExprAST : public ExprAST {
	std::string val;

public:
	StringExprAST(Location loc, std::string val)
		: ExprAST(Expr_String, std::move(loc)), val(std::move(val)) {}

	const std::string &getValue() const { return val; }

	static bool classof(const ExprAST *c) {
		return c && c->getKind() == Expr_String;
	}
};

/// Expression class for builtin load calls.
class LoadExprAST : public ExprAST {
	std::unique_ptr<StringExprAST> arg;

public:
	LoadExprAST(Location loc, std::unique_ptr<StringExprAST> arg)
		: ExprAST(Expr_Load, std::move(loc)), arg(std::move(arg)) {}

	ExprAST *getArg() const { return arg.get(); }

	static bool classof(const ExprAST *c) {
		return c && c->getKind() == Expr_Load;
	}
};

/// Expression class for builtin save calls.
class SaveExprAST : public ExprAST {
	std::unique_ptr<StringExprAST> arg;

public:
	SaveExprAST(Location loc, std::unique_ptr<StringExprAST> arg)
		: ExprAST(Expr_Save, std::move(loc)), arg(std::move(arg)) {}

	ExprAST *getArg() const { return arg.get(); }

	static bool classof(const ExprAST *c) {
		return c && c->getKind() == Expr_Save;
	}
};

/// Expression class for builtin grayscale calls.
class GrayscaleExprAST : public ExprAST {
public:
	GrayscaleExprAST(Location loc)
		: ExprAST(Expr_Grayscale, std::move(loc)) {}

	static bool classof(const ExprAST *c) { return c->getKind() == Expr_Grayscale; }
};

/// Expression class for builtin convolve calls.
class ConvolveExprAST : public ExprAST {
public:
	ConvolveExprAST(Location loc)
		: ExprAST(Expr_Convolve, std::move(loc)) {}

	static bool classof(const ExprAST *c) { return c->getKind() == Expr_Convolve; }
};

/// This class represents a list of expressions to be processed together
class ModuleAST {
	std::unique_ptr<ExprASTList> body;

public:
	ModuleAST(std::unique_ptr<ExprASTList> body)
		: body(std::move(body)) {}

	auto begin() { return body.get()->begin(); }
	auto end() { return body.get()->end(); }
};

void dump(ModuleAST &);

} // namespace imgproc

#endif // IMGPROC_AST_H
