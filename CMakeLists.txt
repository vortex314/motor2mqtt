# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(WIFI_SSID $ENV{WIFI_SSID})
set(WIFI_PASS $ENV{WIFI_PASS})
set(ESPBAUD 921600) # doesn't work

include_directories(main   )
add_definitions(-DWIFI_PASS=${WIFI_PASS} -DWIFI_SSID=${WIFI_SSID} -DESP32_IDF -DMQTT_HOST=limero.ddns.net -DMQTT_PORT=1883 -DHOSTNAME=triac)
# add_compile_options( -mtext-section-literals -ffunction-sections )

set(EXTRA_COMPONENT_DIRS /home/lieven/workspace/limero/esp32_idf)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(motor2mqtt)
