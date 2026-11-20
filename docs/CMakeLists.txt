##
# @file     CMakeLists.txt
# @brief    Build Sphinx traceability documentation and run CTest checks
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(microkernel_docs LANGUAGES NONE)
    get_filename_component(
        DOCS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE
    )
else()
    set(DOCS_REPO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

include(CTest)
enable_testing()

option(
    DOCS_AUTO_INSTALL
    "Create build/docs/venv and install Python doc dependencies if missing"
    OFF
)

set(DOCS_BUILD_DIR "${CMAKE_BINARY_DIR}/docs")
set(DOCS_SPHINX_CONFIG "${DOCS_REPO_ROOT}/config/sphinx")
set(DOCS_DOCTREES_DIR "${DOCS_BUILD_DIR}/doctrees")
set(DOCS_HTML_DIR "${DOCS_BUILD_DIR}/html")
set(DOCS_NEEDS_DIR "${DOCS_BUILD_DIR}/needs")
set(DOCS_NEEDS_JSON "${DOCS_NEEDS_DIR}/needs.json")
set(DOCS_PYTHON_MANIFEST "${DOCS_REPO_ROOT}/requirements.txt")

set(DOCS_PYTHON "")
if(EXISTS "${DOCS_REPO_ROOT}/.venv/bin/python")
    set(DOCS_PYTHON "${DOCS_REPO_ROOT}/.venv/bin/python")
elseif(DOCS_AUTO_INSTALL)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(DOCS_VENV_DIR "${DOCS_BUILD_DIR}/venv")
    if(NOT EXISTS "${DOCS_VENV_DIR}/bin/python")
        message(STATUS "Creating documentation venv in ${DOCS_VENV_DIR}")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m venv "${DOCS_VENV_DIR}"
            RESULT_VARIABLE _venv_rc
        )
        if(NOT _venv_rc EQUAL 0)
            message(FATAL_ERROR "Failed to create venv in ${DOCS_VENV_DIR}")
        endif()
        execute_process(
            COMMAND
                "${DOCS_VENV_DIR}/bin/python" -m pip install
                -r "${DOCS_PYTHON_MANIFEST}"
            RESULT_VARIABLE _pip_rc
        )
        if(NOT _pip_rc EQUAL 0)
            message(
                FATAL_ERROR
                "Failed to install dependencies from ${DOCS_PYTHON_MANIFEST}"
            )
        endif()
    endif()
    set(DOCS_PYTHON "${DOCS_VENV_DIR}/bin/python")
else()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(DOCS_PYTHON "${Python3_EXECUTABLE}")
endif()

execute_process(
    COMMAND
        "${DOCS_PYTHON}" -c
        "import sphinx, sphinx_needs, myst_parser"
    RESULT_VARIABLE _import_rc
    ERROR_VARIABLE _import_err
)
if(NOT _import_rc EQUAL 0)
    message(
        FATAL_ERROR
        "Missing documentation packages in ${DOCS_PYTHON}.\n"
        "Install them with:\n"
        "  python3 -m venv .venv && "
        ".venv/bin/pip install -r ${DOCS_PYTHON_MANIFEST}\n"
        "Or configure with -DDOCS_AUTO_INSTALL=ON.\n"
        "Details: ${_import_err}"
    )
endif()

add_custom_target(
    docs
    ALL
    COMMAND
        "${DOCS_PYTHON}" -m sphinx
        -b html -W --keep-going
        -c "${DOCS_SPHINX_CONFIG}"
        -d "${DOCS_DOCTREES_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${DOCS_HTML_DIR}"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
    COMMENT "Building Sphinx traceability HTML documentation"
    VERBATIM
)

add_custom_target(
    docs-needs
    COMMAND
        "${DOCS_PYTHON}" -m sphinx
        -b needs -W --keep-going
        -c "${DOCS_SPHINX_CONFIG}"
        -d "${DOCS_DOCTREES_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${DOCS_NEEDS_DIR}"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
    COMMENT "Exporting Sphinx-Needs model to ${DOCS_NEEDS_JSON}"
    VERBATIM
)

set_property(
    DIRECTORY
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES "${DOCS_BUILD_DIR}"
)

add_test(
    NAME traceability.unit
    COMMAND
        "${DOCS_PYTHON}" -m unittest discover
        -s "${DOCS_REPO_ROOT}/tests/traceability"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
)
set_tests_properties(
    traceability.unit
    PROPERTIES LABELS "traceability;docs"
)

add_test(
    NAME docs.html
    COMMAND
        "${DOCS_PYTHON}" -m sphinx
        -b html -W --keep-going
        -c "${DOCS_SPHINX_CONFIG}"
        -d "${DOCS_DOCTREES_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${DOCS_HTML_DIR}"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
)
set_tests_properties(
    docs.html
    PROPERTIES LABELS "traceability;docs"
)

add_test(
    NAME docs.needs
    COMMAND
        "${DOCS_PYTHON}" -m sphinx
        -b needs -W --keep-going
        -c "${DOCS_SPHINX_CONFIG}"
        -d "${DOCS_DOCTREES_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${DOCS_NEEDS_DIR}"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
)
set_tests_properties(
    docs.needs
    PROPERTIES LABELS "traceability;docs"
)

add_test(
    NAME docs.audit
    COMMAND
        "${DOCS_PYTHON}"
        "${DOCS_REPO_ROOT}/tools/traceability/check_needs_audit.py"
        --needs-json "${DOCS_NEEDS_JSON}"
    WORKING_DIRECTORY "${DOCS_REPO_ROOT}"
)
set_tests_properties(
    docs.audit
    PROPERTIES
        DEPENDS docs.needs
        LABELS "traceability;docs"
)
