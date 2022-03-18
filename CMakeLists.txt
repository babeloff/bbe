cmake_minimum_required(VERSION 3.21)
project(bbe C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

if (WIN32)
    find_library(REGEX_LIB regex)
    find_path(REGEX_INCLUDE_PATH regex.h)
    find_path(REGEX_LIB_PATH libregex.dll.a)
    set(REGEX_LDFLAGS "-L${REGEX_LIB_PATH} -lregex")
endif()

include(CheckIncludeFile)
check_include_file(config.h HAVE_CONFIG_H)
check_include_file(features.h HAVE_FEATURES_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(error.h HAVE_ERROR_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(regex.h HAVE_REGEX_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(stdio.h HAVE_STDIO_H)

include(CheckTypeSize)
check_type_size("off_t" OFF_T)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_executable(bbe src/bbe.c src/buffer.c src/execute.c src/xmalloc.c)

option (BBE_ENABLE_DOC "Enable building documentation." ON)

if (BBE_ENABLE_DOC)
    find_program (ASCIIDOCTOR_EXE asciidoctor)
    if (NOT ASCIIDOCTOR_EXE)
        message (WARNING "Could not find asciidoctor: skipping docs")
        set (BBE_ENABLE_DOC OFF)
    else ()
        message (STATUS "Using asciidoctor at ${ASCIIDOCTOR_EXE}")
    endif ()
endif ()

if (BBE_ENABLE_DOC)

    set (BBE_DOCDIR ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    set (CMAKE_INSTALL_DOCDIR ${CMAKE_CURRENT_SOURCE_DIR}/install/doc)
    set (CMAKE_INSTALL_MANDIR ${CMAKE_CURRENT_SOURCE_DIR}/install/man)

    set (BBE_STYLESHEET ${BBE_DOCDIR}/stylesheet.css)
    set (BBE_TITLE ${PROJECT_NAME} ${BBE_PACKAGE_VERSION})
    set (BBE_A2M ${ASCIIDOCTOR_EXE} -b manpage -amanmanual='${BBE_TITLE}')
    set (BBE_A2H ${ASCIIDOCTOR_EXE} -d manpage -b html5 -a stylesheeet=${BBE_STYLESHEET} -a version-label=${PROJECT_NAME} -a revnumber=${BBE_PACKAGE_VERSION})

    macro (add_bbe_man NAME SECT)
        add_custom_command (
                OUTPUT ${NAME}.${SECT}
                COMMAND ${BBE_A2M} -o ${NAME}.${SECT} ${BBE_DOCDIR}/${NAME}.adoc
                MAIN_DEPENDENCY ${BBE_DOCDIR}/${NAME}.adoc
        )

        add_custom_command (
                OUTPUT ${NAME}.html
                COMMAND ${BBE_A2H} -o ${NAME}.html ${BBE_DOCDIR}/${NAME}.adoc
                DEPENDS ${BBE_STYLESHEET}
                MAIN_DEPENDENCY ${BBE_DOCDIR}/${NAME}.adoc
        )

        set(BBE_MANS ${BBE_MANS} ${NAME}.${SECT})
        set(BBE_HTMLS ${BBE_HTMLS} ${NAME}.html)

        install (
                FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.html
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
        )
        install (
                FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.${SECT}
                DESTINATION ${CMAKE_INSTALL_MANDIR}/man${SECT}
        )
    endmacro (add_bbe_man)

    add_bbe_man (bbe_manpage 3)

    add_custom_target (man ALL DEPENDS ${BBE_MANS})
    add_custom_target (html ALL DEPENDS ${BBE_HTMLS})

endif ()
