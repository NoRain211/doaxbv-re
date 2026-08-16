#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import argparse
import configparser
import fnmatch
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tarfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "public-export.json"

SECRET_PATTERNS = (
    re.compile(r"-----BEGIN (?:RSA|OPENSSH|EC|DSA|PGP) PRIVATE KEY-----"),
    re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(r"\bgh[pousr]_[A-Za-z0-9]{20,}\b"),
    re.compile(r"\bsk-[A-Za-z0-9]{20,}\b"),
    re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}\b"),
    re.compile(
        r"(?:postgres|postgresql|mysql|mongodb(?:\+srv)?)://"
        r"[^\s:/]+:[^\s@]+@",
        re.IGNORECASE,
    ),
)

BYTE_LITERAL_PATTERNS = (
    re.compile(
        r"(?:0x[0-9A-Fa-f]{2}(?![0-9A-Fa-f])[uUlL]*\s*,\s*){3,}"
        r"0x[0-9A-Fa-f]{2}(?![0-9A-Fa-f])[uUlL]*"
    ),
    re.compile(r'"(?:[0-9A-Fa-f]{2} ){3,}[0-9A-Fa-f]{2}"'),
)

ABSOLUTE_USER_PATH = re.compile(
    r"(?:[A-Za-z]:[\\/]" r"Users[\\/]|/" r"Users/|/" r"home/)",
    re.IGNORECASE,
)
INTERNAL_PATH = re.compile(
    r"(?:private[\\/]recomp[\\/]|reference[\\/]native-host-frozen)",
    re.IGNORECASE,
)


class ExportError(RuntimeError):
    pass


@dataclass(frozen=True)
class Gitlink:
    path: str
    url: str
    commit: str


@dataclass(frozen=True)
class Manifest:
    include: tuple[str, ...]
    quarantine: tuple[str, ...]
    allowed_private_paths: frozenset[str]
    forbidden_extensions: frozenset[str]
    byte_literal_exceptions: frozenset[str]
    max_file_bytes: int
    gitlinks: tuple[Gitlink, ...]


def load_manifest(path: Path = MANIFEST_PATH) -> Manifest:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("version") != 1:
        raise ExportError("unsupported public export manifest version")

    exception_hashes = {
        entry["sha256"].lower() for entry in data["byte_literal_exceptions"]
    }
    gitlinks = tuple(
        Gitlink(
            path=entry["path"],
            url=entry["url"],
            commit=entry["commit"].lower(),
        )
        for entry in data.get("gitlinks", [])
    )
    for gitlink in gitlinks:
        if not re.fullmatch(r"[0-9a-f]{40}", gitlink.commit):
            raise ExportError(f"invalid gitlink commit for {gitlink.path}")
    return Manifest(
        include=tuple(data["include"]),
        quarantine=tuple(data["quarantine"]),
        allowed_private_paths=frozenset(data["allowed_private_paths"]),
        forbidden_extensions=frozenset(
            extension.lower() for extension in data["forbidden_extensions"]
        ),
        byte_literal_exceptions=frozenset(exception_hashes),
        max_file_bytes=int(data["max_file_bytes"]),
        gitlinks=gitlinks,
    )


def run_git(*args: str, cwd: Path = ROOT, binary: bool = False):
    result = subprocess.run(
        ["git", *args],
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=not binary,
    )
    if result.returncode != 0:
        error = result.stderr if not binary else result.stderr.decode(errors="replace")
        raise ExportError(f"git {' '.join(args)} failed: {error.strip()}")
    return result.stdout


def index_entries(root: Path = ROOT) -> tuple[tuple[str, ...], dict[str, str]]:
    files: list[str] = []
    gitlinks: dict[str, str] = {}
    output = run_git("ls-files", "--stage", "-z", cwd=root)
    for record in output.split("\0"):
        if not record:
            continue
        metadata, separator, path = record.partition("\t")
        if not separator:
            raise ExportError("invalid git index entry")
        mode, object_id, stage = metadata.split()
        if stage != "0":
            raise ExportError(f"unmerged git index entry: {path}")
        if mode == "160000":
            gitlinks[path] = object_id.lower()
        else:
            files.append(path)
    return tuple(files), gitlinks


def tree_gitlinks(root: Path = ROOT) -> dict[str, str]:
    gitlinks: dict[str, str] = {}
    output = run_git("ls-tree", "-r", "-z", "HEAD", cwd=root)
    for record in output.split("\0"):
        if not record:
            continue
        metadata, separator, path = record.partition("\t")
        if not separator:
            raise ExportError("invalid git tree entry")
        mode, object_type, object_id = metadata.split()
        if mode == "160000" and object_type == "commit":
            gitlinks[path] = object_id.lower()
    return gitlinks


