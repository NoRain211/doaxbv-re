# SPDX-License-Identifier: GPL-3.0-or-later
"""Move the build to another lifter revision and record the regenerated program.

The script generates the current pin (which must still match the recipe) and the
target revision, rewrites the recipe identities, stages the submodule and lists
changed game functions. It never commits: review and test the new program first.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import build_game
from extract_iso import sha256

ROOT = build_game.ROOT
LIFTER = build_game.LIFTER
BODY = re.compile(r"^void (sub_[0-9A-Fa-f]{8})\(void\)", re.MULTILINE)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=LIFTER, text=True).strip()


def function_hashes(generated):
    """Hash each generated function body so two programs can be compared by name."""
    bodies = {}
    for path in sorted(generated.glob("recomp_[0-9][0-9][0-9][0-9].c")):
        text = path.read_text(encoding="utf-8")
        starts = [match.start() for match in BODY.finditer(text)] + [len(text)]
        for begin, end in zip(starts, starts[1:]):
            name = BODY.match(text, begin).group(1)
            bodies[name] = hashlib.sha256(text[begin:end].encode("utf-8")).hexdigest()
    return bodies


def compare(old, new):
    return (sorted(name for name in old.keys() & new.keys() if old[name] != new[name]),
            sorted(new.keys() - old.keys()), sorted(old.keys() - new.keys()))


def pin_metadata(root, generated, revision):
    """Rewrite every identity that names the lifter or the generated program."""
    recipe_path = root / "tools/game-recipe/recipe.json"
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
    files = {path.name: sha256(path) for path in sorted(generated.iterdir()) if path.is_file()}
    changed = sorted(name for name in files.keys() | recipe["generated_files"].keys()
                     if files.get(name) != recipe["generated_files"].get(name))
    manifest, ebp = build_game.program_manifest(generated)
    recipe.update(lifter_revision=revision, generated_files=files,
                  program_manifest_sha256=manifest, ebp_overrides=ebp)
    recipe_path.write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")

    builder = root / "tools/build_game.py"
    source = builder.read_text(encoding="utf-8")
    for name, value in (("LIFTER_REVISION", revision), ("RECIPE_SHA256", sha256(recipe_path))):
        source, count = re.subn(rf'(?m)^{name} = "[0-9a-f]+"$', f'{name} = "{value}"', source)
        if count != 1:
            raise ValueError(f"Could not update {name} in build_game.py")
    builder.write_text(source, encoding="utf-8")

    export_path = root / "public-export.json"
    export = json.loads(export_path.read_text(encoding="utf-8"))
    links = [link for link in export["gitlinks"] if link["path"] == "tools/xboxrecomp"]
    if len(links) != 1:
        raise ValueError("public-export.json must list tools/xboxrecomp once")
    links[0]["commit"] = revision
    export_path.write_text(json.dumps(export, indent=2) + "\n", encoding="utf-8")
    return changed


def generate(imported, verify_parity, revision=None):
    args = argparse.Namespace(imported=imported, iso=None, extractor=None, generate_only=True)
    return build_game.build(args, verify_parity=verify_parity, lifter_revision=revision) / "generated"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--imported", type=Path, required=True, help="Completed import beneath private/")
    parser.add_argument("--revision", help="Lifter commit (default: the fork's main branch tip)")
    args = parser.parse_args()
    if git("status", "--porcelain"):
        raise SystemExit("tools/xboxrecomp has local changes; commit or discard them first")
    previous = git("rev-parse", "HEAD")
    if args.revision:
        revision = git("rev-parse", "--verify", f"{args.revision}^{{commit}}")
    else:
        git("fetch", "origin", "main")
        revision = git("rev-parse", "FETCH_HEAD")
    old = generate(args.imported, verify_parity=True)
    try:
        git("checkout", "--detach", revision)
        new = generate(args.imported, verify_parity=False, revision=revision)
    except Exception:
        git("checkout", "--detach", previous)
        raise
    changed_files = pin_metadata(ROOT, new, revision)
    subprocess.run(["git", "add", "tools/xboxrecomp"], cwd=ROOT, check=True)
    changed, added, removed = compare(function_hashes(old), function_hashes(new))
    report = new.parent / "changed-functions.txt"
    report.write_text("".join(f"{kind} {name}\n" for kind, names in
                              (("changed", changed), ("added", added), ("removed", removed))
                              for name in names), encoding="utf-8")
    print(f"Lifter pin moved to {revision}. Nothing was committed.")
    print(f"Generated files changed: {', '.join(changed_files) or 'none'}")
    print(f"Functions: {len(changed)} changed, {len(added)} added, {len(removed)} removed; see {report}")


if __name__ == "__main__":
    main()
