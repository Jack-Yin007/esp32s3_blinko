# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(nimble_gatt_server)

# ============================================================================
# Flash FAT分区镜像生成（用于出厂预烧录WAV文件）
# ============================================================================
# 使用方法：
#   1. 将WAV文件放入 flash_image/audio/ 目录
#   2. 将NFC文件放入 flash_image/nfc/ 目录
#   3. 编译时自动生成 flash.bin 镜像
#   4. 烧录命令: esptool.py write_flash 0x310000 build/flash.bin
# ============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/flash_image")
    fatfs_create_spiflash_image(flash ${CMAKE_SOURCE_DIR}/flash_image FLASH_IN_PROJECT)
    message(STATUS "Flash FAT partition image will be generated from flash_image/")
    message(STATUS "  - flash_image/audio/ -> /flash/audio")
    message(STATUS "  - flash_image/nfc/   -> /flash/nfc")
endif()