def configured_gitlinks(root: Path = ROOT) -> dict[str, str]:
    path = root / ".gitmodules"
    if not path.is_file():
        return {}

    parser = configparser.ConfigParser(interpolation=None)
    try:
        parser.read_string(path.read_text(encoding="utf-8"))
    except (UnicodeDecodeError, configparser.Error) as error:
        raise ExportError(f"invalid .gitmodules: {error}") from error

    configured: dict[str, str] = {}
    for section in parser.sections():
        if not section.startswith('submodule "') or not section.endswith('"'):
            raise ExportError(f"invalid .gitmodules section: {section}")
        submodule_path = parser.get(section, "path", fallback="")
        url = parser.get(section, "url", fallback="")
        if not submodule_path or not url:
            raise ExportError(f"incomplete .gitmodules section: {section}")
        if submodule_path in configured:
            raise ExportError(f"duplicate submodule path: {submodule_path}")
        configured[submodule_path] = url
    return configured


def verify_gitlinks(
    actual: dict[str, str], manifest: Manifest, root: Path = ROOT
) -> int:
    errors: list[str] = []
    expected = {gitlink.path: gitlink for gitlink in manifest.gitlinks}
    if len(expected) != len(manifest.gitlinks):
        errors.append("duplicate gitlink path in public-export.json")

    for path in sorted(set(actual) - set(expected)):
        errors.append(f"{path}: undeclared gitlink")
    for path in sorted(set(expected) - set(actual)):
        errors.append(f"{path}: declared gitlink is absent")

    configured = configured_gitlinks(root)
    for path in sorted(set(configured) - set(expected)):
        errors.append(f"{path}: undeclared .gitmodules entry")
    for path, gitlink in expected.items():
        if classify(path, manifest) != "include":
            errors.append(f"{path}: gitlink path is not explicitly included")
        if actual.get(path) != gitlink.commit:
            errors.append(f"{path}: gitlink commit differs from public-export.json")
        if configured.get(path) != gitlink.url:
            errors.append(f"{path}: submodule URL differs from public-export.json")

    if errors:
        raise ExportError("\n".join(errors))
    return len(expected)


