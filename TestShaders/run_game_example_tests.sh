#!/bin/sh

. ./test_framework.sh

for arg in "$@"; do
	case "$arg" in
		"--update-chk")
			update_chk=1
			;;
		*)
			echo "Unexpected argument: $arg"
			exit 1
			;;
	esac
done

find GameExamples -iname '*.hlsl.chk' -print0 |
	while IFS= read -r -d $'\0' hlsl_chk_filename; do
		bin_filename="$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/.shdr/')"

		if [ ! -f "$bin_filename" ]; then
			asm_filename=$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/.txt/')
			hlsl_filename=$(echo "$hlsl_chk_filename" | sed 's/.hlsl.chk/_replace.txt/')
			echo -n "Reconstructing $bin_filename... "
			if cmd_decompiler_copy_reflection_check; then
				reconstruct_shader_binary "$hlsl_filename" "$asm_filename" "$bin_filename" \
					>/dev/null 2>&1 && echo OK || echo Fail
			else
				echo -e "Fail:\n\tcmd_Decompiler does not support --copy-reflection, cannot perform shader reconstruction"
				continue
			fi
		fi

		echo -n "....: $bin_filename..."
		run_decompiler_test "$bin_filename" "$update_chk"
	done
