cmake_minimum_required(VERSION 3.13)

project(binco C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH /Users/timmy/pico/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

pico_sdk_init()

add_executable(binco.elf
        main.cpp
        pinouts.h
        drivers/w5500.c
        drivers/w5500.h
        port/wizchip_port_pico.cpp
        drivers/wizchip_conf.h
        drivers/wizchip_conf.c
        drivers/socket.c
        drivers/socket.h
        port/wizchip_port_pico.h
        drivers/w5500_net.cpp
        drivers/w5500_net.h
        drivers/lcd1602.cpp
        drivers/lcd1602.h
        drivers/Adafruit_NAU7802.cpp
        drivers/Adafruit_NAU7802.h
        drivers/adc_service.cpp
        drivers/adc_service.h

)

# tell the target where to find your own headers
target_include_directories(binco.elf PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/drivers
        ${CMAKE_CURRENT_LIST_DIR}/port
)

target_link_libraries(binco.elf
        pico_stdlib
        hardware_spi
        hardware_i2c
)

pico_enable_stdio_usb(binco.elf 1)
pico_enable_stdio_uart(binco.elf 0)

pico_add_extra_outputs(binco.elf)