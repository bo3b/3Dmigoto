#include "stdafx.h"
#include "float.h"
#include <stdexcept>

#if MIGOTO_DX == 9
#include <d3dx9shader.h>
#endif

using namespace std;

// This is defined in the Windows 10 SDK, but seems not in the 8.0 SDK:
#ifndef DBL_DECIMAL_DIG
#define DBL_DECIMAL_DIG 17
#endif

// for sscanf_s convinience. Explanation in DecompileHLSL.cpp
#define UCOUNTOF(...) (unsigned)_countof(__VA_ARGS__)

static unordered_map<string, vector<DWORD>> codeBin;

static DWORD strToDWORD(string s)
{
	// d3dcompiler_46 symbolic NANs (missing +QNAN and +IND?):
	if (s == "-1.#IND0000")
		return 0xFFC00000;
	if (s == "1.#INF0000")
		return 0x7F800000;
	if (s == "-1.#INF0000")
		return 0xFF800000;
	if (s == "-1.#QNAN000")
		return 0xFFC10000;

	// dx9
	// (I suspect this is *NOT* DX9 specific - more likely this was due to
	// using a newer d3dcompiler/SDK/toolchain, and as per below there are
	// other cases to consider that still require verification -DSS)
	if (s == "1.#INF00")
		return 0x7F800000;
	//dx9

	// FIXME: Write test case to verify and add variants for
	// d3dcompiler_47, which use less zeroes:
	// 1.#INF00
	// -1.#INF00
	// -1.#IND00
	// 1.#QNAN0
	// -1.#QNAN0
	// There are some weird cases that might be bugs in the disassembler,
	// like +IND becomes -IND and -IND becomes +QNAN. We should really
	// allow these with no zeroes at all, but I'd like to verify a few
	// more combinations first and see if those are used for the flags in
	// the mantissa or similar that we might be missing. For now this isn't
	// urgent because the precision fixup code will replace any mismatching
	// NANs with hex strings which will reassemble just fine.
	//  -DSS

	if (s.substr(0, 2) == "0x") {
		DWORD decimalValue;
		sscanf_s(s.c_str(), "0x%x", &decimalValue);
		return decimalValue;
	}
	if (s.find('.') < s.size()) {
		float f = (float)atof(s.c_str());
		DWORD* pF = (DWORD*)&f;
		return *pF;
	}
	return atoi(s.c_str());
}

static uint64_t str_to_raw_double(string &s)
{
	double d;

	// TODO: Parse NAN/INF literals

	if (!s.compare(0, 2, "0x")) {
		uint32_t v1, v2;
		sscanf_s(s.c_str(), "0x%x, 0x%x", &v1, &v2);
		return (uint64_t)v1 | (uint64_t)v2 << 32;
	}

	d = atof(s.c_str());
	return *(uint64_t*)&d;
}

static string convertF(DWORD original)
{
	char buf[80];
	char scientific[80];
	char *scientific_exp = NULL;
	int exp;

	float fOriginal = reinterpret_cast<float &>(original);

	// This printf produces different results depending on the toolchain
	// and/or SDK we are using. e.g. the value 0x3CAAAAAB will produce:
	// VS2013 / Win  8.0 SDK: 2.083333395E-002
	// VS2015 / Win 10.0 SDK: 2.083333395E-02
	// So, we can't hardcode where the exponent is - we have to find it.
	sprintf_s(scientific, 80, "%.9E", fOriginal);

	scientific_exp = strstr(scientific, "E");

	if (!scientific_exp) {
		// For safety, if we ever get called on NAN we will return the
		// hex string. We could return a symbolic value for +/-INF, IND
		// and QNAN, but we're not supposed to get called for these
		// values. We did manage to get a crash here on the vs2015
		// branch since the assembler didn't parse 1.#INF00 from
		// d3dcompiler_47 correctly - it expected d3dcompiler_46's
		// 1.#INF0000. We could concievably hit this case for any
		// variants that strToDWORD() misses, along with a few rare
		// cases that Microsoft's disassembler translates incorrectly
		// (such as -IND, which it prints out as +QNAN).
		sprintf_s(buf, 80, "0x%08x", original);
		return string(buf);
	}

	exp = atoi(scientific_exp + 1);

	switch (exp) {
	case 0:
		sprintf_s(buf, 80, "%.8f", fOriginal);
		break;
	case -1:
		sprintf_s(buf, 80, "%.9f", fOriginal);
		break;
	case -2:
		sprintf_s(buf, 80, "%.10f", fOriginal);
		break;
	case -3:
		sprintf_s(buf, 80, "%.11f", fOriginal);
		break;
	case -4:
		sprintf_s(buf, 80, "%.12f", fOriginal);
		break;
	case -5:
		sprintf_s(buf, 80, "%.13f", fOriginal);
		break;
	case -6:
		sprintf_s(buf, 80, "%.14f", fOriginal);
		break;
	default:
		if (exp < 0)
			sprintf_s(buf, 80, "%.9E", fOriginal);
		else
			sprintf_s(buf, 80, "%.8f", fOriginal);
		break;
	}
	return buf;
}

static string convertD(DWORD v1, DWORD v2)
{
	char buf[80];
	uint64_t q = (uint64_t)v1 | ((uint64_t)v2 << 32);
	double *d = (double*)&q;

	if (isnan(*d) || isinf(*d)) {
		// As above, if we ever get called on a NAN/INF value just
		// output the value as hex to ensure we can parse it back
		// Matching the output of the disassembler with /Lx that splits
		// the hex output into two halves:
		sprintf_s(buf, 80, "0x%08x, 0x%08x", v1, v2);
	} else {
		// %g switches between readable and scientific notation as required, #
		// ensures there is always a radix character (to match the original
		// assembly and avoid potential ambiguities if it turns out that d()
		// could include integers, though unfortunately it has a double meaning
		// and doesn't strip any trailing 0s at all) and DBL_DECIMAL_DIG
		// ensures we can make the round trip from binary to decimal and back
		// without any loss of precision. The trailing 'l' matches the original.
		//
		// We could certainly clean this up further, but this is easy
		sprintf_s(buf, 80, "%#.*gl", DBL_DECIMAL_DIG, *d);
	}
	return buf;
}

void writeLUT()
{
	FILE* f;

	fopen_s(&f, "lut.asm", "wb");
	if (!f)
		return;

	for (unordered_map<string, vector<DWORD>>::iterator it = codeBin.begin(); it != codeBin.end(); ++it) {
		fputs(it->first.c_str(), f);
		fputs(":->", f);
		vector<DWORD> b = it->second;
		int nextOperand = 1;
		for (DWORD i = 0; i < b.size(); i++) {
			if (i == 0) {
				char hex[40];
				shader_ins* ins = (shader_ins*)&b[0];
				if (ins->_11_23 > 0) {
					if (ins->extended)
						sprintf_s(hex, "0x%08X: %d,%d,%d<>%d->", b[0], ins->opcode, ins->_11_23, ins->length, ins->extended);
					else
						sprintf_s(hex, "0x%08X: %d,%d,%d->", b[0], ins->opcode, ins->_11_23, ins->length);
				} else {
					if (ins->extended)
						sprintf_s(hex, "0x%08X: %d,%d<>%d->", b[0], ins->opcode, ins->length, ins->extended);
					else
						sprintf_s(hex, "0x%08X: %d,%d->", b[0], ins->opcode, ins->length);
				}
				fputs(hex, f);
			} else {
				char hex[20];
				sprintf_s(hex, " 0x%08X", b[i]);
				fputs(hex, f);
			}
		}
		fputs("\n", f);
	}
	fclose(f);
}

static void handleSwizzle(string s, token_operand* tOp, bool special = false)
{
	if (special == true){
		// Mask
		tOp->mode = 0; // Mask
		if (s.size() > 0 && s[0] == 'x') {
			tOp->sel |= 0x1;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'y') {
			tOp->sel |= 0x2;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'z') {
			tOp->sel |= 0x4;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'w') {
			tOp->sel |= 0x8;
			s.erase(s.begin());
		}
		return;
	} else if (s.size() == 0) {
		tOp->mode = 0;
		tOp->comps_enum = 0;
		return;
	} else if(s.size() == 4) {
		// Swizzle
		tOp->mode = 1; // Swizzle
		for (int i = 0; i < 4; i++) {
			if (s[i] == 'x')
				tOp->sel |= 0 << (2 * i);
			if (s[i] == 'y')
				tOp->sel |= 1 << (2 * i);
			if (s[i] == 'z')
				tOp->sel |= 2 << (2 * i);
			if (s[i] == 'w')
				tOp->sel |= 3 << (2 * i);
		}
	} else if (s.size() == 1){
		tOp->mode = 2; // Scalar
		if (s[0] == 'x')
			tOp->sel = 0;
		if (s[0] == 'y')
			tOp->sel = 1;
		if (s[0] == 'z')
			tOp->sel = 2;
		if (s[0] == 'w')
			tOp->sel = 3;
	} else {
		// Mask
		tOp->mode = 0; // Mask
		if (s.size() > 0 && s[0] == 'x') {
			tOp->sel |= 0x1;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'y') {
			tOp->sel |= 0x2;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'z') {
			tOp->sel |= 0x4;
			s.erase(s.begin());
		}
		if (s.size() > 0 && s[0] == 'w') {
			tOp->sel |= 0x8;
			s.erase(s.begin());
		}
	}
}

struct special_purpose_register
{
	uint32_t file;

	// This is the value we will set in the comps_enum field of the encoded
	// operand if there is no swizzle. The values have been set based on
	// the code in the original assembler - 2 where the assembler
	// unconditionally called handleSwizzle (and where a bug would cause it
	// to process the entire operand as the swizzle if there was none,
	// hence never hitting the code path that sets comps_enum to 0), 1
	// where the assembler either didn't call handleSwizzle at all, or
	// where it did so conditionally, and 0 as a special case for "null".
	//
	// I'm not certain that this is the best way to handle this - note that
	// the abovementioned bug combined with special handling in dcl_input
	// (that subtracts one from comps_enum when no swizzle is present,
	// effectively clearing it if it was set to 1, or changing 2s to 1s)
	// may interact with this, but this is at least passing all test cases.
	unsigned comps_enum;

	char *name;
};

static struct special_purpose_register special_purpose_registers[] = {
	{ 0x0B, 1, "vPrim" },
	{ 0x0C, 1, "oDepth" },
	{ 0x0D, 0, "null" },
	{ 0x0E, 2, "rasterizer" },
	{ 0x0F, 1, "oMask" },
	{ 0x16, 1, "vOutputControlPointID" },
	{ 0x17, 1, "vForkInstanceID" },
	{ 0x1C, 2, "vDomain" },
	{ 0x20, 2, "vThreadID" },
	{ 0x21, 2, "vThreadGroupID" },
	{ 0x22, 2, "vThreadIDInGroup" },
	{ 0x23, 2, "vCoverage" },
	{ 0x24, 1, "vThreadIDInGroupFlattened" },
	{ 0x25, 1, "vGSInstanceID" }, // instanceCount parameter? See below
	{ 0x26, 1, "oDepthGE" },
	{ 0x27, 1, "oDepthLE" },

	// FIXME: Missing vJoinInstanceID
	// https://msdn.microsoft.com/en-us/library/windows/desktop/hh446905(v=vs.85).aspx

	// XXX * MSDN refers to an 2nd instanceCount parameter to dcl_input vGSInstanceID,
	// but this didn't show up in my test case. My guess is that this is actually
	// the value in dcl_gsinstances, which is [instance(n)] in HLSL:
	// https://msdn.microsoft.com/en-us/library/windows/desktop/hh446903(v=vs.85).aspx
};

static bool assemble_special_purpose_register(string &s, vector<DWORD> &v, token_operand *tOp, bool special)
{
	size_t swiz_pos = s.find('.');
	int i;

	for (i = 0; i < ARRAYSIZE(special_purpose_registers); i++) {
		if (s.compare(0, swiz_pos, special_purpose_registers[i].name))
			continue;

		tOp->file = special_purpose_registers[i].file;

		// comps_enum was set to 2 as a default at the start of assembleOp
		if (swiz_pos != string::npos)
			handleSwizzle(s.substr(swiz_pos + 1), tOp, special);
		else
			tOp->comps_enum = special_purpose_registers[i].comps_enum;

		v.insert(v.begin(), tOp->op);
		return true;
	}

	return false;
}

static vector<DWORD> assembleOp(string s, bool special = false);

static vector<DWORD> assemble_cbvox_operand(string &s, vector<DWORD> &v, token_operand *tOp, bool special, DWORD num)
{
	tOp->num_indices = 2;
	if (s[0] == 'x') { // Indexable temp array
		tOp->file = 3;
		s.erase(s.begin());
	} else if (s[0] == 'o') { // Output register
		tOp->file = 2;
		tOp->num_indices = 1;
		s.erase(s.begin());
	} else if (s[0] == 'v') { // Input register
		tOp->file = 1;
		if (s.size() > 4 && s[1] == 'i' && s[2] == 'c' && s[3] == 'p')  { // Hull shader vicp
			tOp->file = 0x19;
			s.erase(s.begin());
			s.erase(s.begin());
			s.erase(s.begin());
		} else if (s.size() > 4 && s[1] == 'o' && s[2] == 'c' && s[3] == 'p') { // Hull shader vocp
			tOp->file = 0x1A;
			s.erase(s.begin());
			s.erase(s.begin());
			s.erase(s.begin());
		} else if (s[1] == 'p' && s[2] == 'c') { // Patch constant
			tOp->file = 0x1B;
			s.erase(s.begin());
			s.erase(s.begin());
		}
		s.erase(s.begin());
		tOp->num_indices = 1;
		size_t start = s.find("][");
		if (start != string::npos) {
			size_t end = s.find("]", start + 1);
			string index0 = s.substr(s.find("[") + 1, start - 1);
			string index1 = s.substr(start + 2, end - start - 2);
			if (index0.find("+") != string::npos) {
				string sReg = index0.substr(0, index0.find(" + "));
				string sAdd = index0.substr(index0.find(" + ") + 3);
				vector<DWORD> reg = assembleOp(sReg);
				tOp->num_indices = 2;
				tOp->index0_repr = 2;
				int iAdd = atoi(sAdd.c_str());
				if (iAdd) tOp->index0_repr = 3;
				if (index1.find("+") != string::npos) {
					string sReg2 = index1.substr(0, index1.find(" + "));
					string sAdd2 = index1.substr(index1.find(" + ") + 3);
					vector<DWORD> reg2 = assembleOp(sReg2);
					tOp->index1_repr = 2;
					int iAdd2 = atoi(sAdd.c_str());
					if (iAdd2) tOp->index1_repr = 3;
					string swizzle = s.substr(s.find("].") + 2);
					handleSwizzle(swizzle, tOp);
					v.insert(v.begin(), tOp->op);
					if (iAdd) v.push_back(iAdd);
					v.push_back(reg[0]);
					v.push_back(reg[1]);
					if (iAdd2) v.push_back(iAdd2);
					v.push_back(reg2[0]);
					v.push_back(reg2[1]);
					return v;
				}
				string swizzle = s.substr(s.find("].") + 2);
				handleSwizzle(swizzle, tOp);
				v.insert(v.begin(), tOp->op);
				if (iAdd) v.push_back(iAdd);
				v.push_back(reg[0]);
				v.push_back(reg[1]);
				v.push_back(atoi(index1.c_str()));
				return v;
			}
			tOp->num_indices = 2;
			string swizzle = s.substr(s.find('.') + 1);
			handleSwizzle(swizzle, tOp, special);
			v.insert(v.begin(), tOp->op);
			v.push_back(atoi(index0.c_str()));
			v.push_back(atoi(index1.c_str()));
			return v;
		}
	} else if (s[0] == 'i') { // Immediate Constant Buffer
		tOp->file = 9;
		s.erase(s.begin());
		s.erase(s.begin());
		s.erase(s.begin());
		tOp->num_indices = 1;
	} else { // Constant buffer
		tOp->file = 8;
		s.erase(s.begin());
		s.erase(s.begin());
	}
	string sNum;
	bool hasIndex = false;
	if (s.find("[") < s.size()) {
		sNum = s.substr(0, s.find('['));
		hasIndex = true;
	} else {
		sNum = s.substr(0, s.find('.'));
	}
	string index;
	if (hasIndex) {
		size_t start = s.find('[');
		size_t end = s.find(']', start);
		index = s.substr(start + 1, end - start - 1);
	}
	if (hasIndex) {
		if (index.find('+') < index.size()) {
			string s2 = index.substr(index.find('+') + 2);
			DWORD idx = atoi(s2.c_str());
			string s3 = index.substr(0, index.find('+') - 1);
			vector<DWORD> reg = assembleOp(s3);
			if (sNum.size() > 0) {
				num = atoi(sNum.c_str());
				v.push_back(num);
			}
			if (idx != 0) {
				v.push_back(idx);
				if (sNum.size() > 0)
					tOp->index1_repr = 3; // Reg + imm
				else
					tOp->index0_repr = 3; // Reg + imm
			} else {
				if (sNum.size() > 0)
					tOp->index1_repr = 2; // Reg;
				else
					tOp->index0_repr = 2; // Reg;
			}
			for (DWORD i = 0; i < reg.size(); i++) {
				v.push_back(reg[i]);
			}
			handleSwizzle(s.substr(s.find("].") + 2), tOp, special);

			v.insert(v.begin(), tOp->op);
			return v;
		}
		DWORD idx = atoi(index.c_str());
		num = atoi(sNum.c_str());
		v.push_back(num);
		v.push_back(idx);
		if (s.find('.') < s.size()) {
			handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		} else {
			tOp->mode = 1; // Swizzle
			tOp->sel = 0xE4;
		}
		v.insert(v.begin(), tOp->op);
		return v;
	}
	num = atoi(sNum.c_str());
	v.push_back(num);
	handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
	v.insert(v.begin(), tOp->op);
	return v;
}

