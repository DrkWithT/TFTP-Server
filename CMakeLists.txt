cmake_minimum_required(VERSION 3.28)

project(tftpd VERSION 0.1.0 LANGUAGES CXX)

if (DEBUG_MODE)
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -g -Og)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3)
endif ()

set(MY_INCS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/includes")
set(MY_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
add_subdirectory(src)
