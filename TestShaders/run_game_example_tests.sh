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
	local bin_filename="$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/.shdr/')"

	reconstruct_shader "$bin_filename"

	echo -n "....: $bin_filename...$whitespace"
	run_decompiler_test "$bin_filename" "$update_chk"
}

for arg in "$@"; do
	case "$arg" in
		"--update-chk")
			update_chk=1
			;;
		*)

			run_all=0
			run_test "$(echo "$arg" | sed -E 's/.hlsl(.chk)?|.shdr|_replace.txt(.bin)?//').hlsl.chk"
			;;
	esac
done

if [ "$run_all" = 1 ]; then
	find GameExamples -iname '*.hlsl.chk' -a ! -iname '*stripped.hlsl.chk' -print0 |
		while IFS= read -r -d $'\0' hlsl_chk_filename; do
			run_test "$hlsl_chk_filename" "         "
			run_test "$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/_stripped.shdr/')"
		done
fi

[ $TESTS_FAILED = 0 ]
