import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class NonceLogLevelTests(unittest.TestCase):
    def assert_debug_only(self, relative_path: str, message: str) -> None:
        source = (REPO_ROOT / relative_path).read_text(encoding="utf-8")
        escaped = re.escape(message)
        self.assertRegex(source, rf'ESP_LOGD\s*\(\s*TAG,\s*"{escaped}')
        self.assertNotRegex(source, rf'ESP_LOGI\s*\(\s*TAG,\s*"{escaped}')

    def test_per_nonce_details_are_debug_only(self) -> None:
        self.assert_debug_only("main/tasks/asic_result_task.c", "Processing time:")
        self.assert_debug_only("main/tasks/asic_result_task.c", "ID:")
        self.assert_debug_only("main/tasks/scoreboard.c", "New #%d:")

    def test_stratum_payloads_and_successes_are_debug_only(self) -> None:
        self.assert_debug_only("components/stratum/stratum_api.c", "rx:")
        self.assert_debug_only("components/stratum/stratum_api.c", "tx:")
        self.assert_debug_only("components/stratum/stratum_api.c", "Result success")
        self.assert_debug_only("components/stratum/stratum_api.c", "Result failed:")
        self.assert_debug_only("main/tasks/stratum_v1_task.c", "message result accepted")
        self.assert_debug_only("main/tasks/stratum_v1_task.c", "Stratum response time:")
        self.assert_debug_only("main/tasks/stratum_v2_task.c", "Shares accepted:")

    def test_share_failures_remain_visible(self) -> None:
        sv1 = (REPO_ROOT / "main/tasks/stratum_v1_task.c").read_text(encoding="utf-8")
        sv2 = (REPO_ROOT / "main/tasks/stratum_v2_task.c").read_text(encoding="utf-8")
        results = (REPO_ROOT / "main/tasks/asic_result_task.c").read_text(encoding="utf-8")
        self.assertIn('ESP_LOGW(TAG, "message result rejected:', sv1)
        self.assertIn('ESP_LOGW(TAG, "Share rejected:', sv2)
        self.assertIn('ESP_LOGW(TAG, "Invalid work result found,', results)


if __name__ == "__main__":
    unittest.main()