static vector<DWORD> assemble_literal_operand(string &s, vector<DWORD> &v, token_operand *tOp)
{
	tOp->file = 4;
	s.erase(s.begin());
	if (s.find(",") < s.size()) {
		s.erase(s.begin());
		string s1 = s.substr(0, s.find(","));
		s = s.substr(s.find(",") + 1);
		if (s[0] == ' ')
			s.erase(s.begin());
		string s2 = s.substr(0, s.find(","));
		s = s.substr(s.find(",") + 1);
		if (s[0] == ' ')
			s.erase(s.begin());
		string s3 = s.substr(0, s.find(","));
		s = s.substr(s.find(",") + 1);
		if (s[0] == ' ')
			s.erase(s.begin());
		string s4 = s.substr(0, s.find(")"));

		v.push_back(strToDWORD(s1));
		v.push_back(strToDWORD(s2));
		v.push_back(strToDWORD(s3));
		v.push_back(strToDWORD(s4));
	} else {
		tOp->comps_enum = 1; // 1
		s.erase(s.begin());
		s.pop_back();
		v.push_back(strToDWORD(s));
	}
	v.insert(v.begin(), tOp->op);
	return v;
}

static vector<DWORD> assemble_double_operand(string &s, vector<DWORD> &v, token_operand *tOp)
{
	// Examples of double literals (from RE2):
	//   d(0.000000l, 766800.000000l)
	//   d(0.000000l, 40449600.000000l)
	//   d(-40449600.000000l, 0.000000l)
	//   d(0.000000l, -6360.000000l)
	//   d(0.000000l, 6420.000000l)
	//
	// These examples have two doubles each, the first is split over the
	// .xy components and the second split over the .zw components.
	// I'm not positive if there is a variant with only a single double -
	// float literals can only have 1 or 4 (not 2 or 3) and a double by
	// definition uses at two components for each number, so does that mean
	// it will have to always use all four components?
	//
	// Like floats, there are issues with loss of precision since these
	// have been formatted with %f, which will mean we need to extend the
	// disassembler fixup code to include these.
	//
	// If the disassembler ever outputs a hex value (e.g. /Lx flag AKA
	// D3D_DISASM_PRINT_HEX_LITERALS) these instead look like:
	//   d(0x00000000, 0x00000000, 0x00000000, 0x412766a0)
	//   d(0x00000000, 0x00000000, 0x00000000, 0x418349b2)
	//   d(0x00000000, 0xc18349b2, 0x00000000, 0x00000000)
	//   d(0x00000000, 0x00000000, 0x00000000, 0xc0b8d800)
	//   d(0x00000000, 0x00000000, 0x00000000, 0x40b91400)
	// So we need a special case to handle 64bit hex values split into two
	// 32bit components.

	tOp->file = 5; // Double
	tOp->comps_enum = 2; // Use 4 components (until proven otherwise)
	v.push_back(tOp->op);

	size_t comma = s.find(",", 2);
	if (comma == string::npos)
		throw AssemblerParseError(s, "Double literal string missing 2nd value");

	// If the first value is hex it is only the first 32bits of the value
	// and we need to include the 2nd component as well. The 2nd number
	// doesn't need special handling since it scans until the closing
	// bracket and will therefore naturally include the 4th component:
	if (!s.compare(2, 2, "0x")) {
		comma = s.find(",", comma + 1);
		if (comma == string::npos || s.find(",", comma + 1) == string::npos)
			throw AssemblerParseError(s, "Double literal hex string with less components than expected");
	}

	string s1 = s.substr(2, comma - 2);
	string s2 = s.substr(comma + 1, s.find(")", comma) - comma - 1);
	if (s2[0] == ' ')
		s2.erase(s2.begin());

	// printf("double: \"%s\" \"%s\" \"%s\"\n", s.c_str(), s1.c_str(), s2.c_str());

	uint64_t q1 = str_to_raw_double(s1);
	uint64_t q2 = str_to_raw_double(s2);

	v.push_back(q1 & 0xffffffff);
	v.push_back(q1 >> 32);
	v.push_back(q2 & 0xffffffff);
	v.push_back(q2 >> 32);
	return v;
}

static DWORD encode_min_precision_type(const char *type)
{
	if (!strncmp(type, "min", 3)) {
		if (!strncmp(type+3, "16", 2)) { // min16*
			switch(type[5]) {
				case 'f': return 1 << 14;
				case 'i': return 4 << 14;
				case 'u': return 5 << 14;
			};
		}
		if (!strncmp(type+3, "2_8f", 4)) // min10float
			return 2 << 14;
	}
	if (!strncmp(type, "def32", 5))
		return 0;
	printf("WARNING: Unrecognised minimum precision type \"%s\"\n", type);
	return 0;
}

static void parse_min_precision_tag(string &s, vector<DWORD> &v, token_operand *tOp, DWORD *ext)
{
	size_t tag;

	// Windows 8 minimum precision tag can take two forms:
	// "operand {type1}" for either source or destination where their min precision types match
	// "operand {type1 as type2}" when a source operand (type1) needs to be cast to match the destination (type2)
	// However in the second case type2 is not encoded in the source operand, so we only ever need to worry about type1
	// Mapping between assembly & HLSL types:
	//    asm   | HLSL
	//  def32   | *
	//  min16f  | min16float
	//  min2_8f | min10float
	//  min16i  | min16int
	//  min16i  | min12int  <-- No distinction in assembly from min16int? Maybe the unused 3?
	//  min16u  | min16uint

	tag = s.find('{');
	if (tag == string::npos)
		return;

#if 0 // Debugging
	size_t as = s.find(" as ", tag + 1);
	if (as != string::npos) {
		string stype1 = s.substr(tag+1, as - tag - 1);
		string stype2 = s.substr(as + 4, s.size() - as - 5);
		printf("min precision cast from: \"%s\" to: \"%s\"\n", stype1.c_str(), stype2.c_str());
	} else {
		string stype1 = s.substr(tag+1, s.length() - tag - 2);
		printf("min precision type: \"%s\"\n", stype1.c_str());
	}
#endif

	DWORD type1 = encode_min_precision_type(s.c_str() + tag + 1);
	if (type1) {
		tOp->extended = 1;
		*ext |= 0x00000001;
		*ext |= type1;
	}

	// Strip min precision tag to ensure it can't interfere with further
	// parsing. Remove from the preceding space until the closing brace so
	// that if there is an absolute value || around the entire operand it
	// will be in the right position to be processed later:
	s.erase(tag - 1, s.find('}', tag + 1) - tag + 2);
}

static vector<DWORD> assembleOp(string s, bool special)
{
	vector<DWORD> v;
	DWORD op = 0;
	DWORD ext = 0;
	DWORD num = 0;
	DWORD index = 0;
	DWORD value = 0;
	token_operand* tOp = (token_operand*)&op;
	tOp->comps_enum = 2; // 4

	num = atoi(s.c_str());
	if (num != 0) {
		v.push_back(num);
		return v;
	}
	if (s[0] == '-') {
		s.erase(s.begin());
		tOp->extended = 1;
		ext |= 0x41;
	}
	if (s[0] == '|') {
		s.erase(s.begin());
		s.erase(s.end() - 1);
		tOp->extended = 1;
		ext |= 0x81;
	}

#if 0
	// These ones were special cased in the DX9 port, but I don't think we need
	// them since we aren't using Flugan's assembler for DX9? Disabling -DSS
	if (s == "vCoverage.x") {
		v.push_back(0x2300A);
		return v;

	}
	if (s == "rasterizer.x") {
		v.push_back(0x0000E00A);
		return v;

	}
#endif

	// Processing this after absolute value so that |var {type}| will be
	// processed in a natural order, though this function also strips the
	// tag as a secondary measure (either would be sufficient by itself):
	parse_min_precision_tag(s, v, tOp, &ext);

	if (tOp->extended)
		v.push_back(ext);

	if (assemble_special_purpose_register(s, v, tOp, special))
		return v;

	if (s[0] == 'i' && s[1] == 'c' && s[2] == 'b'
	 || s[0] == 'c' && s[1] == 'b'
	 || s[0] == 'C' && s[1] == 'B' // Compatibility with d3dcompiler_47 disassembly -DarkStarSword
	 || s[0] == 'x'
	 || s[0] == 'o'
	 || s[0] == 'v') {
		return assemble_cbvox_operand(s, v, tOp, special, num);
	}

	if (s[0] == 'l')
		return assemble_literal_operand(s, v, tOp);

	if (s[0] == 'd')
		return assemble_double_operand(s, v, tOp);

	if (s[0] == 'r') {
		tOp->file = 0;
	} else if (s[0] == 's') {
		tOp->file = 6;
	} else if (s[0] == 't') {
		tOp->file = 7;
	} else if (s[0] == 'g') {
		tOp->file = 0x1F;
	} else if (s[0] == 'u') {
		tOp->file = 0x1E;
	} else if (s[0] == 'm')
		tOp->file = 0x10;
	else
		throw AssemblerParseError(s, "Unrecognised operand");

	s.erase(s.begin());
	tOp->num_indices = 1;
	num = atoi(s.substr(0, s.find('.')).c_str());
	v.push_back(num);
	if (s.find('.') < s.size()) {
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
	} else {
		handleSwizzle("", tOp, special);
	}
	v.insert(v.begin(), op);
	return v;
}

static vector<string> strToWords(string s)
{
	vector<string> words;
	string::size_type start = 0;
	while (s[start] == ' ') start++;
	string::size_type end = start;
	while (end < s.size() && s[end] != ' ' && s[end] != '(')
		end++;
	words.push_back(s.substr(start, end - start));

	while (s.size() > end) {
		if (s[end] == ' ') {
			start = ++end;
		} else {
			start = end;
		}
		while (s[start] == ' ') start++;
		if (start >= s.size())
			break;
		if (s[start] == '(' || s[start + 1] == '(') {
			end = s.find(')', start) + 1;
			if (end < s.size() && s[end] == ',')
				end++;
		} else if (s[start] == '"') {
			// Strings only exist in the printf / errorf debug
			// instructions, and we need to match until the
			// right-most quote in case there are extra quotes
			// inside the string (which are not escaped).
			end = s.rfind('"') + 1;
			if (end < s.size() && s[end] == ',')
				end++;
		} else {
			end = s.find(' ', start);
			if (s[end + 1] == '+') {
				end = s.find(' ', end + 3);
				if (s.size() > end && s[end + 1] == '+') {
					end = s.find(' ', end + 3);
				}
			}
		}

		// Keep min precision tag with the operand. Since the icb
		// syntax also uses braces we match part of the min precision
		// type here to avoid grouping something we don't want
		//   -DarkStarSword:
		if (end != string::npos && (!s.compare(end, 5, " {min")
		                         || !s.compare(end, 5, " {def"))) {
			// We will match to the next comma or end of line. We
			// can't just match until the closing brace, because of
			// cases where minimum precision and absolute value are
			// combined, e.g.
			// mad r2.xyzw {min16f}, |r2.xyzw {min16f}|, l(-4.000000, -4.000000, -4.000000, -4.000000) {def32 as min16f}, l(1.000000, 1.000000, 1.000000, 1.000000) {def32 as min16f}
			end = s.find(',', end + 5);
			if (end != string::npos)
				end++;
		}

		if (end == string::npos) {
			words.push_back(s.substr(start));
		} else {
			string::size_type length = end - start;
			words.push_back(s.substr(start, length));
		}
	}
	for (size_t i = 0; i < words.size(); i++) {
		string s2 = words[i];
		// Fixed access before start of array -DarkStarSword
		if (!s2.empty() && s2[s2.size() - 1] == ',')
			s2.erase(--s2.end());
		words[i] = s2;
	}
	return words;
}

static DWORD parseAoffimmi(DWORD start, string o)
{
	string nums = o.substr(1, o.size() - 2);
	int n1 = atoi(nums.substr(0, nums.find(',')).c_str());
	nums = nums.substr(nums.find(',') + 1);
	int n2 = atoi(nums.substr(0, nums.find(',')).c_str());
	int n3 = atoi(nums.substr(nums.find(',') + 1).c_str());
	DWORD aoffimmi = start;
	aoffimmi |= (n1 & 0xF) << 9;
	aoffimmi |= (n2 & 0xF) << 13;
	aoffimmi |= (n3 & 0xF) << 17;
	return aoffimmi;
}

static unordered_map<string, vector<DWORD>> hackMap = {
	{ "dcl_output oMask", { 0x02000065, 0x0000F000 } },
};

