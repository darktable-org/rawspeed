# dnglab cross-validation testing

`compare_raf_batch.py` batch-compares rawspeed's Fuji RAF decode against
[dnglab](https://github.com/dnglab/dnglab)/rawler's independent decoder,
across a whole directory of `.RAF` files at once. It was used to validate
the lossy-compressed Fuji RAF decoding support added in this directory's
sibling commits, but works for lossless-compressed and uncompressed Fuji
files too, and isn't specific to any one camera body.

For each file, it converts to DNG via `dnglab convert`, reads a small grid
of pixel values from that DNG via `rawpy`, decodes the original RAF with
`rstest -c`, and diffs the two sets of values. This catches real decode
regressions -- wrong pixel values, not just crashes -- across large,
real-world test sets without needing a human to eyeball every image.

## Prerequisites

- **dnglab**: build or download from
  [github.com/dnglab/dnglab](https://github.com/dnglab/dnglab).
- **rstest**: rawspeed's own lightweight decode-test utility, built via
  this repo's own CMake (`cmake --build . --target rstest`). See
  `src/utilities/rstest/` for details -- no need for a full darktable
  build, just rawspeed itself.
- **Python 3 with rawpy**: `pip install rawpy` (a virtual environment is
  recommended, since some systems' system Python blocks direct `pip
  install` -- see PEP 668 / "externally managed environment").

## Enabling per-pixel diagnostic output

`compare_raf_batch.py` relies on `FujiDecompressor.cpp`'s optional
`RAWSPEED_FUJI_DIAG` diagnostic (see that file for details) to get the
values it compares against dnglab. Set the environment variable before
running rstest, or export it for your whole shell session:

    export RAWSPEED_FUJI_DIAG=1

Without this set, rstest decodes normally but prints no pixel values, and
`compare_raf_batch.py` will report every file as an error ("no DIAG pixel
lines found").

## Usage

    python3 compare_raf_batch.py /path/to/raf/directory \
        --dnglab /path/to/dnglab \
        --rstest /path/to/rstest

Both `--dnglab` and `--rstest` default to whatever's on your `PATH` if
omitted. Converted DNGs are kept (not deleted) in
`<directory>/dng_output/` by default -- pass `--dng-dir` to put them
somewhere else. Run `python3 compare_raf_batch.py --help` for the full
option list.

Only `.RAF`/`.raf` files are scanned. Files that don't route through
`FujiDecompressor` at all (e.g. some non-Fuji-compressed formats, if ever
mixed into the same directory) will show up as errored, not failed --
that's expected, not a bug in the comparison.

## Output

A `PASS`/`FAIL`/`ERROR` line per file, plus a final summary count. On
`FAIL`, the first few mismatched coordinates are printed with both
decoders' values side by side, to help pinpoint the divergence quickly
rather than needing a fresh debugging session per failure.

## Side effects

- Converted `.dng` files are kept on disk (in `<directory>/dng_output/` by
  default, or wherever `--dng-dir` points) -- not deleted automatically.
  Clean them up yourself if you don't want them lingering.

`rstest -c`'s `.hash` sidecar files are deleted automatically, both before
each file runs (so a stale hash from an earlier run is never silently
reused instead of a fresh decode) and after (so the script doesn't leave
files behind in your RAF directory).
