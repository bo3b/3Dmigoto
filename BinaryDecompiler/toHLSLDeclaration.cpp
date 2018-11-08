#include "hlslcc.h"
#include "internal_includes/toHLSLDeclaration.h"
#include "internal_includes/toHLSLOperand.h"
#include "internal_includes/languages.h"
#include "cbstring/bstrlib.h"
#include "internal_includes/debug.h"
#include <math.h>
#include <float.h>

const char* GetDeclaredInputName(const HLSLCrossCompilerContext* psContext, const SHADER_TYPE eShaderType, const Operand* psOperand)
{
	bstring inputName;
	char* cstr;
	InOutSignature* psIn;
	int found = GetInputSignatureFromRegister(psOperand->ui32RegisterNumber, &psContext->psShader->sInfo, &psIn);

	if((psContext->flags & HLSLCC_FLAG_INOUT_SEMANTIC_NAMES) && found)
	{
		if (eShaderType == VERTEX_SHADER) /* We cannot have input and output names conflict, but vs output must match ps input. Prefix vs input. */
			inputName = bformat("in_%s%d", psIn->SemanticName, psIn->ui32SemanticIndex);
		else
		inputName = bformat("%s%d", psIn->SemanticName, psIn->ui32SemanticIndex);
	}
	else if(eShaderType == GEOMETRY_SHADER)
	{
		inputName = bformat("VtxOutput%d", psOperand->ui32RegisterNumber);
	}
	else if(eShaderType == HULL_SHADER)
	{
		inputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
	}
	else if(eShaderType == DOMAIN_SHADER)
	{
		inputName = bformat("HullOutput%d", psOperand->ui32RegisterNumber);
	}
	else if(eShaderType == PIXEL_SHADER)
	{
		if(psContext->flags & HLSLCC_FLAG_TESS_ENABLED)
		{
			inputName = bformat("DomOutput%d", psOperand->ui32RegisterNumber);
		}
		else
		{
			inputName = bformat("VtxGeoOutput%d", psOperand->ui32RegisterNumber);
		}
	}
	else
	{
		ASSERT(eShaderType == VERTEX_SHADER);
		inputName = bformat("dcl_Input%d", psOperand->ui32RegisterNumber);
	}
	if((psContext->flags & HLSLCC_FLAG_INOUT_APPEND_SEMANTIC_NAMES) && found)
	{
		bformata(inputName,"_%s%d", psIn->SemanticName, psIn->ui32SemanticIndex);
	}

	cstr = bstr2cstr(inputName, '\0');
	bdestroy(inputName);
	return cstr;
}