def matches(path: str, patterns: Iterable[str]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def classify(path: str, manifest: Manifest) -> str:
    if matches(path, manifest.include):
        return "include"
    if matches(path, manifest.quarantine):
        return "quarantine"
    return "unclassified"


def canonical_literal(value: str) -> bytes:
    return re.sub(r"\s+", "", value.lower()).encode("ascii")


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def scan_file(path: str, data: bytes, manifest: Manifest) -> list[str]:
    errors: list[str] = []
    suffix = PurePosixPath(path).suffix.lower()

    if suffix in manifest.forbidden_extensions:
        errors.append(f"{path}: forbidden extension")
    if path.startswith("private/") and path not in manifest.allowed_private_paths:
        errors.append(f"{path}: private path is not allowlisted")
    if len(data) > manifest.max_file_bytes:
        errors.append(
            f"{path}: {len(data)} bytes exceeds {manifest.max_file_bytes}"
        )
    if b"\0" in data:
        errors.append(f"{path}: binary NUL byte detected")
        return errors

    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        errors.append(f"{path}: content is not UTF-8 text")
        return errors

    for pattern in SECRET_PATTERNS:
        match = pattern.search(text)
        if match:
            errors.append(
                f"{path}:{line_number(text, match.start())}: secret pattern detected"
            )

    for pattern_name, pattern in (
        ("absolute user path", ABSOLUTE_USER_PATH),
        ("internal-only path", INTERNAL_PATH),
    ):
        match = pattern.search(text)
        if match:
            errors.append(
                f"{path}:{line_number(text, match.start())}: {pattern_name} detected"
            )

    for pattern in BYTE_LITERAL_PATTERNS:
        for match in pattern.finditer(text):
            digest = hashlib.sha256(canonical_literal(match.group())).hexdigest()
            if digest not in manifest.byte_literal_exceptions:
                errors.append(
                    f"{path}:{line_number(text, match.start())}: "
                    f"unreviewed byte literal {digest}"
                )

    return errors


def normalize_public_bytes(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def verify_entries(
    entries: dict[str, bytes], manifest: Manifest, require_public_tree: bool
) -> tuple[int, int]:
    errors: list[str] = []
    included = 0
    quarantined = 0

    for path in sorted(entries):
        disposition = classify(path, manifest)
        if disposition == "unclassified":
            errors.append(f"{path}: path is not classified by public-export.json")
            continue
        if disposition == "quarantine":
            quarantined += 1
            if require_public_tree:
                errors.append(f"{path}: quarantined path exists in the public tree")
            continue

        included += 1
        if require_public_tree and b"\r" in entries[path]:
            errors.append(f"{path}: public text must use LF line endings")
        errors.extend(scan_file(path, entries[path], manifest))

    if included == 0:
        errors.append("public export contains no included files")

    if errors:
        raise ExportError("\n".join(errors))
    return included, quarantined


def worktree_entries(root: Path = ROOT) -> tuple[dict[str, bytes], dict[str, str]]:
    entries: dict[str, bytes] = {}
    paths, gitlinks = index_entries(root)
    for path in paths:
        source = root / Path(path)
        if not source.is_file():
            raise ExportError(f"tracked file is missing from the worktree: {path}")
        entries[path] = source.read_bytes()
    return entries, gitlinks


def archive_entries(root: Path = ROOT) -> tuple[dict[str, bytes], dict[str, str]]:
    archive = run_git("archive", "--format=tar", "HEAD", cwd=root, binary=True)
    entries: dict[str, bytes] = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as stream:
        for member in stream.getmembers():
            if not member.isfile():
                continue
            file_object = stream.extractfile(member)
            if file_object is None:
                raise ExportError(f"unable to read archive member: {member.name}")
            entries[member.name] = file_object.read()
    return entries, tree_gitlinks(root)


def verify_source(require_public_tree: bool) -> None:
    manifest = load_manifest()
    entries, gitlinks = worktree_entries()
    included, quarantined = verify_entries(
        entries, manifest, require_public_tree
    )
    gitlink_count = verify_gitlinks(gitlinks, manifest)
    print(
        "public export verification passed: "
        f"included={included} quarantined={quarantined} gitlinks={gitlink_count}"
    )


def verify_archive() -> None:
    manifest = load_manifest()
    entries, gitlinks = archive_entries()
    included, quarantined = verify_entries(
        entries, manifest, require_public_tree=True
    )
    gitlink_count = verify_gitlinks(gitlinks, manifest)
    print(
        "public archive verification passed: "
        f"included={included} quarantined={quarantined} gitlinks={gitlink_count}"
    )


def export_tree(destination: Path) -> None:
    if run_git("status", "--porcelain", "--untracked-files=all").strip():
        raise ExportError("source worktree must be clean before export")

    manifest = load_manifest()
    entries, gitlinks = worktree_entries()
    verify_entries(entries, manifest, require_public_tree=False)
    verify_gitlinks(gitlinks, manifest)
    included_paths = [
        path for path in sorted(entries) if classify(path, manifest) == "include"
    ]

    destination = destination.resolve()
    if destination.exists():
        raise ExportError(f"destination already exists: {destination}")
    if ROOT == destination or ROOT in destination.parents:
        raise ExportError("destination must be outside the source repository")

    destination.mkdir(parents=True)
    for path in included_paths:
        target = destination / Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(normalize_public_bytes(entries[path]))

    for gitlink in manifest.gitlinks:
        target = destination / Path(gitlink.path)
        target.parent.mkdir(parents=True, exist_ok=True)
        run_git(
            "clone",
            "--no-checkout",
            "--filter=blob:none",
            gitlink.url,
            str(target),
            cwd=destination,
        )
        run_git("checkout", "--detach", gitlink.commit, cwd=target)
        if run_git("rev-parse", "HEAD", cwd=target).strip() != gitlink.commit:
            raise ExportError(f"{gitlink.path}: exported submodule commit differs")

    exported = {
        path: (destination / Path(path)).read_bytes() for path in included_paths
    }
    included, quarantined = verify_entries(
        exported, manifest, require_public_tree=True
    )
    print(
        f"public export created: destination={destination} "
        f"included={included} quarantined={quarantined} "
        f"gitlinks={len(manifest.gitlinks)}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify or create the public export")
    subparsers = parser.add_subparsers(dest="command", required=True)

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--require-public-tree", action="store_true")

    subparsers.add_parser("verify-archive")

    export_parser = subparsers.add_parser("export")
    export_parser.add_argument("--destination", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "verify":
            verify_source(args.require_public_tree)
        elif args.command == "verify-archive":
            verify_archive()
        elif args.command == "export":
            export_tree(args.destination)
        else:
            raise ExportError(f"unknown command: {args.command}")
    except ExportError as error:
        print(f"public export verification failed:\n{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
