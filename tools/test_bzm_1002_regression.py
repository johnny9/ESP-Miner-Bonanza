#!/usr/bin/env python3

import math
import unittest
from unittest import mock

from tools import bzm_1002_regression as regression


def system_info(*, configured_frequency=1200, configured_voltage=3000,
                active_frequency=1200, active_voltage=3000,
                lifecycle="MINING", uptime=100):
    return {
        "frequency": configured_frequency,
        "coreVoltage": configured_voltage,
        "uptimeSeconds": uptime,
        "asicHealth": {
            "fixedFrequencyMHz": active_frequency,
            "fixedVoltageMV": active_voltage,
            "lifecycle": lifecycle,
        },
    }


class ConfiguredTuningTargetTests(unittest.TestCase):
    def test_accepts_the_saved_target_after_the_live_ramp(self):
        matches, detail = regression.configured_tuning_target_status(
            system_info())

        self.assertTrue(matches)
        self.assertIn("active=1200 MHz/3000 mV", detail)

    def test_rejects_the_validated_startup_point_during_a_live_ramp(self):
        matches, detail = regression.configured_tuning_target_status(
            system_info(active_frequency=800, active_voltage=2800))

        self.assertFalse(matches)
        self.assertIn("configured=1200 MHz/3000 mV", detail)

    def test_rejects_missing_or_nonfinite_values(self):
        self.assertFalse(
            regression.configured_tuning_target_status({})[0])
        self.assertFalse(regression.configured_tuning_target_status(
            system_info(active_frequency=math.nan))[0])

    def test_waits_until_the_saved_target_is_reached(self):
        ramping = system_info(active_frequency=1175)
        converged = system_info()
        with mock.patch.object(
                regression, "get_info", side_effect=[ramping, converged]), \
             mock.patch.object(regression.time, "sleep"):
            result = regression.wait_for_tuning_target("http://miner", 10)

        self.assertIs(result, converged)


class FanProjectionTests(unittest.TestCase):
    def test_accepts_the_bridges_rounded_pid_command(self):
        info = system_info()
        info.update({"fanspeed": 73.6223, "fanrpm": 2820})
        info["asicHealth"].update({"fanPercent": 74, "fanRPM": 2820})

        matches, detail = regression.fan_projection_status(info)

        self.assertTrue(matches)
        self.assertIn("roundedCommand=74%", detail)

    def test_rejects_wrong_command_or_rpm_projections(self):
        wrong_command = system_info()
        wrong_command.update({"fanspeed": 73.4, "fanrpm": 2820})
        wrong_command["asicHealth"].update(
            {"fanPercent": 74, "fanRPM": 2820})
        wrong_rpm = system_info()
        wrong_rpm.update({"fanspeed": 74.0, "fanrpm": 2800})
        wrong_rpm["asicHealth"].update(
            {"fanPercent": 74, "fanRPM": 2820})

        self.assertFalse(regression.fan_projection_status(wrong_command)[0])
        self.assertFalse(regression.fan_projection_status(wrong_rpm)[0])

    def test_waits_for_the_bridge_heartbeat_to_catch_the_pid_target(self):
        stale = system_info()
        stale.update({"fanspeed": 76.6169, "fanrpm": 2940})
        stale["asicHealth"].update({"fanPercent": 76, "fanRPM": 2940})
        converged = system_info()
        converged.update({"fanspeed": 76.6169, "fanrpm": 2940})
        converged["asicHealth"].update(
            {"fanPercent": 77, "fanRPM": 2940})
        with mock.patch.object(
                regression, "get_info", side_effect=[stale, converged]), \
             mock.patch.object(regression.time, "sleep"):
            result = regression.wait_for_fan_projection(
                "http://miner", 10)

        self.assertIs(result, converged)


class RestartObservationTests(unittest.TestCase):
    def test_requires_uptime_to_reset_without_a_disconnect(self):
        self.assertFalse(regression.restart_observed(
            600, system_info(uptime=601), False))
        self.assertTrue(regression.restart_observed(
            600, system_info(uptime=3), False))

    def test_accepts_a_new_response_after_a_disconnect(self):
        self.assertTrue(regression.restart_observed(
            1, system_info(uptime=2), True))
        self.assertFalse(regression.restart_observed(
            600, system_info(uptime=700), True))

    def test_rejects_missing_or_nonfinite_uptime(self):
        self.assertFalse(regression.restart_observed(600, {}, True))
        self.assertFalse(regression.restart_observed(
            600, system_info(uptime=math.inf), True))


if __name__ == "__main__":
    unittest.main()
