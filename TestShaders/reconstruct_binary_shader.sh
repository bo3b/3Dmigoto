#!/bin/sh

. ./test_framework.sh

# Regenerate something as close as possible to the original shader binaries by
# combining assembly with compiled reflection information. This processes all
# HLSL files found in GameExamples.

if ! cmd_decompiler_copy_reflection_check; then
	echo cmd_Decompiler does not support --copy-reflection, cannot perform shader reconstruction
	exit 1
fi

find GameExamples -iname '*_replace.txt' -print0 |
	while IFS= read -r -d $'\0' hlsl_filename; do
		asm_filename=$(echo "$hlsl_filename" | sed 's/_replace\.txt/.txt/')
		bin_filename=$(echo "$asm_filename" | sed 's/\.txt/.shdr/')
		if [ ! -f "$bin_filename" ]; then
			echo -n "Attempting to reconstruct $bin_filename... "
			reconstruct_shader_binary "$hlsl_filename" "$asm_filename" "$bin_filename" \
				>/dev/null 2>&1 && echo OK || echo Fail
		fi
	done
