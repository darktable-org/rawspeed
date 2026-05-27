# AI usage

> [!NOTE]
> The modern (as of 2026-06) AI is a misnomer,
> - it is not conscious / not a consciousness,
> - it is not sentient,
> - it is not intelligent,
> - it does not think,
> - it does not understand the code,
> - it is merely a next-token guesser,
>
> ... therefore it is merely an (hyper-) advanced IDE.

> [!NOTE]
> A contribution is any externally-observable interaction with a project.

> [!CAUTION]
> Failure to follow the following rules *may* result in repercussions,
> possibly without a prior warning.

Rules:
1. It is acceptable to use AI when producing contributions.
> [!WARNING]
> Any and all AI usage **MUST** be fully and explicitly disclosed
> in **every** contribution.
2. All contributions shall be done by conscious, sentient beings.
   Fully autonomous contributions by bots are prohibited[^1].
> [!WARNING]
> Dear contributor, the AI is *your* *tool*, and its output is for *your*
> *consumption*. It is **your** responsibility to consume said output,
> interpret it, and then produce the contribution itself.
> **DO NOT** just query it and post the output,
> *especially* so for all non-code contributions!
3. The contributor (conscious, sentient being) solely bears
   the whole responsibility for the contribution,
   they must understand the problem, and the solution,
   and be able to constructively argue about it.
   "well, AI said so, therefore it is" is not an acceptable approach.

[^1]: Unless explicitly allowed by maintainers on case-by-case basis
      *before* the contribution is submitted,
      in which case the bot owner (conscious, sentient being)
      is recognized as the de facto contributor.

# Agent guidance

These notes apply to automated or AI-assisted agents working in this
repository. They are project-wide working rules, not a substitute for
maintainer review.

## Repository stance

RawSpeed is a low-level raw decoder library. Changes here affect image
decoding correctness, memory safety, performance, and downstream applications
that depend on RawSpeed for first-stage RAW decode.

Prefer small, reviewable diffs. Avoid broad rewrites. Preserve existing
behavior unless the change is explicitly about correcting that behavior and has
tests or sample evidence to support it.

## Before changing code

Before editing, inspect the nearby implementation and follow the local style.
RawSpeed has older and newer C++ patterns mixed together; local consistency
matters more than introducing a new style.

For decoder work, identify:

- the exact decoder involved;
- the raw container or compression mode being changed;
- existing samples that must remain unchanged;
- new samples that exercise the new behavior;
- whether the change affects threaded decoding.

Do not guess file formats from filenames alone. Use parsed metadata, headers,
dimensions, and sample hashes.

## Code style

RawSpeed uses C++20 and clang-format. Follow the existing formatting and
include order.

General rules:

- keep helper types local to `.cpp` files where possible;
- prefer small concrete structs over generic abstractions;
- use existing RawSpeed types and utilities such as `Array1DRef`,
  `Array2DRef`, `Optional`, `ByteStream`, `Buffer`, `invariant`, and
  `implicit_cast`;
- prefer existing RawSpeed types over introducing standard-library substitutes
  such as `std::span` or `std::optional`; add new dependencies or public APIs
  only when there is a clear project-level reason;
- avoid large stack allocations in decoder code;
- do not allocate inside hot pixel loops unless unavoidable;
- keep shared decode state immutable when OpenMP may run code concurrently;
- keep per-thread or per-strip mutable state local to the worker/decoder block.

Use `ThrowRDE` for malformed input or unsupported file data. Use `invariant`
only for internal conditions already guaranteed by earlier validation.

## Decoder changes

Decoder changes require stricter discipline than ordinary refactors.

A decoder patch should make clear:

- what input format or mode is being recognized;
- what bytes are parsed and in what order;
- what state is shared and what state is per-thread;
- what existing sample outputs must remain unchanged;
- what new samples prove the new behavior.

Prefer preserving existing decoder structure unless a rewrite is explicitly
justified by correctness or maintainability.

Do not validate decoder correctness by rendered previews or thumbnails.
Embedded JPEG previews are not raw decode proof.

For compressed raw formats, prefer raw-plane comparison, sample hashes, and
deterministic decode checks.

## Sample files and fixtures

Do not commit raw image files to the repository.

Raw samples belong in the reference sample archive or local test fixtures
outside git. Track samples by SHA256, not by filename, because filenames may be
ambiguous or duplicated.

When documenting sample coverage, include the properties that matter for
decoding, such as:

- make and model;
- compression mode;
- bit depth;
- CFA type;
- raw dimensions;
- block/strip layout;
- relevant header values;
- SHA256.

## Testing

For code changes, build and run the normal test suite before presenting the work
as ready. For docs-only changes, run a whitespace/format check such as
`git diff --check`.

Typical local build:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=ReleaseWithAsserts \
  -DBUILD_TESTING=ON \
  -DBUILD_TOOLS=ON \
  -DBUILD_BENCHMARKING=OFF \
  -DBUILD_FUZZERS=OFF

cmake --build build
ctest --test-dir build --output-on-failure
```

For decoder changes, also run sample-based tests when a reference archive is
available.

Use deterministic checks for threaded decoder paths. If OpenMP is involved,
test with more than one thread count, for example:

```bash
OMP_NUM_THREADS=1 ctest --test-dir build --output-on-failure
OMP_NUM_THREADS=2 ctest --test-dir build --output-on-failure
OMP_NUM_THREADS=8 ctest --test-dir build --output-on-failure
```

A decoder change is not complete merely because a file opens. It should preserve
existing sample hashes, produce stable output for new samples, and use raw-plane
or sample-hash evidence where practical.

## Formatting and static analysis

Run clang-format on changed C++ files before finalizing a patch.

Do not mix formatting-only churn with logic changes. If a formatting cleanup is
necessary, keep it separate from behavioral work.

Avoid suppressing compiler, clang-tidy, or sanitizer warnings. If a suppression
is unavoidable, keep it narrow and explain why.

## Commit and PR shape

Prefer commits that each have one reason to exist.

Good commit boundaries:

- recognize a format or mode without enabling decode;
- parse new metadata or side tables;
- refactor existing code with no behavior change;
- add new decode state;
- enable new decoding behavior;
- add or update tests/hashes;
- clean up diagnostics or documentation.

Avoid single large commits that combine refactoring, parsing, decoding,
formatting, and tests.

For AI-assisted work, disclose AI use in the commit message and PR description
as required by the policy above. The human contributor remains responsible for
understanding and defending the diff.

## What not to do

Do not:

- add binary raw samples to git;
- use thumbnails or rendered JPEGs as decoder proof;
- route around a decoder bug by changing downstream application behavior;
- introduce broad rewrites while fixing a narrow format issue;
- mutate shared decoder state inside OpenMP loops;
- rely on filenames when parsed metadata or hashes are available;
- leave local debug scaffolding in production code;
- claim support for a camera or format without sample-backed validation.
