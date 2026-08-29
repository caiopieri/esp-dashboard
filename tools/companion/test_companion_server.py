import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(__file__))
from desk_assistant_server import (
    ActionError,
    CardValidationError,
    _extract_json,
    generate_draft,
    execute_action_request,
    public_actions,
    validate_card,
    validate_card_package,
)


class FakeResponse:
    def __init__(self, payload):
        self.payload = json.dumps(payload).encode("utf-8")

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def read(self, _limit):
        return self.payload


class CompanionContractTests(unittest.TestCase):
    def test_validate_normalizes_safe_card(self):
        card = validate_card({"id": "weather", "title": "Clima", "type": "metric", "data": {"source": "runtime", "namespace": "room", "key": "temp", "value": "23.5"}, "body": {"label": "Sala", "unit": "°C", "max": 100}, "theme": {"accent": "#74F0C1"}})
        self.assertEqual(card["kind"], "declarative")
        self.assertEqual(card["data"]["source"], "runtime")
        self.assertEqual(card["theme"]["accent"], "#74F0C1")

    def test_rejects_executable_or_unknown_type(self):
        with self.assertRaises(CardValidationError):
            validate_card({"id": "run", "title": "Run", "type": "javascript"})

    def test_rejects_long_lists(self):
        with self.assertRaises(CardValidationError):
            validate_card({"id": "list", "title": "List", "type": "list", "body": {"items": list("123456789")}})

    def test_validates_versioned_card_package(self):
        package = validate_card_package({
            "packageVersion": 1,
            "kind": "desk-assistant-card",
            "minimumSchemaVersion": 1,
            "compatibility": {"displays": ["320x240", "e-ink"]},
            "card": {"id": "clock", "title": "Relógio", "type": "clock"},
        })
        self.assertEqual(package["card"]["type"], "clock")
        self.assertEqual(package["compatibility"]["displays"], ["320x240", "e-ink"])

    def test_rejects_unknown_package_display(self):
        with self.assertRaises(CardValidationError):
            validate_card_package({
                "packageVersion": 1,
                "kind": "desk-assistant-card",
                "compatibility": {"displays": ["webgl"]},
                "card": {"id": "x", "title": "X", "type": "text"},
            })

    def test_actions_are_allowlisted_and_need_confirmation(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / "actions.json"
            filename.write_text(json.dumps({"actions": [{
                "id": "open_dashboard",
                "title": "Abrir dashboard",
                "kind": "open_url",
                "url": "http://localhost:4173",
            }]}), encoding="utf-8")
            with patch.dict(os.environ, {"DESK_ACTIONS_FILE": str(filename)}, clear=False):
                self.assertEqual(public_actions()[0]["id"], "open_dashboard")
                challenge = execute_action_request("open_dashboard")
                self.assertEqual(challenge["status"], "confirmation_required")
                with patch("desk_assistant_server.webbrowser.open", return_value=True) as opened:
                    result = execute_action_request("open_dashboard", challenge["confirmationToken"])
                self.assertEqual(result["status"], "executed")
                opened.assert_called_once_with("http://localhost:4173", new=2)

    def test_actions_reject_unknown_id(self):
        with patch.dict(os.environ, {"DESK_ACTIONS_FILE": ""}, clear=False):
            with self.assertRaises(ActionError):
                execute_action_request("not_configured")

    def test_extracts_fenced_json(self):
        self.assertEqual(_extract_json("```json\n{\"card\": {}}\n```"), {"card": {}})

    def test_ai_requires_configuration_before_network(self):
        with patch.dict(os.environ, {"DESK_AI_ENDPOINT": "", "DESK_AI_API_KEY": ""}, clear=False):
            with self.assertRaises(RuntimeError):
                generate_draft("um card de clima")

    def test_ai_response_becomes_normalized_draft(self):
        provider_payload = {
            "choices": [{"message": {"content": "```json\n{\"card\": {\"id\": \"weather\", \"title\": \"Clima\", \"type\": \"metric\", \"data\": {\"source\": \"runtime\", \"namespace\": \"room\", \"key\": \"temp\", \"value\": \"--\"}, \"body\": {\"label\": \"Sala\", \"unit\": \"°C\"}}}\n```"}}]
        }
        with patch.dict(os.environ, {"DESK_AI_ENDPOINT": "http://provider.test/v1/chat/completions", "DESK_AI_API_KEY": "test-key"}, clear=False):
            with patch("desk_assistant_server.request.urlopen", return_value=FakeResponse(provider_payload)) as mocked:
                result = generate_draft("mostre a temperatura da sala")
        self.assertEqual(result["card"]["id"], "weather")
        self.assertEqual(result["card"]["kind"], "declarative")
        self.assertEqual(mocked.call_count, 1)


if __name__ == "__main__":
    unittest.main()
