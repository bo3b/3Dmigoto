#!/bin/sh

. ./test_framework.sh

for arg in "$@"; do
	case "$arg" in
		"--lenient")
			LENIENT=--lenient
			;;
		*)
			echo Invalid argument: "$arg"
			exit 1
			;;
	esac
done

# Test cases that have no (known) ways to produce with fxc:
run_asm_asm_test emit_then_cut.asm
run_asm_asm_test emit_then_cut_stream.asm
run_asm_asm_test resinfo_rcpFloat.asm
run_asm_asm_test sync.asm
run_asm_asm_test uaddc_usubb.asm

# Assembler test cases (Test Driven Development):
run_hlsl_asm_test compute.hlsl compute "cs_5_0" "/Od"
run_hlsl_asm_test debug.hlsl debug "cs_4_0 cs_5_0" "/Od"
run_hlsl_asm_test double_precision.hlsl double_precision "ps_5_0"
run_hlsl_asm_test geometry.hlsl geometry "gs_4_0 gs_5_0"
run_hlsl_asm_test hull.hlsl hull "hs_5_0"
run_hlsl_asm_test min_precision.hlsl min_precision "ps_4_0 ps_5_0"
run_hlsl_asm_test resource_types.hlsl resource_types "ps_4_0 ps_5_0"
run_hlsl_asm_test resource_types5.hlsl resource_types5 "ps_5_0"
run_hlsl_asm_test sv_gsinstanceid.hlsl sv_gsinstanceid "gs_5_0"
run_hlsl_asm_test samplepos.hlsl samplepos "ps_5_0"

# Signature parser test cases (Test Driven Development):
run_hlsl_asm_test signatures_cs.hlsl signatures_cs "cs_5_0"
run_hlsl_asm_test signatures_ds.hlsl signatures_ds "ds_5_0"
run_hlsl_asm_test signatures_gs.hlsl signatures_gs "gs_4_0 gs_5_0"
run_hlsl_asm_test signatures_gs5.hlsl signatures_gs5 "gs_5_0"
run_hlsl_asm_test signatures_hs.hlsl signatures_hs "hs_5_0"
run_hlsl_asm_test signatures_ps.hlsl signatures_ps_SVDepth "ps_5_0" "/D TEST_DEPTH=0"
run_hlsl_asm_test signatures_ps.hlsl signatures_ps_SVDepthGE "ps_5_0" "/D TEST_DEPTH=1"
run_hlsl_asm_test signatures_ps.hlsl signatures_ps_SVDepthLE "ps_5_0" "/D TEST_DEPTH=2"
run_hlsl_asm_test signatures_vs.hlsl signatures_vs "vs_4_0 vs_5_0"

# TDD cases created for the decompiler, but may as well also use them to test the [dis]assembler:
run_hlsl_asm_test structured_buffers.hlsl structured_buffers "ps_4_0 ps_5_0"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_dup_name "ps_4_0 ps_5_0" "/D USE_DUP_NAME"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_prim_types "ps_4_0 ps_5_0" "/D USE_PRIMITIVE_TYPES"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_inner_struct "ps_4_0 ps_5_0" "/D USE_INNER_STRUCT"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_rw ps_5_0 "/D USE_RW_STRUCTURED_BUFFER /D USE_INNER_STRUCT"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_doubles "ps_5_0" "/D USE_PRIMITIVE_TYPES /D USE_DOUBLES"
run_hlsl_asm_test structured_buffers.hlsl structured_buffers_dynamic_indexing "ps_4_0 ps_5_0" "/D USE_DYNAMICALLY_INDEXED_ARRAYS /D USE_INNER_STRUCT"

[ $TESTS_FAILED = 0 ]
