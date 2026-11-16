# RAW probe regression

Run `./regression/probe_roundtrip.ps1` after building cmd_Decompiler. The
`-Decompiler` and `-Fxc` parameters can override the executable paths.

The fixture comes from tests/GS_GI_Sample/RAW.DXBC: instruction numbers and
the hash banner were removed, sample_l annotations were normalized for the
assembler, and input/output signature comments were supplied from the sample's
reference shader. It deliberately has no resource reflection (RDEF).
Printed floating-point literals have the precision of the supplied disassembly;
this is a structural/data-dependency regression, not a bit-exact reconstruction
of the unavailable original binary or a visual rendering comparison.

The test assembles the fixture, decompiles the resulting bytecode, and compiles
the untouched generated HLSL using FXC /O3. It checks the integer clamp,
both mask words, unsigned conversions, and both dynamic t8 lookups inside
loops, including their dependency into t6. Artifacts are kept in an ignored
unique output directory for inspection.

# Integer control and texture-load regression

Run `./regression/integer_control.ps1` in an x64 MSVC developer shell after
building cmd_Decompiler (the same executable override parameters apply).
This requires the Windows SDK, FXC, and D3D11 WARP.

The test assembles `fixtures/integer_control.asm`, decompiles it, and compiles
the generated pixel shader with FXC /O3. A compute wrapper then executes the
unchanged generated function body on WARP and checks four input sets against
expected DWORDs. It covers tracked integer switch selectors, multi-digit case
labels, raw nonzero conditions (including 0x80000000), scalar/vector UINT loads,
and scalar SINT loads. This is a focused semantic regression, not a visual
validation of the complete material shader.
