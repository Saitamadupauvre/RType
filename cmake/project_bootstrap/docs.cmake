# Managed by project-bootstrap [5a7f6436f3f365f9]: edits here are reported by `status`.
find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    set(DOXYGEN_PROJECT_NAME "RType")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/docs")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${PROJECT_SOURCE_DIR}/README.md")
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_WARN_AS_ERROR NO)

    set(rtype_doc_inputs "${PROJECT_SOURCE_DIR}/README.md")
    foreach(dir engine/include engine/src plugins game)
        if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${dir}")
            list(APPEND rtype_doc_inputs "${PROJECT_SOURCE_DIR}/${dir}")
        endif()
    endforeach()

    doxygen_add_docs(docs ${rtype_doc_inputs} COMMENT "Generating API documentation")
endif()
