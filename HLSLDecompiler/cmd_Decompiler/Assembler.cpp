#include "stdafx.h"
#include <string>
#include <vector>
#include <map>

using namespace std;

void handleSwizzle(string s, token_operand* tOp, bool special = false) {
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

DWORD strToDWORD(string s) {
	if (s == "1.#INF00")
		return 0x7F800000;
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

vector<DWORD> assembleOp(string s, bool special = 0) {
	vector<DWORD> v;
	DWORD op = 0;
	DWORD ext = 0;
	DWORD num = 0;
	DWORD index = 0;
	DWORD value = 0;
	token_operand* tOp = (token_operand*)&op;
	tOp->comps_enum = 2; // 4
	if (s == "oDepth") {
		v.push_back(0xC001);
		return v;
	}
	if (s == "null") {
		v.push_back(0xD000);
		return v;
	}
	if (s == "vCoverage") {
		v.push_back(0x23001);
		return v;
	}
	if (s == "vCoverage.x") {
		v.push_back(0x2300A);
		return v;
	}
	if (s == "rasterizer.x") {
		v.push_back(0x0000E00A);
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
	if (tOp->extended) {
		v.push_back(ext);
	}
	if (s[0] == 'i' && s[1] == 'c' && s[2] == 'b' || s[0] == 'c' && s[1] == 'b' || s[0] == 'x' || s[0] == 'v') {
		tOp->num_indices = 2;
		if (s[0] == 'x') {
			tOp->file = 3;
			s.erase(s.begin());
		} else if (s[0] == 'v') {
			tOp->file = 1;
			s.erase(s.begin());
			tOp->num_indices = 1;
		} else if (s[0] == 'i') {
			tOp->file = 9;
			s.erase(s.begin());
			s.erase(s.begin());
			s.erase(s.begin());
			tOp->num_indices = 1;
		} else {
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
		if (hasIndex)
			index = s.substr(s.find('[') + 1, s.find(']') - 1);
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

				v.insert(v.begin(), op);
				return v;
			} else {
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
				v.insert(v.begin(), op);
				return v;
			}
		} else {
			num = atoi(sNum.c_str());
			v.push_back(num);
			handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
			v.insert(v.begin(), op);
			return v;
		}
	} else if (s[0] == 'l') {
		string sOrig = s;;
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
		v.insert(v.begin(), op);
		return v;
	} else if (s[0] == 'r') {
		tOp->file = 0;
	} else if (s[0] == 'o') {
		tOp->file = 2;
	} else if (s[0] == 's') {
		tOp->file = 6;
	} else if (s[0] == 't') {
		tOp->file = 7;
	}
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

vector<string> strToWords(string s) {
	vector<string> words;
	DWORD start = 0;
	DWORD end = start;
	bool braces = false;
	while (true) {
		while ((braces || s[end] != ' ') && end < s.size()) {
			if (s[end] == '(' || s[end] == '[')
				braces = true;
			if (s[end] == ')' || s[end] == ']')
				braces = false;
			end++;
		}
		string s2 = s.substr(start, end++ - start);
		start = end;
		if (s2.size() > 0)
			words.push_back(s2);
		if (start >= s.size()) {
			break;
		}
	}
	for (size_t i = 0; i < words.size(); i++) {
		string s = words[i];
		if (s[s.size() - 1] == ',')
			s.erase(--s.end());
		words[i] = s;
	}
	s = words[0];
	if (s.find('(') < s.size()) {
		words[0] = s.substr(s.find('('));
		words.insert(words.begin(), s.substr(0, s.find('(')));

		s = words[1];
		if (s.find('(', 1) < s.size()) {
			words[1] = s.substr(s.find('(', 1));
			words.insert(words.begin() + 1, s.substr(0, s.find('(', 1)));

			s = words[2];
			if (s.find('(', 1) < s.size()) {
				words[2] = s.substr(s.find('(', 1));
				words.insert(words.begin() + 2, s.substr(0, s.find('(', 1)));
			}
		}
	}
	return words;
}

DWORD parseAoffimmi(DWORD start, string o) {
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

map<string, vector<int>> ldMap = {
	{ "gather4_c_aoffimmi_indexable", { 5, 126, 3 } },
	{ "gather4_aoffimmi_indexable", { 4, 109, 3 } },
	{ "gather4_indexable", { 4, 109, 2 } },
	{ "ld_aoffimmi_indexable", { 3, 45, 3 }},
	{ "ld_indexable", { 3, 45, 2 } },
	{ "ldms_indexable", { 4, 46, 2 } },
	{ "sample_d_indexable", { 6, 73, 2 } },
	{ "sample_b_indexable", { 5, 74, 2 } },
	{ "sample_c_indexable", { 5, 70, 2 } },
	{ "sample_c_lz_indexable", { 5, 71, 2 } },
	{ "sample_c_lz_aoffimmi", { 5, 71, 1 } },
	{ "sample_indexable", { 4, 69, 2 } },
	{ "sample_l_aoffimmi", { 5, 72, 1 } },
	{ "sample_l_aoffimmi_indexable", { 5, 72, 3 } },
	{ "sample_l_indexable", { 5, 72, 2 } },
	{ "resinfo_indexable", { 3, 61, 2 } },
	{ "ld_structured_indexable", { 4, 167, 6 } },
};

map<string, vector<int>> insMap = {
	{ "sample_b", { 5, 74 } },
	{ "sample_c", { 5, 70 } },
	{ "sample_c_lz", { 5, 71 } },
	{ "sample_l", { 5, 72 } },
	{ "bfi", { 5, 140 } },
	{ "swapc", { 5, 142 } },
	{ "imad", { 4, 35 } },
	{ "imul", { 4, 38, 2 } },
	{ "ldms", { 4, 46 } },
	{ "mad", { 4, 50 } },
	{ "movc", { 4, 55 } },
	{ "sample", { 4, 69 } },
	{ "udiv", { 4, 78, 2 } },
	{ "umul", { 4, 81, 2 } },
	{ "ubfe", { 4, 138 } },
	{ "store_structured", { 4, 168 } },
	{ "add", { 3, 0 } },
	{ "and", { 3, 1 } },
	{ "div", { 3, 14 } },
	{ "dp2", { 3, 15 } },
	{ "dp3", { 3, 16 } },
	{ "dp4", { 3, 17 } },
	{ "eq", { 3, 24 } },
	{ "ge", { 3, 29 } },
	{ "iadd", { 3, 30 } },
	{ "ieq", { 3, 32 } },
	{ "ige", { 3, 33 } },
	{ "ilt", { 3, 34 } },
	{ "imax", { 3, 36 } },
	{ "imin", { 3, 37 } },
	{ "ine", { 3, 39 } },
	{ "ishl", { 3, 41 } },
	{ "ishr", { 3, 42 } },
	{ "ld", { 3, 45 } },
	{ "lt", { 3, 49 } },
	{ "min", { 3, 51 } },
	{ "max", { 3, 52 } },
	{ "mul", { 3, 56 } },
	{ "ne", { 3, 57 } },
	{ "or", { 3, 60 } },
	{ "resinfo", { 3, 61 } },
	{ "sincos", { 3, 77, 2 } },
	{ "ult", { 3, 79 } },
	{ "uge", { 3, 80 } },
	{ "umin", { 3, 84 } },
	{ "ushr", { 3, 85 } },
	{ "xor", { 3, 87 } },
	{ "store_uav_typed", { 3, 134 } },
	{ "countbits", { 2, 134 } },
	{ "deriv_rtx", { 2, 11 } },
	{ "deriv_rtx_coarse", { 2, 122 } },
	{ "deriv_rtx_fine", { 2, 123 } },
	{ "deriv_rty", { 2, 12 } },
	{ "deriv_rty_coarse", { 2, 124 } },
	{ "deriv_rty_fine", { 2, 125 } },
	{ "exp", { 2, 25 } },
	{ "frc", { 2, 26 } },
	{ "ftoi", { 2, 27 } },
	{ "ftou", { 2, 28 } },
	{ "ineg", { 2, 40 } },
	{ "itof", { 2, 43 } },
	{ "log", { 2, 47 } },
	{ "mov", { 2, 54 } },
	{ "not", { 2, 59 } },
	{ "round_ne", { 2, 64 } },
	{ "round_ni", { 2, 65 } },
	{ "round_pi", { 2, 66 } },
	{ "round_z", { 2, 67 } },
	{ "rsq", { 2, 68 } },
	{ "sqrt", { 2, 75 } },
	{ "utof", { 2, 86 } },
	{ "rcp", { 2, 129 } },
	{ "sampleinfo", { 2, 111 } },
	{ "f16tof32", { 2, 131 } },
	{ "imm_atomic_alloc", { 2, 178 } },
	{ "breakc_z", { 1, 3, 0 } },
	{ "case", { 1, 6 } },
	{ "discard_z", { 1, 13, 0 } },
	{ "if_z", { 1, 31, 0 } },
	{ "switch", { 1, 76, 0 } },
	{ "break", { 0, 2 } },
	{ "default", { 0, 10 } },
	{ "else", { 0, 18 } },
	{ "endif", { 0, 21 } },
	{ "endloop", { 0, 22 } },
	{ "endswitch", { 0, 23 } },
	{ "loop", { 0, 48 } },
	{ "ret", { 0, 62 } },

	{ "dcl_input", { 1, 95 } },
};

vector<DWORD> assembleIns(string s) {
	vector<DWORD> v;
	vector<string> w = strToWords(s);
	string o = w[0];
	bool bUint = o.find("_uint") < o.size();
	if (bUint) {
		o = o.substr(0, o.find("_uint"));
	} else if (w.size() > 2 && w[2].find("_uint") < w[2].size()) {
		bUint = true;
		w[2] = w[2].substr(0, w[2].find("_uint"));
	}
	bool bNZ = o.find("_nz") < o.size();
	if (bNZ)
		o = o.substr(0, o.find("_nz")).append("_z");
	bool bSat = o.find("_sat") < o.size();
	if (bSat)
		o = o.substr(0, o.find("_sat"));
	DWORD op = 0;
	shader_ins* ins = (shader_ins*)&op;

	if (o.substr(0, 3) == "ps_") {
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "vs_") {
		op = 0x10000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (insMap.find(o) != insMap.end()) {
		vector<int> vIns = insMap[o];
		int numOps = vIns[0];
		vector<vector<DWORD>> Os;
		int numSpecial = 1;
		if (vIns.size() > 2)
			numSpecial = vIns[2];
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + 1], i < numSpecial));
		ins->opcode = vIns[1];
		if (bUint) {
			if (o == "sampleinfo")
				ins->_11_23 = 1;
			else
				ins->_11_23 = 2;
		}
		if (bSat)
			ins->_11_23 = 4;
		if (bNZ)
			ins->_11_23 = 128;
		ins->length = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += Os[i].size();
		v.push_back(op);
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (ldMap.find(o) != ldMap.end()) {
		vector<int> vIns = ldMap[o];
		int numOps = vIns[0];
		vector<vector<DWORD>> Os;
		int startPos = 1 + (vIns[2] & 3);
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + startPos], i == 0));
		ins->opcode = vIns[1];
		if (bUint)
			ins->_11_23 = 2;
		ins->length = 1 + (vIns[2] & 3);
		ins->extended = 1;
		for (int i = 0; i < numOps; i++)
			ins->length += Os[i].size();
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
			if (w[c] == "(texture2d)")
				v.push_back(0x800000C2);
			if (w[c] == "(texture2dms)")
				v.push_back(0x80000102);
			if (w[c] == "(texture3d)")
				v.push_back(0x80000142);
			if (w[c] == "(texture2darray)")
				v.push_back(0x80000202);
			if (w[c] == "(texturecube)")
				v.push_back(0x80000182);
			if (vIns[2] & 4) {
				string stride = w[1].substr(27);
				stride = stride.substr(0, stride.size() - 1);
				DWORD d = 0x80000302;
				d += atoi(stride.c_str()) << 11;
				v.push_back(d);
				// mixed
				v.push_back(0x00199983);
			} else {
				if (w[startPos - 1] == "(float,float,float,float)")
					v.push_back(0x00155543);
				if (w[startPos - 1] == "(uint,uint,uint,uint)")
					v.push_back(0x00111103);
			}
		}
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (o == "dcl_resource_texture1d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 2;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
	} else if (o == "dcl_resource_texture2d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 3;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
	} else if (o == "dcl_resource_texture3d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 5;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
	} else if (o == "dcl_resource_texturecube") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 6;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
	} else if (o == "dcl_resource_texture2darray") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 8;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
	} else if (o == "dcl_resource_texture2dms") {
		vector<DWORD> os = assembleOp(w[3]);
		ins->opcode = 88;
		if (w[1] == "(0)")
			ins->_11_23 = 4;
		if (w[1] == "(2)")
			ins->_11_23 = 68;
		if (w[1] == "(4)")
			ins->_11_23 = 132;
		if (w[1] == "(8)")
			ins->_11_23 = 260;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[2] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[2] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_indexrange") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 91;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_temps") {
		ins->opcode = 104;
		ins->length = 2;
		v.push_back(op);
		v.push_back(atoi(w[1].c_str()));
	} else if (o == "dcl_resource_structured") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 162;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_sampler") {
		vector<DWORD> os = assembleOp(w[1]);
		os[0] = 0x106000;
		ins->opcode = 90;
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
		if (w.size() > 1 && w[1] == "refactoringAllowed") {
			ins->opcode = 106;
			ins->length = 1;
			ins->_11_23 = 1;
		}
		v.push_back(op);
	} else if (o == "dcl_constantbuffer") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 89;
		if (w.size() > 2) {
			if (w[2] == "dynamicIndexed")
				ins->_11_23 = 1;
			else if (w[2] == "immediateIndexed")
				ins->_11_23 = 0;
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 101;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output_siv") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 103;
		if (w[2] == "position")
			os.push_back(1);
		else if (w[2] == "clip_distance")
			os.push_back(2);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_sgv") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 96;
		if (w[2] == "vertex_id")
			os.push_back(6);
		if (w[2] == "instance_id")
			os.push_back(8);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_ps") {
		vector<DWORD> os;
		ins->opcode = 98;
		if (w[1] == "linear") {
			if (w[2] == "noperspective") {
				ins->_11_23 = 4;
				os = assembleOp(w[3], true);
			} else if (w[2] == "centroid") {
				ins->_11_23 = 3;
				os = assembleOp(w[3], true);
			} else {
				ins->_11_23 = 2;
				os = assembleOp(w[2], true);
			}
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_ps_sgv") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 99;
		ins->_11_23 = 1;
		if (w.size() > 2) {
			if (w[2] == "sampleIndex") {
				os.push_back(0xA);
			} else if (w[2] == "is_front_face") {
				os.push_back(0x9);
			}
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		
	} else if (o == "dcl_input_ps_siv") {
		vector<DWORD> os;
		ins->opcode = 100;
		if (w[1] == "linear") {
			if (w[2] == "noperspective") {
				if (w[3] == "centroid") {
					ins->_11_23 = 5;
					os = assembleOp(w[4], true);
					if (w[5] == "position")
						os.push_back(1);
				} else {
					ins->_11_23 = 4;
					os = assembleOp(w[3], true);
					if (w[4] == "position")
						os.push_back(1);
				}
			}
		}
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_indexableTemp") {
		string s1 = w[1].erase(0, 1);
		string s2 = s1.substr(0, s1.find('['));
		string s3 = s1.substr(s1.find('[') + 1);
		s3.erase(s3.end() - 1, s3.end());
		ins->opcode = 105;
		ins->length = 4;
		v.push_back(op);
		v.push_back(atoi(s2.c_str()));
		v.push_back(atoi(s3.c_str()));
		v.push_back(atoi(w[2].c_str()));
	} else if (o == "dcl_immediateConstantBuffer") {
		vector<DWORD> os;
		ins->opcode = 53;
		ins->_11_23 = 3;
		ins->length = 0;
		w.size();
		DWORD length = 2;
		DWORD offset = 3;
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
	}
	/*
	if (o == "add") {
		ins->opcode = 0;
		auto o1 = assembleOp(w[1], 1);
		auto o2 = assembleOp(w[2]);
		auto o3 = assembleOp(w[3]);
		ins->length = 1;
		ins->length += o1.size();
		ins->length += o2.size();
		ins->length += o3.size();
		v.push_back(op);
		v.insert(v.end(), o1.begin(), o1.end());
		v.insert(v.end(), o2.begin(), o2.end());
		v.insert(v.end(), o3.begin(), o3.end());
	}
	*/
	return v;
}

vector<byte> readFile(string fileName) {
	vector<byte> buffer;
	FILE* f;
	fopen_s(&f, fileName.c_str(), "rb");
	if (f != NULL) {
		fseek(f, 0L, SEEK_END);
		int fileSize = ftell(f);
		buffer.resize(fileSize);
		fseek(f, 0L, SEEK_SET);
		int numRead = fread(buffer.data(), 1, buffer.size(), f);
		fclose(f);
	}
	return buffer;
}

vector<string> stringToLines(char* start, int size) {
	vector<string> lines;
	char* pStart = start;
	char* pEnd = pStart;
	char* pRealEnd = pStart + size;
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
		if (s[s.size() - 1] == '\r')
			s.erase(--s.end());
		lines[i] = s;
	}
	return lines;
}

