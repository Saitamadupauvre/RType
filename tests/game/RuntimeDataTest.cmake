file(GLOB_RECURSE expected RELATIVE "${SOURCE}" "${SOURCE}/*")
file(GLOB_RECURSE actual RELATIVE "${COPY}" "${COPY}/*")
list(SORT expected)
list(SORT actual)

if(NOT expected STREQUAL actual)
    message(FATAL_ERROR "${COPY} does not list the same files as ${SOURCE}\n"
                        "expected: ${expected}\n"
                        "actual:   ${actual}")
endif()

foreach(file IN LISTS expected)
    file(SHA256 "${SOURCE}/${file}" source_hash)
    file(SHA256 "${COPY}/${file}" copy_hash)
    if(NOT source_hash STREQUAL copy_hash)
        message(FATAL_ERROR "${COPY}/${file} differs from ${SOURCE}/${file}")
    endif()
endforeach()
