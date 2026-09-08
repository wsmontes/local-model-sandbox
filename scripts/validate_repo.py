#!/usr/bin/env python3
"""Repository-level validation for autonomous agent experiments."""

from __future__ import annotations

import re
import sys
from pathlib import Path

KEBAB_CASE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
RELATIVE_PATH = re.compile(r"(?<![\w.-])((?:\.\./)+[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*)")
TEXT_EXTENSIONS = {
    ".py", ".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs",
    ".sh", ".bash", ".zsh", ".fish",
    ".yaml", ".yml", ".json", ".toml", ".ini", ".cfg", ".conf",
    ".env", ".txt",
}
REQUIRED_METADATA_KEYS = {
    "schema_version",
    "name",
    "version",
    "execution_mode",
    "runtime",
    "entrypoint",
    "contract_version",
}
VALID_EXECUTION_MODES = {"local", "cloud", "hybrid"}


def discover_agents(agents_root: Path) -> list[Path]:
    """Return real agent directories, excluding underscore-prefixed scaffolding."""
    if not agents_root.is_dir():
        return []
    return sorted(
        path for path in agents_root.iterdir()
        if path.is_dir() and not path.name.startswith("_")
    )


def is_kebab_case(name: str) -> bool:
    return bool(KEBAB_CASE.fullmatch(name))


def read_top_level_yaml_scalars(path: Path) -> dict[str, str]:
    """Read simple top-level YAML scalar values without adding a YAML dependency."""
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if not raw_line or raw_line[0].isspace():
            continue
        line = raw_line.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.split(" #", 1)[0].strip()
        if not key or not value or value in {"[]", "{}"}:
            continue
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        values[key] = value
    return values


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def validate_agent_metadata(agent_dir: Path) -> list[str]:
    errors: list[str] = []
    metadata_path = agent_dir / "agent.yaml"
    if not metadata_path.is_file():
        return [f"{agent_dir.name}: missing agent.yaml"]

    try:
        metadata = read_top_level_yaml_scalars(metadata_path)
    except (OSError, UnicodeError) as exc:
        return [f"{agent_dir.name}: cannot read agent.yaml: {exc}"]

    missing = sorted(REQUIRED_METADATA_KEYS - metadata.keys())
    if missing:
        errors.append(
            f"{agent_dir.name}: agent.yaml missing required keys: {', '.join(missing)}"
        )

    execution_mode = metadata.get("execution_mode")
    if execution_mode and execution_mode not in VALID_EXECUTION_MODES:
        errors.append(
            f"{agent_dir.name}: invalid execution_mode {execution_mode!r}; "
            "expected local, cloud, or hybrid"
        )

    entrypoint = metadata.get("entrypoint")
    if entrypoint:
        root = agent_dir.resolve()
        candidate = (agent_dir / entrypoint).resolve()
        if not _is_within(candidate, root):
            errors.append(
                f"{agent_dir.name}: entrypoint escapes agent directory: {entrypoint}"
            )
        elif not candidate.is_file():
            errors.append(
                f"{agent_dir.name}: declared entrypoint does not exist: {entrypoint}"
            )

    return errors


def _should_scan(path: Path) -> bool:
    if path.name in {"README.md", "LICENSE", "LICENSE.md"}:
        return False
    if ".git" in path.parts:
        return False
    return path.suffix.lower() in TEXT_EXTENSIONS or path.suffix == ""


def find_forbidden_references(agent_dir: Path) -> list[str]:
    """Find obvious relative runtime references that escape the agent boundary."""
    errors: list[str] = []
    root = agent_dir.resolve()

    for path in sorted(agent_dir.rglob("*")):
        if path.is_symlink():
            target = path.resolve()
            if not _is_within(target, root):
                errors.append(
                    f"{agent_dir.name}: {path.relative_to(agent_dir)} symlink escapes agent directory"
                )
            continue
        if not path.is_file() or not _should_scan(path):
            continue

        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        for match in RELATIVE_PATH.finditer(text):
            reference = match.group(1)
            target = (path.parent / reference).resolve()
            if not _is_within(target, root):
                errors.append(
                    f"{agent_dir.name}: {path.relative_to(agent_dir)} references "
                    f"path outside agent directory: {reference}"
                )
    return errors


def validate_repository(repo_root: Path) -> list[str]:
    errors: list[str] = []
    agents_root = repo_root / "agents"
    if not agents_root.is_dir():
        return ["repository: missing agents/ directory"]

    for agent_dir in discover_agents(agents_root):
        if not is_kebab_case(agent_dir.name):
            errors.append(
                f"{agent_dir.name}: agent directory name must be lowercase kebab-case"
            )
        errors.extend(validate_agent_metadata(agent_dir))
        errors.extend(find_forbidden_references(agent_dir))

    return errors


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    errors = validate_repository(repo_root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print("OK: repository validation passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
