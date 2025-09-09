add_custom_target(ImgProc)
set_target_properties(ImgProc PROPERTIES FOLDER "MLIR/Examples")

macro(add_toy_chapter name)
  add_dependencies(Toy ${name})
  add_llvm_example(${name} ${ARGN})
endmacro(add_toy_chapter name)

set(LLVM_LINK_COMPONENTS
  Support
  )

add_toy_chapter(imgprocc
  imgprocc.cpp
  parser/AST.cpp
  )

include_directories(include/)
target_link_libraries(imgprocc
  PRIVATE
    MLIRSupport)
