#!/usr/bin/env python3
"""Lightweight integration tests for tools/metrics."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
METRICS = ROOT / "tools" / "metrics"


class MetricsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        directory = Path(self.temp.name)
        self.source = directory / "input.c"
        self.source.write_text("int f(void) { return 0; }\n", encoding="utf-8")
        self.fixture = directory / "fixture.json"
        self.fake = directory / "codemetrics"
        self.fake.write_text("#!/bin/sh\ncat \"$METRICS_FIXTURE\"\n", encoding="utf-8")
        self.fake.chmod(0o755)

    def tearDown(self):
        self.temp.cleanup()

    def run_metrics(self, records=None, source=None, env=None, args=None):
        if records is not None:
            self.fixture.write_text(json.dumps(records), encoding="utf-8")
        if source is not None:
            self.source.write_text(source, encoding="utf-8")
        environment = os.environ.copy()
        environment.update(CODEMETRICS=str(self.fake), METRICS_FIXTURE=str(self.fixture))
        if env:
            environment.update(env)
        command = [str(METRICS), str(self.source)] if args is None else [str(METRICS), *args]
        return subprocess.run(command, text=True, capture_output=True, env=environment)

    @staticmethod
    def record(name="f", cyclomatic=1, cognitive=0):
        return {"file": "input.c", "function": name, "cyclomatic": cyclomatic,
                "cognitive": cognitive, "start_line": 1, "end_line": 1}

    def test_all_metrics_pass(self):
        result = self.run_metrics([self.record(cyclomatic=21, cognitive=21)])
        self.assertEqual(result.returncode, 0)
        self.assertIn("Overall: PASS", result.stdout)

    def test_cyclomatic_at_limit_fails(self):
        result = self.run_metrics([self.record(cyclomatic=22)])
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAIL (cyclomatic)", result.stdout)

    def test_cognitive_at_limit_fails(self):
        result = self.run_metrics([self.record(cognitive=22)])
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAIL (cognitive)", result.stdout)

    def test_file_loc_at_limit_fails(self):
        result = self.run_metrics([self.record()], source="x;\n" * 500)
        self.assertEqual(result.returncode, 1)
        self.assertIn("File LOC", result.stdout)

    def test_multiple_failing_functions_are_named(self):
        records = [self.record("one", 22, 0), self.record("two", 1, 22)]
        result = self.run_metrics(records)
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAIL (cyclomatic)", result.stdout)
        self.assertIn("FAIL (cognitive)", result.stdout)

    def test_missing_file_argument(self):
        result = self.run_metrics(args=[])
        self.assertEqual(result.returncode, 2)
        self.assertIn("usage:", result.stderr)

    def test_nonexistent_file(self):
        result = self.run_metrics(args=["absent.c"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("does not exist", result.stderr)

    def test_missing_codemetrics(self):
        result = self.run_metrics([self.record()], env={"CODEMETRICS": "not-present-anywhere"})
        self.assertEqual(result.returncode, 2)
        self.assertIn("pipx install codemetrics", result.stderr)

    def test_malformed_and_unexpected_json(self):
        for contents in ("not json", "{}", '[{"function": "f"}]'):
            with self.subTest(contents=contents):
                self.fixture.write_text(contents, encoding="utf-8")
                result = self.run_metrics()
                self.assertEqual(result.returncode, 2)
                self.assertIn("codemetrics JSON", result.stderr)


if __name__ == "__main__":
    unittest.main()
