# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic listing checks; optionally exercise a real extract-xiso binary."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from extract_iso import extract, listing_files, sha256


def listing(entries, count=1):
    return "extract-xiso test\nlisting synthetic.iso:\n" + entries + f"\n{count} files in synthetic.iso total 3 bytes\n"


class ExtractionTests(unittest.TestCase):
    def test_paths_and_complete_listing(self):
        self.assertEqual(listing_files(listing("/sub/ (0 bytes)\n/sub/a.txt (3 bytes)")),
                         {"sub/a.txt": 3})
        for name in ("../escape", "a/../escape", "/absolute", "C:/escape", "NUL", "a:stream", "a.", "a\nname"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                listing_files(listing(f"/{name} (3 bytes)"))
        for entries, count in (("/a (3 bytes)\n/A (3 bytes)", 2),
                               ("/a (3 bytes)\n/a/b (3 bytes)", 2),
                               ("/a (3 bytes)", 2), ("/a (3 bytes)\nWARNING: truncated", 1)):
            with self.assertRaises(ValueError):
                listing_files(listing(entries, count))

    @unittest.skipUnless(os.environ.get("EXTRACT_XISO_TEST_TOOL"), "optional real extractor check")
    def test_real_extraction_and_no_overwrite(self):
        private = Path(__file__).resolve().parents[1] / "private"
        with tempfile.TemporaryDirectory(prefix="extract-test-", dir=private) as folder:
            root = Path(folder).resolve()
            self.assertTrue(root.is_relative_to(private.resolve()))
            source = root / "synthetic input"
            source.mkdir()
            (source / "sub").mkdir()
            (source / "sub" / "payload.txt").write_bytes(b"synthetic payload\n")
            iso = root / "synthetic.iso"
            tool = os.environ["EXTRACT_XISO_TEST_TOOL"]
            subprocess.run([tool, "-m", "-c", str(source), str(iso)], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            before = sha256(iso)
            output = root / "result"
            disc = extract(iso, output, tool)
            self.assertEqual((disc / "sub/payload.txt").read_bytes(), b"synthetic payload\n")
            self.assertEqual(before, sha256(iso))
            self.assertTrue((output / "receipt.json").exists())
            with self.assertRaises(FileExistsError):
                extract(iso, output, tool)
            self.assertEqual((disc / "sub/payload.txt").read_bytes(), b"synthetic payload\n")


if __name__ == "__main__":
    unittest.main()
