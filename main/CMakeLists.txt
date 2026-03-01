# main/CMakeLists.txt
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

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
                       REQUIRES espressif__arduino-esp32 app_update nvs_flash ${AUTO_REQUIRES})

# Force the linker to include all symbols from this component to ensure our 
# weak function overrides (verifyRollbackLater, etc) are picked up.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--whole-archive" "${CMAKE_CURRENT_BINARY_DIR}/lib${COMPONENT_NAME}.a" "-Wl,--no-whole-archive")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format -Wno-error=missing-field-initializers)
