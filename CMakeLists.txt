# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(ENABLE_DEBUG_LOGS "Enable firmware diagnostic logging" ON)

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(360System C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(360System 360System.c audio.c sounds.c ultrasonic.c)

pico_set_program_name(360System "360System")
pico_set_program_version(360System "0.1")

if(ENABLE_DEBUG_LOGS)
  target_compile_definitions(360System PRIVATE ENABLE_DEBUG_LOGS=1)
  pico_enable_stdio_uart(360System 1)
  pico_enable_stdio_usb(360System 1)
else()
  target_compile_definitions(360System PRIVATE ENABLE_DEBUG_LOGS=0)
  pico_enable_stdio_uart(360System 0)
  pico_enable_stdio_usb(360System 0)
endif()

# Add the standard library to the build
target_link_libraries(360System
        pico_stdlib pico_multicore hardware_gpio hardware_pwm hardware_irq hardware_sync)

# Add the standard include files to the build
target_include_directories(360System PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(360System
        )

pico_add_extra_outputs(360System)
