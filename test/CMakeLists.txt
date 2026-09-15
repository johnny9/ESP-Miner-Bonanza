# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.16)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "stratum stratum_v2 asic bonanza_display_protocol button_input identify_mode log_level_config hashrate_counter_window" CACHE STRING "List of components to test")

set(IDF_TARGET "esp32s3" CACHE STRING "IDF target")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


idf_build_set_property(COMPILE_DEFINITIONS "-DCONFIG_ASIC_FREQUENCY=100" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DCONFIG_BZM_1002_LEAD_ZEROS=36" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DCONFIG_BZM_1002_SENSOR_TDM_GAP_COUNT=63" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DCONFIG_BZM_1002_MIN_NONCE_DIFFICULTY=16" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DCONFIG_BZM_1002_TELEMETRY_MAX_AGE_MS=2000" APPEND)

project(unit_test_stratum)

option(ENABLE_UBSAN "Enable undefined behavior checks in unit tests" OFF)

if(ENABLE_UBSAN)
    # Instrument repository components and their tests without exhausting RAM
    # by also instrumenting ESP-IDF. Its UBSan runtime aborts on the first error.
    get_filename_component(components_dir "../components" ABSOLUTE)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(component IN LISTS build_components)
        idf_component_get_property(component_dir ${component} COMPONENT_DIR)
        string(FIND "${component_dir}" "${components_dir}/" component_prefix)
        if(component_prefix EQUAL 0)
            idf_component_get_property(component_lib ${component} COMPONENT_LIB)
            # ESP-IDF register macros require the shift-base exception. Its
            # heap guarantees 4-byte alignment, while GCC checks 8-byte alignment
            # for types containing doubles, including cJSON objects.
            target_compile_options(${component_lib} PRIVATE
                -fsanitize=undefined -fno-sanitize=shift-base,alignment)
        endif()
    endforeach()
endif()
