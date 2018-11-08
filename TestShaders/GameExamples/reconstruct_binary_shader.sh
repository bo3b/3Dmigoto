#!/bin/sh

# Attempts to reconstruct the original shader binary from the _replace.txt
# file with original (not recompiled) bytecode and reflection information
# intact. This may not work in all cases - in future try to use export_binary=1
# to get the original shader binaries from the game to be certain nothing is
# munged.

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

for hlsl_filename in "$@"; do
	asm_filename=$(echo "$hlsl_filename" | sed 's/_replace\.txt/.txt/')
	if [ ! -f "$asm_filename" ]; then
		# Extract original assembly from comment, if present:
		if ! grep '\/\*~' "$hlsl_filename" > /dev/null; then
			echo "$hlsl_filename does not contain original assembly"
			continue
		fi
		sed -E '0,/\/\*~+( Original ASM )?~+/d; /~+\*\//,$d' "$hlsl_filename" > "$asm_filename"
	fi

	shader_model=$(grep -av '^\/\/' "$asm_filename" | grep -av '^\s*$'|head -n 1)

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
done
