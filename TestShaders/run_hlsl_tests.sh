#!/bin/sh

. ./test_framework.sh

for arg in "$@"; do
	case "$arg" in
		"--update-chk")
			UPDATE_CHK=1
			;;
		*)
			echo Invalid argument: "$arg"
			exit 1
			;;
	esac
done

run_hlsl_test structured_buffers.hlsl structured_buffers "ps_4_0 ps_5_0"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name "ps_4_0 ps_5_0" "/D USE_DUP_NAME"
run_hlsl_test structured_buffers.hlsl structured_buffers_prim_types "ps_4_0 ps_5_0" "/D USE_PRIMITIVE_TYPES"
run_hlsl_test structured_buffers.hlsl structured_buffers_inner_struct "ps_4_0 ps_5_0" "/D USE_INNER_STRUCT"
run_hlsl_test structured_buffers.hlsl structured_buffers_rw ps_5_0 "/D USE_RW_STRUCTURED_BUFFER /D USE_INNER_STRUCT"
# Decompiler needs more work generally to support doubles, but we enable the test now anyway:
run_hlsl_test structured_buffers.hlsl structured_buffers_doubles "ps_5_0" "/D USE_PRIMITIVE_TYPES /D USE_DOUBLES"
# We don't expect dynamically indexed arrays inside structs to decompile yet when reflection info is present:
run_hlsl_test structured_buffers.hlsl structured_buffers_dynamic_indexing "ps_4_0 ps_5_0" "/D USE_DYNAMICALLY_INDEXED_ARRAYS /D USE_INNER_STRUCT"

for test in $(find BinaryDecompiler -name '*.o'); do
	echo -n "....: $test (hlsl)..."
	run_decompiler_test "$test"
done

[ $TESTS_FAILED = 0 ]