vector<DWORD> ComputeHash(byte const* input, DWORD size) {
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

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24 ))
static enum { FOURCC_DXBC = FOURCC('D', 'X', 'B', 'C') }; //DirectX byte code
static enum { FOURCC_SHDR = FOURCC('S', 'H', 'D', 'R') }; //Shader model 4 code
static enum { FOURCC_SHEX = FOURCC('S', 'H', 'E', 'X') }; //Shader model 5 code
static enum { FOURCC_RDEF = FOURCC('R', 'D', 'E', 'F') }; //Resource definition (e.g. constant buffers)
static enum { FOURCC_ISGN = FOURCC('I', 'S', 'G', 'N') }; //Input signature
static enum { FOURCC_IFCE = FOURCC('I', 'F', 'C', 'E') }; //Interface (for dynamic linking)
static enum { FOURCC_OSGN = FOURCC('O', 'S', 'G', 'N') }; //Output signature

static enum { FOURCC_ISG1 = FOURCC('I', 'S', 'G', '1') }; //Input signature with Stream and MinPrecision
static enum { FOURCC_OSG1 = FOURCC('O', 'S', 'G', '1') }; //Output signature with Stream and MinPrecision

typedef struct DXBCContainerHeaderTAG
{
	unsigned fourcc;
	uint32_t unk[4];
	uint32_t one;
	uint32_t totalSize;
	uint32_t chunkCount;
} DXBCContainerHeader;

