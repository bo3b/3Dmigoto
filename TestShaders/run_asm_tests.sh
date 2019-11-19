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

# Test cases from HLSLCrossCompiler
run_bin_asm_test BinaryDecompiler/apps/shaders/ExtrudeGS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/ExtrudePS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/ExtrudeVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/ClippingVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/compute.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/idPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/idVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/postProcessing/invertPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/postProcessing/monochromePS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/postProcessing/sobel.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/templatePostFXPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/templatePostFXVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/templatePS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/templateVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/wavyPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/generic/wavyVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/IntegerVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/LambertLitPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/LambertLitSolidPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/LambertLitVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/SubroutinesPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/SubroutinesVS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/tessellationDS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/tessellationHS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/tessellationPS.o
run_bin_asm_test BinaryDecompiler/apps/shaders/tessellationVS.o
run_bin_asm_test BinaryDecompiler/cs5/BasicCompute11.o
run_bin_asm_test BinaryDecompiler/cs5/BasicCompute11Double.o
run_bin_asm_test BinaryDecompiler/cs5/BasicCompute11StructuredBuffer.o
run_bin_asm_test BinaryDecompiler/cs5/BasicCompute11StructuredBufferDouble.o
run_bin_asm_test BinaryDecompiler/cs5/Issue11.o
run_bin_asm_test BinaryDecompiler/cs5/Issue11Struct.o
run_bin_asm_test BinaryDecompiler/cs5/Issue34.o
run_bin_asm_test BinaryDecompiler/cs5/ThreadGroupSharedMem.o
run_bin_asm_test BinaryDecompiler/ds5/basic.o
run_bin_asm_test BinaryDecompiler/gs4/CubeMap_Inst.o
run_bin_asm_test BinaryDecompiler/gs4/PipesGS.o
run_bin_asm_test BinaryDecompiler/gs5/instance.o
run_bin_asm_test BinaryDecompiler/gs5/stream.o
run_bin_asm_test BinaryDecompiler/hs5/basic.o
run_bin_asm_test BinaryDecompiler/hs5/basic_change_pos.o
run_bin_asm_test BinaryDecompiler/hs5/basic_NoOptimisation.o
run_bin_asm_test BinaryDecompiler/hs5/DecalTessellation11.o
run_bin_asm_test BinaryDecompiler/hs5/issue32.o
run_bin_asm_test BinaryDecompiler/hs5/issue32b.o
run_bin_asm_test BinaryDecompiler/hs5/two_fork_phases.o
run_bin_asm_test BinaryDecompiler/ps4/constTexCoord.o
run_bin_asm_test BinaryDecompiler/ps4/derivative.o
run_bin_asm_test BinaryDecompiler/ps4/discard_nz.o
run_bin_asm_test BinaryDecompiler/ps4/for_loop.o
run_bin_asm_test BinaryDecompiler/ps4/fxaa.o
run_bin_asm_test BinaryDecompiler/ps4/HDAO.o
run_bin_asm_test BinaryDecompiler/ps4/issue26.o
run_bin_asm_test BinaryDecompiler/ps4/issue8.o
run_bin_asm_test BinaryDecompiler/ps4/load.o
run_bin_asm_test BinaryDecompiler/ps4/loadWithOffset.o
run_bin_asm_test BinaryDecompiler/ps4/primID.o
run_bin_asm_test BinaryDecompiler/ps4/RaycastTerrainShootRayPS.o
run_bin_asm_test BinaryDecompiler/ps4/resinfo.o
run_bin_asm_test BinaryDecompiler/ps5/array_of_textures.o
run_bin_asm_test BinaryDecompiler/ps5/atomic_counter.o
run_bin_asm_test BinaryDecompiler/ps5/atomic_mem.o
run_bin_asm_test BinaryDecompiler/ps5/conservative_depth_ge.o
run_bin_asm_test BinaryDecompiler/ps5/conservative_depth_le.o
run_bin_asm_test BinaryDecompiler/ps5/ContactHardeningShadows11PS.o
run_bin_asm_test BinaryDecompiler/ps5/coverage.o
run_bin_asm_test BinaryDecompiler/ps5/evaluateAttrib.o
run_bin_asm_test BinaryDecompiler/ps5/gather.o
run_bin_asm_test BinaryDecompiler/ps5/interfaces.o
run_bin_asm_test BinaryDecompiler/ps5/interfaces_multifunc.o
run_bin_asm_test BinaryDecompiler/ps5/interface_arrays.o
run_bin_asm_test BinaryDecompiler/ps5/interpolation.o
run_bin_asm_test BinaryDecompiler/ps5/load_store.o
run_bin_asm_test BinaryDecompiler/ps5/lod.o
run_bin_asm_test BinaryDecompiler/ps5/precision.o
run_bin_asm_test BinaryDecompiler/ps5/resinfo.o
run_bin_asm_test BinaryDecompiler/ps5/retc.o
run_bin_asm_test BinaryDecompiler/ps5/sample.o
run_bin_asm_test BinaryDecompiler/ps5/sample1D.o
run_bin_asm_test BinaryDecompiler/ps5/sample1DLod.o
run_bin_asm_test BinaryDecompiler/ps5/sample3D.o
run_bin_asm_test BinaryDecompiler/ps5/sample3DLod.o
run_bin_asm_test BinaryDecompiler/ps5/sampleInteger.o
run_bin_asm_test BinaryDecompiler/ps5/twoSideDepthWrite.o
run_bin_asm_test BinaryDecompiler/quarantined/ps5/this.o
run_bin_asm_test BinaryDecompiler/vs4/array_input.o
run_bin_asm_test BinaryDecompiler/vs4/bitwiseNot.o
run_bin_asm_test BinaryDecompiler/vs4/constBufferSwapRegister.o
run_bin_asm_test BinaryDecompiler/vs4/continuec.o
run_bin_asm_test BinaryDecompiler/vs4/default_const.o
run_bin_asm_test BinaryDecompiler/vs4/issue20.o
run_bin_asm_test BinaryDecompiler/vs4/issue21.o
run_bin_asm_test BinaryDecompiler/vs4/matrix_array.o
run_bin_asm_test BinaryDecompiler/vs4/minmax.o
run_bin_asm_test BinaryDecompiler/vs4/mov.o
run_bin_asm_test BinaryDecompiler/vs4/multiple_const_buffers.o
run_bin_asm_test BinaryDecompiler/vs4/shift.o
run_bin_asm_test BinaryDecompiler/vs4/struct_const.o
run_bin_asm_test BinaryDecompiler/vs4/switch.o
run_bin_asm_test BinaryDecompiler/vs4/xor.o
run_bin_asm_test BinaryDecompiler/vs5/any.o
run_bin_asm_test BinaryDecompiler/vs5/bits.o
run_bin_asm_test BinaryDecompiler/vs5/const_temp.o
run_bin_asm_test BinaryDecompiler/vs5/exp.o
run_bin_asm_test BinaryDecompiler/vs5/issue28.o
run_bin_asm_test BinaryDecompiler/vs5/issue35.o
run_bin_asm_test BinaryDecompiler/vs5/mad_imm.o
run_bin_asm_test BinaryDecompiler/vs5/mov.o
run_bin_asm_test BinaryDecompiler/vs5/precision.o
run_bin_asm_test BinaryDecompiler/vs5/rcp.o
run_bin_asm_test BinaryDecompiler/vs5/sincos.o
run_bin_asm_test BinaryDecompiler/vs5/tempArray.o

[ $TESTS_FAILED = 0 ]
