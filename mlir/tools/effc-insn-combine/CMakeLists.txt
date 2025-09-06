# mlir/tools/effc-insn-combine/CMakeLists.txt

add_llvm_executable(effc-insn-combine
  effc-insn-combine.cpp
)

target_link_libraries(effc-insn-combine
  PRIVATE
  MLIRTransforms 
  MLIROptLib   # brings in MlirOptMain + pass/dialect infra
  MLIRArithDialect
  MLIRSCFDialect
  MLIRFuncDialect
)

mlir_check_link_libraries(effc-insn-combine)

