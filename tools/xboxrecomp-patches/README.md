# xboxrecomp source patches

`fsubp-destination.patch` fixes the FSUBP lifter to subtract ST(0) from the
specified stack register before popping. The previous implementation always
wrote ST(1), corrupting calculations that use another destination. The implicit
destination and explicit ST(1) output remain unchanged.

Apply this source patch before regenerating C with xboxrecomp. It assumes the
lifter already has `_fmt_fpu_stack_register` and the generated runtime supports
`fp_st(index)`. It does not provide those earlier stack-register changes.

From this repository, with the xboxrecomp checkout and patch paths substituted:

```text
git -C <xboxrecomp-root> apply --check <absolute-path-to-fsubp-destination.patch>
git -C <xboxrecomp-root> apply <absolute-path-to-fsubp-destination.patch>
python tools/xboxrecomp-patches/check_fsubp.py <xboxrecomp-root>
```

The check uses the checkout's Python dependencies and three synthetic
instructions. It checks the emitted subtraction destination and pop order;
it needs no game files, compiler, generated program, or audio device. After it
passes, regenerate through the checkout's supported `tools.recomp` entry point
and rebuild the runner. Do not apply the change to generated C.