static unordered_map<string, vector<int>> ldMap = {
	// Hint: Compiling for shader model 5 always uses _indexable variants,
	//       so use shader model 4 to test vanilla and _aoffimmi (address
	//       offset immediate) variants. resource_types.hlsl has test cases
	//       for most of these - compile it for both shader models.
	// NOTE: There are also basic (non indexable, non address offset
	//       immediate) variants in the instruction table. A couple of
	//       those are not verified as AFAIK they are only present in
	//       shader model 5+ and compiling for that shader model always
	//       seems to use the _indexable variants found here.
	//               -DarkStarSword

	{ "ld_aoffimmi",                    { 3, 0x2d, 1 } },
	{ "ld_indexable",                   { 3, 0x2d, 2 } },
	{ "ld_aoffimmi_indexable",          { 3, 0x2d, 3 } },

	{ "ldms_aoffimmi",                  { 4, 0x2e, 1 } }, // Added and verified -DarkStarSword
	{ "ldms_indexable",                 { 4, 0x2e, 2 } },
	{ "ldms_aoffimmi_indexable",        { 4, 0x2e, 3 } },

	// _aoffimmi doesn't make sense for resinfo. Be aware that there are
	// _uint and _rcpfloat variants handled elsewhere in the code.
	//   -DarkStarSword
	{ "resinfo_indexable",              { 3, 0x3d, 2 } },

	{ "sample_aoffimmi",                { 4, 0x45, 1 } },
	{ "sample_indexable",               { 4, 0x45, 2 } },
	{ "sample_aoffimmi_indexable",      { 4, 0x45, 3 } },

	{ "sample_c_aoffimmi",              { 5, 0x46, 1 } },
	{ "sample_c_indexable",             { 5, 0x46, 2 } },
	{ "sample_c_aoffimmi_indexable",    { 5, 0x46, 3 } }, // Added and verified -DarkStarSword

	{ "sample_c_lz_aoffimmi",           { 5, 0x47, 1 } },
	{ "sample_c_lz_indexable",          { 5, 0x47, 2 } },
	{ "sample_c_lz_aoffimmi_indexable", { 5, 0x47, 3 } },

	{ "sample_l_aoffimmi",              { 5, 0x48, 1 } },
	{ "sample_l_indexable",             { 5, 0x48, 2 } },
	{ "sample_l_aoffimmi_indexable",    { 5, 0x48, 3 } },

	{ "sample_d_aoffimmi",              { 6, 0x49, 1 } }, // Added and verified -DarkStarSword
	{ "sample_d_indexable",             { 6, 0x49, 2 } },
	{ "sample_d_aoffimmi_indexable",    { 6, 0x49, 3 } }, // Added and verified -DarkStarSword

	{ "sample_b_aoffimmi",              { 5, 0x4a, 1 } }, // Added and verified -DarkStarSword
	{ "sample_b_indexable",             { 5, 0x4a, 2 } },
	{ "sample_b_aoffimmi_indexable",    { 5, 0x4a, 3 } }, // Added and verified -DarkStarSword

	{ "gather4_aoffimmi",               { 4, 0x6d, 1 } }, // Unverified (not in SM4 so only indexable variants?)
	{ "gather4_indexable",              { 4, 0x6d, 2 } },
	{ "gather4_aoffimmi_indexable",     { 4, 0x6d, 3 } },

	// _aoffimmi doesn't make sense for bufinfo
	{ "bufinfo_indexable",              { 2, 0x79, 2 } },

	{ "gather4_c_aoffimmi",             { 5, 0x7e, 1 } }, // Unverified (not in SM4 so only indexable variants?)
	{ "gather4_c_indexable",            { 5, 0x7e, 2 } },
	{ "gather4_c_aoffimmi_indexable",   { 5, 0x7e, 3 } },

	// gather4_po variants do not have an _aoffimmi variant by definition
	// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447084(v=vs.85).aspx
	{ "gather4_po_indexable",           { 5, 0x7f, 2 } },
	{ "gather4_po_c_indexable",         { 6, 0x80, 2 } },

	// RWTexture2D (etc), ByteAddressBuffer and StructuredBuffer have no
	// variants of .Load that takes an offset, so there are no _aoffimmi
	// variants for these:
	{ "ld_uav_typed_indexable",         { 3, 0xa3, 2 } },
	{ "ld_raw_indexable",               { 3, 0xa5, 2 } },
	{ "ld_structured_indexable",        { 4, 0xa7, 2 } },
};

static unordered_map<string, vector<int>> insMap = {
	{ "add",                       { 3, 0x00    } },
	{ "and",                       { 3, 0x01    } },
	{ "break",                     { 0, 0x02    } },
	{ "breakc_nz",                 { 1, 0x03, 0 } },
	{ "breakc_z",                  { 1, 0x03, 0 } },
	// TODO: call                     , 0x04
	// TODO: callc                    , 0x05
	{ "case",                      { 1, 0x06    } },
	{ "continue",                  { 0, 0x07    } },
	{ "continuec_nz",              { 1, 0x08, 0 } },
	{ "continuec_z",               { 1, 0x08, 0 } },
	{ "cut",                       { 0, 0x09    } },
	{ "default",                   { 0, 0x0a    } },
	{ "deriv_rtx",                 { 2, 0x0b    } },
	{ "deriv_rty",                 { 2, 0x0c    } },
	{ "discard_nz",                { 1, 0x0d, 0 } },
	{ "discard_z",                 { 1, 0x0d, 0 } },
	{ "div",                       { 3, 0x0e    } },
	{ "dp2",                       { 3, 0x0f    } },
	{ "dp3",                       { 3, 0x10    } },
	{ "dp4",                       { 3, 0x11    } },
	{ "else",                      { 0, 0x12    } },
	{ "emit",                      { 0, 0x13    } },
	{ "emit_then_cut",             { 0, 0x14    } }, // Partially verified - assembled & disassembled OK, but did not check against compiled shader -DSS
	{ "endif",                     { 0, 0x15    } },
	{ "endloop",                   { 0, 0x16    } },
	{ "endswitch",                 { 0, 0x17    } },
	{ "eq",                        { 3, 0x18    } },
	{ "exp",                       { 2, 0x19    } },
	{ "frc",                       { 2, 0x1a    } },
	{ "ftoi",                      { 2, 0x1b    } },
	{ "ftou",                      { 2, 0x1c    } },
	{ "ge",                        { 3, 0x1d    } },
	{ "iadd",                      { 3, 0x1e    } },
	{ "if_nz",                     { 1, 0x1f, 0 } },
	{ "if_z",                      { 1, 0x1f, 0 } },
	{ "ieq",                       { 3, 0x20    } },
	{ "ige",                       { 3, 0x21    } },
	{ "ilt",                       { 3, 0x22    } },
	{ "imad",                      { 4, 0x23    } },
	{ "imax",                      { 3, 0x24    } },
	{ "imin",                      { 3, 0x25    } },
	{ "imul",                      { 4, 0x26, 2 } },
	{ "ine",                       { 3, 0x27    } },
	{ "ineg",                      { 2, 0x28    } },
	{ "ishl",                      { 3, 0x29    } },
	{ "ishr",                      { 3, 0x2a    } },
	{ "itof",                      { 2, 0x2b    } },
	// TODO: label                    , 0x2c
	{ "ld",                        { 3, 0x2d    } }, // See also load table
	{ "ldms",                      { 4, 0x2e    } }, // See also load table
	{ "log",                       { 2, 0x2f    } },
	{ "loop",                      { 0, 0x30    } },
	{ "lt",                        { 3, 0x31    } },
	{ "mad",                       { 4, 0x32    } },
	{ "min",                       { 3, 0x33    } },
	{ "max",                       { 3, 0x34    } },
	// TODO: Custom data              , 0x35
	//       dcl_immediateConstantBuffer implemented elsewhere
	//       Other types from binary decompiler:
	//        - comment
	//        - debuginfo
	//        - opaque
	//        - shader message
	{ "mov",                       { 2, 0x36    } },
	{ "movc",                      { 4, 0x37    } },
	{ "mul",                       { 3, 0x38    } },
	{ "ne",                        { 3, 0x39    } },
	{ "nop",                       { 0, 0x3a    } }, // Added and verified -DarkStarSword
	{ "not",                       { 2, 0x3b    } },
	{ "or",                        { 3, 0x3c    } },
	{ "resinfo",                   { 3, 0x3d    } }, // See also load table
	{ "ret",                       { 0, 0x3e    } },
	{ "retc_nz",                   { 1, 0x3f, 0 } },
	{ "retc_z",                    { 1, 0x3f, 0 } },
	{ "round_ne",                  { 2, 0x40    } },
	{ "round_ni",                  { 2, 0x41    } },
	{ "round_pi",                  { 2, 0x42    } },
	{ "round_nz",                  { 2, 0x43    } },
	{ "round_z",                   { 2, 0x43    } },
	{ "rsq",                       { 2, 0x44    } },
	{ "sample",                    { 4, 0x45    } }, // See also load table
	{ "sample_c",                  { 5, 0x46    } }, // See also load table
	{ "sample_c_lz",               { 5, 0x47    } }, // See also load table
	{ "sample_l",                  { 5, 0x48    } }, // See also load table
	{ "sample_d",                  { 6, 0x49    } }, // See also load table
	{ "sampled",                   { 6, 0x49    } }, // Hmmm, possible typo? -DSS
	{ "sample_b",                  { 5, 0x4a    } }, // See also load table
	{ "sqrt",                      { 2, 0x4b    } },
	{ "switch",                    { 1, 0x4c, 0 } },
	{ "sincos",                    { 3, 0x4d, 2 } },
	{ "udiv",                      { 4, 0x4e, 2 } },
	{ "ult",                       { 3, 0x4f    } },
	{ "uge",                       { 3, 0x50    } },
	{ "umul",                      { 4, 0x51, 2 } },
	{ "umax",                      { 3, 0x53    } },
	{ "umin",                      { 3, 0x54    } },
	{ "ushr",                      { 3, 0x55    } },
	{ "utof",                      { 2, 0x56    } },
	{ "xor",                       { 3, 0x57    } },
	// dcl_resource                     0x58 // Implemented elsewhere
	// dcl_constantbuffer               0x59 // implemented elsewhere
	// dcl_sampler                      0x5a // Implemented elsewhere
	// dcl_indexrange                   0x5b // Implemented elsewhere
	// dcl_outputtopology               0x5c // Implemented elsewhere
	// dcl_inputprimitive               0x5d // Implemented elsewhere
	// dcl_maxout                       0x5e // Implemented elsewhere
	// dcl_input                        0x5f // Implemented elsewhere
	// dcl_input_sgv                    0x60 // Implemented elsewhere
	// dcl_input_siv                    0x61 // Implemented elsewhere
	// dcl_input_ps                     0x62 // Implemented elsewhere
	// dcl_input_ps_sgv                 0x63 // Implemented elsewhere
	// dcl_input_ps_siv                 0x64 // Implemented elsewhere
	// dcl_output                       0x65 // Implemented elsewhere
	// dcl_output_sgv                   0x66 // Implemented elsewhere
	// dcl_output_siv                   0x67 // Implemented elsewhere
	// dcl_temps                        0x68 // Implemented elsewhere
	// dcl_indexableTemp                0x69 // Implemented elsewhere
	// dcl_globalFlags                  0x6a // Implemented elsewhere
	// RESERVED_10                      0x6b
	{ "lod",                       { 4, 0x6c    } },
	{ "gather4",                   { 4, 0x6d    } }, // See also load table
	//"samplepos",                 { 3, 0x6e    } }, // Implemented elsewhere
	{ "sampleinfo",                { 2, 0x6f    } },
	// RESERVED_10_1                    0x70
	// hs_decls                         0x71 // Implemented elsewhere
	// hs_control_point_phase           0x72 // Implemented elsewhere
	// hs_fork_phase                    0x73 // Implemented elsewhere
	// hs_join_phase                    0x74 // Implemented elsewhere
	// emit_stream                      0x75 // Implemented elsewhere
	// cut_stream                       0x76 // Implemented elsewhere
	// emit_then_cut_stream             0x77 // Implemented elsewhere
	// TODO: interface_call             0x78
	{ "bufinfo",                   { 2, 0x79    } }, // Unverified (not in SM4 so only indexable variants?). See also load table.
	{ "deriv_rtx_coarse",          { 2, 0x7a    } },
	{ "deriv_rtx_fine",            { 2, 0x7b    } },
	{ "deriv_rty_coarse",          { 2, 0x7c    } },
	{ "deriv_rty_fine",            { 2, 0x7d    } },
	{ "gather4_c",                 { 5, 0x7e    } }, // Unverified (not in SM4 so only indexable variants?). See also load table.
	{ "gather4_po",                { 5, 0x7f    } }, // Unverified (not in SM4 so only indexable variants?). See also load table.
	{ "gather4_po_c",              { 6, 0x80    } }, // Unverified (not in SM4 so only indexable variants?). See also load table.
	{ "rcp",                       { 2, 0x81    } },
	{ "f32tof16",                  { 2, 0x82    } },
	{ "f16tof32",                  { 2, 0x83    } },
	{ "uaddc",                     { 4, 0x84    } }, // Partially verified - assembled & disassembled OK, but did not check against compiled shader -DSS
	{ "usubb",                     { 4, 0x85    } }, // Partially verified - assembled & disassembled OK, but did not check against compiled shader -DSS
	{ "countbits",                 { 2, 0x86    } },
	{ "firstbit_hi",               { 2, 0x87    } },
	{ "firstbit_lo",               { 2, 0x88    } },
	{ "firstbit_shi",              { 2, 0x89    } }, // Added and verified -DarkStarSword
	{ "ubfe",                      { 4, 0x8a    } },
	{ "ibfe",                      { 4, 0x8b    } },
	{ "bfi",                       { 5, 0x8c    } },
	{ "bfrev",                     { 2, 0x8d    } },
	{ "swapc",                     { 5, 0x8e, 2 } },
	// dcl_stream                       0x8f // Implemented elsewhere
	// dcl_function_body                0x90 // TODO
	// dcl_function_table               0x91 // TODO
	// dcl_interface                    0x92 // TODO
	// dcl_input_control_point_count    0x93 // Implemented elsewhere
	// dcl_output_control_point_count   0x94 // Implemented elsewhere
	// dcl_tessellator_domain           0x95 // Implemented elsewhere
	// dcl_tessellator_partitioning     0x96 // Implemented elsewhere
	// dcl_tessellator_output_primitive 0x97 // Implemented elsewhere
	// dcl_hs_max_tessfactor            0x98 // Implemented elsewhere
	// dcl_hs_fork_phase_instance_count 0x99 // Implemented elsewhere
	// dcl_hs_join_phase_instance_count 0x9a // TODO
	{ "dcl_thread_group",          { 3, 0x9b    } },
	// dcl_uav_typed_*                  0x9c // Implemented elsewhere
	{ "dcl_uav_raw",               { 1, 0x9d, 0 } }, // _glc variant handled elsewhere
	{ "dcl_uav_structured",        { 2, 0x9e, 0 } }, // _glc variant handled elsewhere
	{ "dcl_tgsm_raw",              { 2, 0x9f, 0 } },
	{ "dcl_tgsm_structured",       { 3, 0xa0, 0 } },
	// dcl_resource_raw                 0xa1 // Implemented elsewhere
	// dcl_resource_structured          0xa2 // Implemented elsewhere
	{ "ld_uav_typed",              { 3, 0xa3    } }, // Unverified (not in SM4 so only indexable variants?) See also load table.
	// store_uav_typed                  0xa4 // Implemented elsewhere
	{ "ld_raw",                    { 3, 0xa5    } }, // See also load table
	{ "store_raw",                 { 3, 0xa6    } },
	{ "ld_structured",             { 4, 0xa7    } }, // See also load table
	{ "store_structured",          { 4, 0xa8    } },
	{ "atomic_and",                { 3, 0xa9, 0 } },
	{ "atomic_or",                 { 3, 0xaa, 0 } },
	{ "atomic_xor",                { 3, 0xab, 0 } }, // Added and verified -DarkStarSword
	{ "atomic_cmp_store",          { 4, 0xac, 0 } }, // Added and verified -DarkStarSword
	{ "atomic_iadd",               { 3, 0xad, 0 } },
	{ "atomic_imax",               { 3, 0xae, 0 } },
	{ "atomic_imin",               { 3, 0xaf, 0 } },
	{ "atomic_umax",               { 3, 0xb0, 0 } },
	{ "atomic_umin",               { 3, 0xb1, 0 } },
	{ "imm_atomic_alloc",          { 2, 0xb2    } },
	{ "imm_atomic_consume",        { 2, 0xb3    } },
	{ "imm_atomic_iadd",           { 4, 0xb4    } },
	{ "imm_atomic_and",            { 4, 0xb5    } },
	{ "imm_atomic_or",             { 4, 0xb6    } }, // Added and verified -DarkStarSword
	{ "imm_atomic_xor",            { 4, 0xb7    } }, // Added and verified -DarkStarSword
	{ "imm_atomic_exch",           { 4, 0xb8    } },
	{ "imm_atomic_cmp_exch",       { 5, 0xb9    } },
	{ "imm_atomic_imax",           { 4, 0xba    } }, // Added and verified -DarkStarSword
	{ "imm_atomic_imin",           { 4, 0xbb    } }, // Added and verified -DarkStarSword
	{ "imm_atomic_umax",           { 4, 0xbc    } }, // Added and verified -DarkStarSword
	{ "imm_atomic_umin",           { 4, 0xbd    } }, // Added and verified -DarkStarSword
	// sync_*                           0xbe // Implemented elsewhere
	{ "dadd",                      { 3, 0xbf    } }, // Added and verified -DarkStarSword
	{ "dmax",                      { 3, 0xc0    } }, // Added and verified -DarkStarSword
	{ "dmin",                      { 3, 0xc1    } }, // Added and verified -DarkStarSword
	{ "dmul",                      { 3, 0xc2    } }, // Added and verified -DarkStarSword
	{ "deq",                       { 3, 0xc3    } }, // Added and verified -DarkStarSword
	{ "dge",                       { 3, 0xc4    } }, // Added and verified -DarkStarSword
	{ "dlt",                       { 3, 0xc5    } }, // Added and verified -DarkStarSword
	{ "dne",                       { 3, 0xc6    } }, // Added and verified -DarkStarSword
	{ "dmov",                      { 2, 0xc7    } }, // Unverified
	{ "dmovc",                     { 4, 0xc8    } }, // Added and verified -DarkStarSword
	{ "dtof",                      { 2, 0xc9    } }, // Added and verified -DarkStarSword
	{ "ftod",                      { 2, 0xca    } }, // Added and verified -DarkStarSword
	{ "eval_snapped",              { 3, 0xcb    } }, // Added and verified -DarkStarSword
	{ "eval_sample_index",         { 3, 0xcc    } },
	{ "eval_centroid",             { 2, 0xcd    } }, // Added and verified -DarkStarSword
	{ "dcl_gsinstances",           { 1, 0xce    } }, // Added and verified -DarkStarSword
	{ "abort",                     { 0, 0xcf    } }, // Debug layer instruction. Added and verified -DarkStarSword
	// TODO: debug_break                0xd0
	// RESERVED_11                      0xd1
	{ "ddiv",                      { 3, 0xd2    } }, // Added and verified -DarkStarSword
	{ "dfma",                      { 4, 0xd3    } }, // Added and verified -DarkStarSword
	{ "drcp",                      { 2, 0xd4    } }, // Added and verified -DarkStarSword
	{ "msad",                      { 4, 0xd5    } }, // Added and verified -DarkStarSword
	{ "dtoi",                      { 2, 0xd6    } }, // Added and verified -DarkStarSword
	{ "dtou",                      { 2, 0xd7    } }, // Added and verified -DarkStarSword
	{ "itod",                      { 2, 0xd8    } }, // Added and verified -DarkStarSword
	{ "utod",                      { 2, 0xd9    } }, // Added and verified -DarkStarSword
};

