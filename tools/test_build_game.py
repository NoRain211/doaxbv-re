# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic checks for variable-size authenticated generated programs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from build_game import ROOT, program_manifest, verify_files
from extract_iso import sha256


class ProgramManifestTests(unittest.TestCase):
    def test_parity_rejects_changed_or_missing_file(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "input.json"
            source.write_text("[]\n", encoding="utf-8")
            expected = {source.name: sha256(source)}
            verify_files(root, expected)
            source.write_text("[1]\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "parity check failed"):
                verify_files(root, expected)
            source.unlink()
            with self.assertRaisesRegex(ValueError, "parity check failed"):
                verify_files(root, expected)

    def test_variable_chunks_and_cmake_authentication(self):
        private = ROOT / "private"
        private.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="manifest-test-", dir=private) as folder:
            root = Path(folder).resolve()
            self.assertTrue(root.is_relative_to(private.resolve()))
            generated = root / "generated"
            generated.mkdir()
            (generated / "recomp_0000.c").write_text("void sub_00001000(void) {}\n")
            (generated / "recomp_0001.c").write_text("void sub_00002000(void) {}\n")
            (generated / "recomp_dispatch.c").write_text("/* synthetic dispatch */\n")
            (generated / "recomp_funcs.h").write_text("/* synthetic declarations */\n")
            manifest, ebp = program_manifest(generated)
            self.assertEqual(ebp, 0)

            def configure(name, digest):
                return subprocess.run(
                    ["cmake", "-S", str(ROOT / "recomp-runtime"), "-B", str(root / name),
                     f"-DRECOMP_PROGRAM_DIR={generated}",
                     f"-DRECOMP_PROGRAM_MANIFEST_SHA256={digest}",
                     "-DRECOMP_PROGRAM_EBP_EXPECTED=0"], capture_output=True, text=True,
                    timeout=60)

            good = configure("valid", manifest)
            self.assertEqual(good.returncode, 0, good.stdout + good.stderr)
            (generated / "recomp_0001.c").write_text("/* changed after receipt */\n")
            wrong = configure("changed", manifest)
            self.assertNotEqual(wrong.returncode, 0)
            self.assertIn("does not match its authenticated manifest", wrong.stdout + wrong.stderr)
            (generated / "recomp_0001.c").rename(generated / "recomp_0002.c")
            with self.assertRaisesRegex(ValueError, "non-contiguous"):
                program_manifest(generated)
            gap = configure("gap", manifest)
            self.assertNotEqual(gap.returncode, 0)
            self.assertIn("chunk sequence has a gap", gap.stdout + gap.stderr)


if __name__ == "__main__":
    unittest.main()
