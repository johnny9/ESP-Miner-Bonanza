#!/usr/bin/env python3
"""Validate the repository's lightweight feature-specification structure."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPECS = ROOT / "specs"
FEATURE_FILES = ("SPEC.md", "intent.md", "acceptance.md", "design.md", "risks.md")
ALLOWED_LIFECYCLES = {"proposed", "implementing", "supported", "deprecated", "retired"}
PLACEHOLDER = re.compile(r"\{\{[A-Z0-9_]+\}\}")
INDEX_ROW = re.compile(
    r"^\| `([^`]+)` \| [^|]+ \| `?([a-z]+)`? \| "
    r"\[SPEC\.md\]\(([^)]+/SPEC\.md)\) \|",
    re.MULTILINE,
)
MARKDOWN_LINK = re.compile(r"\[[^]]*\]\(([^)]+)\)")
REQUIRED_DESIGN_HEADINGS = (
    "## Components and responsibilities",
    "## Authority and dependency direction",
    "## Current implementation boundary",
    "## Relationships to other feature slices",
)


def feature_specs() -> list[Path]:
    return sorted(
        path for path in SPECS.rglob("SPEC.md") if "_template" not in path.parts
    )


def check(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def check_index(specs: list[Path], failures: list[str]) -> None:
    index = (SPECS / "INDEX.md").read_text(encoding="utf-8")
    entries = INDEX_ROW.findall(index)
    indexed = [SPECS / target for _, _, target in entries]
    check(
        Counter(indexed) == Counter(specs),
        "specs/INDEX.md must list every live feature SPEC.md exactly once",
        failures,
    )
    for area, lifecycle, target in entries:
        spec_path = SPECS / target
        check(lifecycle in ALLOWED_LIFECYCLES, f"invalid lifecycle in index: {lifecycle}", failures)
        if not spec_path.is_file():
            continue
        check(area == spec_path.relative_to(SPECS).parts[0], f"wrong index area for {target}", failures)
        text = spec_path.read_text(encoding="utf-8")
        match = re.search(r"^- \*\*Lifecycle:\*\* ([a-z]+)$", text, re.MULTILINE)
        check(match is not None, f"missing lifecycle metadata: {target}", failures)
        if match:
            check(match.group(1) == lifecycle, f"index lifecycle differs for {target}", failures)


def check_features(specs: list[Path], failures: list[str]) -> None:
    for spec in specs:
        relative = spec.relative_to(SPECS)
        directory = spec.parent
        missing = [name for name in FEATURE_FILES if not (directory / name).is_file()]
        check(not missing, f"missing companion files for {relative}: {', '.join(missing)}", failures)
        text = spec.read_text(encoding="utf-8")
        check(re.search(r"^- \*\*Spec ID:\*\* \S+$", text, re.MULTILINE) is not None,
              f"missing Spec ID: {relative}", failures)
        check(re.search(r"^- \*\*Last reconciled:\*\* (never|\d{4}-\d{2}-\d{2})$", text, re.MULTILINE) is not None,
              f"missing Last reconciled metadata: {relative}", failures)

        if missing:
            continue
        intent = (directory / "intent.md").read_text(encoding="utf-8")
        design = (directory / "design.md").read_text(encoding="utf-8")
        risks = (directory / "risks.md").read_text(encoding="utf-8")
        check("## Non-goals" in intent, f"missing Non-goals heading: {relative}", failures)
        for heading in REQUIRED_DESIGN_HEADINGS:
            check(heading in design, f"missing {heading[3:]} heading: {relative}", failures)
        component_header = re.search(
            r"\|\s*Component\s*\|[^\n]*\|\s*Implementation pointer\s*\|", design
        )
        check(component_header is not None,
              f"design component table must include Implementation pointer: {relative}", failures)
        has_in_scope = re.search(r"(?:^### In$|\*\*In:\*\*)", risks, re.MULTILINE)
        has_out_scope = re.search(r"(?:^### Out$|\*\*Out:\*\*)", risks, re.MULTILINE)
        check(has_in_scope is not None and has_out_scope is not None,
              f"risks must declare in-scope and out-of-scope: {relative}", failures)


def check_documents(failures: list[str]) -> None:
    for document in sorted(SPECS.rglob("*.md")):
        is_template = "_template" in document.parts
        if not is_template:
            check(PLACEHOLDER.search(document.read_text(encoding="utf-8")) is None,
                  f"template placeholder in {document.relative_to(ROOT)}", failures)
        if is_template:
            continue
        text = document.read_text(encoding="utf-8")
        for target in MARKDOWN_LINK.findall(text):
            if "://" in target or target.startswith("#"):
                continue
            target_path = target.split("#", 1)[0]
            if target_path:
                check((document.parent / target_path).exists(),
                      f"broken link in {document.relative_to(ROOT)}: {target}", failures)


def main() -> int:
    failures: list[str] = []
    specs = feature_specs()
    check_index(specs, failures)
    check_features(specs, failures)
    check_documents(failures)
    if failures:
        print("Specification validation failed:", file=sys.stderr)
        print("\n".join(f"- {failure}" for failure in failures), file=sys.stderr)
        return 1
    print("Specification validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
