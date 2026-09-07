#!/usr/bin/env python3
"""Check FSUBP destinations using synthetic instructions and an existing lifter."""

import argparse
from pathlib import Path
import sys

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tool_root", type=Path, help="xboxrecomp checkout root")
    args = parser.parse_args()
    root = args.tool_root.resolve()
    if not (root / "tools/recomp/lifter.py").is_file():
        parser.error("tool_root must contain tools/recomp/lifter.py")
    sys.path.insert(0, str(root))

    from tools.recomp.disasm import Instruction, Operand
    from tools.recomp.lifter import Lifter

    for register, destination in (
        (None, "fp_st1()"),
        ("st(1)", "fp_st1()"),
        ("st(2)", "fp_st(2)"),
    ):
        instruction = Instruction(0, 2, "fsubp", register or "", "")
        if register is not None:
            instruction.operands = [Operand(type="reg", reg=register)]
        expected = [f"{destination} -= fp_top(); fp_pop(); /* fsubp */"]
        actual = Lifter().lift_instruction(instruction)
        if actual != expected:
            raise SystemExit(
                f"FSUBP {register or '(default)'}: expected {expected!r}, got {actual!r}")
    print("FSUBP destination checks passed: default, ST(1), ST(2)")


if __name__ == "__main__":
    main()