static void assembleResourceDeclarationType(string *type, vector<DWORD> *v)
{
	// The resource declarations all use the same format strings and
	// encoding, so do this once, consistently, and handle all confirmed
	// values. Use resource_types.hlsl to check the values -DarkStarSword
	//
	// XXX: Should parse each keyword separately in case we ever run into
	// more complicated mixed types, which is theoretically possible, but
	// we've never seen the compiler produce it in practice.

	if (*type == "(float,float,float,float)")
		v->push_back(0x5555);
	if (*type == "(uint,uint,uint,uint)")
		v->push_back(0x4444);
	if (*type == "(sint,sint,sint,sint)")
		v->push_back(0x3333);
	if (*type == "(snorm,snorm,snorm,snorm)")
		v->push_back(0x2222);
	if (*type == "(unorm,unorm,unorm,unorm)")
		v->push_back(0x1111);
	if (*type == "(double,<continued>,<unused>,<unused>)")
		v->push_back(0x9987);
	if (*type == "(double,<continued>,double,<continued>)")
		v->push_back(0x8787);
	if (*type == "(mixed,mixed,mixed,mixed)") // TestShaders/GameExamples/DR3/cc5538d28f8fd45e-vs
		v->push_back(0x6666);
	// FIXME: Fail gracefully if we don't recognise the type, since doing
	// nothing here will cause a hang!
}

static void assembleSystemValue(string *sv, vector<DWORD> *os)
{
	// All possible system values used in any of the dcl_*_s?v
	// declarations (s?v = system value). Not all system values make sense
	// for all types of shaders, and some are only inputs or only outputs,
	// but it's not our responsibility to validate that - we just want to
	// handle all possible cases. -DarkStarSword

	if (*sv == "position")
		os->push_back(1);
	else if (*sv == "clip_distance")
		os->push_back(2);
	else if (*sv == "cull_distance")
		os->push_back(3);
	else if (*sv == "rendertarget_array_index")
		os->push_back(4);
	else if (*sv == "viewport_array_index")
		os->push_back(5);
	else if (*sv == "vertex_id")
		os->push_back(6);
	else if (*sv == "primitive_id")
		os->push_back(7);
	else if (*sv == "instance_id")
		os->push_back(8);
	else if (*sv == "is_front_face")
		os->push_back(9);
	else if (*sv == "sampleIndex")
		os->push_back(10);
	else if (*sv == "finalQuadUeq0EdgeTessFactor")
		os->push_back(11);
	else if (*sv == "finalQuadVeq0EdgeTessFactor")
		os->push_back(12);
	else if (*sv == "finalQuadUeq1EdgeTessFactor")
		os->push_back(13);
	else if (*sv == "finalQuadVeq1EdgeTessFactor")
		os->push_back(14);
	else if (*sv == "finalQuadUInsideTessFactor")
		os->push_back(15);
	else if (*sv == "finalQuadVInsideTessFactor")
		os->push_back(16);
	else if (*sv == "finalTriUeq0EdgeTessFactor")
		os->push_back(17);
	else if (*sv == "finalTriVeq0EdgeTessFactor")
		os->push_back(18);
	else if (*sv == "finalTriWeq0EdgeTessFactor")
		os->push_back(19);
	else if (*sv == "finalTriInsideTessFactor")
		os->push_back(20);
	else if (*sv == "finalLineDetailTessFactor")
		os->push_back(21);
	else if (*sv == "finalLineDensityTessFactor")
		os->push_back(22);

	// FIXME: Fail gracefully if we don't recognise the system value,
	// otherwise we might generate a corrupt shader and crash DirectX.
}

static int interpolationMode(vector<string> &w, int def)
{
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dn280473(v=vs.85).aspx

	if (w[1] == "constant")
		return 1;
	if (w[1] != "linear")
		return def;

	if (w[2] == "noperspective") {
		if (w[3] == "sample")
			return 7;
		if (w[3] == "centroid")
			return 5;
		return 4;
	}
	if (w[2] == "centroid")
		return 3;
	if (w[2] == "sample")
		return 6;

	return 2;
}

static unsigned parseSyncFlags(string *w)
{
	unsigned flags = 0;
	int pos = 4;

	// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447241(v=vs.85).aspx
	//
	// Some of these variants (_sat_ugroup) have only been validated by
	// assembling & disassembling them as we weren't able to coerce fcx to
	// generate them, but that does not mean we won't ever see them, as
	// they could be generated by compiler optimisations that recognise
	// that a particular UAV does not need to be globally coherent.
	//
	// Variants we have validated with compiled shaders written in HLSL:
	//
	// sync_g           - GroupMemoryBarrier()
	// sync_g_t         - GroupMemoryBarrierWithGroupSync()
	// sync_uglobal     - DeviceMemoryBarrier()
	// sync_uglobal_t   - DeviceMemoryBarrierWithGroupSync()
	// sync_uglobal_g   - AllMemoryBarrier()
	// sync_uglobal_g_t - AllMemoryBarrierWithGroupSync()
	//
	//   -DarkStarSword

	while (true) {
		if (w->substr(pos, 2) == "_t") {
			pos += 2;
			flags |= 0x1;
			continue;
		}
		if (w->substr(pos, 2) == "_g") {
			pos += 2;
			flags |= 0x2;
			continue;
		}
		if (w->substr(pos, 11) == "_sat_ugroup") {
			// NOTE: MSDN does not mention the "_sat"
			pos += 11;
			flags |= 0x4;
			continue;
		}
		if (w->substr(pos, 8) == "_uglobal") {
			pos += 8;
			flags |= 0x8;
			continue;
		}
		if (w->substr(pos, 12) == "_sat_uglobal") {
			// Combination of _sat_ugroup and _uglobal flags
			// Worth noting that _ugroup is a lighter version of
			// _uglobal, so I guess they used the fact that these
			// flags don't make sense to use together to overload
			// the meaning of this combination.
			pos += 12;
			flags |= 0xc;
			continue;
		}
		return flags;
	}

}

static void check_num_ops(string &s, vector<string> &w, int min_expected, int max_expected = -1)
{
	int num_operands = (int)w.size() - 1;
	char buf[80];

	// We will throw a parse error if there are too few operands. That
	// should be relatively uncontroversial, since an exception will have
	// already been thrown when the assembler tries to access w out of
	// bounds.
	//
	// Somewhat more controversial is that we are also going to throw an
	// exception if there are more operands than expected. Before this
	// would have assembled the expected operands and silently dropped the
	// extras, so this may have been masking bugs... or, it could be
	// masking someone using the wrong comment character that will now
	// start failing.
	//
	// But at the end of the day, the benefits of warning when someone
	// makes a mistake vastly outweigh the potential for a fix to be broken
	// by this - and they can always stick to an old version of 3DMigoto...
	// or fix their bug.

	if (max_expected == -1)
		max_expected = min_expected;

	if (num_operands < min_expected || (max_expected && num_operands > max_expected)) {
		_snprintf_s(buf, 80, _TRUNCATE,
			"Invalid number of operands for instruction. Expected %i-%i, found %i",
			min_expected, max_expected, num_operands);
		throw AssemblerParseError(s, buf);
	}
}

static string translate_string_operand(string &in)
{
	// Strip quote characters:
	string ret = in.substr(1, in.size() - 2);

	// Translate escape sequences:
	for (size_t pos = ret.find('\\'); pos < ret.size() - 1; pos = ret.find('\\', pos + 1)) {
		switch(ret[pos+1]) {
			case 'b':
				ret.replace(pos, 2, "\b");
				break;
			case 'n':
				ret.replace(pos, 2, "\n");
				break;
			case 'r':
				ret.replace(pos, 2, "\r");
				break;
			case 't':
				ret.replace(pos, 2, "\t");
				break;
			case '\\':
				ret.replace(pos, 2, "\\");
				break;
			// The disassembler does not encode all non-printable
			// characters as escape sequences, so some will show up
			// as an ambiguous dot "." instead. We could add a
			// fixup case for this, but it's obscure enough that
			// it's not worth it. At least \a \f \v are affected.
		}
	}
	return ret;
}

// Printf/errorf instructions can potentially allow us to extract data from the
// shader when the debug layer is enabled, potentially making them quite valuable
static vector<DWORD> assemble_printf(string &s, vector<DWORD> &v, vector<string> &w, bool errorf)
{
	shader_ins ins = {0};
	ins.opcode = 0x35;
	ins._11_23 = 0x4;
	v.push_back(ins.op);

	uint32_t insLen = 0;
	v.push_back(insLen); // placeholder
	if (errorf)
		v.push_back(0x00200103);
	else
		v.push_back(0x00200102);
	v.push_back(1); // ?

	check_num_ops(s, w, 1, 0);
	string msg = translate_string_operand(w[1]);
	uint32_t msgLen = (uint32_t)msg.size();
	v.push_back(msgLen);

	uint32_t numOps = (uint32_t)w.size() - 2;
	v.push_back(numOps);
	v.push_back(numOps * 2);
	for (uint32_t i = 0; i < numOps; i++) {
		vector<DWORD> os = assembleOp(w[i + 2]);
		v.insert(v.end(), os.begin(), os.end());
	}

	// Resize large enough to fit the message with a NULL
	// terminator, rounded up for padding:
	uintptr_t msgOff = (uintptr_t)v.size() * 4;
	insLen = (msgLen + 4) / 4 + (uint32_t)v.size();
	v.resize(insLen);
	v[1] = insLen;
	memcpy((char*)v.data() + msgOff, msg.c_str(), msgLen);

	return v;
}

static vector<DWORD> assemble_undecipherable_custom_data(string &s, vector<DWORD> &v, vector<string> &w)
{
	uint32_t numOps, word, i;

	check_num_ops(s, w, 2, 0);
	if (w[1] != "custom" || w[2] != "data")
		throw AssemblerParseError(s, "Unrecognised instruction");

	// If we only have the words "undecipherable custom data" it's from the
	// MS disassembler and there is nothing we can really do with it, but
	// if it went through our disassembler fixup path we appended a hexdump
	// of the instruction that we can now reassemble:

	numOps = (uint32_t)w.size() - 3;
	for (i = 0; i < numOps; i++) {
		sscanf_s(w[i + 3].c_str(), "%x", &word);
		v.push_back(word);
	}

	return v;
}

