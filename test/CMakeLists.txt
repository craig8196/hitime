
cmake_minimum_required(VERSION 3.16)
enable_testing()

project(prove)

add_executable(prove prove.c)
target_include_directories(prove PRIVATE ../include)
target_link_libraries(prove PRIVATE hitime)
if (CODE_COVERAGE)
    target_code_coverage(prove)
endif()
add_test(NAME prove COMMAND prove)

add_executable(perform perform.c)
set_source_files_properties(perform PROPERTIES COMPILE_FLAGS "-O2" )
target_include_directories(perform PRIVATE ../include)
target_link_libraries(perform PRIVATE hitime)
add_test(NAME perform COMMAND perform)

