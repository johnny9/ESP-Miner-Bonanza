cmake_minimum_required(VERSION 3.22)

project(esp_miner_host_tests LANGUAGES C)

include(CTest)
include(CheckCCompilerFlag)
include(FetchContent)

# cJSON 1.7.19 predates CMake 4 and declares an older policy baseline.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

option(ESP_MINER_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" ON)
option(ESP_MINER_ENABLE_COVERAGE "Enable GCC coverage instrumentation" OFF)

if(ESP_MINER_ENABLE_SANITIZERS AND ESP_MINER_ENABLE_COVERAGE)
    message(FATAL_ERROR "Sanitizers and coverage must run in separate builds")
endif()

if(ESP_MINER_ENABLE_COVERAGE AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Native coverage requires GCC and gcov")
endif()

# Keep native dependencies exact and independent from packages installed on
# the developer's machine. These versions match ESP-IDF 6.0.2.
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG bf560290f6020737eafaa8b5cbd2177c3956c03f
    GIT_SHALLOW FALSE
)

set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(ENABLE_TARGET_EXPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG c859b25da02955fef659d658b8f324b5cde87be3
    GIT_SHALLOW FALSE
)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GEN_FILES OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-4.1.0/mbedtls-4.1.0.tar.bz2
    URL_HASH SHA256=377a09cf8eb81b5fb2707045e5522d5489d3309fed5006c9874e60558fc81d10
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(unity cjson mbedtls)

target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE=1)

# Use the inventory checker's manifest as the test registration source of
# truth. Production sources and platform adapters remain explicit below.
set(host_test_manifest "${CMAKE_CURRENT_SOURCE_DIR}/test_sources.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${host_test_manifest}")
file(STRINGS "${host_test_manifest}" host_test_lines)
set(host_test_sources)
foreach(source IN LISTS host_test_lines)
    string(REGEX REPLACE "#.*$" "" source "${source}")
    string(STRIP "${source}" source)
    if(NOT source STREQUAL "")
        list(APPEND host_test_sources "../${source}")
    endif()
endforeach()

add_executable(
    esp_miner_host_tests
    src/host_test_runner.c
    src/host_platform_stubs.c
    src/esp_log_host.c
    ${host_test_sources}
    ../components/power_management/power_policy.c
    ../main/power/asic_reset.c
    ../components/asic/test/bm1366_test_instance.c
    ../components/asic/test/bm1368_test_instance.c
    ../components/asic/test/bm1370_test_instance.c
    ../components/asic/test/bm1373_test_instance.c
    ../components/asic/test/bm1397_test_instance.c
    ../components/asic/test/bm13xx_test_harness.c
    ../components/asic/test/bm1397_test_harness.c
    ../components/asic/test/result_task_test_instance.c
    ../components/stratum/test/job_pipeline_test_harness.c
    ../components/stratum/test/mining_allocator_fault_injector.c
    ../components/stratum/test/mining_test_instance.c
    ../components/asic/asic_job_store.c
    ../components/asic/asic_result_handler.c
    ../components/asic/bitmain_job_packet.c
    ../components/asic/bm_hashrate.c
    ../components/asic/bm_result.c
    ../components/asic/asic_share.c
    ../components/asic/asic_capabilities.c
    ../components/asic/bzm_balanced_ramp.c
    ../components/asic/bzm_bringup.c
    ../components/asic/bzm_dispatch_gate.c
    ../components/asic/bzm_frequency.c
    ../components/asic/bzm_frequency_qualification.c
    ../components/asic/bzm_lease_guard.c
    ../components/asic/bzm_overheat_recovery.c
    ../components/asic/bzm_power.c
    ../components/asic/bzm.c
    ../components/asic/bzm_result_dedup.c
    ../components/asic/bzm_work.c
    ../components/asic/bzm_registers.c
    ../components/asic/bzm_running_evidence.c
    ../components/asic/bzm_runtime_health.c
    ../components/asic/bzm_supervisor.c
    ../components/asic/bzm_telemetry.c
    ../components/asic/bzm_topology.c
    ../components/asic/bzm_tps546_verify.c
    ../components/asic/bzm_validation.c
    ../components/bonanza_display_protocol/bonanza_display_protocol.c
    ../components/button_input/button_input.c
    ../components/hashrate_counter_window/hashrate_counter_window.c
    ../components/identify_mode/identify_mode.c
    ../components/log_level_config/log_level_config.c
    ../components/stratum/mining_job.c
    ../main/http_server/bzm_ota_guard.c
    ../components/stratum/stratum_protocol.c
    ../components/stratum/sv1_protocol.c
    ../components/stratum/miner_job.c
    ../components/stratum/coinbase_decoder.c
    ../components/stratum/base58.c
    ../components/mining_job/asic_job.c
    ../components/stratum/mining.c
    ../components/stratum/utils.c
    ../components/stratum/hash_bytes.c
    ../components/stratum/segwit_addr.c
    ../components/stratum_v2/sv2_protocol.c
    ../components/asic/pll.c
    ../components/asic/asic_common.c
    ../components/asic/crc.c
)

