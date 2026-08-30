#!/usr/bin/env python3
"""Package a self-contained, student-downloadable source tree for one lab variant.

Only what a team actually needs is included: the common source/include/build
files, and the serial reference for their variant. Nothing from charge/ or the
other nine variants ships. The serial reference is renamed
to `pipeline-serial.cpp` and wired into CMakeLists.txt directly (no VARIANT cache
variable, no -DVARIANT flag, no environment variable), and the variant's spec
(`charge/variant-N/spec.md`) is folded into the end of the packaged README.md
instead of shipping as its own file.

Usage:
    charge/create-student-variant.py <variant> [--source DIR] [--output DIR]
"""

import argparse
import shutil
import sys
import tarfile
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SOURCE = (
    Path.cwd()
)
DEFAULT_OUTPUT = SCRIPT_DIR / "dist"

ARCHIVE_ROOT_NAME = "inf8601-labo01"

EXCLUDE_TOP_LEVEL_DIRS = {".git", ".plans", "charge", "build"}
EXCLUDE_TOP_LEVEL_PREFIXES = ("build-",)


def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "variant",
        type=int,
        choices=range(1, 11),
        metavar="VARIANT",
        help="variant number (1-10)",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"path to the unified lab source tree (default: {DEFAULT_SOURCE})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"directory to write the .tar.gz into (default: {DEFAULT_OUTPUT})",
    )
    return parser.parse_args()


def check_source(source: Path, variant: int):
    if not source.is_dir():
        sys.exit(f"error: source tree not found: {source}")

    spec = source / "charge" / f"variant-{variant}" / "spec.md"
    serial = source / "serials" / f"pipeline-serial.variant-{variant}.cpp"
    if not spec.is_file():
        sys.exit(f"error: missing spec for variant {variant}: {spec}")
    if not serial.is_file():
        sys.exit(f"error: missing serial reference for variant {variant}: {serial}")


def copy_tree(source: Path, staging: Path, variant: int):
    serial_name = f"pipeline-serial.variant-{variant}.cpp"

    def ignore(dir_path, names):
        rel = Path(dir_path).relative_to(source)
        ignored = set()

        for name in names:
            if (
                rel == Path(".")
                and (
                    name in EXCLUDE_TOP_LEVEL_DIRS
                    or name.startswith(EXCLUDE_TOP_LEVEL_PREFIXES)
                )
                or name.endswith(".png")
            ):
                ignored.add(name)

        if rel == Path("serials"):
            ignored |= {name for name in names if name != serial_name}

        return ignored

    shutil.copytree(source, staging, ignore=ignore)
    (staging / "serials" / serial_name).rename(
        staging / "serials" / "pipeline-serial.cpp"
    )


def replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        sys.exit(
            f"error: expected exactly one occurrence of {what} to patch; "
            "source file has changed upstream, update this script"
        )
    return text.replace(old, new, 1)


def patch_readme(staging: Path, source: Path, variant: int):
    path = staging / "README.md"
    text = path.read_text()

    # The unified README already reads like the student-facing version (Code section bullet,
    # Spécifications body) except for the variant number in the Spécifications heading.
    text = replace_once(
        text,
        "## Spécifications\n",
        f"## Spécifications — Variant {variant}\n",
        "the Spécifications heading",
    )

    spec_text = (source / "charge" / f"variant-{variant}" / "spec.md").read_text()
    _, _, stages = spec_text.partition(
        "\n\n"
    )  # drop the "# Variant N" heading, already in the section title above
    text = text.rstrip("\n") + "\n\n## Étages du pipeline\n\n" + stages.strip() + "\n"

    path.write_text(text)


def make_archive(staging: Path, output: Path, variant: int) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    archive_path = output / f"inf8601-labo01-variant-{variant}.tar.gz"

    with tarfile.open(archive_path, "w:gz") as tar:
        tar.add(staging, arcname=ARCHIVE_ROOT_NAME)

    return archive_path


def main():
    args = parse_args()
    check_source(args.source, args.variant)

    with tempfile.TemporaryDirectory(prefix="student-variant-") as tmp:
        staging = Path(tmp) / ARCHIVE_ROOT_NAME
        copy_tree(args.source, staging, args.variant)
        patch_readme(staging, args.source, args.variant)

        archive_path = make_archive(staging, args.output, args.variant)

    size_mb = archive_path.stat().st_size / (1024 * 1024)
    print(f"wrote {archive_path} ({size_mb:.1f} MiB)")


if __name__ == "__main__":
    main()
