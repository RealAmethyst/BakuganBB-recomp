# Normalize only a staged copy; never change a dependency's build output.
if(NOT DEFINED INPUT OR NOT DEFINED OLD_RPATH)
    message(FATAL_ERROR "INPUT and OLD_RPATH are required")
endif()
file(RPATH_CHANGE FILE "${INPUT}" OLD_RPATH "${OLD_RPATH}" NEW_RPATH "$ORIGIN")
