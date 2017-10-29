#pragma once

struct DrawCallInfo
{
	UINT VertexCount, IndexCount, InstanceCount;
	UINT FirstVertex, FirstIndex, FirstInstance;

	ID3D11Buffer *indirect_buffer;
	UINT args_offset;

	bool skip, hunting_skip, DrawInstancedIndirect;

	DrawCallInfo(UINT VertexCount, UINT IndexCount, UINT InstanceCount,
			UINT FirstVertex, UINT FirstIndex, UINT FirstInstance,
			ID3D11Buffer *indirect_buffer, UINT args_offset,
			bool DrawInstancedIndirect) :
		VertexCount(VertexCount),
		IndexCount(IndexCount),
		InstanceCount(InstanceCount),
		FirstVertex(FirstVertex),
		FirstIndex(FirstIndex),
		FirstInstance(FirstInstance),
		indirect_buffer(indirect_buffer),
		args_offset(args_offset),
		DrawInstancedIndirect(DrawInstancedIndirect),
		skip(false),
		hunting_skip(false)
	{}
};

