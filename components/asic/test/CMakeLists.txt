file(GLOB ASIC_TEST_SOURCES "*.c")
list(APPEND ASIC_TEST_SOURCES
     "../../../main/http_server/bzm_ota_guard.c"
     "../../../main/device_pins.c"
     "../../../main/power/TPS546.c")

idf_component_register(SRCS ${ASIC_TEST_SOURCES}
                       INCLUDE_DIRS "." "../../../main"
                                    "../../../main/http_server" "../../../main/power"
                                    "../../../main/tasks" "../../../main/self_test"
                                    "../../stratum/test"
                       REQUIRES cmock stratum stratum_v2 asic esp_driver_gpio esp_driver_i2c)
