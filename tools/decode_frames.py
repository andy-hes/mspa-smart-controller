#!/usr/bin/env python3
"""Decode and summarize MSpa sniffer logs (JSONL)."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Decode MSpa sniffer JSONL")
    parser.add_argument("logfile", help="Path to sniffer_log.jsonl")
    args = parser.parse_args()

    path = Path(args.logfile)
    if not path.exists():
        raise SystemExit(f"Log file not found: {path}")

    by_direction = Counter()
    by_command = Counter()
    validity = Counter()
    unknown_count = 0

    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            direction = rec.get("direction", "unknown")
            decoded = rec.get("decoded", "")
            valid = rec.get("valid_checksum")

            by_direction[direction] += 1
            validity[str(valid)] += 1
            by_command[decoded.split(" ")[0] if decoded else "unknown"] += 1
            if "unknown_cmd" in decoded or decoded in {"unknown_sync", "not_4_bytes"}:
                unknown_count += 1

    print("=== MSpa Sniffer Summary ===")
    print(f"Total frames: {sum(by_direction.values())}")
    print("By direction:")
    for k, v in sorted(by_direction.items()):
        print(f"  {k}: {v}")

    print("Checksum validity:")
    for k, v in sorted(validity.items()):
        print(f"  {k}: {v}")

    print("Top decoded commands:")
    for cmd, count in by_command.most_common(20):
        print(f"  {cmd}: {count}")

    print(f"Unknown/unsupported frames: {unknown_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
