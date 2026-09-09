# SPDX-License-Identifier: GPL-3.0-or-later
"""Launch a separately built runner using the completed drag-and-drop import."""
from datetime import datetime
import json
import os
from pathlib import Path
import subprocess
import sys


def launch(root):
    candidates = [root / "recomp_program_runner.exe",
                  root / "build/recomp-program/Release/recomp_program_runner.exe"]
    runner = next((path for path in candidates if path.is_file()), None)
    if runner is None:
        raise ValueError("No playable runner found. This Alpha does not include one. "
                         "See docs/building.md for the remaining generation/build prerequisites.")
    imported = root / "private/imported-disc"
    receipt = json.loads((imported / "receipt.json").read_text(encoding="utf-8"))
    if receipt.get("status") != "extracted":
        raise ValueError("Extraction is incomplete. Drop your ISO onto ExtractIso.cmd first.")
    images = list((imported / "disc").glob("*.xbe"))
    if len(images) != 1:
        raise ValueError("Expected exactly one root XBE in the extracted disc; cannot choose safely.")
    log_path = root / "private" / ("run-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f") + ".log")
    env = dict(os.environ, RECOMP_AUDIO_GAIN="0.2")
    print(f"Starting runner. Log: {log_path}", flush=True)
    with log_path.open("x", encoding="utf-8") as log:
        result = subprocess.run([str(runner), "--xbe", str(images[0]), "--vsync"],
                                cwd=images[0].parent, env=env, stdout=log,
                                stderr=subprocess.STDOUT,
                                creationflags=getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0))
    print(f"Runner exited with code {result.returncode}. See {log_path}")
    return result.returncode


if __name__ == "__main__":
    try:
        sys.exit(launch(Path(__file__).resolve().parents[1]))
    except (OSError, ValueError) as error:
        print(f"Cannot launch: {error}", file=sys.stderr)
        sys.exit(1)