static vector<DWORD> assembleIns(string s)
{
	unsigned msaa_samples = 0;

	if (hackMap.find(s) != hackMap.end()) {
		auto v = hackMap[s];
		return v;
	}
	DWORD op = 0;
	shader_ins* ins = (shader_ins*)&op;
	size_t pos = s.find("[precise");
	if (pos != string::npos) {
		size_t endPos = s.find("]", pos) + 1;
		string precise = s.substr(pos, endPos - pos);
		s.erase(pos, endPos - pos);
		int x = 0;
		int y = 0;
		int z = 0;
		int w = 0;
		if (precise == "[precise]") {
			x = 256;
			y = 512;
			z = 1024;
			w = 2048;
		}
		if (precise.find("x") != string::npos)
			x = 256;
		if (precise.find("y") != string::npos)
			y = 512;
		if (precise.find("z") != string::npos)
			z = 1024;
		if (precise.find("w") != string::npos)
			w = 2048;
		ins->_11_23 = x | y | z | w;
	}
	// Handles _uint variant of resinfo instruction:
	pos = s.find("_uint");
	if (pos != string::npos) {
		s.erase(pos, 5);
		ins->_11_23 = 2;
	}
	// resinfo_rcpfloat partially verified - assembled & disassembled OK,
	// but did not check against compiled shader as HLSL lacks an intrinsic
	// that maps to this, and fxc does not seem to optimise to use it, but
	// that does not necessarily mean we will never see it. Note that MSDN
	// refers to this as _rcpFloat, but the disassembler uses _rcpfloat.
	//   -DarkStarSword
	pos = s.find("_rcpfloat");
	if (pos != string::npos) {
		s.erase(pos, 9);
		ins->_11_23 = 1;
	}
	vector<DWORD> v;
	vector<string> w = strToWords(s);
	string o = w[0];
	if (o == "sampleinfo" && ins->_11_23 == 2)
		ins->_11_23 = 1;
	if (s.find("_opc") < s.size()) {
		o = o.substr(0, o.find("_opc"));
		ins->_11_23 = 4096;
	}
	bool bNZ = o.find("_nz") < o.size();
	bool bZ = o.find("_z") < o.size();
	bool bSat = o.find("_sat") < o.size();
	if (bSat) o = o.substr(0, o.find("_sat"));
	bool bGlc = o.find("_glc") < o.size(); // Globally coherent UAV declaration
	if (bGlc) o = o.substr(0, o.find("_glc"));

	if (o == "hs_decls") {
		check_num_ops(s, w, 0);
		ins->opcode = 0x71;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_fork_phase") {
		check_num_ops(s, w, 0);
		ins->opcode = 0x73;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_join_phase") {
		check_num_ops(s, w, 0);
		ins->opcode = 0x74;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_control_point_phase") {
		check_num_ops(s, w, 0);
		ins->opcode = 0x72;
		ins->length = 1;
		v.push_back(op);
	} else if (o.substr(0, 3) == "ps_") {
		check_num_ops(s, w, 0);
		op = 0x00000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "vs_") {
		check_num_ops(s, w, 0);
		op = 0x10000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "gs_") {
		check_num_ops(s, w, 0);
		op = 0x20000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "hs_") {
		check_num_ops(s, w, 0);
		op = 0x30000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "ds_") {
		check_num_ops(s, w, 0);
		op = 0x40000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "cs_") {
		check_num_ops(s, w, 0);
		op = 0x50000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (w[0].substr(0, 4) == "sync") {
		ins->opcode = 0xbe;
		check_num_ops(s, w, 0);
		ins->_11_23 = parseSyncFlags(&w[0]);
		ins->length = 1;
		v.push_back(op);
	} else if (w[0] == "store_uav_typed") {
		ins->opcode = 0x86;
		int numOps = 3;
		check_num_ops(s, w, numOps);
		if (w[1][0] == 'u') {
			ins->opcode = 0xa4;
		}
		vector<vector<DWORD>> Os;
		int numSpecial = 1;
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + 1], i < numSpecial));
		ins->length = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += (int)Os[i].size();
		v.push_back(op);
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (insMap.find(o) != insMap.end()) {
		vector<int> vIns = insMap[o];
		int numOps = vIns[0];
		check_num_ops(s, w, numOps);
		vector<vector<DWORD>> Os;
		int numSpecial = 1;
		if (vIns.size() > 2)
			numSpecial = vIns[2];
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + 1], i < numSpecial));
		ins->opcode = vIns[1];
		if (bSat)
			ins->_11_23 |= 0x04;
		if (bNZ)
			ins->_11_23 |= 0x80;
		if (bZ)
			ins->_11_23 |= 0x00;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += (int)Os[i].size();
		v.push_back(op);
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (ldMap.find(o) != ldMap.end()) {
		vector<int> vIns = ldMap[o];
		int numOps = vIns[0];
		vector<vector<DWORD>> Os;
		int startPos = 1 + (vIns[2] & 3);
		//startPos = w.size() - numOps;
		check_num_ops(s, w, startPos + numOps - 1);
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + startPos], i == 0));
		ins->opcode = vIns[1];
		ins->length = 1 + (vIns[2] & 3);
		ins->extended = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += (int)Os[i].size();
		v.push_back(op);
		if (vIns[2] == 3)
			v.push_back(parseAoffimmi(0x80000001, w[1]));
		if (vIns[2] == 1)
			v.push_back(parseAoffimmi(1, w[1]));
		if (vIns[2] & 2) {
			int c = 1;
			if (vIns[2] == 3)
				c = 2;
			if (w[c] == "(texture1d)")
				v.push_back(0x80000082);
			if (w[c] == "(texture1darray)")
				v.push_back(0x800001C2);
			if (w[c] == "(texture2d)")
				v.push_back(0x800000C2);
			if (w[c] == "(texture2dms)")
				v.push_back(0x80000102);
			if (w[c] == "(texture2dmsarray)")
				v.push_back(0x80000242);
			if (w[c] == "(texture3d)")
				v.push_back(0x80000142);
			if (w[c] == "(texture2darray)")
				v.push_back(0x80000202);
			if (w[c] == "(texturecube)")
				v.push_back(0x80000182);
			if (w[c] == "(texturecubearray)")
				v.push_back(0x80000282);
			if (w[c] == "(buffer)")
				v.push_back(0x80000042);
			if (w[c] == "(raw_buffer)")
				v.push_back(0x800002C2);
			if (w[1].find("stride") != string::npos) {
				string stride = w[1].substr(27);
				stride = stride.substr(0, stride.size() - 1);
				DWORD d = 0x80000302;
				d += atoi(stride.c_str()) << 11;
				v.push_back(d);
			}
			if (w[startPos - 1] == "(float,float,float,float)")
				v.push_back(0x00155543);
			if (w[startPos - 1] == "(uint,uint,uint,uint)")
				v.push_back(0x00111103);
			if (w[startPos - 1] == "(sint,sint,sint,sint)")
				v.push_back(0x000CCCC3);
			if (w[startPos - 1] == "(mixed,mixed,mixed,mixed)")
				v.push_back(0x00199983);
			if (w[startPos - 1] == "(unorm,unorm,unorm,unorm)")
				v.push_back(0x00044443);
			// Added snorm and double types -DarkStarSword
			if (w[startPos - 1] == "(snorm,snorm,snorm,snorm)")
				v.push_back(0x00088883);
			if (w[startPos - 1] == "(double,<continued>,<unused>,<unused>)")
				v.push_back(0x002661c3);
			if (w[startPos - 1] == "(double,<continued>,double,<continued>)")
				v.push_back(0x0021e1c3);
		}
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (o == "dcl_input") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1], 1);
		ins->opcode = 0x5f;
		ins->length = 1 + os.size();
		// Should sort special value for text constants.
		if ((os[0] & 0xFF0) == 0)
			os[0] -= 1;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1], 1);
		ins->opcode = 0x65;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_resource_raw") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0xa1;
		ins->length = 3;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_resource_buffer") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 1;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture1d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 2;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture1darray") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 7;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_texture1d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 2;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_texture1darray") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 7;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture2d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 3;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_buffer") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 1;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture3d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 5;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_texture3d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 5;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texturecube") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 6;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texturecubearray") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 10;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture2darray") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x58;
		ins->_11_23 = 8;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_texture2d") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 3;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_uav_typed_texture2darray") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 0x9c;
		ins->_11_23 = 8;
		if (bGlc)
			ins->_11_23 |= 0x20;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[1], &v);
	} else if (o == "dcl_resource_texture2dms") {
		check_num_ops(s, w, 3);
		vector<DWORD> os = assembleOp(w[3]);
		ins->opcode = 0x58;
		// Changed this to calculate the value rather than hard coding
		// a small handful of values that we've seen. -DarkStarSword
		sscanf_s(w[1].c_str(), "(%d)", &msaa_samples);
		ins->_11_23 = (msaa_samples << 5) | 4;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[2], &v);
	} else if (o == "dcl_resource_texture2dmsarray") {
		check_num_ops(s, w, 3);
		vector<DWORD> os = assembleOp(w[3]);
		ins->opcode = 0x58;
		// Changed this to calculate the value rather than hard coding
		// a small handful of values that we've seen. -DarkStarSword
		sscanf_s(w[1].c_str(), "(%d)", &msaa_samples);
		ins->_11_23 = (msaa_samples << 5) | 9;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		assembleResourceDeclarationType(&w[2], &v);
	} else if (o == "dcl_indexrange") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 0x5b;
		ins->length = 2 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_temps") {
		ins->opcode = 0x68;
		ins->length = 2;
		v.push_back(op);
		check_num_ops(s, w, 1);
		v.push_back(atoi(w[1].c_str()));
	} else if (o == "dcl_resource_structured") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0xa2;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_sampler") {
		check_num_ops(s, w, 1, 2);
		vector<DWORD> os = assembleOp(w[1]);
		os[0] = 0x106000;
		ins->opcode = 0x5a;
		if (w.size() > 2) {
			if (w[2] == "mode_default") {
				ins->_11_23 = 0;
			} else if (w[2] == "mode_comparison") {
				ins->_11_23 = 1;
			}
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_globalFlags") {
		ins->opcode = 0x6a;
		ins->length = 1;
		ins->_11_23 = 0;
		for (unsigned i = 1; i < w.size(); i += 2) {
			// Changed this to use a loop rather than parsing a
			// fixed number of arguments. Added double precision,
			// minimum precision, skipOptimization and 11.1 shader
			// extension flags.
			// FIXME: Missing D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE
			// FIXME: Missing D3D_SHADER_REQUIRES_64_UAVS
			// FIXME: Missing D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING
			// FIXME: Missing D3D_SHADER_REQUIRES_TILED_RESOURCES
			//   - https://docs.microsoft.com/en-gb/windows/desktop/api/d3d11shader/nf-d3d11shader-id3d11shaderreflection-getrequiresflags
			//   -DarkStarSword
			string s = w[i];
			if (s == "refactoringAllowed")
				ins->_11_23 |= 0x01;
			if (s == "enableDoublePrecisionFloatOps")
				ins->_11_23 |= 0x02;
			if (s == "forceEarlyDepthStencil")
				ins->_11_23 |= 0x04;
			if (s == "enableRawAndStructuredBuffers")
				ins->_11_23 |= 0x08;
			if (s == "skipOptimization")
				ins->_11_23 |= 0x10;
			if (s == "enableMinimumPrecision")
				ins->_11_23 |= 0x20;
			if (s == "enable11_1DoubleExtensions")
				ins->_11_23 |= 0x40;
			if (s == "enable11_1ShaderExtensions")
				ins->_11_23 |= 0x80;
		}
		v.push_back(op);
	} else if (o == "dcl_constantbuffer") {
		check_num_ops(s, w, 1, 2);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x59;
		if (w.size() > 2) {
			if (w[2] == "dynamicIndexed")
				ins->_11_23 = 1;
			else if (w[2] == "immediateIndexed")
				ins->_11_23 = 0;
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output_sgv") {
		// Added and verified. Used when writing to SV_IsFrontFace in a
		// geometry shader. -DarkStarSword
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 0x66;
		assembleSystemValue(&w[2], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output_siv") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 0x67;
		assembleSystemValue(&w[2], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_siv") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 0x61;
		assembleSystemValue(&w[2], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_sgv") {
		check_num_ops(s, w, 2);
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 0x60;
		assembleSystemValue(&w[2], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_ps") {
		vector<DWORD> os;
		ins->opcode = 0x62;
		// Switched to use common interpolation mode parsing to catch
		// more variants -DarkStarSword
		ins->_11_23 = interpolationMode(w, 0); // FIXME: Default?
		os = assembleOp(w[w.size() - 1], true);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_ps_sgv") {
		// Fixed for d3dcompiler_47 disassembly that includes an
		// interpolationMode missing from d3dcompiler_46 disassembly
		// e.g.
		// d3dcompiler_46: dcl_input_ps_sgv v6.x, is_front_face
		// d3dcompiler_47: dcl_input_ps_sgv constant v6.x, is_front_face
		//   -DarkStarSword
		check_num_ops(s, w, 2, 5);
		vector<DWORD> os = assembleOp(w[w.size() - 2], true);
		ins->opcode = 0x63;
		ins->_11_23 = interpolationMode(w, 1);
		if (w.size() > 2)
			assembleSystemValue(&w[w.size() - 1], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_ps_siv") {
		vector<DWORD> os;
		ins->opcode = 0x64;
		// Switched to use common interpolation mode parsing (fixes
		// missing linear noperspective sample case in WATCH_DOGS2) and
		// system value parsing (fixes missing viewport_array_index)
		//   -DarkStarSword
		check_num_ops(s, w, 2, 5);
		ins->_11_23 = interpolationMode(w, 0); // FIXME: Default?
		os = assembleOp(w[w.size() - 2], true);
		assembleSystemValue(&w[w.size() - 1], &os);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_indexableTemp") {
		check_num_ops(s, w, 2);
		string s1 = w[1].erase(0, 1);
		string s2 = s1.substr(0, s1.find('['));
		string s3 = s1.substr(s1.find('[') + 1);
		s3.erase(s3.end() - 1, s3.end());
		ins->opcode = 0x69;
		ins->length = 4;
		v.push_back(op);
		v.push_back(atoi(s2.c_str()));
		v.push_back(atoi(s3.c_str()));
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_immediateConstantBuffer") {
		vector<DWORD> os;
		ins->opcode = 0x35;
		ins->_11_23 = 3;
		ins->length = 0;
		DWORD length = 2;
		DWORD offset = 3;
		// The modulus here is by 5, matching the below offset += 5
		if ((w.size() - offset) % 5 != 0)
			throw AssemblerParseError(s, "Immediate Constant Buffer must have a multiple of four values");
		while (offset < w.size()) {
			string s1 = w[offset + 0];
			s1 = s1.substr(0, s1.find(','));
			string s2 = w[offset + 1];
			s2 = s2.substr(0, s2.find(','));
			string s3 = w[offset + 2];
			s3 = s3.substr(0, s3.find(','));
			string s4 = w[offset + 3];
			s4 = s4.substr(0, s4.find('}'));
			os.push_back(strToDWORD(s1));
			os.push_back(strToDWORD(s2));
			os.push_back(strToDWORD(s3));
			os.push_back(strToDWORD(s4));
			length += 4;
			offset += 5;
		}
		v.push_back(op);
		v.push_back(length);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_tessellator_partitioning") {
		ins->opcode = 0x96;
		ins->length = 1;
		check_num_ops(s, w, 1);
		if (w[1] == "partitioning_integer")
			ins->_11_23 = 1;
		else if (w[1] == "partitioning_pow2")
			ins->_11_23 = 2;
		else if (w[1] == "partitioning_fractional_odd")
			ins->_11_23 = 3;
		else if (w[1] == "partitioning_fractional_even")
			ins->_11_23 = 4;
		// Added pow2 -DarkStarSword
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ff471446(v=vs.85).aspx
		v.push_back(op);
	} else if (o == "dcl_tessellator_output_primitive") {
		ins->opcode = 0x97;
		ins->length = 1;
		check_num_ops(s, w, 1);
		if (w[1] == "output_point")
			ins->_11_23 = 1;
		else if (w[1] == "output_line")
			ins->_11_23 = 2;
		else if (w[1] == "output_triangle_cw")
			ins->_11_23 = 3;
		else if (w[1] == "output_triangle_ccw")
			ins->_11_23 = 4;
		// Added output_point -DarkStarSword
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ff471445(v=vs.85).aspx
		v.push_back(op);
	} else if (o == "dcl_tessellator_domain") {
		ins->opcode = 0x95;
		ins->length = 1;
		check_num_ops(s, w, 1);
		if (w[1] == "domain_isoline")
			ins->_11_23 = 1;
		else if (w[1] == "domain_tri")
			ins->_11_23 = 2;
		else if (w[1] == "domain_quad")
			ins->_11_23 = 3;
		v.push_back(op);
	} else if (o == "dcl_stream") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x8f;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "emit_stream") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x75;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "cut_stream") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x76;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "emit_then_cut_stream") {
		// Partially verified - assembled & disassembled OK, but did not
		// check against compiled shader as fxc never generates this
		//   -DarkStarSword
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x77;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_outputtopology") {
		ins->opcode = 0x5c;
		ins->length = 1;
		check_num_ops(s, w, 1);
		if (w[1] == "pointlist")
			ins->_11_23 = 1;
		else if (w[1] == "trianglestrip")
			ins->_11_23 = 5;
		else if (w[1] == "linestrip")
			ins->_11_23 = 3;
		// Added point list -DarkStarSword
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509661(v=vs.85).aspx
		v.push_back(op);
	} else if (o == "dcl_output_control_point_count") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x94;
		ins->_11_23 = os[0];
		ins->length = 1;
		v.push_back(op);
	} else if (o == "dcl_input_control_point_count") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x93;
		ins->_11_23 = os[0];
		ins->length = 1;
		v.push_back(op);
	} else if (o == "dcl_maxout") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x5e;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_inputprimitive") {
		ins->opcode = 0x5d;
		ins->length = 1;
		check_num_ops(s, w, 1);
		if (w[1] == "point")
			ins->_11_23 = 1;
		else if (w[1] == "line")
			ins->_11_23 = 2;
		else if (w[1] == "triangle")
			ins->_11_23 = 3;
		else if (w[1] == "lineadj")
			ins->_11_23 = 6;
		else if (w[1] == "triangleadj")
			ins->_11_23 = 7;
		// Added "lineadj" -DarkStarSword
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509609(v=vs.85).aspx
		v.push_back(op);
	} else if (o == "dcl_hs_max_tessfactor") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x98;
		ins->length = 1 + os.size() - 1;
		v.push_back(op);
		v.insert(v.end(), os.begin() + 1, os.end());
	} else if (o == "dcl_hs_fork_phase_instance_count") {
		check_num_ops(s, w, 1);
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 0x99;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "samplepos") {
		// samplepos can either be used with a texture register, or the
		// rasterizer. In the former case it has an extra 0 appended.
		vector<vector<DWORD>> os;
		ins->opcode = 0x6e;
		int numOps = 3;
		check_num_ops(s, w, numOps);
		for (int i = 0; i < numOps; i++)
			os.push_back(assembleOp(w[i + 1], i < 1));

		// When the instruction operates on a texture register
		// (GetSamplerPosition) there is an extra 0 inserted that is
		// not present when used on the rasterizer register
		// (GetRenderTargetSamplePosition). It's not clear if there are
		// any cases where this should be non-zero:
		if (w[2][0] == 't') {
			numOps++;
			os.push_back(vector<DWORD>{0});
		}

		ins->length = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += (int)os[i].size();

		v.push_back(op);
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), os[i].begin(), os[i].end());
	} else if (o == "printf") {
		return assemble_printf(s, v, w, false);
	} else if (o == "errorf") {
		return assemble_printf(s, v, w, true);
	} else if (o == "undecipherable") {
		return assemble_undecipherable_custom_data(s, v, w);
	} else {
		throw AssemblerParseError(s, "Unrecognised instruction");
	}

	return v;
}

