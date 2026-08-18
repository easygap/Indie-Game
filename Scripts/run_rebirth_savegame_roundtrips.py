"""Run REBIRTH save/exit/restart round trips across independent UE processes.

The runner intentionally uses only explicit local executable paths and argument
lists. It performs no shell evaluation, network access, registry writes, policy
changes, or security exclusions. Keeping process orchestration out of a large
PowerShell script also avoids Defender confusing the QA workload with a
post-exploitation toolkit.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone


COMPLETE_MARKER = "REBIRTH_SPIKE_HARNESS PASS complete boundary=2 cat_choices=5 ch02_time=2 p5=4 p3=7 endings=2"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def archive_manifest(root: Path) -> list[dict[str, object]]:
    resolved = root.resolve(strict=True)
    return [
        {
            "path": path.relative_to(resolved).as_posix(),
            "sizeBytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in sorted(
            (candidate for candidate in resolved.rglob("*") if candidate.is_file()),
            key=lambda candidate: str(candidate).casefold(),
        )
    ]


def assert_archive_unchanged(
    root: Path, before: list[dict[str, object]]
) -> None:
    after = archive_manifest(root)
    if after != before:
        raise RuntimeError(
            "Shipping archive changed during save-game round trips: "
            f"before={len(before)} after={len(after)}"
        )


def resolve_development_runtime(project_root: Path, explicit: str | None) -> Path:
    if explicit:
        runtime = Path(explicit).resolve(strict=True)
    else:
        project_file = project_root / "IndieGame.uproject"
        association = str(
            json.loads(project_file.read_text(encoding="utf-8"))["EngineAssociation"]
        )
        candidates: list[Path] = []
        environment_editor = os.environ.get("IG_UNREAL_EDITOR", "").strip()
        if environment_editor:
            candidate = Path(environment_editor)
            if candidate.name.casefold() in {
                "unrealeditor.exe",
                "unrealeditor-cmd.exe",
            }:
                candidate = candidate.with_name("UnrealEditor-Cmd.exe")
            else:
                candidate = (
                    candidate
                    / "Engine"
                    / "Binaries"
                    / "Win64"
                    / "UnrealEditor-Cmd.exe"
                )
            candidates.append(candidate)
        for program_files in (
            os.environ.get("ProgramFiles"),
            os.environ.get("ProgramFiles(x86)"),
        ):
            if program_files:
                candidates.append(
                    Path(program_files)
                    / "Epic Games"
                    / f"UE_{association}"
                    / "Engine"
                    / "Binaries"
                    / "Win64"
                    / "UnrealEditor-Cmd.exe"
                )
        runtime = next(
            (candidate.resolve() for candidate in candidates if candidate.is_file()),
            None,
        )
        if runtime is None:
            raise FileNotFoundError(
                f"UnrealEditor-Cmd.exe for UE {association} was not found"
            )
    if runtime.name.casefold() != "unrealeditor-cmd.exe":
        raise RuntimeError(
            "Development save-game validation requires UnrealEditor-Cmd.exe: "
            f"{runtime}"
        )
    return runtime


class SaveGameRoundTripRunner:
    def __init__(
        self,
        project_root: Path,
        evidence_directory: Path,
        timeout_seconds: int,
        runtime_command: str | None,
        archive_directory: str | None,
    ) -> None:
        self.project_root = project_root.resolve(strict=True)
        self.project_file = self.project_root / "IndieGame.uproject"
        self.evidence = evidence_directory.resolve()
        self.timeout_seconds = timeout_seconds
        self.using_packaged_shipping = bool(archive_directory)
        self.archive_root: Path | None = None
        self.archive_manifest_before: list[dict[str, object]] = []

        if self.evidence.exists() and any(self.evidence.iterdir()):
            raise RuntimeError(
                f"Evidence directory must be absent or empty: {self.evidence}"
            )
        self.evidence.mkdir(parents=True, exist_ok=True)
        self.user_directory = self.evidence / "UserData"
        self.snapshot_directory = self.evidence / "SaveSnapshots"
        self.user_directory.mkdir()
        self.snapshot_directory.mkdir()

        if self.using_packaged_shipping:
            self.archive_root = Path(str(archive_directory)).resolve(strict=True)
            launcher = self.archive_root / "Windows" / "IndieGame.exe"
            runtime = (
                self.archive_root
                / "Windows"
                / "IndieGame"
                / "Binaries"
                / "Win64"
                / "IndieGame-Win64-Shipping.exe"
            )
            if not launcher.is_file() or not runtime.is_file():
                raise FileNotFoundError(
                    "Archive root must contain the launcher and Shipping runtime: "
                    f"{self.archive_root}"
                )
            self.runtime_command = runtime.resolve()
            self.archive_manifest_before = archive_manifest(self.archive_root)
            if not self.archive_manifest_before:
                raise RuntimeError(f"Shipping archive is empty: {self.archive_root}")
        else:
            self.runtime_command = resolve_development_runtime(
                self.project_root, runtime_command
            )

        now_utc = datetime.now(timezone.utc)
        self.run_id = (
            now_utc.strftime("%Y%m%dT%H%M%S")
            + f"{now_utc.microsecond // 1000:03d}Z_{os.getpid()}"
        )
        self.results: list[dict[str, object]] = []

    def run_probe(
        self,
        mode: str,
        slot: str,
        expected_marker: str,
        *,
        ch02_time_checkpoint: int = -1,
        p5_checkpoint: int = -1,
        p3_checkpoint: int = -1,
        cat_choice: str | None = None,
        ending: str | None = None,
    ) -> None:
        if cat_choice:
            case_name = f"{mode}_{cat_choice}"
        elif ch02_time_checkpoint >= 0:
            case_name = f"{mode}_{ch02_time_checkpoint}"
        elif p5_checkpoint >= 0:
            case_name = f"{mode}_{p5_checkpoint}"
        elif p3_checkpoint >= 0:
            case_name = f"{mode}_{p3_checkpoint}"
        elif ending:
            case_name = f"{mode}_{ending}"
        else:
            case_name = mode

        log_path = self.evidence / f"{case_name}.log"
        receipt_path = self.evidence / f"{case_name}.receipt.txt"
        if self.using_packaged_shipping and receipt_path.exists():
            raise RuntimeError(f"Packaged receipt is not fresh: {receipt_path}")

        map_or_project = (
            "/Game/Maps/Prologue_Morning"
            if self.using_packaged_shipping
            else str(self.project_file)
        )
        arguments = [
            map_or_project,
            "-game",
            "-unattended",
            "-nosplash",
            "-nullrhi",
            "-nosound",
            "-RenderOffscreen",
            "-stdout",
            "-FullStdOutLogOutput",
            f"-abslog={log_path}",
            f"-UserDir={self.user_directory}",
            f"-IGRebirthPersistenceProbe={mode}",
            f"-IGRebirthValidationSlot={slot}",
        ]
        if self.using_packaged_shipping:
            arguments.append(f"-IGRebirthProbeResultPath={receipt_path}")
        if mode == "CatChoiceRead":
            arguments.append("-IGChapterTwo")
        if p3_checkpoint >= 0:
            arguments.append(f"-IGRebirthP3Checkpoint={p3_checkpoint}")
        if ch02_time_checkpoint >= 0:
            arguments.append(
                f"-IGRebirthCH02TimeCheckpoint={ch02_time_checkpoint}"
            )
        if p5_checkpoint >= 0:
            arguments.append(f"-IGRebirthP5Checkpoint={p5_checkpoint}")
        if cat_choice:
            arguments.append(f"-IGRebirthCatChoiceCase={cat_choice}")
        if ending:
            arguments.append(f"-IGRebirthEnding={ending}")

        creation_flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        try:
            completed = subprocess.run(
                [str(self.runtime_command), *arguments],
                cwd=self.runtime_command.parent,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=self.timeout_seconds,
                check=False,
                creationflags=creation_flags,
            )
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(
                f"{case_name} timed out after {self.timeout_seconds} seconds"
            ) from error
        if completed.returncode != 0:
            raise RuntimeError(f"{case_name} exited with code {completed.returncode}")

        receipt_hash: str | None = None
        if self.using_packaged_shipping:
            if not receipt_path.is_file():
                raise RuntimeError(f"{case_name} did not create a Shipping receipt")
            actual_receipt = receipt_path.read_text(encoding="utf-8-sig").strip()
            if actual_receipt != expected_marker:
                raise RuntimeError(
                    f"{case_name} returned an invalid receipt: {actual_receipt}"
                )
            receipt_hash = sha256_file(receipt_path)
        else:
            if not log_path.is_file():
                raise RuntimeError(f"{case_name} did not create a log")
            log_text = log_path.read_text(encoding="utf-8-sig", errors="replace")
            if "REBIRTH_SPIKE FAIL" in log_text or expected_marker not in log_text:
                raise RuntimeError(
                    f"{case_name} did not report {expected_marker!r}"
                )

        log_hash = sha256_file(log_path) if log_path.is_file() else None
        save_files = list(self.user_directory.rglob(f"{slot}.sav"))
        preserves_snapshot = mode in {
            "BoundaryBeforeWrite",
            "BoundaryAfterWrite",
            "CatChoiceWrite",
            "CH02TimeWrite",
            "P5Write",
            "P3Write",
            "EndingWrite",
            "EndingCommit",
        }
        expects_deletion = mode in {
            "BoundaryBeforeRead",
            "BoundaryAfterRead",
            "CatChoiceRead",
            "CH02TimeRead",
            "P5Read",
            "P3Read",
            "EndingVerify",
        }
        snapshot_path: Path | None = None
        snapshot_hash: str | None = None
        save_deleted = False
        if preserves_snapshot:
            if len(save_files) != 1 or save_files[0].stat().st_size <= 0:
                raise RuntimeError(
                    f"{case_name} expected one non-empty {slot}.sav, "
                    f"found {len(save_files)}"
                )
            snapshot_path = self.snapshot_directory / f"{case_name}.sav"
            shutil.copy2(save_files[0], snapshot_path)
            snapshot_hash = sha256_file(snapshot_path)
        elif expects_deletion:
            save_deleted = not save_files
            if not save_deleted:
                raise RuntimeError(f"{case_name} left {slot}.sav after verification")

        self.results.append(
            {
                "case": case_name,
                "status": "PASS",
                "logPath": str(log_path),
                "logSha256": log_hash,
                "receiptPath": str(receipt_path)
                if self.using_packaged_shipping
                else None,
                "receiptSha256": receipt_hash,
                "saveSnapshotPath": str(snapshot_path) if snapshot_path else None,
                "saveSnapshotSha256": snapshot_hash,
                "saveDeleted": save_deleted,
            }
        )
        print(f"REBIRTH_SPIKE_HARNESS PASS case={case_name}", flush=True)

    def execute(self) -> None:
        for phase in ("Before", "After"):
            slot = f"RebirthBoundary_{self.run_id}_{phase}"
            self.run_probe(
                f"Boundary{phase}Write",
                slot,
                "REBIRTH_SPIKE PASS s5_boundary_write "
                f"phase={phase.lower()} immutable=1",
            )
            beat_count = 1 if phase == "After" else 0
            safe_state_count = 3 if phase == "After" else 1
            self.run_probe(
                f"Boundary{phase}Read",
                slot,
                "REBIRTH_SPIKE PASS s5_boundary_resume "
                f"phase={phase.lower()} transient_tags=0 "
                f"safe_states={safe_state_count} beat_count={beat_count} "
                "slot_deleted=1",
            )

        for cat_choice in (
            "CapLeft",
            "CapWaited",
            "CupLeft",
            "CupWaited",
            "PassedBy",
        ):
            slot = f"RebirthCatChoice_{self.run_id}_{cat_choice}"
            self.run_probe(
                "CatChoiceWrite",
                slot,
                f"REBIRTH_SPIKE PASS s5_cat_write case={cat_choice}",
                cat_choice=cat_choice,
            )
            self.run_probe(
                "CatChoiceRead",
                slot,
                "REBIRTH_SPIKE PASS s5_cat_resume "
                f"case={cat_choice} ch01_physical=1 ch02_physical=1 "
                "slot_deleted=1",
                cat_choice=cat_choice,
            )

        for checkpoint in range(2):
            slot = f"RebirthCH02Time_{self.run_id}_{checkpoint}"
            self.run_probe(
                "CH02TimeWrite",
                slot,
                "REBIRTH_SPIKE PASS s6_ch02_time_write "
                f"checkpoint={checkpoint}",
                ch02_time_checkpoint=checkpoint,
            )
            self.run_probe(
                "CH02TimeRead",
                slot,
                "REBIRTH_SPIKE PASS s6_ch02_time_resume "
                f"checkpoint={checkpoint} exact=1",
                ch02_time_checkpoint=checkpoint,
            )

        for checkpoint in range(4):
            slot = f"RebirthP5_{self.run_id}_{checkpoint}"
            self.run_probe(
                "P5Write",
                slot,
                f"REBIRTH_SPIKE PASS s3_p5_write checkpoint={checkpoint}",
                p5_checkpoint=checkpoint,
            )
            self.run_probe(
                "P5Read",
                slot,
                "REBIRTH_SPIKE PASS s3_p5_resume "
                f"checkpoint={checkpoint} exact=1",
                p5_checkpoint=checkpoint,
            )

        for checkpoint in range(7):
            slot = f"RebirthP3_{self.run_id}_{checkpoint}"
            self.run_probe(
                "P3Write",
                slot,
                f"REBIRTH_SPIKE PASS s3_p3_write checkpoint={checkpoint}",
                p3_checkpoint=checkpoint,
            )
            self.run_probe(
                "P3Read",
                slot,
                "REBIRTH_SPIKE PASS s3_p3_resume "
                f"checkpoint={checkpoint} exact=1",
                p3_checkpoint=checkpoint,
            )

        for ending in ("A", "B"):
            slot = f"RebirthEnding_{self.run_id}_{ending}"
            self.run_probe(
                "EndingWrite",
                slot,
                f"REBIRTH_SPIKE PASS s4_ending_write ending={ending}",
                ending=ending,
            )
            self.run_probe(
                "EndingCommit",
                slot,
                "REBIRTH_SPIKE PASS s4_common_commit "
                f"ending={ending} once=1",
                ending=ending,
            )
            self.run_probe(
                "EndingVerify",
                slot,
                "REBIRTH_SPIKE PASS s4_common_restore "
                f"ending={ending} common_once=1 branch_exclusive=1",
                ending=ending,
            )

        if self.archive_root:
            assert_archive_unchanged(
                self.archive_root, self.archive_manifest_before
            )
        commit_sha = subprocess.run(
            ["git", "-C", str(self.project_root), "rev-parse", "--verify", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        summary = {
            "schemaVersion": 1,
            "runId": self.run_id,
            "status": "PASS",
            "commitSha": commit_sha,
            "runtime": str(self.runtime_command),
            "packagedShipping": self.using_packaged_shipping,
            "archiveDirectory": str(self.archive_root) if self.archive_root else None,
            "archiveFileCount": len(self.archive_manifest_before),
            "archiveUnchanged": self.using_packaged_shipping,
            "userDirectory": str(self.user_directory),
            "finishedAtUtc": datetime.now(timezone.utc).isoformat(),
            "memoryBoundaryProcessRestarts": 2,
            "catChoiceProcessRestarts": 5,
            "ch02TimeProcessRestarts": 2,
            "p5ProcessRestarts": 4,
            "p3ProcessRestarts": 7,
            "endingProcessRestarts": 4,
            "processCount": len(self.results),
            "results": self.results,
        }
        (self.evidence / "summary.json").write_text(
            json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(
            COMPLETE_MARKER
            + f" packaged={int(self.using_packaged_shipping)} "
            + f"processes={len(self.results)} "
            + f"archive_files={len(self.archive_manifest_before)} "
            + f"summary={self.evidence / 'summary.json'}",
            flush=True,
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--evidence-directory", required=True)
    parser.add_argument("--runtime-command")
    parser.add_argument("--archive-directory")
    parser.add_argument("--timeout-seconds", type=int, default=180)
    arguments = parser.parse_args()
    if not 30 <= arguments.timeout_seconds <= 1800:
        parser.error("--timeout-seconds must be between 30 and 1800")
    if bool(arguments.runtime_command) == bool(arguments.archive_directory):
        parser.error(
            "provide exactly one of --runtime-command or --archive-directory"
        )
    return arguments


def main() -> int:
    arguments = parse_arguments()
    runner = SaveGameRoundTripRunner(
        Path(arguments.project_root),
        Path(arguments.evidence_directory),
        arguments.timeout_seconds,
        arguments.runtime_command,
        arguments.archive_directory,
    )
    runner.execute()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:  # noqa: BLE001 - harness must report any failure.
        print(f"REBIRTH_SPIKE_HARNESS FAIL {error}", file=sys.stderr, flush=True)
        raise