typedef struct DXBCChunkHeaderTAG
{
	unsigned fourcc;
	unsigned size;
} DXBCChunkHeader;

typedef struct DXBCCodeHeaderTAG
{
	unsigned short version;
	unsigned short type;
	unsigned size;  //Number of DWORDs in the chunk
} DXBCCodeHeader;

void assembler(string asmFile, vector<byte> & bc) {

	vector<byte> byteCode;
	byteCode.resize(sizeof(DXBCContainerHeader) + 4 + sizeof(DXBCChunkHeader));

	DXBCContainerHeaderTAG * fileHeader = (DXBCContainerHeader *)&byteCode[0];
	fileHeader->fourcc = FOURCC_DXBC;
	fileHeader->chunkCount = 1;
	fileHeader->one = 0;

	DWORD * chunkOffsets = (DWORD *)&byteCode[sizeof(DXBCContainerHeader)];
	chunkOffsets[0] = sizeof(DXBCContainerHeader) + 4;

	DXBCChunkHeader * chunkHeader = (DXBCChunkHeader *)&byteCode[sizeof(DXBCContainerHeader) + 4];
	chunkHeader->fourcc = FOURCC_SHDR;



	char* asmBuffer;
	int asmSize;
	vector<byte> asmBuf;
	asmBuf = readFile(asmFile);
	asmBuffer = (char*)asmBuf.data();
	asmSize = asmBuf.size();

	vector<string> lines = stringToLines(asmBuffer, asmSize);
	bool codeStarted = false;
	bool multiLine = false;
	string s2;
	vector<DWORD> o;

	for (DWORD i = 0; i < lines.size(); i++) {
		string s = lines[i];
		if (memcmp(s.c_str(), "//", 2) != 0) {
			vector<DWORD> v;
			if (!codeStarted) {
				if (s.size() > 0 && s[0] != ' ') {
					codeStarted = true;
					vector<DWORD> ins = assembleIns(s);
					o.insert(o.end(), ins.begin(), ins.end());
					o.push_back(0);
				}
			}
			else if (s.find("{ {") < s.size()) {
				s2 = s;
				multiLine = true;
			}
			else if (s.find("} }") < s.size()) {
				s2.append("\n");
				s2.append(s);
				s = s2;
				multiLine = false;
				vector<DWORD> ins = assembleIns(s);
				o.insert(o.end(), ins.begin(), ins.end());
			}
			else if (multiLine) {
				s2.append("\n");
				s2.append(s);
			}
			else if (s.size() > 0) {
				vector<DWORD> ins = assembleIns(s);
				o.insert(o.end(), ins.begin(), ins.end());
			}
		}
	}

	chunkHeader->size = 4 * o.size();
	fileHeader->totalSize = byteCode.size() + chunkHeader->size;

	DXBCCodeHeader * codeHeader = (DXBCCodeHeader *)&o[0];
	codeHeader->size = o.size();

	vector<byte> newCode(4 * o.size());
	memcpy(newCode.data(), o.data(), 4 * o.size());

	byteCode.insert(byteCode.end(), newCode.begin(), newCode.end());

	vector<DWORD> hash = ComputeHash((byte const*)byteCode.data() + 20, byteCode.size() - 20);

	fileHeader = (DXBCContainerHeader *)&byteCode[0];

	fileHeader->unk[0] = hash[0];
	fileHeader->unk[1] = hash[1];
	fileHeader->unk[2] = hash[2];
	fileHeader->unk[3] = hash[3];



	byteCode.swap(bc);
}