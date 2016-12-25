cmake_minimum_required(VERSION 2.8)

project(adir01pcpp LANGUAGES CXX)

function(target_enable_warning target)
    target_compile_options(
        ${target}
        PRIVATE
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic>
        $<$<CXX_COMPILER_ID:GNU>:-pedantic -Wall -Wextra>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        )
endfunction()

add_subdirectory(src)
add_subdirectory(example)
