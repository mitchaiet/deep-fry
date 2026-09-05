# SPDX-License-Identifier: AGPL-3.0-only
# LLD embeds Windows manifests itself, without CMake's Windows-only mt.exe
# wrapper. Preserve CMake's remaining MSVC command-line and response-file rules.
foreach(_deepfry_language C CXX)
    foreach(_deepfry_rule LINK_EXECUTABLE CREATE_SHARED_LIBRARY CREATE_SHARED_MODULE)
        set(_deepfry_variable "CMAKE_${_deepfry_language}_${_deepfry_rule}")
        if(DEFINED ${_deepfry_variable})
            string(REGEX REPLACE "^.*<CMAKE_LINKER>" "<CMAKE_LINKER>"
                ${_deepfry_variable} "${${_deepfry_variable}}")
            if(NOT "${${_deepfry_variable}}" MATCHES "/manifest:embed")
                string(REPLACE "<LINK_FLAGS>" "/manifest:embed <LINK_FLAGS>"
                    ${_deepfry_variable} "${${_deepfry_variable}}")
            endif()
        endif()
    endforeach()
endforeach()
