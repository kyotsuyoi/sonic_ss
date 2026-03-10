#!/usr/bin/env python3
"""
adjust_map_x.py
Usage: python adjust_map_x.py <map-file> <subtract_amount>
Reads a tab-separated MAP file and subtracts <subtract_amount> from the X (2nd column) on each data line.
Saves the file in-place and prints a short summary.
"""
import sys
from pathlib import Path

def adjust_map(path: Path, amount: int):
    if not path.exists():
        print(f"File not found: {path}")
        return 1
    changed = 0
    lines_out = []
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            raw = line.rstrip("\n")
            if not raw.strip():
                lines_out.append(line)
                continue
            parts = raw.split("\t")
            if len(parts) >= 2:
                try:
                    x = int(parts[1])
                    parts[1] = str(x - amount)
                    changed += 1
                except Exception:
                    # leave line unchanged if parsing fails
                    pass
            lines_out.append("\t".join(parts) + "\n")
    backup = path.with_suffix(path.suffix + ".bak")
    try:
        path.replace(backup)
    except Exception:
        # fallback: write backup copy
        try:
            path.rename(backup)
        except Exception:
            print("Warning: could not create backup file.")
    with path.open("w", encoding="utf-8") as f:
        f.writelines(lines_out)
    print(f"Processed {path} — adjusted X on {changed} lines. Backup saved as {backup.name}.")
    return 0

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python adjust_map_x.py <map-file> <subtract_amount>")
        sys.exit(2)
    map_path = Path(sys.argv[1])
    try:
        amount = int(sys.argv[2])
    except ValueError:
        print("subtract_amount must be an integer")
        sys.exit(2)
    sys.exit(adjust_map(map_path, amount))
