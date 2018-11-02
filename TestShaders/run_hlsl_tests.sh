#!/bin/sh -e

# TODO: Convert this into an automated test suite run whenever we build
#       cmd_Decompiler, possibly as a windows batch or powershell script, or
#       adopt an existing test harness. Do this prior to vs2017 update.
# TODO: Add existing assembly test cases
# TODO: Adopt Test Driven Development and add all the failing test cases
#       (mostly for the HLSL decompiler) as well

# Pass these in via environment variables, like
# $ export FXC="/cygdrive/c/Program Files (x86)/Windows Kits/10/bin/10.0.17134.0/x64/fxc.exe"
# $ export FXC="/cygdrive/c/Program Files (x86)/Windows Kits/8.0/bin/x64/fxc.exe"
# $ export CMD_DECOMPILER=~/"3DMigoto/x64/Zip Release/cmd_Decompiler.exe"
# $ ./run_hlsl_tests.sh
if [ -z "$FXC" ]; then
	FXC=fxc.exe
fi
if [ -z "$CMD_DECOMPILER" ]; then
	CMD_DECOMPILER=cmd_Decompiler.exe
fi

OUTPUT_DIR=output
TEST_DIR="$PWD"
mkdir -p "$OUTPUT_DIR"
rm -v "$OUTPUT_DIR"/* || true

# No good way to machine verify the decompiler, so go for the next best thing -
# verify that the output matches a previous run. This is of course subject to
# starting to fail after benign changes in the decompiler that affect output in
# some unrelated way, or if an update to the fxc compiler produces a different
# result. This is still better than no verification at all, and at least gives
# us a chance to review failures due to such changes.
check_decompiler_result()
{
	decompiled="$1"
	check="$2"

	if [ ! -f "$check" ]; then
		echo "TEST WARNING: Decompiler test case lacks verification file $check"
		return
	fi

	if ! cmp "$decompiled" "$check"; then
		echo
		echo "TEST FAILED: $decompiled does not match $check"
		exit 1
	fi
}

run_hlsl_test()
{
	src="$1"
	dst="$2"
	model="$3"
	flags="$4"

	compiled="$dst.bin"
	assembled="$dst.asm"
	decompiled="$dst.hlsl"
	recompiled="${dst}_recompiled.bin"
	check="$dst.chk"
	stripped="${dst}_stripped.bin"
	stripped_asm="${dst}_stripped.asm"
	decompiled_stripped="${dst}_stripped.out"
	recompiled_stripped="${dst}_stripped_recompiled.bin"
	check_stripped="${dst}_stripped.chk"

	"$FXC" /nologo "$src" /T "$model" $flags /Fo "$OUTPUT_DIR\\$compiled" /Fc "$OUTPUT_DIR\\$assembled"
	cd "$OUTPUT_DIR"
		"$CMD_DECOMPILER" -D "$compiled" # produces "$decompiled"
		"$FXC" /nologo "$decompiled" /T "$model" /Fo "$recompiled"
		check_decompiler_result "$decompiled" "$TEST_DIR/$check"

		# "$FXC" /nologo "$src" /T "$model" /Qstrip_reflect /Fo "$stripped" /Fc "$stripped_asm"
		"$FXC" /nologo /dumpbin "$compiled" /Qstrip_reflect /Fo "$stripped" /Fc "$stripped_asm"
		"$CMD_DECOMPILER" -D "$stripped" # produces "$decompiled_stripped"
		"$FXC" /nologo "$decompiled_stripped" /T "$model" /Fo "$recompiled_stripped"
		check_decompiler_result "$decompiled_stripped" "$TEST_DIR/$check_stripped"
	cd -
}

run_hlsl_test structured_buffers.hlsl structured_buffers_sm4 ps_4_0
run_hlsl_test structured_buffers.hlsl structured_buffers_sm5 ps_5_0
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm4 ps_4_0 "/D USE_DUP_NAME"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm5 ps_5_0 "/D USE_DUP_NAME"
run_hlsl_test structured_buffers.hlsl structured_buffers_rw ps_5_0 "/D USE_RW_STRUCTURED_BUFFER"
