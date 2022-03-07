cmake_minimum_required(VERSION 3.21)
project(bbe C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

include(CheckIncludeFile)
check_include_file(config.h HAVE_CONFIG_H)
check_include_file(features.h HAVE_FEATURES_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(error.h HAVE_ERROR_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(stdio.h HAVE_STDIO_H)

include(CheckTypeSize)
check_type_size("off_t" OFF_T)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_executable(bbe src/bbe.c src/buffer.c src/execute.c src/xmalloc.c)
