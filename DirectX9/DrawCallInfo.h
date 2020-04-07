#pragma once
#include <d3d9.h>
enum class DrawCall {
	Draw,
	DrawIndexed,
	DrawUP,
	DrawIndexedUP,
	DrawTriPatch,
	DrawRectPatch,
	Invalid
};
struct DrawCallInfo
{
	DrawCall type;
	::D3DPRIMITIVETYPE primitive_type;
	UINT PrimitiveCount;
	UINT StartVertex;
	INT BaseVertexIndex;
	UINT MinVertexIndex;
	UINT NumVertices;
	UINT StartIndex;
	const void *pVertexStreamZeroData;
	UINT VertexStreamZeroStride;
	const void *pIndexData;
	::D3DFORMAT IndexDataFormat;
	UINT Handle;
	const float *pNumSegs;
	const ::D3DRECTPATCH_INFO *pRectPatchInfo;
	const ::D3DTRIPATCH_INFO *pTriPatchInfo;
	bool skip, hunting_skip;
	DrawCallInfo() :
		type(DrawCall::Invalid),
		primitive_type(::D3DPRIMITIVETYPE(-1)),
		PrimitiveCount(0),
		StartVertex(0),
		BaseVertexIndex(0),
		MinVertexIndex(0),
		NumVertices(0),
		StartIndex(0),
		pVertexStreamZeroData(NULL),
		VertexStreamZeroStride(0),
		pIndexData(NULL),
		IndexDataFormat(::D3DFORMAT(-1)),
		Handle(0),
		pNumSegs(NULL),
		pRectPatchInfo(NULL),
		pTriPatchInfo(NULL),
		skip(false),
		hunting_skip(false)
	{}
	DrawCallInfo(DrawCall type, ::D3DPRIMITIVETYPE primitive_type, UINT PrimitiveCount,
		UINT StartVertex, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex,
		const void *pVertexStreamZeroData, UINT VertexStreamZeroStride,
		const void *pIndexData, ::D3DFORMAT IndexDataFormat,
		UINT Handle, const float *pNumSegs,
		const ::D3DRECTPATCH_INFO *pRectPatchInfo,
		const ::D3DTRIPATCH_INFO *pTriPatchInfo
		) :
		type(type),
		primitive_type(primitive_type),
		PrimitiveCount(PrimitiveCount),
		StartVertex(StartVertex),
		NumVertices(NumVertices),
		BaseVertexIndex(BaseVertexIndex),
		MinVertexIndex(MinVertexIndex),
		StartIndex(StartIndex),
		pVertexStreamZeroData(pVertexStreamZeroData),
		VertexStreamZeroStride(VertexStreamZeroStride),
		pIndexData(pIndexData),
		IndexDataFormat(IndexDataFormat),
		Handle(Handle),
		pNumSegs(pNumSegs),
		pRectPatchInfo(pRectPatchInfo),
		pTriPatchInfo(pTriPatchInfo),
		skip(false),
		hunting_skip(false)
	{}
};
