#!/usr/bin/env python3

import math
import unittest
from unittest import mock

from tools import bzm_1002_regression as regression


def system_info(*, configured_frequency=1200, configured_voltage=3000,
                active_frequency=1200, active_voltage=3000,
                lifecycle="MINING", uptime=100, valid_results=10,
                work_age=10, fault=""):
    return {
        "frequency": configured_frequency,
        "coreVoltage": configured_voltage,
        "uptimeSeconds": uptime,
        "currentWorkAgeSeconds": work_age,
        "asicHealth": {
            "fixedFrequencyMHz": active_frequency,
            "fixedVoltageMV": active_voltage,
            "lifecycle": lifecycle,
            "asicCount": 4,
            "expectedAsicCount": 4,
            "activeEngineCount": 944,
            "expectedEngineCount": 944,
            "locallyValidResults": valid_results,
            "lastFault": fault,
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


class MiningProofTests(unittest.TestCase):
    def test_requires_a_new_proof_and_fresh_pool_work(self):
        ready, detail = regression.mining_proof_status(
            system_info(valid_results=11, work_age=20), 10, 90)
        self.assertTrue(ready)
        self.assertIn("validResults=11 baseline=10", detail)

        self.assertFalse(regression.mining_proof_status(
            system_info(valid_results=10), 10, 90)[0])
        self.assertFalse(regression.mining_proof_status(
            system_info(valid_results=11, work_age=-1), 10, 90)[0])
        self.assertFalse(regression.mining_proof_status(
            system_info(valid_results=11, work_age=91), 10, 90)[0])

    def test_waits_through_transient_mining_until_a_new_proof_arrives(self):
        no_proof = system_info(valid_results=10)
        proven = system_info(valid_results=11)
        with mock.patch.object(
                regression, "get_info", side_effect=[no_proof, proven]), \
             mock.patch.object(regression.time, "sleep"):
            result = regression.wait_for_mining_proof(
                "http://miner", 10, 10, 90)

        self.assertIs(result, proven)

    def test_fails_immediately_when_startup_latches_a_fault(self):
        faulted = system_info(
            lifecycle="FAULT", valid_results=10,
            fault="initial dispatch and local nonce proof missed its deadline")
        with mock.patch.object(regression, "get_info", return_value=faulted):
            with self.assertRaisesRegex(RuntimeError, "entered FAULT"):
                regression.wait_for_mining_proof(
                    "http://miner", 10, 10, 90)


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
