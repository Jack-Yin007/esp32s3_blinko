# ============================================================================
# Pet Robot Project - 3层架构 CMakeLists.txt
# 宠物机器人项目构建配置
# 
# 架构说明:
# ┌─────────────────────────────────────────┐
# │  应用层 (Application Layer)              │  - 业务逻辑和应用控制
# ├─────────────────────────────────────────┤
# │  驱动层 (Driver Layer)                   │  - 硬件驱动和BLE通信
# ├─────────────────────────────────────────┤
# │  系统层 (System Layer - ESP-IDF)        │  - ESP-IDF框架和FreeRTOS
# └─────────────────────────────────────────┘
# ============================================================================

# 主程序选择: pet_main.c (3层架构) 或 test_simple.c (测试) 或 main.c (原版)
set(MAIN_PROGRAM "pet_main.c")

# ============================================================================
# 版本信息生成 (Version Information)
# ============================================================================
# 获取Git信息
execute_process(
    COMMAND git rev-parse --short=6 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# 如果Git命令失败，设置默认值
if(NOT GIT_COMMIT_ID)
    set(GIT_COMMIT_ID "nogit")
endif()

if(NOT GIT_BRANCH_NAME)
    set(GIT_BRANCH_NAME "unknown")
endif()

# 获取当前日期 (YYYYMMDD格式)
string(TIMESTAMP BUILD_DATE_STR "%Y%m%d")

# 组合版本号: YYYYMMDD-branch-commit
set(FIRMWARE_VERSION "${BUILD_DATE_STR}-${GIT_BRANCH_NAME}-${GIT_COMMIT_ID}")

message(STATUS "Firmware Version: ${FIRMWARE_VERSION}")
message(STATUS "  Build Date: ${BUILD_DATE_STR}")
message(STATUS "  Git Branch: ${GIT_BRANCH_NAME}")
message(STATUS "  Git Commit: ${GIT_COMMIT_ID}")

# ============================================================================
# 应用层源文件 (Application Layer)
# ============================================================================
set(app_srcs
    ${MAIN_PROGRAM}              # 主程序入口
    "src/pet_config.c"           # 宠物配置
    "src/version.c"              # 版本信息
)

# 可选应用层文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/pet_state_machine.c")
    list(APPEND app_srcs "src/app/pet_state_machine.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/pet_behavior.c")
    list(APPEND app_srcs "src/app/pet_behavior.c")
endif()

# 情绪系统 (第二阶段)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/emotion_manager.c")
    list(APPEND app_srcs "src/app/emotion_manager.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/action_table.c")
    list(APPEND app_srcs "src/app/action_table.c")
endif()

# 触发系统 (第三阶段)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/trigger_detector.c")
    list(APPEND app_srcs "src/app/trigger_detector.c")
endif()

# 动作执行器
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/action_executor.c")
    list(APPEND app_srcs "src/app/action_executor.c")
endif()

# ============================================================================
# 驱动层源文件 (Driver Layer)
# ============================================================================
set(driver_srcs
    # 传统驱动 (保留兼容)
    "src/heart_rate_mock.c"      # 心率模拟
    # "src/led.c"                # LED控制 (旧版，已被led_enhanced.c取代)
    "src/audio.c"                # 音频处理
    "src/adpcm.c"                # ADPCM音频压缩
    "src/log_collector.c"        # BLE日志收集器 (16KB RAM循环缓冲区)
    "src/fs_service.c"           # BLE文件系统浏览服务
    "src/self_test.c"            # 设备自检系统 (14 devices)
    "src/self_test_trigger.c"   # BLE触发自检 (停止任务并执行)
)

# BLE通信驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/gap.c")
    list(APPEND driver_srcs "src/drivers/gap.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/gatt_svc.c")
    list(APPEND driver_srcs "src/drivers/gatt_svc.c")
endif()

# 新架构驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/led_driver.c")
    list(APPEND driver_srcs "src/drivers/led_driver.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/heart_rate_sensor.c")
    list(APPEND driver_srcs "src/drivers/heart_rate_sensor.c")
endif()

# 触摸传感器驱动 (TTP223)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/touch_sensor.c")
    list(APPEND driver_srcs "src/drivers/touch_sensor.c")
endif()

# PIR人体感应传感器驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/pir_sensor.c")
    list(APPEND driver_srcs "src/drivers/pir_sensor.c")
endif()

# LED增强驱动 (15种模式)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/led_enhanced.c")
    list(APPEND driver_srcs "src/drivers/led_enhanced.c")
endif()

# 震动马达驱动 (3种模式)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/vibrator.c")
    list(APPEND driver_srcs "src/drivers/vibrator.c")
endif()

# 马达驱动 (点头+摇头)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/motor.c")
    list(APPEND driver_srcs "src/drivers/motor.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/servo_motor.c")
    list(APPEND driver_srcs "src/drivers/servo_motor.c")
endif()

# 音频播放驱动 (9种音效)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/audio_player.c")
    list(APPEND driver_srcs "src/drivers/audio_player.c")
endif()

# I2S全双工音频系统 (MAX98357A + INMP441)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/i2s_audio.c")
    list(APPEND driver_srcs "src/drivers/i2s_audio.c")
endif()

# 播放+录制测试功能
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/playback_test.c")
    list(APPEND driver_srcs "src/drivers/playback_test.c")
endif()

# 音频播放+录制同步测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/playback_recorder_test.c")
    list(APPEND driver_srcs "src/drivers/playback_recorder_test.c")
endif()

# MAX98357A I2S音频DAC驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/max98357a.c")
    list(APPEND driver_srcs "src/drivers/max98357a.c")
endif()

# AW9535 GPIO扩展器驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/aw9535.c")
    list(APPEND driver_srcs "src/drivers/aw9535.c")
endif()

# AW9535引脚映射配置
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/aw9535_pinmap.c")
    list(APPEND driver_srcs "src/drivers/aw9535_pinmap.c")
endif()

# 麦克风驱动 (声音检测)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/microphone.c")
    list(APPEND driver_srcs "src/drivers/microphone.c")
endif()

# 舵机驱动
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/servo_driver.c")
    list(APPEND driver_srcs "src/drivers/servo_driver.c")
endif()

# 音频驱动 (旧版，保留兼容)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/audio_driver.c")
    list(APPEND driver_srcs "src/drivers/audio_driver.c")
endif()

# 硬件接口实现 (占位符，等待驱动团队实现)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/hardware_interface.c")
    list(APPEND driver_srcs "src/drivers/hardware_interface.c")
endif()

# Flash存储工具 (WAV文件复制)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/flash_storage.c")
    list(APPEND driver_srcs "src/drivers/flash_storage.c")
endif()

# GPIO扩展器驱动 (AW9535)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/aw9535.c")
    list(APPEND driver_srcs "src/drivers/aw9535.c")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/sd_card.c")
    list(APPEND driver_srcs "src/drivers/sd_card.c")
endif()



# NFC驱动 (PN532)
list(APPEND driver_srcs "src/drivers/indatachange.c")

# 语音识别驱动 (UART0)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/voice_recognition.c")
    list(APPEND driver_srcs "src/drivers/voice_recognition.c")
endif()

# NFC名片存储驱动 (File System)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/nfc_storage.c")
    list(APPEND driver_srcs "src/nfc_storage.c")
    message(STATUS "  [NFC] nfc_storage.c included (file system based)")
endif()

# Charger (CW6305)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/cw6116.c")
    list(APPEND driver_srcs "src/drivers/cw6116.c")
    message(STATUS "  [CHARGER] cw6116.c included (file system based)")
endif()

# ============================================================================
# BLE层源文件 (BLE Layer)
# ============================================================================
set(ble_srcs)

# BLE OTA固件升级
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ble/ble_ota.c")
    list(APPEND ble_srcs "src/ble/ble_ota.c")
    message(STATUS "  [BLE] ble_ota.c included (OTA firmware update)")
endif()

# 合并所有源文件
set(pet_srcs ${app_srcs} ${driver_srcs} ${ble_srcs})

# ============================================================================
# 包含目录 - 3层架构
# ============================================================================
set(include_dirs 
    "./include"                   # 主包含目录
    "./include/app"              # 应用层头文件
    "./include/drivers"          # 驱动层头文件
    "./include/ble"              # BLE层头文件 (OTA等)
)

# ============================================================================
# ESP-IDF组件注册
# ============================================================================
idf_component_register(
    SRCS ${pet_srcs}
    # 私有依赖项 (系统层组件)
    PRIV_REQUIRES 
        bt                        # 蓝牙组件
        nvs_flash                 # NVS存储
        esp_driver_gpio           # GPIO驱动
        esp_driver_ledc           # LEDC驱动
        esp_driver_uart           # UART驱动
        esp_driver_i2s            # I2S驱动
        esp_timer                 # 定时器组件
        esp_system                # 系统组件
        freertos                  # FreeRTOS
        log                       # 日志组件
        driver                    # 通用驱动
        fatfs                     # FAT文件系统（用于音频和存储分区）
        app_update                # OTA更新组件
        bootloader_support        # Bootloader支持（OTA需要）
        fatfs                     # FAT文件系统（用于音频和存储分区）
        
    # 包含目录
    INCLUDE_DIRS ${include_dirs}
    
    # 公共依赖 (供其他组件使用)
    REQUIRES 
        bt
        driver
)

# ============================================================================
# 添加版本定义到编译选项
# ============================================================================
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    FIRMWARE_VERSION="${FIRMWARE_VERSION}"
    BUILD_DATE="${BUILD_DATE_STR}"
    GIT_BRANCH="${GIT_BRANCH_NAME}"
    GIT_COMMIT="${GIT_COMMIT_ID}"
)

# ============================================================================
# 构建信息输出
# ============================================================================
message(STATUS "===============================================")
message(STATUS "  Pet Robot Project - 3层架构")
message(STATUS "===============================================")
message(STATUS "  固件版本: ${FIRMWARE_VERSION}")
message(STATUS "  应用层源文件: ${app_srcs}")
message(STATUS "  驱动层源文件: ${driver_srcs}")
message(STATUS "  包含目录: ${include_dirs}")
message(STATUS "===============================================")
