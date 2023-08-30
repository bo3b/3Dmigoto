#!/bin/bash
# Bashisms allow us to save 30 seconds over calling external commands like sed
# in cygwin. Granted cygwin is horribly inefficient since Windows lacks an
# efficient copy on write fork() or clone() style system call, but never
# underestimate the cost associated with spawning processes

. ./test_framework.sh

run_all=1

reconstruct_shader()
{
	local bin_filename="$1"
	[ -f "$bin_filename" ] && return

	if [[ "$bin_filename" == *"_stripped"* ]]; then
		local unstripped_filename="${bin_filename/_stripped/}"
		reconstruct_shader "$unstripped_filename"
		echo -n "Reconstructing $bin_filename... "
		"$FXC" /nologo /dumpbin "$unstripped_filename" /Qstrip_reflect /Fo "$bin_filename" \
			</dev/null >/dev/null 2>&1 && echo OK || echo Fail
		return
	fi

	local asm_filename="${bin_filename/.shdr/.txt}"
	local hlsl_filename="${bin_filename/.shdr/_replace.txt}"
	echo -n "Reconstructing $bin_filename... "
	if cmd_decompiler_copy_reflection_check; then
		reconstruct_shader_binary "$hlsl_filename" "$asm_filename" "$bin_filename" \
			>/dev/null 2>&1 && echo OK || echo Fail
	else
		echo -e "Fail:\n\tcmd_Decompiler does not support --copy-reflection, cannot perform shader reconstruction"
		continue
	fi

}

run_test()
{
	local hlsl_chk_filename="$1"
	local whitespace="$2"
	local source_bin_filename="${hlsl_chk_filename/.hlsl.chk/.bin}"

	if [ -z "$ASM" -a -z "$ASM_RECONSTRUCTED" -a -z "$HLSL" ]; then
		local ASM=1
		local HLSL=1
	fi

	if [ -f "$source_bin_filename" ]; then
		bin_filename="$source_bin_filename"
		if [ -n "$ASM" ]; then
			echo -n "....: $bin_filename (asm)... $whitespace"
			run_assembler_test "$bin_filename"
		fi
	else
		# _stripped shader reconstructions are used for HLSL and
		# reconstructed asm tests only, not regular asm tests.
		[[ "$hlsl_chk_filename" == *"_stripped"* ]] && [ -z "$HLSL" -a -z "$ASM_RECONSTRUCTED" ] && return

		local reconstruct_filename="${hlsl_chk_filename/.hlsl.chk/.shdr}"
		reconstruct_shader "$reconstruct_filename"
		bin_filename="$reconstruct_filename"

		if [ -n "$ASM" ]; then
			local compiled_filename="${reconstruct_filename/.shdr/_replace.txt.bin}"
			if [ -f "$compiled_filename" ]; then
				echo -n "....: $compiled_filename (asm)... $whitespace"
				run_assembler_test "$compiled_filename"
			fi
		fi

		if [ -n "$ASM_RECONSTRUCTED" ]; then
			# Note: The idea of doing assembly verification tests of the
			# reconstructed shaders is inherently flawed since they have already
			# passed through the assembler and disassembler, which will
			# likely have already eliminated anything we might have found.
			# We are generally better off running the verification
			# test on the compiled output directly (as above).
			echo -n "....: $bin_filename (asm)... $whitespace"
			run_assembler_test "$bin_filename"
		fi
	fi

	if [ -n "$HLSL" ]; then
		echo -n "....: $bin_filename (hlsl)...$whitespace"
		run_decompiler_test "$bin_filename"
	fi
}

for arg in "$@"; do
	case "$arg" in
		"--update-chk")
			UPDATE_CHK=1
			;;
		"--asm")
			ASM=1
			;;
		"--asm-reconstructed")
			ASM_RECONSTRUCTED=1
			;;
		"--hlsl")
			HLSL=1
			;;
		"--lenient")
			LENIENT=--lenient
			;;
		*)

			run_all=0
			run_test "$(echo "$arg" | sed -E 's/.hlsl(.chk)?|.shdr|.bin|_replace.txt(.bin)?//').hlsl.chk"
			;;
	esac
done

if [ "$run_all" = 1 ]; then
	find GameExamples \( -iname '*.hlsl.chk' -a ! -iname '*stripped.hlsl.chk' -o -iname '*-?s.bin' \) -print0 |
		while IFS= read -r -d $'\0' filename; do
			if [[ "$filename" == *".bin" ]]; then
				run_test "$filename" "         "
				run_test "${filename/.bin/_stripped.bin}"
			elif [ ! -f "${filename/.hlsl.chk/.bin}" ]; then
				run_test "$filename" "         "
				run_test "${filename/.hlsl.chk/_stripped.hlsl.chk}"
			fi
		done
fi

[ $TESTS_FAILED = 0 ]
