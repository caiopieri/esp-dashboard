import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import usage_agent


class UsageAgentTests(unittest.TestCase):
    def test_extract_nested_claude_token(self):
        blob = json.dumps({"claudeAiOauth": {"accessToken": "token-value"}})
        self.assertEqual(usage_agent.extract_access_token(blob), "token-value")

    def test_percent_accepts_ratio_and_percent(self):
        self.assertEqual(usage_agent.percent("0.45"), 45)
        self.assertEqual(usage_agent.percent("45"), 45)
        self.assertEqual(usage_agent.percent("120"), 100)

    def test_sum_openai_results(self):
        data = {"data": [{"results": [{"input_tokens": 100, "output_tokens": 25, "num_model_requests": 2}]}]}
        self.assertEqual(usage_agent.sum_openai_results(data), {"tokens": 125, "requests": 2})

    def test_omniroute_builds_provider_windows(self):
        rows = [
            {"provider": "claude", "window_key": "session (5h)", "remaining_percentage": 10, "next_reset_at": "2099-01-01T01:00:00Z"},
            {"provider": "claude", "window_key": "weekly (7d)", "remaining_percentage": 68, "next_reset_at": "2099-01-02T01:00:00Z"},
            {"provider": "codex", "window_key": "session", "remaining_percentage": 40, "next_reset_at": "2099-01-01T02:00:00Z"},
            {"provider": "agy", "window_key": "gemini-2.5-flash", "remaining_percentage": 75, "next_reset_at": "2099-01-01T03:00:00Z"},
            {"provider": "agy", "window_key": "gemini-2.5-pro", "remaining_percentage": 55, "next_reset_at": "2099-01-01T04:00:00Z"},
            {"provider": "agy", "window_key": "gemini_weekly", "remaining_percentage": 90, "next_reset_at": "2099-01-02T04:00:00Z"},
        ]
        snapshots = usage_agent.build_omniroute_snapshots(rows, {})
        self.assertEqual(snapshots["claude"]["session_percent"], 90)
        self.assertEqual(snapshots["claude"]["weekly_percent"], 32)
        self.assertIsNone(snapshots["chatgpt"]["session_percent"])
        self.assertEqual(snapshots["chatgpt"]["weekly_percent"], 60)
        self.assertEqual(snapshots["gemini"]["session_percent"], 45)
        self.assertEqual(snapshots["gemini"]["weekly_percent"], 10)

    def test_config_is_created_without_secrets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            config = usage_agent.load_config(path)
            config["device_url"] = "http://192.168.1.10"
            usage_agent.save_config(config, path)
            saved = json.loads(path.read_text())
            self.assertEqual(saved["device_url"], "http://192.168.1.10")
            self.assertNotIn("api_key", path.read_text().lower())


if __name__ == "__main__":
    unittest.main()
