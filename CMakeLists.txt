# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(AromaKit-test2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(AromaKit-test2 src/main.cpp src/bt_spp.c src/st7565.cpp)

pico_set_program_name(AromaKit-test2 "AromaKit-test2")
pico_set_program_version(AromaKit-test2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(AromaKit-test2 0)
pico_enable_stdio_usb(AromaKit-test2 1)

set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

add_subdirectory(lib/pi-pico-LCD/lcd_display build)

# Add the standard library to the build
target_link_libraries(AromaKit-test2
        pico_stdlib
        pico_multicore
        hardware_spi
        pico_btstack_cyw43
        pico_cyw43_arch_threadsafe_background
        pico_btstack_classic
        lcd_display)

# Add the standard include files to the build
target_include_directories(AromaKit-test2 PRIVATE
        ${BTSTACK_ROOT}/src
        ${CMAKE_CURRENT_LIST_DIR}/src
)

# Add any user requested libraries
target_link_libraries(AromaKit-test2 

        )

target_compile_definitions(AromaKit-test2 PRIVATE
    #PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000
    CYW43_LWIP=0
    #WANT_HCI_DUMP=1 # This enables btstack debug
    )

pico_add_extra_outputs(AromaKit-test2)

