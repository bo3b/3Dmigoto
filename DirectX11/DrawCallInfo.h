#pragma once

#include <d3d11_1.h>

enum class DrawCall {
	Draw,
	DrawIndexed,
	DrawInstanced,
	DrawIndexedInstanced,
	DrawInstancedIndirect,
	DrawIndexedInstancedIndirect,
	DrawAuto
};

struct DrawCallInfo
{
	DrawCall type;

	UINT VertexCount, IndexCount, InstanceCount;
	UINT FirstVertex, FirstIndex, FirstInstance;

	ID3D11Buffer *indirect_buffer;
	UINT args_offset;

	bool skip, hunting_skip;

	DrawCallInfo(DrawCall type,
			UINT VertexCount, UINT IndexCount, UINT InstanceCount,
			UINT FirstVertex, UINT FirstIndex, UINT FirstInstance,
			ID3D11Buffer *indirect_buffer, UINT args_offset) :
		type(type),
		VertexCount(VertexCount),
		IndexCount(IndexCount),
		InstanceCount(InstanceCount),
		FirstVertex(FirstVertex),
		FirstIndex(FirstIndex),
		FirstInstance(FirstInstance),
		indirect_buffer(indirect_buffer),
		args_offset(args_offset),
		skip(false),
		hunting_skip(false)
	{}
};

