#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check both returns of the recovered Rest predicate without game files."""
import json
from pathlib import Path
import struct
import sys
import unittest

tool, recovery, work = map(Path, sys.argv[1:])
sys.dont_write_bytecode = True
sys.path.insert(0, str(tool.resolve()))
from tools.recomp import config
from tools.recomp.translator import BatchTranslator

check = unittest.TestCase()
owner, yes, no, end = 0x11AE00, 0x11AE26, 0x11AE2C, 0x11AE2F
# Independently assembled synthetic switch: two choices and a default return.
# Padding before the indirect jump is reachable; the table is outside the body.
code = bytearray(b'\x90' * 0x41)
code[:5] = b'\x83\xf8\x01\x77\x27'  # cmp eax,1; ja false
code[0x1F:0x26] = b'\xff\x24\x85' + struct.pack('<I', owner + 0x30)
code[0x26:0x2C] = b'\xb8\x01\x00\x00\x00\xc3'  # eax=1; ret
code[0x2C:0x2F] = b'\x31\xc0\xc3'  # eax=0; ret
code[0x30:0x38] = struct.pack('<II', yes, no)
code[0x40] = 0xC3
config._install([config.Section('.text', owner, len(code), 0, len(code), True)],
                owner, 0, 'synthetic Rest predicate')
image = work / 'rest-synthetic.bin'
image.write_bytes(code)
functions = work / 'rest-functions.json'
functions.write_text(json.dumps([
    {'start': f'0x{a:08X}', 'end': f'0x{b:08X}', 'section': '.text'}
    for a, b in [(owner, yes), (no, end), (owner + 0x40, owner + 0x41)]
]))
def generate(bounds=None):
    return BatchTranslator(image, functions, recovery_json_path=bounds,
                           output_dir=str(work), seh_prolog=0, seh_epilog=0)

broken = generate()
check.assertNotIn(yes, broken.func_db)
check.assertNotIn(f'loc_{yes:08X}:', broken.translate_single(owner))
fixed = generate(recovery)
check.assertEqual(fixed.func_db[owner]['end'], end)
check.assertNotIn(no, fixed.func_db)
body = fixed.translate_single(owner)
for target in (yes, no):
    check.assertIn(f'if (_jt == 0x{target:08X}u) goto loc_{target:08X};', body)
    check.assertIn(f'loc_{target:08X}:', body)
    check.assertNotIn(f'sub_{target:08X}', body)
check.assertIn('eax = 1;', body)
check.assertIn('eax = 0;', body)
check.assertEqual(body.count('esp += 4; return; /* ret */'), 2)
print('Rest switch retains both local returns and its complete owner')
