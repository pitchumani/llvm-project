//===- AST.cpp - Helper for printing out the ImgProc AST -----------------===//
//
// This file implements the AST dump for the ImgProc language.
//
//===-----------------------------------------------------------------------===//

#include "imgproc/AST.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace imgproc;

namespace {

/// Helper to manage increase and decrease indentation
struct Indent {
    Indent(int &level) : level(level) { ++level; }
    ~Indent() { --level; }
    int &level;
};

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

	void indent() {
		for (int i = 0; i < curIndent; ++i)
			llvm::errs() << "  ";
	}
	int curIndent = 0;
	void INDENT() {
		Indent level_(curIndent);
		indent();
	}
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
    if (!expr) {
        INDENT();
        llvm::errs() << "<null>\n";
        return;
    }
	#if 1
 	llvm::TypeSwitch<ExprAST *>(expr)
 		.Case<StringExprAST, LoadExprAST, SaveExprAST, GrayscaleExprAST,
 			  ConvolveExprAST>([&](auto *node) {
 				  this->dump(node);
 			  })
 		.Default([&](ExprAST *) {
 			// no match
			INDENT();
 			llvm::errs() << "<unknown Expr, kind " << expr->getKind() << ">\n";
 		});
	#else
    if (auto *load = llvm::dyn_cast<LoadExprAST>(expr))
        return dump(load);
    if (auto *save = llvm::dyn_cast<SaveExprAST>(expr))
        return dump(save);
    if (auto *grayscale = llvm::dyn_cast<GrayscaleExprAST>(expr))
        return dump(grayscale);
    if (auto *convolve = llvm::dyn_cast<ConvolveExprAST>(expr))
        return dump(convolve);
    if (auto *str = llvm::dyn_cast<StringExprAST>(expr))
        return dump(str);

    INDENT();
    llvm::errs() << "<unknown Expr, kind " << expr->getKind() << ">\n";
	#endif
}

void ASTDumper::dump(StringExprAST *str) {
	INDENT();
	llvm::errs() << "\"" << str->getValue() << "\" " << loc(str) << "\n";
}

void ASTDumper::dump(LoadExprAST *node) {
	INDENT();
	llvm::errs() << "load [ " << loc(node) << "\n";
	if (ExprAST *arg = node->getArg()) {
		INDENT();
		dump(arg);
	}
	INDENT();
	llvm::errs() << "]\n";
}

void ASTDumper::dump(SaveExprAST *node) {
	INDENT();
	llvm::errs() << "save [ " << loc(node) << "\n";
	if (ExprAST *arg = node->getArg()) {
		INDENT();
		dump(arg);
	}
	INDENT();
	llvm::errs() << "]\n";
}

void ASTDumper::dump(GrayscaleExprAST *node) {
	INDENT();
	llvm::errs() << "grayscale [ " << loc(node) << " ]\n";
}

void ASTDumper::dump(ConvolveExprAST *node) {
	INDENT();
	llvm::errs() << "convolve [ " << loc(node) << " ]\n";
}

void ASTDumper::dump(ModuleAST *node) {
	llvm::errs() << "Module:\n";
	for (const auto &e : *node) {
		dump(e.get());
	}
}

namespace imgproc {

void dump(ModuleAST &module) { ASTDumper().dump(&module); }

} // namespace imgproc