# Keep the imported libbase58 implementation unchanged in this foundation PR.
# Its three VLAs should be audited separately from the host-test migration.
set_source_files_properties(
    ../components/stratum/base58.c
    PROPERTIES COMPILE_OPTIONS -Wno-vla
)

# Characterization compiles complete, unchanged driver sources. BM1368's
# legacy loop counters compare against sizeof; BM1397's no-op version-mask
# entry point intentionally ignores its argument. Keep these accommodations
# confined to those instances; test bodies retain the strict warning set.
set_source_files_properties(../components/asic/test/bm1368_test_instance.c
    PROPERTIES COMPILE_OPTIONS -Wno-sign-compare)
set_source_files_properties(../components/asic/test/bm1397_test_instance.c
    PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)

target_compile_features(esp_miner_host_tests PRIVATE c_std_11)
target_compile_definitions(
    esp_miner_host_tests
    PRIVATE
        _POSIX_C_SOURCE=200809L
        # Same sensor cadence as the Bonanza QEMU fixtures and production default.
        CONFIG_BZM_1002_SENSOR_TDM_GAP_COUNT=63
)
target_compile_options(
    esp_miner_host_tests
    PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/include/host_test_compat.h
        -ffunction-sections
        -fdata-sections
        -Wall
        -Wextra
        -Werror
)

function(esp_miner_add_supported_warning target warning)
    string(MAKE_C_IDENTIFIER "${warning}" warning_id)
    set(support_variable "ESP_MINER_SUPPORTS_${warning_id}")
    check_c_compiler_flag("-Werror ${warning}" "${support_variable}")
    if(${support_variable})
        target_compile_options("${target}" PRIVATE "${warning}")
    endif()
endfunction()

set(
    esp_miner_strict_warnings
    -Wformat=2
    -Wvla
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wundef
    -Wimplicit-fallthrough
    -Wdate-time
    -Wduplicated-branches
    -Wduplicated-cond
    -Wlogical-op
    -Wunreachable-code
    -Wbidi-chars=any
    -Wnull-dereference
    -Wpointer-arith
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND esp_miner_strict_warnings -Wcast-align=strict)
else()
    list(APPEND esp_miner_strict_warnings -Wcast-align)
endif()

foreach(warning IN LISTS esp_miner_strict_warnings)
    esp_miner_add_supported_warning(esp_miner_host_tests "${warning}")
endforeach()

target_link_options(esp_miner_host_tests PRIVATE -Wl,--gc-sections)

target_include_directories(
    esp_miner_host_tests
    PRIVATE
        include
        ${cjson_SOURCE_DIR}
        ../components/mining_job/include
        ../components/stratum/include
        ../components/stratum_v2/include
        ../components/power_management/include
        ../components/asic/include
        ../components/asic/private_include
        ../components/bonanza_display_protocol/include
        ../components/button_input/include
        ../components/hashrate_counter_window/include
        ../components/identify_mode/include
        ../components/log_level_config/include
        ../main/http_server
        ../main/power
        ../main
        ../main/tasks
        ../main/self_test
        ../components/stratum/test
)

target_link_libraries(
    esp_miner_host_tests
    PRIVATE
        unity
        cjson
        TF-PSA-Crypto::tfpsacrypto
        m
)

if(ESP_MINER_ENABLE_SANITIZERS)
    target_compile_options(
        esp_miner_host_tests
        PRIVATE
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer
    )
    target_link_options(
        esp_miner_host_tests
        PRIVATE
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
    )
endif()

if(ESP_MINER_ENABLE_COVERAGE)
    target_compile_options(
        esp_miner_host_tests
        PRIVATE
            --coverage
            -fprofile-abs-path
            -O0
            -g3
    )
    target_link_options(esp_miner_host_tests PRIVATE --coverage)
endif()

add_test(NAME host.unit COMMAND esp_miner_host_tests)
set_tests_properties(host.unit PROPERTIES LABELS "host;unit")
