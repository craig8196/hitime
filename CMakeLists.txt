
set(TARGET_NAME hitime)
set(SOURCES src/hitime.c src/hitime_extra.c)
set(PUBLIC_INCLUDES include/hitime.h)

cmake_minimum_required(VERSION 3.16)
enable_testing()
project(${TARGET_NAME} VERSION 1.1.3 DESCRIPTION "Heirarchical timeout management tool.")

include(GNUInstallDirs)

if(CODE_COVERAGE)
    set(CMAKE_BUILD_TYPE DEBUG)
    include(cmake-scripts/code-coverage.cmake)
endif()

add_subdirectory(test)


add_library(${TARGET_NAME} STATIC ${SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${TARGET_NAME} PROPERTIES PUBLIC_HEADER ${PUBLIC_INCLUDES})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_C_FLAGS_DEBUG "-O0")

target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

configure_file("${TARGET_NAME}.pc.in" "${TARGET_NAME}.pc" @ONLY)

target_include_directories(${TARGET_NAME} PRIVATE include)
target_include_directories(${TARGET_NAME} PRIVATE src)

if(CODE_COVERAGE)
    target_code_coverage(${TARGET_NAME})
endif()

if(BUILD_DOCUMENTATION)
    set(MYPROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set(MYPROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(PROJECT_README_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    configure_file(${doxyfile_in} ${doxyfile} @ONLY)
    include(cmake-scripts/doxygen.cmake)
    build_docs(ADD_TO_DOC ${TARGET_NAME})
endif()

install(TARGETS ${TARGET_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/${TARGET_NAME}.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)