static string assembleAndCompare(string s, vector<DWORD> v)
{
	string s2;
	int numSpaces = 0;
	while (memcmp(s.c_str(), " ", 1) == 0) {
		s.erase(s.begin());
		numSpaces++;
	}
	size_t lastLiteral = 0;
	size_t lastEnd = 0;
	vector<DWORD> v2;
	string sNew = s;
	string s3;
	bool valid = true;

	try {
		v2 = assembleIns(s);
	} catch (AssemblerParseError) {
		// Parse error, but not much we can / should do at this point
		// since we're acting as a disassembler (maybe we're
		// disassembling a shader with an instruction, operand, etc. we
		// don't yet support), so just return the assembly unchanged.
		// The parse error will be caught elsewhere, either when
		// attempting to assembling the shader, or when running a
		// validation pass over it.
		return s;
	}

	if (v2.size() > 0) {
		if (v2.size() == v.size()) {
			for (DWORD i = 0; i < v.size(); i++) {
				if (v[i] == 0x1835) {
					int size = v[++i];
					int loopSize = (size - 2) / 4;
					lastLiteral = sNew.find("{ { ");
					for (int j = 0; j < loopSize; j++) {
						i++;
						lastLiteral = sNew.find("{ ", lastLiteral + 1);
						lastEnd = sNew.find(",", lastLiteral + 1);
						s3 = sNew.substr(lastLiteral + 2, lastEnd - 2 - lastLiteral);
						if (v[i] != v2[i]) {
							string sLiteral = convertF(v[i]);
							string sBegin = sNew.substr(0, lastLiteral + 2); // +2 matches length of "{ "
							lastLiteral = sBegin.size();
							sBegin.append(sLiteral);
							sBegin.append(sNew.substr(lastEnd));
							sNew = sBegin;
						}
						i++;
						lastLiteral = sNew.find(",", lastLiteral + 1);
						lastEnd = sNew.find(",", lastLiteral + 1);
						s3 = sNew.substr(lastLiteral + 2, lastEnd - 2 - lastLiteral);
						if (v[i] != v2[i]) {
							string sLiteral = convertF(v[i]);
							string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
							if (sNew[lastLiteral + 1] == ' ')
								sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
							lastLiteral = sBegin.size();
							sBegin.append(sLiteral);
							sBegin.append(sNew.substr(lastEnd));
							sNew = sBegin;
						}
						i++;
						lastLiteral = sNew.find(",", lastLiteral + 1);
						lastEnd = sNew.find(",", lastLiteral + 1);
						s3 = sNew.substr(lastLiteral + 2, lastEnd - 2 - lastLiteral);
						if (v[i] != v2[i]) {
							string sLiteral = convertF(v[i]);
							string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
							if (sNew[lastLiteral + 1] == ' ')
								sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
							lastLiteral = sBegin.size();
							sBegin.append(sLiteral);
							sBegin.append(sNew.substr(lastEnd));
							sNew = sBegin;
						}
						i++;
						lastLiteral = sNew.find(",", lastLiteral + 1);
						lastEnd = sNew.find("}", lastLiteral + 1);
						s3 = sNew.substr(lastLiteral + 2, lastEnd - 2 - lastLiteral);
						if (v[i] != v2[i]) {
							string sLiteral = convertF(v[i]);
							string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
							if (sNew[lastLiteral + 1] == ' ')
								sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
							lastLiteral = sBegin.size();
							sBegin.append(sLiteral);
							sBegin.append(sNew.substr(lastEnd));
							sNew = sBegin;
						}
					}
					i++;
				} else if (v[i] == 0x4001) { // One component float literal
					i++;
					lastLiteral = sNew.find("l(", lastLiteral + 1);
					lastEnd = sNew.find(")", lastLiteral);
					if (v[i] != v2[i]) {
						string sLiteral = convertF(v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 2); // +2 matches length of "l("
						lastLiteral = sBegin.size();
						sBegin.append(sLiteral);
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
				} else if (v[i] == 0x4002) { // Four component float literal
					i++;
					lastLiteral = sNew.find("l(", lastLiteral);
					lastEnd = sNew.find(",", lastLiteral);
					if (v[i] != v2[i]) {
						string sLiteral = convertF(v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 2); // +2 matches length of "l("
						lastLiteral = sBegin.size();
						sBegin.append(sLiteral);
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
					i++;
					lastLiteral = sNew.find(",", lastLiteral + 1);
					lastEnd = sNew.find(",", lastLiteral + 1);
					if (v[i] != v2[i]) {
						string sLiteral = convertF(v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
						if (sNew[lastLiteral + 1] == ' ')
							sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
						lastLiteral = sBegin.size();
						sBegin.append(sLiteral);
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
					i++;
					lastLiteral = sNew.find(",", lastLiteral + 1);
					lastEnd = sNew.find(",", lastLiteral + 1);
					if (v[i] != v2[i]) {
						string sLiteral = convertF(v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
						if (sNew[lastLiteral + 1] == ' ')
							sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
						lastLiteral = sBegin.size();
						sBegin.append(sLiteral);
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
					i++;
					lastLiteral = sNew.find(",", lastLiteral + 1);
					lastEnd = sNew.find(")", lastLiteral + 1);
					if (v[i] != v2[i]) {
						string sLiteral = convertF(v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 1); // BUG FIXED: Was using +2, but "," is only length 1 -DSS
						if (sNew[lastLiteral + 1] == ' ')
							sBegin = sNew.substr(0, lastLiteral + 2); // Keep the space
						sBegin.append(sLiteral);
						lastLiteral = sBegin.size();
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
				} else if (v[i] == 0x5002) { // Double literal (two doubles x two components/double)
					i += 2;
					lastLiteral = sNew.find("d(", lastLiteral);
					lastEnd = sNew.find(",", lastLiteral);
					// If it's a hex literal it is split over four components instead of two:
					if (!sNew.compare(lastLiteral + 2, 2, "0x"))
						lastEnd = sNew.find(",", lastLiteral + 1);
					if (v[i-1] != v2[i-1] || v[i] != v2[i]) {
						string sLiteral = convertD(v[i-1], v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 2);
						lastLiteral = sBegin.size();
						sBegin.append(sLiteral);
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
					i += 2;
					lastLiteral = sNew.find(",", lastLiteral + 1);
					// If it's a hex literal it is split over four components instead of two:
					if (!sNew.compare(lastLiteral + 2, 2, "0x"))
						lastLiteral = sNew.find(",", lastLiteral + 1);
					lastEnd = sNew.find(")", lastLiteral + 1);
					if (v[i-1] != v2[i-1] || v[i] != v2[i]) {
						string sLiteral = convertD(v[i-1], v[i]);
						string sBegin = sNew.substr(0, lastLiteral + 1);
						if (sNew[lastLiteral + 1] == ' ')
							sBegin = sNew.substr(0, lastLiteral + 2);
						sBegin.append(sLiteral);
						lastLiteral = sBegin.size();
						sBegin.append(sNew.substr(lastEnd));
						sNew = sBegin;
					}
				} else if (v[i] != v2[i])
					valid = false;
			}
		} else {
			valid = false;
		}
		if (valid) {
			s2 = "!success ";
			s2.append(s);
			// codeBin[s2] = v;
		} else {
			if (v.size() == (v2.size() + 1)) {
				valid = true;
				int j = 0;
				for (size_t i = 1; i < v2.size() && valid; i++) {
					if (v[i + j] != v2[i]) {
						if (v[i + 1] == 0x1 && (v[i] & 0x7FFFFFFF) == v2[i]) {
							j = 1;
						} else {
							valid = false;
						}
					}
				}
			}
			if (valid) {
				s2 = "!success ";
				s2.append(s);
				// codeBin[s2] = v;
			} else {
				s2 = s;
				s2.append(" orig");
				codeBin[s2] = v;
				s2 = s;
				s2.append(" fail");
				codeBin[s2] = v2;
			}
		}
	} else {
		if (s != "undecipherable custom data") {
			s2 = "!missing ";
			s2.append(s);
			codeBin[s2] = v;
		}
	}
	string ret = "";
	for (int i = 0; i < numSpaces; i++) {
		ret.append(" ");
	}
	ret.append(sNew);
	return ret;
}

vector<string> stringToLines(const char* start, size_t size)
{
	vector<string> lines;
	const char* pStart = start;
	const char* pEnd = pStart;
	const char* pRealEnd = pStart + size;
	while (true) {
		while (*pEnd != '\n' && pEnd < pRealEnd) {
			pEnd++;
		}
		if (*pStart == 0) {
			break;
		}
		string s(pStart, pEnd++);
		pStart = pEnd;
		lines.push_back(s);
		if (pStart >= pRealEnd) {
			break;
		}
	}
	for (unsigned int i = 0; i < lines.size(); i++) {
		string s = lines[i];
		// Bug fixed: This would not strip carriage returns from DOS
		// style newlines if they were the only character on the line,
		// corrupting the resulting shader binary. -DarkStarSword
		if (s.size() >= 1 && s[s.size() - 1] == '\r')
			s.erase(--s.end());

		// Strip whitespace from the end of each line. This isn't
		// strictly necessary, but the MS disassembler inserts an extra
		// space after "ret ", "else " and "endif ", which has been a
		// gotcha for trying to match it with ShaderRegex since it's
		// easy to miss the fact that there is a space there and not
		// understand why the pattern isn't matching. By removing
		// excess spaces from the end of each line now we can make this
		// gotcha go away.
		while (s.size() >= 1 && s[s.size() - 1] == ' ')
			s.erase(--s.end());

		lines[i] = s;
	}
	return lines;
}
static vector<string> stringToLinesDX9(const char* start, size_t size) {
	vector<string> lines;
	const char* pStart = start;
	const char* pEnd = pStart;
	const char* pRealEnd = pStart + size;
	while (true) {
		while (*pEnd != '\n' && pEnd < pRealEnd) {
			pEnd++;
		}
		if (*pStart == 0) {
			break;
		}
		string s(pStart, pEnd++);
		pStart = pEnd;
		lines.push_back(s);
		if (pStart >= pRealEnd) {
			break;
		}
	}
	for (unsigned int i = 0; i < lines.size(); i++) {
		string s = lines[i];
		// Bug fixed: This would not strip carriage returns from DOS
		// style newlines if they were the only character on the line,
		// corrupting the resulting shader binary. -DarkStarSword
		if (s.size() >= 1 && s[s.size() - 1] == '\r')
			s.erase(--s.end());

		// Strip whitespace from the end of each line. This isn't
		// strictly necessary, but the MS disassembler inserts an extra
		// space after "ret ", "else " and "endif ", which has been a
		// gotcha for trying to match it with ShaderRegex since it's
		// easy to miss the fact that there is a space there and not
		// understand why the pattern isn't matching. By removing
		// excess spaces from the end of each line now we can make this
		// gotcha go away.
		while (s.size() >= 1 && s[s.size() - 1] == ' ')
			s.erase(--s.end());

		while (s.size() >= 1 && s[0] == ' ')
			s.erase(s.begin());

		lines[i] = s;
	}
	return lines;
}

static void hexdump_instruction(string &s, vector<DWORD> &v,
		vector<string> &lines, DWORD *i,
		int *multiLines, uint32_t line_byte_offset,
		int hexdump_mode)
{
	string hd;
	char buf[16];
	vector<DWORD> v2;
	string parse_error;

	try {
		v2 = assembleIns(s.substr(s.find_first_not_of(" ")));
	} catch (AssemblerParseError &e) {
		parse_error = e.desc;
	}

	// In mode 2 we only hexdump bad instructions:
	if (hexdump_mode == 2 && v == v2 && parse_error.empty())
		return;

	_snprintf_s(buf, 16, 16, "// %08x:", line_byte_offset);
	hd += buf;
	for (auto val : v) {
		_snprintf_s(buf, 16, 16, " %08x", val);
		hd += buf;
	}

	if (parse_error.empty()) {
		if (v != v2) {
			hd += "\n// * BUG * :";
			for (auto val : v2) {
				_snprintf_s(buf, 16, 16, " %08x", val);
				hd += buf;
			}
		}
	} else {
		hd += "\n// * BUG * : " + parse_error + ":";
	}

	vector<string>::iterator pos = lines.begin() + (*i)++;
	if (*multiLines)
		pos -= *multiLines - 1;
	*multiLines = 0;

	lines.insert(pos, hd);
}

static void encode_custom_data(string &s, vector<DWORD> &v)
{
	char buf[16];

	for (auto val : v) {
		_snprintf_s(buf, 16, 16, " %08x", val);
		s += buf;
	}
}

// This function aims to patch the RDEF comment block from d3dcompiler_47 to
// look like the old d3dcompiler_46 format so that ShaderRegex patterns that
// match it do not have to be updated for the newer format. There are some
// other differences ("cb" vs "CB" in dcl_constantbuffer lines, number of
// digits used in certain number formatting routines), but this is the most
// likely to interfere with ShaderRegex patterns.
static inline void patch_d3dcompiler_47_rdef(string *line, int *rdef_state)
{
	char name[256], type[16], format[16], dim[16], bind_type[16];
	int bind_idx, count, numRead;

	switch (*rdef_state) {
	case 0:
		if (line->compare("// Name                                 Type  Format         Dim      HLSL Bind  Count"))
			return;
		*line = "// Name                                 Type  Format         Dim Slot Elements";
		++*rdef_state;
		return;
	case 1:
		if (!line->compare("// ------------------------------ ---------- ------- ----------- -------------- ------"))
			*line = "// ------------------------------ ---------- ------- ----------- ---- --------";
		++*rdef_state;
		return;
	case 2:
		if (!line->compare("//")) {
			++*rdef_state;
			return;
		}
		numRead = sscanf_s(line->c_str(), "// %s %s %s %s %[a-z]%d %d",
			name, UCOUNTOF(name), type, UCOUNTOF(type), format, UCOUNTOF(format), dim, UCOUNTOF(dim),
			&bind_type, UCOUNTOF(bind_type), &bind_idx, &count);
		if (numRead == 7) {
			vector<char> buf(line->length() + 1); // d3dcompiler_47 lines should always be longer
			_snprintf_s(buf.data(), buf.size(), _TRUNCATE,
					"// %-30s %10s %7s %11s %4d %8d",
					name, type, format, dim, bind_idx, count);
			*line = buf.data();
		}
		return;
	}
}

static char txt_swiz[] = {
	'x', 'y', 'z', 'w'
};

// This function replaces the byte offsets in constant buffer and structured
// buffer declarations with indices, components, and/or ranges to both make it
// easier to match these up by eye, and to make ShaderRegex easier to grab and
// use variables with non-fixed offsets.
//
// Variables that fit within a single index will have the offset replaced with
// an index and components, and the Size is replaced with a Component count:
//
//   float localIblMipmapBias;          // Offset:   80 Size:     4 [unused]
//   float screenAspectRatio;           // Offset:   84 Size:     4 [unused]
//   float2 invResolution;              // Offset:   88 Size:     8
//   float4 shadowMapSizeAndInvSize;    // Offset:   96 Size:    16 [unused]
//                                                -- vv --
//   float localIblMipmapBias;          // Index:    5.x              Components:     1 [unused]
//   float screenAspectRatio;           // Index:    5.y              Components:     1 [unused]
//   float2 invResolution;              // Index:    5.zw             Components:     2
//   float4 shadowMapSizeAndInvSize;    // Index:    6.xyzw           Components:     4 [unused]
//
// Matrices or other items occupying up to four indices are expanded like this
// to allow for ShaderRegex to easily match all indices:
//
//   row_major float4x4 g_mViewProj;    // Offset:    0 Size:    64 [unused]
//                                                -- vv --
//   row_major float4x4 g_mViewProj;    // Index:    0 1 2 3          Components:    16 [unused]
//
// Items occupying more than four indices are expanded into a range as a
// compromise between clarity and ShaderRegex matches (can still match
// first/last in the pattern, and patching the shader to use dynamic indexing
// can allow other indices to be used in code):
//
//   row_major float4x4 g_mLightProj[4];// Offset:  128 Size:   256 [unused]
//                                                -- vv --
//   row_major float4x4 g_mLightProj[4];// Index:    8-23             Components:    64 [unused]
//
// Items occupying multiple indices, but where the last index is only partially
// occupied are also expanded to ranges, with the final component shown:
//
//   float _NeighborIsAdapting[4];      // Offset:   96 Size:    52
//                                                -- vv --
//   float _NeighborIsAdapting[4];      // Index:    6-9.x            Components:    13
//
// Entries in embedded structures that lack the "Size" field cannot be
// processed as richly as straight constant buffer entries since we can't
// (trivially) calculate their end index/component, but their Offset is still
// turned into an Index with a component shown if it is not aligned to x:
//
//       int m_PatchX;                  // Offset:    0
//       int m_PatchZ;                  // Offset:    4
//                                                -- vv --
//       int m_PatchX;                  // Index:    0
//       int m_PatchZ;                  // Index:    0.y
//
static inline void replace_cb_offsets_with_indices(string *line)
{
	int numRead, end1 = 0, end2 = 0;
	unsigned offset = 0, size = 0;
	unsigned first_swiz, last_swiz;
	unsigned first_idx, last_idx;
	size_t comment;
	string suffix;
	char buf[32];
	unsigned n = 0;

	comment = line->find("// Offset:");
	if (comment == string::npos)
		return;

	numRead = sscanf_s(line->c_str() + comment, "// Offset: %u%n Size: %u%n",
		&offset, &end1, &size, &end2);
	if (numRead < 1)
		return;

	if (numRead == 1)
		suffix = line->substr(comment + end1);
	else
		suffix = line->substr(comment + end2);
	line->resize(comment);

	last_idx = first_idx = offset / 16;
	last_swiz = first_swiz = offset % 16 / 4;
	if (numRead == 2 && size >= 4) {
		last_idx = (offset + size - 4) / 16;
		last_swiz = (offset + size - 4) % 16 / 4;
	}

	_snprintf_s(buf, 32, _TRUNCATE, "// Index: %4u", first_idx);
	*line += buf;

	if (first_swiz || (numRead == 2 && size <= 16)) {
		// Offset is not on a cb boundary, or this entry has 4
		// or less components (if known). Show the swizzle:
		for (n = 0; first_swiz + n <= (first_idx == last_idx ? last_swiz : first_swiz); n++)
			buf[1+n] = txt_swiz[first_swiz + n];
		buf[n+1] = '\0';
		buf[0] = '.'; n++;
		*line += buf;
	}

	if (last_idx > first_idx) {
		if ((last_idx - first_idx < 4) && (first_swiz == 0 && last_swiz == 3)) {
			// 2-4 indices, show each for ShaderRegex matching:
			for (unsigned i = first_idx+1; i <= last_idx; i++) {
				n += _snprintf_s(buf, 32, _TRUNCATE, " %u", i);
				*line += buf;
			}
		} else {
			// More than 4 indices or misaligned.
			// Unwieldy, so shorten to a range:
			n += _snprintf_s(buf, 32, _TRUNCATE, "-%u", last_idx);
			*line += buf;

			if (last_swiz != 3) {
				// Final component does not end on a cb boundary, show it:
				n += _snprintf_s(buf, 32, _TRUNCATE, ".%c", txt_swiz[last_swiz]);
				*line += buf;
			}
		}
	}
	// Minimum 15 character (3 characters x (room for 4 digits + 1 space))
	// field length for the above will keep most entries aligned:
	for (; n < 3*5; n++)
		*line += ' ';

	if (numRead == 2) {
		_snprintf_s(buf, 32, _TRUNCATE, " Components: %5u", size / 4);
		*line += buf;
		if (size % 4) {
			_snprintf_s(buf, 32, _TRUNCATE, "+%u", size % 4);
			*line += buf;
		}
	} else {
		line->resize(line->find_last_not_of(" ") + 1);
	}

	*line += suffix;
}

#if MIGOTO_DX == 9
HRESULT disassemblerDX9(vector<byte> *buffer, vector<byte> *ret, const char *comment)
{
	char* asmBuffer;
	size_t asmSize;
	vector<byte> asmBuf;
	ID3DBlob* pDissassembly = NULL;
	LPD3DXBUFFER pD3DXDissassembly = NULL;
	HRESULT ok = D3DDisassemble(buffer->data(), buffer->size(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, comment, &pDissassembly);
	if (FAILED(ok))
		ok = D3DDisassemble(buffer->data(), buffer->size(), NULL, comment, &pDissassembly);
	if (FAILED(ok))
		ok = D3DDisassemble(buffer->data(), buffer->size(), D3D_DISASM_DISABLE_DEBUG_INFO, comment, &pDissassembly);
	if (FAILED(ok)){
		//below sometimes give an access violation for some reason
		//ok = D3DXDisassembleShader((DWORD*)buffer->data(), false, NULL, &pD3DXDissassembly);
		//if (FAILED(ok))
			return ok;
		//asmBuffer = (char*)pD3DXDissassembly->GetBufferPointer();
		//asmSize = pD3DXDissassembly->GetBufferSize();
	}
	else {
		asmBuffer = (char*)pDissassembly->GetBufferPointer();
		asmSize = pDissassembly->GetBufferSize();
	}
	vector<string> lines = stringToLinesDX9(asmBuffer, asmSize);
	ret->clear();
	for (size_t i = 0; i < lines.size(); i++) {
		for (size_t j = 0; j < lines[i].size(); j++) {
			ret->insert(ret->end(), lines[i][j]);
		}
		ret->insert(ret->end(), '\n');
	}
	if (pDissassembly)
		pDissassembly->Release();
	if (pD3DXDissassembly)
		pD3DXDissassembly->Release();
	return S_OK;
}
#endif

HRESULT disassembler(vector<byte> *buffer, vector<byte> *ret, const char *comment,
		int hexdump, bool d3dcompiler_46_compat,
		bool disassemble_undecipherable_data,
		bool patch_cb_offsets)
{
	byte fourcc[4];
	DWORD fHash[4];
	DWORD one;
	DWORD fSize;
	DWORD numChunks;
	vector<DWORD> chunkOffsets;
	int rdef_state = 0;

	// TODO: Add robust error checking here (buffer is at least as large as
	// the header, etc). I've added a check for numChunks < 1 as that
	// would lead to codeByteStart being used uninitialised
	byte* pPosition = buffer->data();
	std::memcpy(fourcc, pPosition, 4);
	pPosition += 4;
	std::memcpy(fHash, pPosition, 16);
	pPosition += 16;
	one = *(DWORD*)pPosition;
	pPosition += 4;
	fSize = *(DWORD*)pPosition;
	pPosition += 4;
	numChunks = *(DWORD*)pPosition;
	if (numChunks < 1)
		return S_FALSE;
	pPosition += 4;
	chunkOffsets.resize(numChunks);
	std::memcpy(chunkOffsets.data(), pPosition, 4 * numChunks);

	char* asmBuffer;
	size_t asmSize;
	vector<byte> asmBuf;
	ID3DBlob* pDissassembly = NULL;

	// We disable debug info in the disassembler as it interferes with our
	// ability to match assembly lines with bytecode below
	HRESULT ok = D3DDisassemble(buffer->data(), buffer->size(),
			D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS |
			D3D_DISASM_DISABLE_DEBUG_INFO,
			comment, &pDissassembly);
	if (FAILED(ok))
		return ok;

	asmBuffer = (char*)pDissassembly->GetBufferPointer();
	asmSize = pDissassembly->GetBufferSize();

	byte* codeByteStart;
	int codeChunk = 0;
	for (DWORD i = 1; i <= numChunks; i++) {
		codeChunk = numChunks - i;
		codeByteStart = buffer->data() + chunkOffsets[numChunks - i];
		if (memcmp(codeByteStart, "SHEX", 4) == 0 || memcmp(codeByteStart, "SHDR", 4) == 0)
			break;
	}
	// FIXME: If neither SHEX or SHDR was found in the shader, codeByteStart will be garbage
	vector<string> lines = stringToLines(asmBuffer, asmSize);
	DWORD* codeStart = (DWORD*)(codeByteStart + 8);
	bool codeStarted = false;
	bool multiLine = false;
	int multiLines = 0;
	string s2;
	vector<DWORD> o;
	for (DWORD i = 0; i < lines.size(); i++) {
		uint32_t line_byte_offset = (uint32_t)((byte*)codeStart - buffer->data());
		string s = lines[i];

		if (!memcmp(s.c_str(), "//", 2)) {
			if (d3dcompiler_46_compat)
				patch_d3dcompiler_47_rdef(&lines[i], &rdef_state);
			if (patch_cb_offsets)
				replace_cb_offsets_with_indices(&lines[i]);
			continue;
		}

		vector<DWORD> v;
		if (!codeStarted) {
			if (s.size() > 0 && s[0] != ' ') {
				codeStarted = true;
				v.push_back(*codeStart);
				codeStart += 2;
				s = assembleAndCompare(s, v);
				lines[i] = s;
			}
		} else if (s.find("{ {") < s.size()) {
			s2 = s;
			multiLine = true;
			multiLines = 1;
		} else if (s.find("} }") < s.size()) {
			s2.append("\n");
			s2.append(s);
			s = s2;
			multiLine = false;
			multiLines++;
			shader_ins* ins = (shader_ins*)codeStart;
			v.push_back(*codeStart);
			codeStart++;
			DWORD length = *codeStart;
			v.push_back(*codeStart);
			codeStart++;
			for (DWORD j = 2; j < length; j++) {
				v.push_back(*codeStart);
				codeStart++;
			}
			s = assembleAndCompare(s, v);
			auto sLines = stringToLines(s.c_str(), s.size());
			size_t startLine = i - sLines.size() + 1;
			for (size_t j = 0; j < sLines.size(); j++) {
				lines[startLine + j] = sLines[j];
			}
			//lines[i] = s;
		} else if (multiLine) {
			s2.append("\n");
			s2.append(s);
			multiLines++;
		} else if (s.size() > 0) {
			shader_ins* ins = (shader_ins*)codeStart;
			v.push_back(*codeStart);
			codeStart++;

			for (DWORD j = 1; j < ins->length; j++) {
				v.push_back(*codeStart);
				codeStart++;
			}
			if (s == "undecipherable custom data") {
				// Changed this to use the instruction length
				// in the next word like the below printf
				// instead of scanning for a word that happened
				// to have some bits zeroed. Removed the case
				// to skip processing custom data immediately
				// following a ret. -DarkStarSword
				uint32_t len = *codeStart;
				v.push_back(*codeStart++);
				for (uint32_t j = 1; j < len - 1; j++)
					v.push_back(*codeStart++);
				if (disassemble_undecipherable_data)
					encode_custom_data(s, v);
				else
					s = "";
			} else if (v[0] == 0x00002035) {
				// Opcode 0x35 is custom data (specifically printf/errorf).
				// Instruction length is in the next word instead:
				uint32_t len = *codeStart;
				v.push_back(*(codeStart)++);
				for (uint32_t j = 1; j < len - 1; j++)
					v.push_back(*(codeStart)++);
				s = assembleAndCompare(s, v);
			} else {
				s = assembleAndCompare(s, v);
			}
			lines[i] = s;
		}

		if (hexdump && !multiLine)
			hexdump_instruction(s, v, lines, &i, &multiLines, line_byte_offset, hexdump);
	}
	ret->clear();
	for (size_t i = 0; i < lines.size(); i++) {
		for (size_t j = 0; j < lines[i].size(); j++) {
			ret->insert(ret->end(), lines[i][j]);
		}
		ret->insert(ret->end(), '\n');
	}

	pDissassembly->Release();

	return S_OK;
}

static void preprocessLine(string &line)
{
	const char *p;
	size_t i;

	for (p = line.c_str(), i = 0; *p; p++, i++) {
		// Replace tabs with spaces:
		if (*p == '\t')
			line[i] = ' ';

		// Skip over strings:
		if (*p == '"') {
			p = strrchr(p, '"');
			i = p - line.c_str();
		}

		// Strip C style comments:
		if (!memcmp(p, "//", 2)) {
			line.resize(i);
			return;
		}
	}
}

// For anyone confused about what this hash function is doing, there is a
// clearer implementation here, with details of how this differs from MD5:
// https://github.com/DarkStarSword/3d-fixes/blob/master/dx11shaderanalyse.py
static vector<DWORD> ComputeHash(byte const* input, DWORD size)
{
	DWORD esi;
	DWORD ebx;
	DWORD i = 0;
	DWORD edi;
	DWORD edx;
	DWORD processedSize = 0;

	DWORD sizeHash = size & 0x3F;
	bool sizeHash56 = sizeHash >= 56;
	DWORD restSize = sizeHash56 ? 120 - 56 : 56 - sizeHash;
	DWORD loopSize = (size + 8 + restSize) >> 6;
	DWORD Dst[16];
	DWORD Data[] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	DWORD loopSize2 = loopSize - (sizeHash56 ? 2 : 1);
	DWORD start_0 = 0;
	DWORD* pSrc = (DWORD*)input;
	DWORD h[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
	if (loopSize > 0) {
		while (i < loopSize) {
			if (i == loopSize2) {
				if (!sizeHash56) {
					Dst[0] = size << 3;
					DWORD remSize = size - processedSize;
					std::memcpy(&Dst[1], pSrc, remSize);
					std::memcpy(&Dst[1 + remSize / 4], Data, restSize);
					Dst[15] = (size * 2) | 1;
					pSrc = Dst;
				} else {
					DWORD remSize = size - processedSize;
					std::memcpy(&Dst[0], pSrc, remSize);
					std::memcpy(&Dst[remSize / 4], Data, 64 - remSize);
					pSrc = Dst;
				}
			} else if (i > loopSize2) {
				Dst[0] = size << 3;
				std::memcpy(&Dst[1], &Data[1], 56);
				Dst[15] = (size * 2) | 1;
				pSrc = Dst;
			}

			// initial values from memory
			edx = h[0];
			ebx = h[1];
			edi = h[2];
			esi = h[3];

			edx = _rotl((~ebx & esi | ebx & edi) + pSrc[0] + 0xD76AA478 + edx, 7) + ebx;
			esi = _rotl((~edx & edi | edx & ebx) + pSrc[1] + 0xE8C7B756 + esi, 12) + edx;
			edi = _rotr((~esi & ebx | esi & edx) + pSrc[2] + 0x242070DB + edi, 15) + esi;
			ebx = _rotr((~edi & edx | edi & esi) + pSrc[3] + 0xC1BDCEEE + ebx, 10) + edi;
			edx = _rotl((~ebx & esi | ebx & edi) + pSrc[4] + 0xF57C0FAF + edx, 7) + ebx;
			esi = _rotl((~edx & edi | ebx & edx) + pSrc[5] + 0x4787C62A + esi, 12) + edx;
			edi = _rotr((~esi & ebx | esi & edx) + pSrc[6] + 0xA8304613 + edi, 15) + esi;
			ebx = _rotr((~edi & edx | edi & esi) + pSrc[7] + 0xFD469501 + ebx, 10) + edi;
			edx = _rotl((~ebx & esi | ebx & edi) + pSrc[8] + 0x698098D8 + edx, 7) + ebx;
			esi = _rotl((~edx & edi | ebx & edx) + pSrc[9] + 0x8B44F7AF + esi, 12) + edx;
			edi = _rotr((~esi & ebx | esi & edx) + pSrc[10] + 0xFFFF5BB1 + edi, 15) + esi;
			ebx = _rotr((~edi & edx | edi & esi) + pSrc[11] + 0x895CD7BE + ebx, 10) + edi;
			edx = _rotl((~ebx & esi | ebx & edi) + pSrc[12] + 0x6B901122 + edx, 7) + ebx;
			esi = _rotl((~edx & edi | ebx & edx) + pSrc[13] + 0xFD987193 + esi, 12) + edx;
			edi = _rotr((~esi & ebx | esi & edx) + pSrc[14] + 0xA679438E + edi, 15) + esi;
			ebx = _rotr((~edi & edx | edi & esi) + pSrc[15] + 0x49B40821 + ebx, 10) + edi;

			edx = _rotl((~esi & edi | esi & ebx) + pSrc[1] + 0xF61E2562 + edx, 5) + ebx;
			esi = _rotl((~edi & ebx | edi & edx) + pSrc[6] + 0xC040B340 + esi, 9) + edx;
			edi = _rotl((~ebx & edx | ebx & esi) + pSrc[11] + 0x265E5A51 + edi, 14) + esi;
			ebx = _rotr((~edx & esi | edx & edi) + pSrc[0] + 0xE9B6C7AA + ebx, 12) + edi;
			edx = _rotl((~esi & edi | esi & ebx) + pSrc[5] + 0xD62F105D + edx, 5) + ebx;
			esi = _rotl((~edi & ebx | edi & edx) + pSrc[10] + 0x02441453 + esi, 9) + edx;
			edi = _rotl((~ebx & edx | ebx & esi) + pSrc[15] + 0xD8A1E681 + edi, 14) + esi;
			ebx = _rotr((~edx & esi | edx & edi) + pSrc[4] + 0xE7D3FBC8 + ebx, 12) + edi;
			edx = _rotl((~esi & edi | esi & ebx) + pSrc[9] + 0x21E1CDE6 + edx, 5) + ebx;
			esi = _rotl((~edi & ebx | edi & edx) + pSrc[14] + 0xC33707D6 + esi, 9) + edx;
			edi = _rotl((~ebx & edx | ebx & esi) + pSrc[3] + 0xF4D50D87 + edi, 14) + esi;
			ebx = _rotr((~edx & esi | edx & edi) + pSrc[8] + 0x455A14ED + ebx, 12) + edi;
			edx = _rotl((~esi & edi | esi & ebx) + pSrc[13] + 0xA9E3E905 + edx, 5) + ebx;
			esi = _rotl((~edi & ebx | edi & edx) + pSrc[2] + 0xFCEFA3F8 + esi, 9) + edx;
			edi = _rotl((~ebx & edx | ebx & esi) + pSrc[7] + 0x676F02D9 + edi, 14) + esi;
			ebx = _rotr((~edx & esi | edx & edi) + pSrc[12] + 0x8D2A4C8A + ebx, 12) + edi;

			edx = _rotl((esi ^ edi ^ ebx) + pSrc[5] + 0xFFFA3942 + edx, 4) + ebx;
			esi = _rotl((edi ^ ebx ^ edx) + pSrc[8] + 0x8771F681 + esi, 11) + edx;
			edi = _rotl((ebx ^ edx ^ esi) + pSrc[11] + 0x6D9D6122 + edi, 16) + esi;
			ebx = _rotr((edx ^ esi ^ edi) + pSrc[14] + 0xFDE5380C + ebx, 9) + edi;
			edx = _rotl((esi ^ edi ^ ebx) + pSrc[1] + 0xA4BEEA44 + edx, 4) + ebx;
			esi = _rotl((edi ^ ebx ^ edx) + pSrc[4] + 0x4BDECFA9 + esi, 11) + edx;
			edi = _rotl((ebx ^ edx ^ esi) + pSrc[7] + 0xF6BB4B60 + edi, 16) + esi;
			ebx = _rotr((edx ^ esi ^ edi) + pSrc[10] + 0xBEBFBC70 + ebx, 9) + edi;
			edx = _rotl((esi ^ edi ^ ebx) + pSrc[13] + 0x289B7EC6 + edx, 4) + ebx;
			esi = _rotl((edi ^ ebx ^ edx) + pSrc[0] + 0xEAA127FA + esi, 11) + edx;
			edi = _rotl((ebx ^ edx ^ esi) + pSrc[3] + 0xD4EF3085 + edi, 16) + esi;
			ebx = _rotr((edx ^ esi ^ edi) + pSrc[6] + 0x04881D05 + ebx, 9) + edi;
			edx = _rotl((esi ^ edi ^ ebx) + pSrc[9] + 0xD9D4D039 + edx, 4) + ebx;
			esi = _rotl((edi ^ ebx ^ edx) + pSrc[12] + 0xE6DB99E5 + esi, 11) + edx;
			edi = _rotl((ebx ^ edx ^ esi) + pSrc[15] + 0x1FA27CF8 + edi, 16) + esi;
			ebx = _rotr((edx ^ esi ^ edi) + pSrc[2] + 0xC4AC5665 + ebx, 9) + edi;

			edx = _rotl(((~esi | ebx) ^ edi) + pSrc[0] + 0xF4292244 + edx, 6) + ebx;
			esi = _rotl(((~edi | edx) ^ ebx) + pSrc[7] + 0x432AFF97 + esi, 10) + edx;
			edi = _rotl(((~ebx | esi) ^ edx) + pSrc[14] + 0xAB9423A7 + edi, 15) + esi;
			ebx = _rotr(((~edx | edi) ^ esi) + pSrc[5] + 0xFC93A039 + ebx, 11) + edi;
			edx = _rotl(((~esi | ebx) ^ edi) + pSrc[12] + 0x655B59C3 + edx, 6) + ebx;
			esi = _rotl(((~edi | edx) ^ ebx) + pSrc[3] + 0x8F0CCC92 + esi, 10) + edx;
			edi = _rotl(((~ebx | esi) ^ edx) + pSrc[10] + 0xFFEFF47D + edi, 15) + esi;
			ebx = _rotr(((~edx | edi) ^ esi) + pSrc[1] + 0x85845DD1 + ebx, 11) + edi;
			edx = _rotl(((~esi | ebx) ^ edi) + pSrc[8] + 0x6FA87E4F + edx, 6) + ebx;
			esi = _rotl(((~edi | edx) ^ ebx) + pSrc[15] + 0xFE2CE6E0 + esi, 10) + edx;
			edi = _rotl(((~ebx | esi) ^ edx) + pSrc[6] + 0xA3014314 + edi, 15) + esi;
			ebx = _rotr(((~edx | edi) ^ esi) + pSrc[13] + 0x4E0811A1 + ebx, 11) + edi;
			edx = _rotl(((~esi | ebx) ^ edi) + pSrc[4] + 0xF7537E82 + edx, 6) + ebx;
			h[0] += edx;
			esi = _rotl(((~edi | edx) ^ ebx) + pSrc[11] + 0xBD3AF235 + esi, 10) + edx;
			h[3] += esi;
			edi = _rotl(((~ebx | esi) ^ edx) + pSrc[2] + 0x2AD7D2BB + edi, 15) + esi;
			h[2] += edi;
			ebx = _rotr(((~edx | edi) ^ esi) + pSrc[9] + 0xEB86D391 + ebx, 11) + edi;
			h[1] += ebx;

			processedSize += 0x40;
			pSrc += 16;
			i++;
		}
	}
	vector<DWORD> hash(4);
	std::memcpy(hash.data(), h, 16);
	return hash;
}

// origByteCode is modified in this function, so passing it by value!
// asmFile is not modified, so passing it by pointer -DarkStarSword
vector<byte> assembler(vector<char> *asmFile, vector<byte> origBytecode,
		vector<AssemblerParseError> *parse_errors)
{
	byte fourcc[4];
	DWORD fHash[4];
	DWORD one;
	DWORD fSize;
	DWORD numChunks;
	vector<DWORD> chunkOffsets;

	// TODO: Add robust error checking here (origBytecode is at least as large as
	// the header, etc). I've added a check for numChunks < 1 as that
	// would lead to codeByteStart being used uninitialised
	byte* pPosition = origBytecode.data();
	std::memcpy(fourcc, pPosition, 4);
	pPosition += 4;
	std::memcpy(fHash, pPosition, 16);
	pPosition += 16;
	one = *(DWORD*)pPosition;
	pPosition += 4;
	fSize = *(DWORD*)pPosition;
	pPosition += 4;
	numChunks = *(DWORD*)pPosition;
	if (numChunks < 1)
		throw std::invalid_argument("assembler: Bad shader binary");
	pPosition += 4;
	chunkOffsets.resize(numChunks);
	std::memcpy(chunkOffsets.data(), pPosition, 4 * numChunks);

	char* asmBuffer;
	size_t asmSize;
	asmBuffer = asmFile->data();
	asmSize = asmFile->size();
	byte* codeByteStart;
	int codeChunk = 0;
	for (DWORD i = 1; i <= numChunks; i++) {
		codeChunk = numChunks - i;
		codeByteStart = origBytecode.data() + chunkOffsets[numChunks - i];
		if (memcmp(codeByteStart, "SHEX", 4) == 0 || memcmp(codeByteStart, "SHDR", 4) == 0)
			break;
	}
	// FIXME: If neither SHEX or SHDR was found in the shader, codeByteStart will be garbage
	vector<string> lines = stringToLines(asmBuffer, asmSize);
	DWORD* codeStart = (DWORD*)(codeByteStart + 8);
	bool codeStarted = false;
	bool multiLine = false;
	string s2;
	vector<DWORD> o;
	for (DWORD i = 0; i < lines.size(); i++) {
		try {
			string s = lines[i];
			preprocessLine(s);
			vector<DWORD> v;
			if (!codeStarted) {
				if (s.size() > 0 && s[0] != ' ') {
					codeStarted = true;
					vector<DWORD> ins = assembleIns(s);
					o.insert(o.end(), ins.begin(), ins.end());
					o.push_back(0);
				}
			} else if (s.find("{ {") < s.size()) {
				s2 = s;
				multiLine = true;
			} else if (s.find("} }") < s.size()) {
				s2.append("\n");
				s2.append(s);
				s = s2;
				multiLine = false;
				vector<DWORD> ins = assembleIns(s);
				o.insert(o.end(), ins.begin(), ins.end());
			} else if (multiLine) {
				s2.append("\n");
				s2.append(s);
			} else if (s.find_first_not_of(" ") != string::npos) {
				vector<DWORD> ins = assembleIns(s);
				o.insert(o.end(), ins.begin(), ins.end());
			}
		} catch (AssemblerParseError &e) {
			e.line_no = i + 1;
			e.update_msg();

			// Since we never used to warn about parse errors there
			// may well be shaders with problems in the wild that
			// happen to pass anyway (e.g. I've seen at least one
			// example of someone including the ~~~~~~~~/ line from
			// the HLSL comment in an assembly shader).
			//
			// If the caller has passed somewhere to store the
			// parse errors we will store them there so that they
			// can display a warning, but we will continue parsing
			// the rest of the shader as before. Otherwise we will
			// throw an exception and stop parsing now.
			if (!parse_errors)
				throw;

			parse_errors->push_back(e);
		}
	}
	codeStart = (DWORD*)(codeByteStart); // Endian bug, not that we care
	auto it = origBytecode.begin() + chunkOffsets[codeChunk] + 8;
	size_t codeSize = codeStart[1];
	origBytecode.erase(it, it + codeSize);
	size_t newCodeSize = 4 * o.size();
	codeStart[1] = (DWORD)newCodeSize;
	vector<byte> newCode(newCodeSize);
	o[1] = (DWORD)o.size();
	memcpy(newCode.data(), o.data(), newCodeSize);
	it = origBytecode.begin() + chunkOffsets[codeChunk] + 8;
	origBytecode.insert(it, newCode.begin(), newCode.end());
	DWORD* dwordBuffer = (DWORD*)origBytecode.data();
	for (DWORD i = codeChunk + 1; i < numChunks; i++) {
		dwordBuffer[8 + i] += (DWORD)(newCodeSize - codeSize);
	}
	dwordBuffer[6] = (DWORD)origBytecode.size();
	vector<DWORD> hash = ComputeHash((byte const*)origBytecode.data() + 20, (DWORD)origBytecode.size() - 20);
	dwordBuffer[1] = hash[0];
	dwordBuffer[2] = hash[1];
	dwordBuffer[3] = hash[2];
	dwordBuffer[4] = hash[3];
	return origBytecode;
}
#if MIGOTO_DX == 9
vector<byte> assemblerDX9(vector<char> *asmFile)
{
	vector<byte> ret;
	LPD3DXBUFFER pAssembly;
	HRESULT hr = D3DXAssembleShader(asmFile->data(), (UINT)asmFile->size(), NULL, NULL, 0, &pAssembly, NULL);
	if (!FAILED(hr)) {
		size_t size = pAssembly->GetBufferSize();
		LPVOID buffer = pAssembly->GetBufferPointer();
		ret.resize(size);
		std::memcpy(ret.data(), buffer, size);
		pAssembly->Release();
		// FIXME: Pass warnings back to the caller
	} else {
		// FIXME: Pass error messages back to the caller
		throw std::invalid_argument("assembler: Bad shader assembly (FIXME: RETURN ERROR MESSAGES)");
	}
	return ret;
}
#endif
