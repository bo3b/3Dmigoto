#!/bin/sh -e

. ./test_framework.sh

run_hlsl_test structured_buffers.hlsl structured_buffers_sm4 ps_4_0
run_hlsl_test structured_buffers.hlsl structured_buffers_sm5 ps_5_0
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm4 ps_4_0 "/D USE_DUP_NAME"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm5 ps_5_0 "/D USE_DUP_NAME"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm4 ps_4_0 "/D USE_PRIMITIVE_TYPES"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm5 ps_5_0 "/D USE_PRIMITIVE_TYPES"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm4 ps_4_0 "/D USE_INNER_STRUCT"
run_hlsl_test structured_buffers.hlsl structured_buffers_dup_name_sm5 ps_5_0 "/D USE_INNER_STRUCT"
run_hlsl_test structured_buffers.hlsl structured_buffers_rw ps_5_0 "/D USE_RW_STRUCTURED_BUFFER"
