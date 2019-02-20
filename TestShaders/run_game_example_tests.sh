#!/bin/sh

. ./test_framework.sh

run_all=1

reconstruct_shader()
{
	local bin_filename="$1"
	[ -f "$bin_filename" ] && return

	if echo "$bin_filename" | grep _stripped > /dev/null; then
		local unstripped_filename="$(echo "$bin_filename" | sed 's/_stripped//')"
		reconstruct_shader "$unstripped_filename"
		echo -n "Reconstructing $bin_filename... "
		"$FXC" /nologo /dumpbin "$unstripped_filename" /Qstrip_reflect /Fo "$bin_filename" \
			>/dev/null 2>&1 && echo OK || echo Fail
		return
	fi

	local asm_filename=$(echo "$bin_filename" | sed 's/.shdr/.txt/')
	local hlsl_filename=$(echo "$bin_filename" | sed 's/.shdr/_replace.txt/')
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
	local reconstruct_filename="$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/.shdr/')"
	local source_bin_filename="$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/.bin/')"

	if [ -f "$source_bin_filename" ]; then
		bin_filename="$source_bin_filename"
	else
		reconstruct_shader "$reconstruct_filename"
		bin_filename="$reconstruct_filename"
	fi

	echo -n "....: $bin_filename...$whitespace"
	run_decompiler_test "$bin_filename"
}

for arg in "$@"; do
	case "$arg" in
		"--update-chk")
			UPDATE_CHK=1
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
			if echo "$filename" | grep "\.bin$" >/dev/null; then
				run_test "$filename" "         "
				run_test "$(echo "$filename" | sed 's/.bin/_stripped.bin/')"
			elif [ ! -f "$(echo "$filename" | sed 's/.hlsl.chk/.bin/')" ]; then
				run_test "$filename" "         "
				run_test "$(echo "$filename" | sed 's/.hlsl.chk/_stripped.shdr/')"
			fi
		done
fi

[ $TESTS_FAILED = 0 ]
