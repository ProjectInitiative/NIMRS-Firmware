cmake_minimum_required(VERSION 3.16)

# Set target before project()
set(IDF_TARGET esp32s3)

# Use GIT_HASH from Nix environment if available
if(DEFINED ENV{GIT_HASH})
    set(PROJECT_VER $ENV{GIT_HASH})
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(nimrs-firmware)
