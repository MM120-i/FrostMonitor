if(NOT CONFIG STREQUAL "Debug")
    return()
endif()

if(EXISTS "${ASAN_DLL}")
    file(COPY_FILE "${ASAN_DLL}" "${DEST}/clang_rt.asan_dynamic-x86_64.dll" ONLY_IF_DIFFERENT)
endif()