# main/CMakeLists.txt
file(GLOB_RECURSE SOURCES "src/*.cpp")

idf_component_register(SRCS "main.cpp" ${SOURCES}
                       INCLUDE_DIRS "." "src"
                       REQUIRES arduino-esp32 ArduinoJson NmraDcc ESP8266Audio app_update)
