#pragma once

#include <string>
#include <vector>
#include <string>

struct ParseParameters
{
	const void *bytecode;
	const char *decompiled;
	long decompiledSize;

	bool fixSvPosition;
	bool fixLightPosition;
	bool recompileVs;
	bool ZeroOutput;
	char ZRepair_DepthTextureReg1, ZRepair_DepthTextureReg2;
	std::string ZRepair_DepthTexture1, ZRepair_DepthTexture2;
	std::vector<std::string> ZRepair_Dependencies1, ZRepair_Dependencies2;
	std::string ZRepair_ZPosCalc1, ZRepair_ZPosCalc2;
	std::string ZRepair_PositionTexture;
	bool ZRepair_DepthBuffer;
	std::vector<std::string> InvTransforms;
	std::string ZRepair_WorldPosCalc;
	std::string BackProject_Vector1, BackProject_Vector2;
	std::string ObjectPos_ID1, ObjectPos_ID2, ObjectPos_MUL1, ObjectPos_MUL2;
	std::string MatrixPos_ID1, MatrixPos_MUL1;
};

const std::string DecompileBinaryHLSL(ParseParameters &params, bool &patched, std::string &shaderModel, bool &errorOccurred);
