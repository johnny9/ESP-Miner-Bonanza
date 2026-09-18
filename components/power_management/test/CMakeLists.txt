idf_component_register(
    SRCS "test_power_policy.c" "test_power_owner.c" "test_asic_reset.c"
         "../../../main/power/asic_reset.c"
         "../../../main/tasks/power_management_task.c"
    INCLUDE_DIRS "." "../../../main" "../../../main/power" "../../../main/tasks"
    REQUIRES unity power_management asic stratum stratum_v2)
