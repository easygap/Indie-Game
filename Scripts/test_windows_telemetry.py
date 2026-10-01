"""잘못된 프로세스·누락·변조를 정상 계측으로 처리하지 않는지 확인한다."""

import copy
import tempfile
import unittest
from pathlib import Path
import json
import csv

import summarize_windows_telemetry as telemetry


def present_rows():
    return [{"Application": "IndieGame-Win64-Shipping.exe", "ProcessID": "123",
             "SwapChainAddress": "0x123", "Runtime": "DXGI", "Dropped": "0",
             "msInPresentAPI": "0.2", "msUntilDisplayed": "2", "msBetweenPresents": "16",
             "msBetweenDisplayChange": "16.67", "PresentMode": "Composed: Flip",
             "QPCTime": str(100 + index * .016)} for index in range(120)]


def memory_rows():
    return [{"ElapsedSeconds": str(index + 1), "ProcessId": "123", "DedicatedBytes": "2000000000",
             "SharedBytes": "100000000", "ValidCounters": "2", "QueryStatus": "ok"} for index in range(10)]


class WindowsTelemetryTest(unittest.TestCase):
    def test_display_and_memory_are_separate_observations(self):
        result = telemetry.analyze_present(present_rows(), 123, "IndieGame-Win64-Shipping.exe")
        self.assertTrue(result["display_observation_sufficient"])
        self.assertEqual(result["displayed_frames"], 120)
        self.assertEqual(telemetry.analyze_memory(memory_rows(), 123)["sampled_peak_dedicated_gb"], 2)

    def test_other_process_is_rejected(self):
        for rows, key, analyze in [(present_rows(), "ProcessID", lambda r: telemetry.analyze_present(r, 123, "IndieGame-Win64-Shipping.exe")),
                                   (memory_rows(), "ProcessId", lambda r: telemetry.analyze_memory(r, 123))]:
            rows[3][key] = "456"
            with self.assertRaisesRegex(ValueError, "다른 프로세스"):
                analyze(rows)

    def test_unshown_frames_do_not_pass(self):
        rows = present_rows()
        for row in rows:
            row.update(Dropped="1", msUntilDisplayed="0", msBetweenDisplayChange="0")
        result = telemetry.analyze_present(rows, 123, "IndieGame-Win64-Shipping.exe")
        self.assertFalse(result["display_observed"])
        self.assertFalse(result["display_observation_sufficient"])

    def test_nan_and_reversed_clock_are_rejected(self):
        for field, value in [("msBetweenPresents", "nan"), ("msUntilDisplayed", "-1"), ("QPCTime", "99")]:
            rows = present_rows()
            rows[3][field] = value
            with self.assertRaises(ValueError):
                telemetry.analyze_present(rows, 123, "IndieGame-Win64-Shipping.exe")

    def test_system_uptime_is_not_reported_as_display_interval(self):
        rows = present_rows()
        rows[40]["msBetweenDisplayChange"] = "6492976.66"
        result = telemetry.analyze_present(rows, 123, "IndieGame-Win64-Shipping.exe")
        self.assertTrue(result["display_observation_sufficient"])
        self.assertFalse(result["display_interval_valid"])
        self.assertEqual(result["display_interval_out_of_capture_count"], 1)
        self.assertIsNone(result["between_display_changes_ms"])
        self.assertEqual(result["reported_between_display_changes_ms"]["max"], 6492976.66)

    def test_absent_memory_remains_absent(self):
        rows = memory_rows()
        rows[0].update(DedicatedBytes="-1", SharedBytes="-1", ValidCounters="0", QueryStatus="unavailable")
        result = telemetry.analyze_memory(rows, 123)
        self.assertEqual(result["valid_coverage"], .9)
        self.assertEqual(result["unavailable_samples"], 1)
        rows[0]["DedicatedBytes"] = "0"
        with self.assertRaises(ValueError):
            telemetry.analyze_memory(rows, 123)

    def test_memory_budget_uses_decimal_gigabytes(self):
        rows = memory_rows()
        rows[3]["DedicatedBytes"] = "5600000000"
        self.assertFalse(telemetry.analyze_memory(rows, 123)["sampled_peak_within_5_5gb"])

    def test_all_zero_memory_does_not_pass(self):
        rows = memory_rows()
        for row in rows:
            row["DedicatedBytes"] = "0"
        with self.assertRaises(ValueError):
            telemetry.analyze_memory(rows, 123)

    def test_original_hash_and_sample_count_are_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, rows in [("present.csv", present_rows()), ("memory.csv", memory_rows())]:
                with (root / name).open("w", encoding="utf-8", newline="") as handle:
                    writer = csv.DictWriter(handle, fieldnames=rows[0])
                    writer.writeheader()
                    writer.writerows(rows)
            metadata = {"processId": 123, "presentationMode": "Windowed", "offscreen": False,
                        "presentCsv": "present.csv", "memoryCsv": "memory.csv",
                        "presentCsvSha256": telemetry.digest(root / "present.csv"),
                        "memoryCsvSha256": telemetry.digest(root / "memory.csv"),
                        "sampleCount": 10, "queryFailures": 0, "shippingSha256": "a" * 64,
                        "executableName": "IndieGame-Win64-Shipping.exe"}
            for field, value, message in [("presentCsvSha256", "0" * 64, "원본이 바뀌었습니다"),
                                           ("sampleCount", 9, "행 수"), ("queryFailures", 1, "누락된"),
                                           ("presentCsv", "../present.csv", "같은 폴더")]:
                changed = copy.deepcopy(metadata)
                changed[field] = value
                (root / "windows-telemetry.json").write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    telemetry.summarize(root)


if __name__ == "__main__":
    unittest.main()
