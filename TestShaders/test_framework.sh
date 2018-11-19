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

if [ ! -x "$FXC" -o ! -x "$CMD_DECOMPILER" ]; then
	echo Please set FXC and CMD_DECOMPILER environment variables
	exit 1
fi

if [ -t 1 ]; then
	# stdout is connected to a terminal, use ANSI colour escape sequences
	# FIXME: Detect if the terminal supports this
	ANSI_RED='\033[0;31m'
	ANSI_GREEN='\033[0;32m'
	ANSI_NORM='\033[0m'
fi

OUTPUT_DIR=output
TEST_DIR="$PWD"
mkdir -p "$OUTPUT_DIR"
rm -v "$OUTPUT_DIR"/* 2>/dev/null || true
TESTS_FAILED=0

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
	update_chk="$3"

	if [ ! -f "$check" ]; then
		if [ "$update_chk" = 1 ]; then
			echo -n " Created .chk file."
			cp "$decompiled" "$check"
		else
			echo -n " No .chk file."
		fi
		# Not really a failure, so return true to allow the test case
		# to pass if the recompilation step succeeds:
		true
		return
	fi

	skip1=$(grep "^// ---- Created with" "$decompiled" | wc -c)
	skip2=$(grep "^// ---- Created with" "$check" | wc -c)

	if ! cmp "$decompiled" "$check" "$skip1" "$skip2" >/dev/null; then
		if [ "$update_chk" = 1 ]; then
			echo -n " Updated .chk file."
			cp "$decompiled" "$check"
			true
			return
		fi
		echo -n " Does not match .chk file."
		false
		return
	fi

	true
}

# This runs the HLSL decompiler over a binary shader, compares the result to a
# .hlsl.chk file if present and attempts to recompile the shader with FXC
run_decompiler_test()
{
	compiled="$1"
	update_chk="$2"
	dst="$(echo "$compiled" | sed -r 's/\.[^.]+$//')"

	decompiled="$dst.hlsl"
	recompiled="${dst}_recompiled.bin"
	check="$decompiled.chk"

	fail=0

	rm "$decompiled" "$recompiled" 2>/dev/null
	model=$(timeout 5s "$FXC" /nologo /dumpbin "$compiled" | grep -av '^\/\/' | head -n 1 | tr -d '\r')
	if [ -z "$model" ]; then
		echo -n " Unable to get shader model - bad binary?"
		fail=1
	else
		"$CMD_DECOMPILER" -D "$compiled" >/dev/null 2>&1 # produces "$decompiled"
		if [ ! -f "$decompiled" ]; then
			echo -n " HLSL decompilation failed."
			fail=1
		else
			check_decompiler_result "$decompiled" "$check" "$update_chk" || fail=1
			if ! "$FXC" /nologo "$decompiled" /T "$model" /Fo "$recompiled" >/dev/null 2>&1; then
				echo -n " Recompilation failed."
				fail=1
			fi
		fi
	fi

	if [ "$fail" = 1 ]; then
		echo -e "\r${ANSI_RED}FAIL${ANSI_NORM}"
		TESTS_FAILED=1
	else
		echo -e "\r${ANSI_GREEN}PASS${ANSI_NORM}"
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
	check="$decompiled.chk"
	stripped="${dst}_stripped.bin"
	stripped_asm="${dst}_stripped.asm"
	decompiled_stripped="${dst}_stripped.out"
	recompiled_stripped="${dst}_stripped_recompiled.bin"
	check_stripped="${dst}_stripped.chk"

	"$FXC" /nologo "$src" /T "$model" $flags /Fo "$OUTPUT_DIR\\$compiled" /Fc "$OUTPUT_DIR\\$assembled"
	cd "$OUTPUT_DIR"
		# FIXME: Call out to run_decompiler_test()

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

cmd_decompiler_copy_reflection_check()
{
	"$CMD_DECOMPILER" --version 2>/dev/null
}

reconstruct_shader_binary()
{
	local hlsl_filename="$1"
	local asm_filename="$2"
	local bin_filename="$3"
	local rm_asm=0

	if [ ! -f "$asm_filename" ]; then
		# Extract original assembly from comment, if present:
		if ! grep '\/\*~' "$hlsl_filename" > /dev/null; then
			echo "$hlsl_filename does not contain original assembly"
			continue
		fi
		sed -E '0,/\/\*~+( Original ASM )?~+/d; /~+\*\//,$d' "$hlsl_filename" > "$asm_filename"
		rm_asm=1
	fi

	local shader_model=$(grep -av '^\/\/' "$asm_filename" | grep -av '^\s*$'|head -n 1)

	# We want the reflection information in the reconstructed binary, but
	# cmd_Decompiler currently does not assemble it, so we can only get it
	# by compiling the HLSL file - of course, for many of these test
	# shaders we expect the compilation to fail outright or may not use the
	# complete reflection information, so we might have to massage the HLSL
	# by hand a little first.
	"$FXC" /T "$shader_model" "$hlsl_filename" /Fo "$hlsl_filename.bin"

	# We don't really trust the _replace.txt file to test the decompiler
	# since it already went through the decompiler, so we want to use the
	# assembly to reconstruct the original binary, but since cmd_Decompiler
	# doesn't assemble the reflection information and we really want that
	# to adequately test the decompiler we will compromise and copy the
	# reflection information from the HLSL compiled binary:
	"$CMD_DECOMPILER" -a "$asm_filename" --copy-reflection "$hlsl_filename.bin"

	if [ "$rm_asm" = "1" ]; then
		rm "$asm_filename"
	fi
}
