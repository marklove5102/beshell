
# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# js 源文件目录
set(JS_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/js")             

# js 镜像文件路径
set(JS_IMG_PATH "${CMAKE_CURRENT_LIST_DIR}/img/js.bin")

set(MKLITTLEFS_PATH "${CMAKE_CURRENT_LIST_DIR}/components/beshell/tools/mklittlefs.exe")
if(NOT EXISTS ${MKLITTLEFS_PATH})
    set(MKLITTLEFS_PATH "${CMAKE_CURRENT_LIST_DIR}/managed_components/become-cool__beshell/tools/mklittlefs.exe")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(demo)


# 自动打包 js.bin
file(GLOB_RECURSE JS_SRC_FILES
    RELATIVE ${JS_SRC_DIR}
    ${JS_SRC_DIR}/*.js
)
list(TRANSFORM JS_SRC_FILES PREPEND ${JS_SRC_DIR}/)

add_custom_command(
    OUTPUT "${JS_IMG_PATH}"
    COMMAND echo "Packing javascipt img ..."
    COMMAND ${MKLITTLEFS_PATH} -c "${JS_SRC_DIR}" -s 524288 "${JS_IMG_PATH}" -d 5
    DEPENDS ${JS_SRC_FILES}
    VERBATIM
)

# idf.py build 时，同时也会打包 js.bin
add_custom_target(pack-js ALL DEPENDS "${JS_IMG_PATH}")

# 烧录 js 文件系统镜像
add_custom_target(flash-js
    COMMAND echo "Flashing javascipt img ..."
    COMMAND python ${IDF_PATH}/components/esptool_py/esptool/esptool.py
        --chip ${IDF_TARGET}
        --baud 921600
        --after hard_reset
        write_flash
        0x290000
        "${JS_IMG_PATH}"
    DEPENDS ${CUSTOM_BIN_PATH}
    VERBATIM
)

# idf.py flash 时，同时也自动烧录 js 文件系统镜像
add_dependencies(flash flash-js)



# 自动包含 CMakeLists.update_exmaple.txt（如果存在）
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.update_example.txt")
    include("${CMAKE_CURRENT_LIST_DIR}/CMakeLists.update_example.txt")
endif()