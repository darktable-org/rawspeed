#!/usr/bin/env python3
"""
Batch-compares rawspeed's (patched) Fuji lossy-RAF decode against dnglab's
known-good decode, across every .RAF file in a directory.

For each file:
  1. Converts it to DNG via `dnglab convert`.
  2. Reads a 5x5 grid of pixel values (at relative image-fraction
     coordinates -- matches the DIAG spot-check compiled into
     FujiDecompressor.cpp) from the DNG via rawpy.
  3. Runs `rstest -c` on the original RAF and parses its
     "DIAG (row,col) = val" stdout lines. -c forces an actual decode +
     hash computation even when no .hash file exists yet -- with no
     flags, rstest silently skips files that have no accompanying .hash,
     which would otherwise make every file look like a decode failure
     here even though nothing was ever decoded. Note: this creates a
     .hash file next to each RAF as a side effect (harmless, but you'll
     see them appear in the directory).
  4. Compares the two sets of values and reports PASS/FAIL per file.

Requires: rawpy installed (pip install rawpy) in your active environment,
dnglab and rstest binaries built and reachable.

Usage:
    python3 compare_raf_batch.py /path/to/raf/directory \\
        --dnglab /path/to/dnglab \\
        --rstest /path/to/rstest
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

import rawpy

DIAG_PIXEL_RE = re.compile(r"DIAG \((\d+),(\d+)\) = (\d+)")
FRACTIONS = [0.02, 0.25, 0.5, 0.75, 0.98]


def dnglab_ground_truth(dnglab_bin: str, raf_path: Path, dng_dir: Path) -> dict:
    """Convert raf_path to DNG via dnglab (kept on disk in dng_dir, not
    deleted), then read the same relative-fraction pixel grid via rawpy.
    Returns {(row, col): value}."""
    dng_path = dng_dir / (raf_path.stem + ".dng")
    result = subprocess.run(
        [dnglab_bin, "convert", str(raf_path), str(dng_path)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"dnglab convert failed: {result.stderr.strip()}")

    raw = rawpy.imread(str(dng_path))
    img = raw.raw_image
    height, width = img.shape

    values = {}
    for row_frac in FRACTIONS:
        for col_frac in FRACTIONS:
            row = min(height - 1, int(row_frac * height))
            col = min(width - 1, int(col_frac * width))
            values[(row, col)] = int(img[row, col])
    return values


def _hash_file_for(raf_path: Path) -> Path:
    """The .hash sidecar file rstest -c creates next to a RAF, e.g.
    DSCF1234.RAF -> DSCF1234.RAF.hash."""
    return raf_path.with_name(raf_path.name + ".hash")


def rstest_values(rstest_bin: str, raf_path: Path) -> dict:
    """Runs rstest -c on raf_path and parses its DIAG pixel lines from
    stdout. Returns {(row, col): value}.

    -c forces an actual decode + hash computation even when no .hash file
    exists yet -- with no flags, rstest silently skips files that have no
    accompanying .hash, which would otherwise make every file look like a
    decode failure here even though nothing was ever decoded.

    A .hash file is deleted before running (so a stale hash from a
    previous run never gets silently reused/compared instead of a fresh
    decode) and after (so this script doesn't leave sidecar files behind
    that someone re-running it later, without this context, would find
    confusing)."""
    hash_path = _hash_file_for(raf_path)
    hash_path.unlink(missing_ok=True)

    result = subprocess.run(
        [rstest_bin, "-c", str(raf_path)], capture_output=True, text=True,
    )
    combined_output = result.stdout + result.stderr

    hash_path.unlink(missing_ok=True)

    values = {}
    for row, col, val in DIAG_PIXEL_RE.findall(combined_output):
        values[(int(row), int(col))] = int(val)

    if not values:
        raise RuntimeError(
            "no DIAG pixel lines found in rstest output -- either the "
            "spot-check diagnostic isn't compiled into FujiDecompressor.cpp, "
            "or this file isn't a compressed Fuji RAF (uncompressed RAFs "
            "never go through FujiDecompressor at all, so no DIAG output "
            "is expected for those -- not necessarily an error)."
        )
    return values


def compare_one(dnglab_bin, rstest_bin, raf_path: Path, dng_dir: Path):
    truth = dnglab_ground_truth(dnglab_bin, raf_path, dng_dir)
    got = rstest_values(rstest_bin, raf_path)

    mismatches = []
    for coord, truth_val in truth.items():
        got_val = got.get(coord)
        if got_val is None:
            mismatches.append((coord, truth_val, "MISSING"))
        elif got_val != truth_val:
            mismatches.append((coord, truth_val, got_val))

    return mismatches, len(truth)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path,
                       help="Directory containing .RAF files to test")
    parser.add_argument("--dnglab", default="dnglab",
                       help="Path to the dnglab binary (default: dnglab on PATH)")
    parser.add_argument("--rstest", default="rstest",
                       help="Path to the rstest binary (default: rstest on PATH)")
    parser.add_argument("--dng-dir", type=Path, default=None,
                       help="Directory to keep converted DNGs in (default: "
                            "<directory>/dng_output -- created if missing, "
                            "files are NOT deleted afterward)")
    args = parser.parse_args()

    raf_files = sorted(args.directory.glob("*.RAF")) + sorted(args.directory.glob("*.raf"))
    if not raf_files:
        print(f"No .RAF files found in {args.directory}")
        sys.exit(1)

    dng_dir = args.dng_dir or (args.directory / "dng_output")
    dng_dir.mkdir(parents=True, exist_ok=True)

    print(f"Found {len(raf_files)} RAF file(s). Comparing against dnglab...")
    print(f"Converted DNGs will be kept in: {dng_dir}\n")

    passed, failed, errored = [], [], []

    for raf_path in raf_files:
        print(f"{raf_path.name} ... ", end="", flush=True)
        try:
            mismatches, total = compare_one(
                args.dnglab, args.rstest, raf_path, dng_dir
            )
        except Exception as e:
            print(f"ERROR ({e})")
            errored.append((raf_path, str(e)))
            continue

        if not mismatches:
            print(f"PASS ({total}/{total} coordinates match)")
            passed.append(raf_path)
        else:
            print(f"FAIL ({len(mismatches)}/{total} coordinates differ)")
            for coord, truth_val, got_val in mismatches[:5]:
                print(f"    {coord}: dnglab={truth_val} rstest={got_val}")
            if len(mismatches) > 5:
                print(f"    ... and {len(mismatches) - 5} more")
            failed.append(raf_path)

    print("\n" + "=" * 60)
    print(f"Summary: {len(passed)} passed, {len(failed)} failed, "
          f"{len(errored)} errored (of {len(raf_files)} total)")
    if failed:
        print("\nFailed files:")
        for p in failed:
            print(f"  {p}")
    if errored:
        print("\nErrored files:")
        for p, err in errored:
            print(f"  {p}: {err}")


if __name__ == "__main__":
    main()
