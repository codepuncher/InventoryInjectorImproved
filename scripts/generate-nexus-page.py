#!/usr/bin/env python3
"""Generate nexus-page BBCode from README.md user-facing content.

Reads the block between <!-- nexus:start --> and <!-- nexus:end --> in README.md,
converts it to BBCode, substitutes it into the <!-- generated:start/end --> region
of docs/nexus-page.md in memory, and prints the result to stdout.

Usage:
    python3 scripts/generate-nexus-page.py
    python3 scripts/generate-nexus-page.py | xclip -selection clipboard
"""

import argparse
import re
import sys
from pathlib import Path

README = Path(__file__).parent.parent / "README.md"
NEXUS_TEMPLATE = Path(__file__).parent.parent / "docs" / "nexus-page.md"

NEXUS_START = "<!-- nexus:start -->"
NEXUS_END = "<!-- nexus:end -->"
GEN_START = "<!-- generated:start -->"
GEN_END = "<!-- generated:end -->"
BBCODE_FENCE_OPEN = "```bbcode"
BBCODE_FENCE_CLOSE = "```"


def extract_block(text: str, start_marker: str, end_marker: str) -> str:
    start = text.find(start_marker)
    if start == -1:
        sys.exit(f"error: marker not found: {start_marker!r}")
    end = text.find(end_marker, start + len(start_marker))
    if end == -1:
        sys.exit(f"error: marker not found: {end_marker!r}")
    return text[start + len(start_marker):end].strip()


def md_to_bbcode(md: str) -> str:
    lines = md.splitlines()
    output: list[str] = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # ## Heading
        if line.startswith("## "):
            heading = line[3:].strip()
            if output:
                output.append("")
            output.append(f"[font=Times New Roman][size=5]{heading.upper()}[/size][/font]")
            i += 1
            while i < len(lines) and lines[i].strip() == "":
                i += 1
            continue

        # Unordered list block
        if line.startswith("- "):
            items: list[str] = []
            while i < len(lines) and lines[i].startswith("- "):
                items.append(convert_inline(lines[i][2:].strip()))
                i += 1
            output.append("[list]")
            for item in items:
                output.append(f"[*]{item}")
            output.append("[/list]")
            continue

        # Ordered list block
        if re.match(r"\d+\. ", line):
            items = []
            while i < len(lines) and re.match(r"\d+\. ", lines[i]):
                items.append(convert_inline(re.sub(r"^\d+\. ", "", lines[i])))
                i += 1
            output.append("[list=1]")
            for item in items:
                output.append(f"[*]{item}")
            output.append("[/list]")
            continue

        # Bold label line (e.g. **Mod manager (recommended):**) becomes a sub-heading
        label = re.fullmatch(r"\*\*(.+?):?\*\*", line.strip())
        if label:
            output.append(f"[font=Times New Roman][size=4]{label.group(1).upper()}[/size][/font]")
            i += 1
            continue

        # Blank line
        if line.strip() == "":
            output.append("")
            i += 1
            continue

        # Plain paragraph (pass through inline conversion)
        stripped = line.strip()
        if stripped and not stripped.startswith("<!--"):
            output.append(convert_inline(line))

        i += 1

    # Collapse consecutive blank lines to single blank
    result = re.sub(r"\n{3,}", "\n\n", "\n".join(output))
    return result.strip()


def convert_inline(text: str) -> str:
    # [text](url) → [url=url]text[/url]
    # Note: link text containing ] or URLs containing unbalanced ) are not supported.
    text = re.sub(r"\[([^\]]+)\]\(([^)]*(?:\([^)]*\)[^)]*)*)\)", r"[url=\2]\1[/url]", text)
    # **text** → [b]text[/b]
    text = re.sub(r"\*\*(.+?)\*\*", r"[b]\1[/b]", text)
    # `code` → [font=Courier New]code[/font]
    text = re.sub(r"`([^`]+)`", r"[font=Courier New]\1[/font]", text)
    return text


def extract_bbcode_fence(text: str) -> str:
    start = text.find(BBCODE_FENCE_OPEN)
    if start == -1:
        sys.exit(f"error: {BBCODE_FENCE_OPEN!r} fence not found in nexus-page.md")
    newline = text.find("\n", start)
    if newline == -1:
        sys.exit(f"error: no newline after {BBCODE_FENCE_OPEN!r} in nexus-page.md")
    start = newline + 1
    end = text.find(f"\n{BBCODE_FENCE_CLOSE}", start)
    if end == -1:
        sys.exit("error: closing ``` fence not found in nexus-page.md")
    return text[start:end]


def inject_generated(bbcode_content: str, generated: str) -> str:
    start = bbcode_content.find(GEN_START)
    if start == -1:
        sys.exit(f"error: marker not found: {GEN_START!r}")
    end = bbcode_content.find(GEN_END, start + len(GEN_START))
    if end == -1:
        sys.exit(f"error: marker not found: {GEN_END!r}")
    return (
        bbcode_content[:start]
        + GEN_START + "\n"
        + generated + "\n"
        + bbcode_content[end:]
    )


def main() -> None:
    argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    ).parse_args()

    readme = README.read_text(encoding="utf-8")
    template = NEXUS_TEMPLATE.read_text(encoding="utf-8")

    md_block = extract_block(readme, NEXUS_START, NEXUS_END)
    bbcode_generated = md_to_bbcode(md_block)

    bbcode_content = extract_bbcode_fence(template)
    result = inject_generated(bbcode_content, bbcode_generated)
    result = result.replace(GEN_START + "\n", "", 1).replace("\n" + GEN_END, "", 1)

    print(result)


if __name__ == "__main__":
    main()
