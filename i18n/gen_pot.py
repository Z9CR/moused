#!/usr/bin/env python3
"""Extract `_("...")` translation strings from all C++ sources and generate a
gettext .pot template.

Usage:
    python i18n/get_pot.py [output.pot]
        (default output: i18n/moused.pot)
"""

from __future__ import annotations

import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "moused.pot"

SOURCE_EXTS = {".cpp", ".hpp"}
# directories (by name) to skip while scanning ROOT
SKIP_DIRS = {".git", "build", "i18n", "_deps"}

# `_("...")` with C-style escapes; `_` must not be glued to an identifier
# (so `foo_("...")` or `bar_ ( "...")` are not matched).
MSGID_RE = re.compile(r'(?<![A-Za-z0-9_])_\s*\(\s*"((?:[^"\\]|\\.)*)"')


def strip_comments(text: str) -> str:
    """Blank out `//` and `/* */` comments, preserving line numbers.

    Walks the text so `//` inside a string literal (e.g. `_("http://x")`) is
    kept, and `_("...")` written inside a comment is discarded.
    """
    out: list[str] = []
    i, n = 0, len(text)
    state = "code"  # code | string | line_comment | block_comment
    while i < n:
        c = text[i]
        if state == "code":
            if c == '"':
                state = "string"
                out.append(c)
            elif text.startswith("//", i):
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            elif text.startswith("/*", i):
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            else:
                out.append(c)
        elif state == "string":
            out.append(c)
            if c == "\\":  # escaped char: copy it verbatim, don't close the string
                if i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
            elif c == '"':
                state = "code"
        elif state == "line_comment":
            if c == "\n":
                state = "code"
                out.append(c)
            else:
                out.append(" ")
        elif state == "block_comment":
            if text.startswith("*/", i):
                state = "code"
                out.append("  ")
                i += 2
                continue
            out.append(c if c == "\n" else " ")
        i += 1
    return "".join(out)


def extract(path: Path) -> list[tuple[str, str, int]]:
    """Return [(msgid, file_rel, line), ...] for every `_("...")` in path."""
    text = path.read_text(encoding="utf-8", errors="replace")
    cleaned = strip_comments(text)
    rel = path.relative_to(ROOT).as_posix()
    found: list[tuple[str, str, int]] = []
    for m in MSGID_RE.finditer(cleaned):
        line = cleaned.count("\n", 0, m.start()) + 1
        found.append((m.group(1), rel, line))
    return found


def to_po_string(s: str) -> str:
    """Render a msgid as a single-line .po string.

    The source literal already carries C escapes (`\"`, `\\`, `\n`, ...), so it
    is copied verbatim; only raw control characters (multi-line literals) are
    converted to their escapes.
    """
    s = s.replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")
    return f'"{s}"'


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_OUTPUT

    entries: dict[str, list[tuple[str, int]]] = {}
    for ext in SOURCE_EXTS:
        for path in sorted(ROOT.rglob(f"*{ext}")):
            if any(part in SKIP_DIRS for part in path.relative_to(ROOT).parts):
                continue
            for msgid, rel, line in extract(path):
                entries.setdefault(msgid, []).append((rel, line))

    for locs in entries.values():
        locs.sort()

    # same order xgettext uses: group by first occurrence location
    ordered = sorted(entries.items(), key=lambda kv: (kv[1][0][0], kv[1][0][1]))

    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M%z")
    lines = [
        "# SOME DESCRIPTIVE TITLE.",
        "# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER",
        "# This file is distributed under the same license as the moused package.",
        "# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.",
        "#",
        "#, fuzzy",
        'msgid ""',
        'msgstr ""',
        '"Project-Id-Version: moused\\n"',
        '"Report-Msgid-Bugs-To: \\n"',
        f'"POT-Creation-Date: {now}\\n"',
        '"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n"',
        '"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n"',
        '"Language-Team: LANGUAGE <LL@li.org>\\n"',
        '"Language: \\n"',
        '"MIME-Version: 1.0\\n"',
        '"Content-Type: text/plain; charset=UTF-8\\n"',
        '"Content-Transfer-Encoding: 8bit\\n"',
        '"Plural-Forms: nplurals=1; plural=0;\\n"',
        "",
    ]
    for msgid, locs in ordered:
        for rel, line in locs:
            lines.append(f"#: {rel}:{line}")
        lines.append(f"msgid {to_po_string(msgid)}")
        lines.append('msgstr ""')
        lines.append("")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {len(entries)} msgids ({sum(len(v) for v in entries.values())} "
          f"occurrences) to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
