"""Run with mining-qa-testcode's miner-test and an explicit writable LAN profile.

This extension uses the runner's device/OTA/baseline/cleanup lifecycle. It does
not need USB, change pool credentials, or replace the production self-test.
"""
import asyncio
import json
import time

from miner_testcode.artifacts import append_jsonl
from miner_testcode.errors import InterfaceError
from miner_testcode.testcase import MinerTestCase


class SelfTestRegressionTest(MinerTestCase):
    async def _status(self):
        data = await self.device.api.request(
            "GET", "/api/system/selftest", max_bytes=4096, timeout=8
        )
        status = json.loads(data)
        self.assertIn(status["status"], {"idle", "running", "passed", "failed", "cancelled"})
        append_jsonl(self.artifacts.path / "self-test.jsonl", {"at": time.time(), **status})
        return status

    async def _wait(self, predicate, timeout=240):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                status = await self._status()
            except InterfaceError:
                await asyncio.sleep(1)
                continue
            if predicate(status):
                return status
            await asyncio.sleep(1)
        self.fail("Self-test did not reach the expected state within its deadline")

    async def _start(self):
        await self._wait_normal_boot()
        self.assertEqual((await self._status())["status"], "idle")
        self.addAsyncCleanup(self._stop_diagnostic)
        # A reboot can close the response after accepting the one-shot write.
        # Never retry a start: subsequent status must independently prove it.
        try:
            await self.device.api.post_json("/api/system/selftest", {"action": "start"})
        except InterfaceError:
            pass
        return await self._wait(lambda s: s["status"] != "idle", timeout=120)

    async def _stop_diagnostic(self):
        try:
            status = await self._status()
        except InterfaceError:
            status = await self._wait(lambda s: True, timeout=120)
        if status["active"] and status["status"] != "passed":
            try:
                await self.device.api.post_json("/api/system/selftest", {"action": "cancel"})
            except InterfaceError:
                pass
        await self._wait_normal_boot()
        # Runner cleanup will independently compare and restore its baseline.

    async def _wait_normal_boot(self):
        await self._wait(lambda s: s["status"] == "idle", timeout=120)
        # HTTP and idle status are available before Bonanza's staged startup.
        # Let its controller finish before the runner compares pause state.
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            try:
                info = await self.device.current_info()
            except InterfaceError:
                await asyncio.sleep(1)
                continue
            health = info.get("asicHealth") or {}
            self.assertFalse(health.get("lastFaultCode"), health)
            if (health.get("lifecycle", "MINING") == "MINING"
                    and not info.get("miningPaused") and info.get("workReceived", 0) > 0):
                return
            await asyncio.sleep(1)
        self.fail("Normal mining did not resume after diagnostic reboot")

    async def _assert_local_mining(self):
        info = await self.device.current_info()
        self.assertEqual(info["workReceived"], 0, "Diagnostic boot received pool work")
        self.assertEqual(info["sharesAccepted"], 0, "Diagnostic proof reached pool accounting")
        if info["ASICModel"] == "BZM":
            health = info["asicHealth"]
            self.assertEqual(health["asicCount"], 4)
            self.assertEqual(health["activeEngineCount"], 944)
            self.assertEqual(health["fixedFrequencyMHz"], 800)
            self.assertEqual(health["fixedVoltageMV"], 2800)
            self.assertEqual(health["lastFaultCode"], 0)
            append_jsonl(self.artifacts.path / "local-asic-evidence.jsonl", {
                "at": time.time(), "workReceived": info["workReceived"],
                "sharesAccepted": info["sharesAccepted"], "asicHealth": health,
            })

    async def test_01_local_self_test_completes_and_confirms_shutdown(self):
        for action in ({}, {"action": "invalid"}):
            with self.assertRaisesRegex(InterfaceError, "400"):
                await self.device.api.post_json("/api/system/selftest", action)
        status = await self._start()
        self.assertEqual(status["status"], "running", status)
        with self.assertRaisesRegex(InterfaceError, "409"):
            await self.device.api.post_json("/api/system/selftest", {"action": "start"})
        status = await self._wait(lambda s: s["acceptedNonces"] >= 2 or s["status"] != "running")
        self.assertEqual(status["status"], "running", status)
        await self._assert_local_mining()
        status = await self._wait(lambda s: s["status"] in {"passed", "failed", "cancelled"})
        self.assertEqual(status["status"], "passed", status)
        self.assertTrue(status["cleanupConfirmed"], status)
        self.assertFalse(status["workerRunning"], status)
        self.assertGreaterEqual(status["acceptedNonces"], 2)
        self.logger.info("Local self-test passed with nonce proof and confirmed shutdown")
        await self._wait_normal_boot()

    async def test_02_cancel_local_work_and_return_to_normal_mining(self):
        status = await self._start()
        self.assertEqual(status["status"], "running", status)
        status = await self._wait(lambda s: s["workerRunning"] or s["status"] != "running")
        self.assertEqual(status["status"], "running", status)
        await self._assert_local_mining()
        try:
            await self.device.api.post_json("/api/system/selftest", {"action": "cancel"})
        except InterfaceError:
            pass
        status = await self._wait(lambda s: s["status"] == "idle", timeout=120)
        self.assertFalse(status["workerRunning"], status)
        await self._wait_normal_boot()
        self.logger.info("Cancellation stopped local work and returned through a normal boot")
