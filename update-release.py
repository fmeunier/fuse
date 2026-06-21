#!/usr/bin/env python3
"""
update-release.py — bump the release date (and optionally version) in
man pages, README, and configure.ac.

Usage:
    ./update-release.py --date "12th June, 2026"
    ./update-release.py --date "12th June, 2026" --version "1.4.6"
    ./update-release.py --date "12th June, 2026" --version "1.4.6.1"
    ./update-release.py --date "12th June, 2026" --dry-run

Only touches the .TH line in man/*.1, the title and last line of README,
and the version defines in configure.ac. Copyright lines are never modified.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MAN_DIR = ROOT / "man"
README = ROOT / "README"
CONFIGURE_AC = ROOT / "configure.ac"

# .TH line example:
#   .TH tzxlist 1 "10th March, 2026" "Version 1.4.5" "Emulators"
TH_RE = re.compile(
    r'^(\s*\.TH\s+\S+\s+\d+\s+)'           # prefix up to date
    r'"([^"]*)"'                            # old date in quotes
    r'(\s+)'                                # whitespace before version
    r'"([^"]*)"'                            # old version in quotes
    r'(.*)',                                # rest of line
    re.MULTILINE,
)


def update_man_page(path, new_date, new_version, dry_run):
    """Return True if the file was (or would be) changed."""
    text = path.read_text()
    new_text, count = TH_RE.subn(
        lambda m: (m.group(1)
                   + f'"{new_date}"'
                   + m.group(3)
                   + (f'"{new_version}"' if new_version else f'"{m.group(4)}"')
                   + m.group(5)),
        text,
    )
    if count == 0:
        print(f"  skip  {path.name}  (no .TH match)")
        return False
    if dry_run:
        print(f"  would update  {path.name}")
    else:
        path.write_text(new_text)
        print(f"  updated  {path.name}")
    return True


def update_readme(new_date, new_version, dry_run):
    """Update README release metadata.

    Updates the title line version when requested and the last line date.
    The last line is expected to be just the date, e.g. "10th March, 2026".
    """
    if not README.exists():
        print(f"  skip  README  (not found)")
        return False

    text = README.read_text()
    lines = text.splitlines(keepends=True)
    changed = False
    updates = []

    if new_version:
        title_pattern = re.compile(
            r'^(The Free Unix Spectrum Emulator \(Fuse\) )\S+(\r?\n?)$'
        )
        match = title_pattern.match(lines[0]) if lines else None
        if not match:
            print(f"  skip  README  (title line doesn't match expected format)")
            return False
        new_title = f"{match.group(1)}{new_version}{match.group(2)}"
        if lines[0] != new_title:
            lines[0] = new_title
            changed = True
            updates.append(f"title -> {new_version}")

    # The last non-empty line should be the date
    last_line = lines[-1].strip()
    if not last_line:
        print(f"  skip  README  (last line is blank)")
        return False

    # Only update if the last line looks like a date (no other content)
    date_pattern = re.compile(
        r'^\d+(?:st|nd|rd|th)\s+\w+,\s+\d{4}$'
    )
    if not date_pattern.match(last_line):
        print(f"  skip  README  (last line '{last_line}' doesn't look like a date)")
        return False

    new_last_line = new_date + "\n"
    if lines[-1] != new_last_line:
        lines[-1] = new_last_line
        changed = True
        updates.append(f"last line -> {new_date}")

    if not changed:
        print(f"  skip  README  (already up to date)")
        return False

    new_text = "".join(lines)
    detail = ", ".join(updates)
    if dry_run:
        print(f"  would update  README  ({detail})")
    else:
        README.write_text(new_text)
        print(f"  updated  README  ({detail})")
    return True


def parse_version(version_str):
    """Parse a version string like '1.4.6' or '1.4.6.0' into components.

    Returns (major, minor, micro, nano) — nano defaults to 0.
    """
    parts = version_str.split(".")
    if len(parts) < 3 or len(parts) > 4:
        print(f"error: version '{version_str}' must be X.Y.Z or X.Y.Z.N",
              file=sys.stderr)
        sys.exit(1)
    major = parts[0]
    minor = parts[1]
    micro = parts[2]
    nano = parts[3] if len(parts) == 4 else "0"
    return major, minor, micro, nano


def update_configure_ac(version_str, dry_run):
    """Update the version defines in configure.ac."""
    if not CONFIGURE_AC.exists():
        print(f"  skip  configure.ac  (not found)")
        return False

    major, minor, micro, nano = parse_version(version_str)

    text = CONFIGURE_AC.read_text()

    replacements = {
        r'm4_define\(\[fuse_version\],\s*\[\S+\]\)':
            f'm4_define([fuse_version], [{version_str}])',
        r'm4_define\(\[fuse_major_version\],\s*\[\S+\]\)':
            f'm4_define([fuse_major_version], [{major}])',
        r'm4_define\(\[fuse_minor_version\],\s*\[\S+\]\)':
            f'm4_define([fuse_minor_version], [{minor}])',
        r'm4_define\(\[fuse_micro_version\],\s*\[\S+\]\)':
            f'm4_define([fuse_micro_version], [{micro}])',
        r'm4_define\(\[fuse_nano_version\],\s*\[\S+\]\)':
            f'm4_define([fuse_nano_version],  [{nano}])',
    }

    changed = False
    for pattern, replacement in replacements.items():
        new_text, count = re.subn(pattern, replacement, text)
        if count:
            changed = True
        text = new_text

    if not changed:
        print(f"  skip  configure.ac  (no version defines found)")
        return False

    if dry_run:
        print(f"  would update  configure.ac  (version -> {version_str})")
    else:
        CONFIGURE_AC.write_text(text)
        print(f"  updated  configure.ac  (version -> {version_str})")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Update release date (and optionally version) in man pages, README, and configure.ac.",
    )
    parser.add_argument(
        "--date",
        required=True,
        help='New release date string, e.g. "12th June, 2026"',
    )
    parser.add_argument(
        "--version",
        default=None,
        help='New version string, e.g. "1.4.6" or "1.4.6.1". If omitted, version is left unchanged.',
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would change without writing files.",
    )
    args = parser.parse_args()

    if not MAN_DIR.is_dir():
        print(f"error: man directory not found at {MAN_DIR}", file=sys.stderr)
        sys.exit(1)

    man_pages = sorted(MAN_DIR.glob("*.1"))
    if not man_pages:
        print(f"error: no *.1 files found in {MAN_DIR}", file=sys.stderr)
        sys.exit(1)

    print(f"Scanning {len(man_pages)} man page(s), README, and configure.ac ...")
    print(f"  new date    : {args.date}")
    if args.version:
        print(f"  new version : {args.version}")
    print()

    changed = 0
    for mp in man_pages:
        if update_man_page(mp, args.date, args.version, args.dry_run):
            changed += 1

    if update_readme(args.date, args.version, args.dry_run):
        changed += 1

    if args.version and update_configure_ac(args.version, args.dry_run):
        changed += 1

    print()
    if args.dry_run:
        print(f"Dry run — {changed} file(s) would be updated.")
    else:
        print(f"Done — {changed} file(s) updated.")


if __name__ == "__main__":
    main()
