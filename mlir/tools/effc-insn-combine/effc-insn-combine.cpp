#include "mlir/IR/Dialect.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

// docs: https://mlir.llvm.org/docs/PatternRewriter/
// arith dialect: https://mlir.llvm.org/docs/Dialects/ArithOps/
// scf dialect: https://mlir.llvm.org/docs/Dialects/SCFDialect/
// examples: https://github.com/llvm/llvm-project/tree/main/mlir/examples

namespace mlir {

/// Try to fold `lhs << shAmt` if `lhs` is a constant
/// Otherwise, build a new shift left (arith.shli) using
/// the provided RHS constant.
static Value buildShiftedOrFoldConst(PatternRewriter &rewriter, Location loc,
                                     Value lhs, arith::ConstantOp rhsConst) {
  if (auto c = lhs.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = dyn_cast<IntegerAttr>(c.getValue())) {
      APInt v = intAttr.getValue();
      auto rhsIntAttr = cast<IntegerAttr>(rhsConst.getValue());
      unsigned shAmt = (unsigned)rhsIntAttr.getValue().getZExtValue();
      APInt shifted = v.shl(shAmt);
      auto ty = c.getType();
      auto shiftedAttr = IntegerAttr::get(ty, shifted);
      return rewriter.create<arith::ConstantOp>(loc, shiftedAttr).getResult();
    }
  }
  // Fall back to creating a shift instruction.
  return rewriter.create<arith::ShLIOp>(loc, lhs, rhsConst).getResult();
}

/// scf.index_switch (single case) result used (single use) by arith.shli(const
/// ==> cmpi(eq) + select(shift(caseYield), shift(defaultYield))
class IndexSwitchToCmpPattern : public RewritePattern {
public:
	explicit IndexSwitchToCmpPattern(MLIRContext *context)
		: RewritePattern(scf::IndexSwitchOp::getOperationName(), 1, context) {}

	LogicalResult matchAndRewrite(Operation *op,
								  PatternRewriter &rewriter) const override {
		// skip if op is not index_switch
		auto switchOp = dyn_cast<scf::IndexSwitchOp>(op);
		if (!switchOp) {
			return failure();
		}

		// skip if there are more than one case (except default)
		// and the number of results is more than one
		if ((switchOp.getNumCases() != 1) ||
			(switchOp.getNumResults() != 1)) {
			return failure();
		}

		// skip if switch result is used more than once
		Value switchResult = switchOp.getResult(0);
		if (!switchResult.hasOneUse()) {
			return failure();
		}

		// skip if the switch result user is not shift left
		auto user = *switchResult.getUsers().begin();
		auto shiftLeftOp = dyn_cast<arith::ShLIOp>(user);
		if (!shiftLeftOp) {
			return failure();
		}

		// skip if the left shift amount is not a constant
		auto rhsConst = shiftLeftOp.getRhs().getDefiningOp<arith::ConstantOp>();
		if (!rhsConst) {
			return failure();
		}

		// get the case and default yield values
		int64_t caseVal = switchOp.getCases()[0];
		auto getYieldVal = [&](Region &region) -> std::optional<Value> {
			if (region.empty() || region.front().empty())
				return std::nullopt;
			auto term = region.front().getTerminator();
			auto yield = dyn_cast<scf::YieldOp>(term);
			if (!yield || yield.getNumOperands() != 1)
				return std::nullopt;
			return yield.getOperand(0);
		};

		auto caseYieldOpt = getYieldVal(switchOp.getCaseRegions()[0]);
		auto defaultYieldOpt = getYieldVal(switchOp.getDefaultRegion());
		if (!caseYieldOpt || !defaultYieldOpt) {
			return failure();
		}
		Value caseYieldVal = *caseYieldOpt;
		Value defaultYieldVal = *defaultYieldOpt;

		Location swLoc = switchOp.getLoc();
		Value cmpLhs, cmpRhs;
		Type cmpTy;
		// try to get integer type operands from switch index and case
		auto indexArg = switchOp.getArg();
		if (auto castOp = indexArg.getDefiningOp<arith::IndexCastOp>()) {
			Value src = castOp.getIn();
			if (src.getType().isa<IntegerType>()) {
				cmpTy = src.getType();
				cmpLhs = src;
				auto iattr = rewriter.getIntegerAttr(cmpTy, caseVal);
				cmpRhs = rewriter.create<arith::ConstantOp>(swLoc, iattr);
			}
		}
		// if couldn't get integer type from switch index,
		// get the index type operands for compare
		if (!cmpLhs) {
			cmpTy = indexArg.getType();
			cmpLhs = indexArg;
			cmpRhs = rewriter.create<arith::ConstantOp>(
				swLoc, rewriter.getIndexAttr(caseVal));
		}

		// create cmpIOp that replaces switch's index == case value
		Value cond = rewriter.create<arith::CmpIOp>(
			swLoc, arith::CmpIPredicate::eq, cmpLhs, cmpRhs);

		// case and default yield values are left shifted
		// try to fold the shift left operation (if possible)
		// and use those values as true and false part of the selectOp

		Location shlLoc = shiftLeftOp.getLoc();

		// create true part value for selectOp
		Value trueVal =
			buildShiftedOrFoldConst(rewriter, shlLoc, caseYieldVal, rhsConst);
		// crate false part value for selectOp
		Value falseVal =
			buildShiftedOrFoldConst(rewriter, shlLoc, defaultYieldVal, rhsConst);

		// the true and false part values should match the result type
		// check and skip if the result type not matched
		Type resultTy = shiftLeftOp.getType();
		if ((trueVal.getType() != resultTy) ||
			(falseVal.getType() != resultTy)) {
			return failure();
		}
		// create the selectOp with the computed true and false parts
		Value selectOp = rewriter.create<arith::SelectOp>(
			shlLoc, cond, trueVal, falseVal);
		// replace shift left operation with select operation
		rewriter.replaceOp(shiftLeftOp, selectOp);
		// remove the switch operation as it is replaced by cmp op
		rewriter.eraseOp(switchOp);
		return success();
	}
};

class InstCombinePass : public PassWrapper<InstCombinePass, OperationPass<func::FuncOp>> {
    StringRef getArgument() const final {
        return "instcombine";
    }

    StringRef getDescription() const final {
        return "A simple pass to optimize some scf and arith operations";
    }

    void runOnOperation() override {
        // TODO: please implement logic here
		RewritePatternSet patterns(&getContext());
		patterns.add<IndexSwitchToCmpPattern>(&getContext());
		FrozenRewritePatternSet patternSet(std::move(patterns));
		if (failed(applyPatternsAndFoldGreedily(getOperation(), patternSet)))
			signalPassFailure();
    }
};

}

int main(int argc, char **argv) {
  //mlir::registerAllPasses();
  mlir::PassRegistration<mlir::InstCombinePass>();

  mlir::DialectRegistry registry;
  registry.insert<mlir::arith::ArithDialect,
      mlir::scf::SCFDialect,
      mlir::func::FuncDialect>();
  //registerAllDialects(registry);

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "Custom optimizer driver\n", registry));
}
