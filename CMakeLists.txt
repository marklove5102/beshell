cmake_minimum_required(VERSION 3.24)


# source files
file(GLOB_RECURSE src_beshell
    ./src/beshell/*.c
    ./src/beshell/*.cpp
)
list(FILTER src_beshell EXCLUDE REGEX ".*/src/beshell/deps/.*")

# quickjs source files
set(src_quickjs
    ./src/beshell/deps/quickjs/quickjs-libc.c
    ./src/beshell/deps/quickjs/quickjs.c
    ./src/beshell/deps/quickjs/libregexp.c
    ./src/beshell/deps/quickjs/cutils.c
    # deps/quickjs/dtoa.c
    ./src/beshell/deps/quickjs/libunicode.c
)

# littlefs source files
file(GLOB_RECURSE src_littlefs
    ./src/beshell/deps/littlefs/src/*.c
)

# >= IDF 5.0
if (IDF_VERSION_MAJOR GREATER_EQUAL 5)

    set(requires esp_adc)

    # >= IDF 5.4
    if (IDF_VERSION_MINOR GREATER_EQUAL 4)
        set(requires ${requires} esp_driver_tsens)
    endif ()

# < IDF 5.0
else ()
    set(requires esp_adc_cal)
endif ()



idf_component_register(
    SRCS
        ${src_beshell}
        ${src_quickjs}
        ${src_littlefs}
    INCLUDE_DIRS
        ./src
    PRIV_REQUIRES
        driver
        mbedtls
        vfs
        fatfs
        spi_flash
        wear_levelling
        esp_timer
        nvs_flash
        esp_wifi
        efuse
        bt
        ${requires}
)


target_include_directories(${COMPONENT_LIB} PRIVATE src/beshell) 

target_link_options(${COMPONENT_TARGET} PRIVATE -DIS_LITTLE_ENDIAN)
set(CMAKE_CXX_STANDARD 11)


target_link_options(${COMPONENT_TARGET} PRIVATE -DCONFIG_FREERTOS_USE_TRACE_FACILITY)
target_link_options(${COMPONENT_TARGET} PRIVATE -DCONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS)
target_link_options(${COMPONENT_TARGET} PRIVATE "-Wl,--no-warn-common")


# quickjs version
file(READ "${COMPONENT_DIR}/src/beshell/deps/quickjs/VERSION" QJS_VERSION_CONTENT)
add_definitions(-DCONFIG_QUICKJS_VERSION="${QJS_VERSION_CONTENT}")



# c olny
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers>)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-missing-brace>)

# c++ olny
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>)

# c and c++
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=return-type)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=misleading-indentation)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=stringop-truncation)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-but-set-variable)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-pointer-arith)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-implicit-fallthrough)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-return-type)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-sign-compare)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-conversion)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-label)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-value)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-but-set-variable)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-pointer-arith)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-comment)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-type-limits)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-stack-protector)
target_compile_options(${COMPONENT_LIB} PRIVATE -fpermissive)
target_compile_options(${COMPONENT_LIB} PRIVATE -nostdlib)
target_compile_options(${COMPONENT_LIB} PRIVATE -ffunction-sections)
target_compile_options(${COMPONENT_LIB} PRIVATE -fdata-sections)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstrict-volatile-bitfields)


# for Logger
if(CONFIG_BESHELL_LOGGER_ENABLE_WRAP)
    target_link_options(${COMPONENT_TARGET} PUBLIC
        "-Wl,--wrap=write"
        "-Wl,--wrap=printf"
        "-Wl,--wrap=vprintf"
    )
    target_compile_options(${COMPONENT_TARGET} PUBLIC
        -fno-builtin-write
        -fno-builtin-printf
        -fno-builtin-vprintf
    )
    message(STATUS "Logger module enabled")
else()
    message(STATUS "Logger module disabled, use default write/printf/vprintf")
endif ()


# string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# string(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#clang
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_link_options(${COMPONENT_TARGET} PRIVATE -D__STDC_NO_ATOMICS__=1)
endif ()
message("Operating System: ${CMAKE_SYSTEM_NAME}")
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_subdirectory(pc)
endif ()
