# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic checks for moving the lifter pin."""
import json
from pathlib import Path
import tempfile
import unittest

from update_lifter_pin import compare, function_hashes, pin_metadata


class LifterPinTests(unittest.TestCase):
    def test_metadata_and_function_comparison(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "tools/game-recipe").mkdir(parents=True)
            old, new = root / "old", root / "new"
            for generated, second in ((old, "return;"), (new, "eax = 1;")):
                generated.mkdir()
                (generated / "recomp_0000.c").write_text(
                    "void sub_00001000(void) {}\nvoid sub_00002000(void) { " + second + " }\n")
                (generated / "recomp_dispatch.c").write_text("/* dispatch */\n")
                (generated / "recomp_funcs.h").write_text("/* declarations */\n")
            (new / "recomp_0001.c").write_text("void sub_00003000(void) {}\n")
            recipe = {"lifter_revision": "0" * 40, "generated_files": {"gone.c": "0" * 64}}
            (root / "tools/game-recipe/recipe.json").write_text(json.dumps(recipe), encoding="utf-8")
            (root / "tools/build_game.py").write_text(
                f'LIFTER_REVISION = "{"0" * 40}"\nRECIPE_SHA256 = "{"0" * 64}"\n', encoding="utf-8")
            (root / "public-export.json").write_text(json.dumps({"gitlinks": [
                {"path": "tools/xboxrecomp", "commit": "0" * 40}]}), encoding="utf-8")

            changed = pin_metadata(root, new, "a" * 40)

            self.assertIn("gone.c", changed)
            self.assertIn("recomp_0001.c", changed)
            written = json.loads((root / "tools/game-recipe/recipe.json").read_text(encoding="utf-8"))
            self.assertEqual(written["lifter_revision"], "a" * 40)
            self.assertNotIn("gone.c", written["generated_files"])
            self.assertIn(f'LIFTER_REVISION = "{"a" * 40}"', (root / "tools/build_game.py").read_text())
            for name in ("tools/game-recipe/recipe.json", "tools/build_game.py", "public-export.json"):
                self.assertNotIn(b"\r", (root / name).read_bytes())
            export = json.loads((root / "public-export.json").read_text(encoding="utf-8"))
            self.assertEqual(export["gitlinks"][0]["commit"], "a" * 40)
            self.assertEqual(compare(function_hashes(old), function_hashes(new)),
                             (["sub_00002000"], ["sub_00003000"], []))


if __name__ == "__main__":
    unittest.main()
