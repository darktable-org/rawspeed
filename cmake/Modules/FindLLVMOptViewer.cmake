find_program(LLVMOPTVIEWER_PATH NAMES opt-viewer.py)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMOptViewer
  DEFAULT_MSG
  LLVMOPTVIEWER_PATH)

SET_PACKAGE_PROPERTIES(LLVMOptViewer PROPERTIES
  URL https://llvm.org/
  DESCRIPTION "Tool to visualize optimization records"
  PURPOSE "Used for rendering *.opt.yaml optimization records into HTML report"
)
