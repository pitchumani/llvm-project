//===- AST.cpp - Helper for printing out the ImgProc AST -----------------===//
//
// This file implements the AST dump for the ImgProc language.
//
//===-----------------------------------------------------------------------===//

#include "imgproc/AST.h"

#include "llvm/ADT/Twine.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace imgproc;

namespace {

/// Helper class that implement the AST tree traversal and print the nodes
class ASTDumper {
public:
	void dump(ModuleAST *node);

private:
	void dump(ExprAST *expr);
	void dump(StringExprAST *expr);
	void dump(LoadExprAST *expr);
	void dump(SaveExprAST *expr);
	void dump(GrayscaleExprAST *expr);
	void dump(ConvolveExprAST *expr);
};

} // namespace

/// Return formatted string for the ocation of any node
template <typename T>
static std::string loc(T *node) {
	const auto &loc = node->loc();
	return (llvm::Twine("@") + *loc.file + ":" + llvm::Twine(loc.line) + ":" +
			llvm::Twine(loc.col)).str();
}

/// Dispatch to a generic expressions to the appropriate subclass
void ASTDumper::dump(ExprAST *expr) {
	llvm::TypeSwitch<ExprAST *>(expr)
		.Case<StringExprAST, LoadExprAST, SaveExprAST, GrayscaleExprAST,
			  ConvolveExprAST>([&](auto *node) {
				  this->dump(node);
			  })
		.Default([&](ExprAST *) {
			// no match
			llvm::errs() << "<unknown Expr, kind " << expr->getKind() << ">\n";
		});
}

void ASTDumper::dump(StringExprAST *str) {
	llvm::errs() << "\"" << str->getValue() << loc(str) << "\"\n";
}

void ASTDumper::dump(LoadExprAST *node) {
	llvm::errs() << "load [ " << loc(node) << "\n";
	dump(node->getArg());
	llvm::errs() << "]\n";
}

void ASTDumper::dump(SaveExprAST *node) {
	llvm::errs() << "save [ " << loc(node) << "\n";
	dump(node->getArg());
	llvm::errs() << "]\n";
}

void ASTDumper::dump(GrayscaleExprAST *node) {
	llvm::errs() << "grayscale [ " << loc(node) << "]\n";
}

void ASTDumper::dump(ConvolveExprAST *node) {
	llvm::errs() << "convolve [ " << loc(node) << "]\n";
}

void ASTDumper::dump(ModuleAST *node) {
	llvm::errs() << "Module:\n";
	for (auto &e : *node) {
		dump(&e);
	}
}

namespace imgproc {

void dump(ModuleAST &module) { ASTDumper().dump(&module); }

} // namespace imgproc
