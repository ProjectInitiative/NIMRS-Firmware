# main/CMakeLists.txt
file(GLOB_RECURSE SOURCES "src/*.cpp")

idf_component_register(SRCS "main.cpp" ${SOURCES}
                       INCLUDE_DIRS "." "src"
                       REQUIRES arduino-esp32 ArduinoJson_7_3_0 NmraDcc_2_0_17 ESP8266Audio_1_9_9 app_update)
