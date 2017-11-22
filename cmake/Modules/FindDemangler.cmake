set(_demangler "NOTFOUND")

function(findDemangler package target)
  if(_demangler)
    return()
  endif()

  find_package(${package})

  if(NOT DEFINED ${package}_FOUND OR NOT ${package}_FOUND OR NOT TARGET ${target})
    return()
  endif()

  set_package_properties(Demangler PROPERTIES
                         DESCRIPTION "Just an alias for ${package}")

  get_property(_demangler TARGET ${target} PROPERTY IMPORTED_LOCATION)
  set_package_properties(${package} PROPERTIES
                         TYPE REQUIRED)

  set(_demangler "${_demangler}" PARENT_SCOPE)
endfunction()

findDemangler(LLVMCXXFilt llvm-cxxfilt)
findDemangler(CppFilt     c++filt)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Demangler
  DEFAULT_MSG
  _demangler)

add_executable(demangler IMPORTED GLOBAL)
set_property(TARGET demangler PROPERTY IMPORTED_LOCATION "${_demangler}")
