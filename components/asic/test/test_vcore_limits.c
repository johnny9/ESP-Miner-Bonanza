/* Isolated instance of the production VCORE implementation. Only NVS ports
 * are replaced; configuration selection and voltage arithmetic stay real.
 * ESP peripheral headers make this a QEMU fixture. No power I/O is invoked. */
#include "sdkconfig.h"
/* The test application does not load main/Kconfig.projbuild. Supply its pin
 * defaults to compile the unused GPIO paths; tests only call the getter. */
#ifndef CONFIG_GPIO_ASIC_ENABLE
#define CONFIG_GPIO_ASIC_ENABLE 10
#endif
#ifndef CONFIG_GPIO_PLUG_SENSE
#define CONFIG_GPIO_PLUG_SENSE 12
#endif

#define nvs_config_has_key vcore_fixture_has_key
#define nvs_config_get_float vcore_fixture_get_float
#define nvs_config_get_u16 vcore_fixture_get_u16
#define VCORE_init vcore_fixture_init
#define VCORE_is_initialized vcore_fixture_is_initialized
#define VCORE_set_voltage vcore_fixture_set_voltage
#define VCORE_bzm_set_rail_enabled vcore_fixture_set_rail_enabled
#define VCORE_bzm_set_runtime_voltage vcore_fixture_set_runtime_voltage
#define VCORE_bzm_force_regulator_off vcore_fixture_force_regulator_off
#define VCORE_bzm_snapshot vcore_fixture_snapshot
#define VCORE_get_voltage_mv vcore_fixture_get_voltage_mv
#define VCORE_get_voltage_min_mv vcore_fixture_get_voltage_min_mv
#define VCORE_check_fault vcore_fixture_check_fault
#define VCORE_get_fault_string vcore_fixture_get_fault_string
#define VCORE_get_phase_count vcore_fixture_get_phase_count
#include "../../../main/power/vcore.c"

#include "unity.h"

static bool override_minimum;
static float minimum_v;
static GlobalState state;

bool vcore_fixture_has_key(NvsConfigKey key)
{
    return override_minimum && key == NVS_CONFIG_TPS546_VOUT_MIN;
}
float vcore_fixture_get_float(NvsConfigKey key)
{
    TEST_ASSERT_EQUAL(NVS_CONFIG_TPS546_VOUT_MIN, key);
    return minimum_v;
}
uint16_t vcore_fixture_get_u16(NvsConfigKey key)
{
    (void)key;
    TEST_FAIL_MESSAGE("No integer NVS override should be read");
    return 0;
}

TEST_CASE("VCORE minimum rounds up per domain and honors NVS overrides", "[power-management][qemu-integration]")
{
    state = (GlobalState){.DEVICE_CONFIG = {
        .TPS546 = true,
        .family = {.id = HEX, .name = "Hex", .voltage_domains = 3,
                   .tps546_config = &TPS546_CONFIG_HEX},
    }};
    override_minimum = false;
    TEST_ASSERT_EQUAL_INT16(834, VCORE_get_voltage_min_mv(&state));
    override_minimum = true;
    minimum_v = 2.8f;
    TEST_ASSERT_EQUAL_INT16(934, VCORE_get_voltage_min_mv(&state));
    override_minimum = false;
}

TEST_CASE("VCORE minimum preserves default non-PMBus and zero-domain behavior", "[power-management][qemu-integration]")
{
    state = (GlobalState){.DEVICE_CONFIG = {
        .TPS546 = true, .family = {.name = "Default", .voltage_domains = 0},
    }};
    override_minimum = false;
    TEST_ASSERT_EQUAL_INT16(1000, VCORE_get_voltage_min_mv(&state));
    state.DEVICE_CONFIG.TPS546 = false;
    TEST_ASSERT_EQUAL_INT16(0, VCORE_get_voltage_min_mv(&state));
}

TEST_CASE("Bonanza voltage minimum follows its fixed board profile", "[power-management][qemu-integration]")
{
    state = (GlobalState){.DEVICE_CONFIG = {
        .TPS546 = true, .bonanza_bridge = true,
        .family = {.id = BONANZA, .name = "Bonanza", .voltage_domains = 1},
    }};
    override_minimum = true;
    minimum_v = 5.0f;
    TEST_ASSERT_EQUAL_INT16(2100, VCORE_get_voltage_min_mv(&state));
    override_minimum = false;
}
