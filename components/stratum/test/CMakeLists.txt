file(GLOB STRATUM_TEST_SOURCES "*.c")
list(APPEND STRATUM_TEST_SOURCES
    "../../../main/tasks/stratum_submission.c"
    "../../../main/tasks/stratum_work.c")
idf_component_register(SRCS ${STRATUM_TEST_SOURCES}
                    INCLUDE_DIRS "." "../../../main" "../../../main/tasks"
                    REQUIRES cmock stratum stratum_v2 asic espressif__cjson)
