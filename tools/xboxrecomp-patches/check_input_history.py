#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate synthetic expiry logic through the shipped input-history/Radio recovery."""

import argparse
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tool_root", type=Path)
    parser.add_argument("recovery", type=Path)
    parser.add_argument("work", type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.tool_root.resolve()))
    from tools.recomp import config
    from tools.recomp.translator import BatchTranslator

    check = unittest.TestCase()
    owner, end = 0x14250, 0x1431A
    splits = [0x14285, 0x142A5, 0x14300, 0x14308]
    radio, radio_end = 0xE3B90, 0xE675D
    # Independently assembled test instructions, not extracted game bytes. The
    # recipe supplies only the real boundaries; the synthetic CFG exercises two
    # cap comparisons feeding a shared JGE, plus split expiry JG/JLE branches.
    code = bytearray(b"\x90" * (end - owner))

    def put(address, instruction):
        offset = address - owner
        code[offset:offset + len(instruction)] = instruction

    put(owner, b"\x85\xc9\x74\x31")              # test ecx,ecx; jz alternate cap
    put(owner + 4, b"\xbf\x0c\x00\x00\x00\x39\xf8")  # edi=12; cmp eax,edi
    put(0x14283, b"\xeb\x20")                    # jmp shared JGE
    put(0x14285, b"\xbf\x06\x00\x00\x00\x39\xf8")    # edi=6; cmp eax,edi
    put(0x142A5, b"\x7d\x07\x89\xc7")           # jge ready; edi=eax
    put(0x142AE, b"\x31\xdb")                    # ready: ebx=0
    put(0x142FC, b"\x39\xfb")                    # cmp ebx,edi
    put(0x14300, b"\x7f\x15\x43")               # jg done; inc ebx
    put(0x14306, b"\x39\xfb")                    # cmp ebx,edi
    put(0x14308, b"\x7e\xf6\x89\xf8")           # jle expiry branch; eax=edi
    put(end - 1, b"\xc3")                        # ret
    code += b"\xc3"                              # next detected function

    # Separate synthetic sections keep the large address gap out of the fixture.
    radio_base = radio - 1
    radio_code = b"\xc3" + b"\x90" * (radio_end - radio - 1) + b"\xc3\xc3"
    config._install([
        config.Section(".text", owner, len(code), 0, len(code), True),
        config.Section("radio", radio_base, len(radio_code), len(code), len(radio_code), True),
    ], owner, 0, "synthetic input-history regression")
    image = args.work / "synthetic.bin"
    image.write_bytes(code + radio_code)
    bounds = [owner, *splits, end]
    ranges = list(zip(bounds, bounds[1:])) + [
        (end, end + 1), (radio_base, radio), (radio_end, radio_end + 1)]
    functions = args.work / "functions.json"
    functions.write_text(json.dumps([
        {"start": f"0x{start:08X}", "end": f"0x{stop:08X}", "section": ".text"}
        for start, stop in ranges
    ]), encoding="utf-8")

    def generate(recovery=None):
        return BatchTranslator(image, functions, recovery_json_path=recovery,
                               output_dir=str(args.work), seh_prolog=0, seh_epilog=0)

    # Negative control: the original fragment has lost the comparison's flags.
    broken = generate()
    check.assertIn("if (_flags", broken.translate_single(0x142A5))
    check.assertNotIn(radio, broken.func_db)

    recovered = generate(args.recovery)
    check.assertEqual(recovered.func_db[owner]["end"], end)
    body = recovered.translate_single(owner)
    check.assertEqual(body.count("void sub_00014250(void)"), 1)
    check.assertEqual(body.count("cmp eax, edi - flags set for next jcc"), 2)
    check.assertEqual(body.count("if (CMP_GE(eax, edi)) goto loc_000142AE;"), 2)
    check.assertIn("if (CMP_G(ebx, edi)) goto loc_00014317;", body)
    check.assertIn("if (CMP_LE(ebx, edi)) goto loc_00014300;", body)
    check.assertNotIn("if (_flags", body)
    for split in splits:
        check.assertNotIn(split, recovered.func_db)
        check.assertNotIn(f"sub_{split:08X}", body)
    check.assertIn(radio, recovered.func_db, "Radio callback recovery is missing")
    check.assertEqual(recovered.func_db[radio]["end"], radio_end)
    check.assertIn("void sub_000E3B90(void)", recovered.translate_single(radio))
    print("Input-history comparisons/branches share one body; Radio range retained")


if __name__ == "__main__":
    main()
