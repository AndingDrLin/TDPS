cmake_minimum_required(VERSION 3.20)
project(TDPSFirmware C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(TDPS_TARGET_MCU "Build for STM32F407VET6 target" OFF)
option(TDPS_BUILD_TESTS "Build PC stub tests" ON)

set(TDPS_CORE_SOURCES
    Src/lf_app.c
    Src/lf_chassis.c
    Src/lf_config.c
    Src/lf_control.c
    Src/lf_future_hooks.c
    Src/lf_radar.c
    Src/lf_sensor.c
    Src/wireless_hooks.c
    Src/wl_app.c
    Src/wl_config.c
    Src/wl_lora.c
    Src/wl_protocol.c
)

add_library(tdps_core STATIC ${TDPS_CORE_SOURCES})
target_include_directories(tdps_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
)

target_compile_options(tdps_core PRIVATE -Wall -Wextra)

if(TDPS_TARGET_MCU)
    target_sources(tdps_core PRIVATE
        Src/lf_platform_stm32f4_hal.c
        Src/lf_radar_uart.c
        Src/wl_platform_stm32f4.c
        Src/tdps_uart_callbacks.c
    )
    target_compile_definitions(tdps_core PUBLIC
        LF_USE_STM32F4_HAL_PORT
        WL_USE_STM32F4_HAL_PORT
        STM32F407xx
        USE_HAL_DRIVER
    )

    add_executable(tdps_firmware Src/main.c)
    target_link_libraries(tdps_firmware PRIVATE tdps_core)
else()
    target_sources(tdps_core PRIVATE
        Src/lf_platform_stub.c
        Src/wl_platform_stub.c
    )

    if(TDPS_BUILD_TESTS)
        add_executable(test_lf test/test_lf_stub.c)
        target_link_libraries(test_lf PRIVATE tdps_core m)

        add_executable(test_wl test/test_wl_stub.c)
        target_link_libraries(test_wl PRIVATE tdps_core)
    endif()
endif()
