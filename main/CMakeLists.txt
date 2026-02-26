# main/CMakeLists.txt
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Auto-detect all Arduino components from the symlinked components directory
set(AUTO_REQUIRES "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../components")
    file(GLOB component_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/../components/*")
    foreach(dir ${component_dirs})
        if(IS_DIRECTORY ${dir})
            get_filename_component(comp_name ${dir} NAME)
            list(APPEND AUTO_REQUIRES ${comp_name})
        endif()
    endforeach()
endif()

idf_component_register(SRCS "main.cpp" ${SOURCES}
                       INCLUDE_DIRS "." "src"
                       REQUIRES espressif__arduino-esp32 app_update ${AUTO_REQUIRES})
