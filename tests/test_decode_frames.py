import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run_decode(log_path: Path) -> str:
    cmd = [sys.executable, str(ROOT / "tools" / "decode_frames.py"), str(log_path)]
    return subprocess.check_output(cmd, text=True)


def test_decode_frames_summary_counts(tmp_path: Path):
    log = tmp_path / "sniffer.jsonl"
    records = [
        {
            "ts_iso": "2026-05-13T22:00:00+02:00",
            "direction": "spa_to_remote",
            "raw_hex": "A5 03 01 A9",
            "valid_checksum": True,
            "decoded": "filter? value=1",
        },
        {
            "ts_iso": "2026-05-13T22:00:01+02:00",
            "direction": "remote_to_spa",
            "raw_hex": "A5 99 01 3F",
            "valid_checksum": True,
            "decoded": "unknown_cmd_0x99 value=1",
        },
        {
            "ts_iso": "2026-05-13T22:00:02+02:00",
            "direction": "spa_to_remote",
            "raw_hex": "A5 03 01 00",
            "valid_checksum": False,
            "decoded": "filter? value=1",
        },
    ]
    with log.open("w", encoding="utf-8") as f:
        for r in records:
            f.write(json.dumps(r) + "\n")

    out = run_decode(log)
    assert "Total frames: 3" in out
    assert "spa_to_remote: 2" in out
    assert "remote_to_spa: 1" in out
    assert "Unknown/unsupported frames: 1" in out
