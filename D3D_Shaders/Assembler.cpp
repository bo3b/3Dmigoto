#include "stdafx.h"

using namespace std;

FILE* failFile = NULL;
static unordered_map<string, vector<DWORD>> codeBin;

string convertF(DWORD original) {
	char buf[80];
	char buf2[80];

	float fOriginal = reinterpret_cast<float &>(original);
	sprintf_s(buf2, 80, "%.9E", fOriginal);
	size_t len = strlen(buf2);
	if (buf2[len - 4] == '-') {
		int exp = atoi(buf2 + len - 3);
		switch (exp) {
		case 1:
			sprintf_s(buf, 80, "%.9f", fOriginal);
			break;
		case 2:
			sprintf_s(buf, 80, "%.10f", fOriginal);
			break;
		case 3:
			sprintf_s(buf, 80, "%.11f", fOriginal);
			break;
		case 4:
			sprintf_s(buf, 80, "%.12f", fOriginal);
			break;
		case 5:
			sprintf_s(buf, 80, "%.13f", fOriginal);
			break;
		case 6:
			sprintf_s(buf, 80, "%.14f", fOriginal);
			break;
		default:
			sprintf_s(buf, 80, "%.9E", fOriginal);
			break;
		}
	} else {
		int exp = atoi(buf2 + len - 3);
		switch (exp) {
		case 0:
			sprintf_s(buf, 80, "%.8f", fOriginal);
			break;
		default:
			sprintf_s(buf, 80, "%.8f", fOriginal);
			break;
		}
	}
	string sLiteral(buf);
	DWORD newDWORD = strToDWORD(sLiteral);
	if (newDWORD != original) {
		if (failFile == NULL)
			fopen_s(&failFile, "debug.txt", "wb");
		if (failFile) {
			FILE *f = failFile;
			fprintf(f, "%s\n", sLiteral.c_str());
			fprintf(f, "o:%08X\n", original);
			fprintf(f, "n:%08X\n", newDWORD);
			fprintf(f, "\n");
		}
	}
	return sLiteral;
}

void writeLUT() {
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

string assembleAndCompare(string s, vector<DWORD> v) {
	string s2;
	int numSpaces = 0;
	while (memcmp(s.c_str(), " ", 1) == 0) {
		s.erase(s.begin());
		numSpaces++;
	}
	size_t lastLiteral = 0;
	size_t lastEnd = 0;
	vector<DWORD> v2 = assembleIns(s);
	string sNew = s;
	string s3;
	bool valid = true;
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
				} else if (v[i] == 0x4001) {
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
				} else if (v[i] == 0x4002) {
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

HRESULT disassembler(vector<byte> *buffer, vector<byte> *ret, const char *comment) {
	byte fourcc[4];
	DWORD fHash[4];
	DWORD one;
	DWORD fSize;
	DWORD numChunks;
	vector<DWORD> chunkOffsets;

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
	HRESULT ok = D3DDisassemble(buffer->data(), buffer->size(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, comment, &pDissassembly);
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
		string s = lines[i];
		if (s.find("#line") != string::npos)
			break;
		if (memcmp(s.c_str(), "//", 2) != 0) {
			vector<DWORD> v;
			if (!codeStarted) {
				if (s.size() > 0 && s[0] != ' ') {
					codeStarted = true;
					v.push_back(*codeStart);
					codeStart += 2;
					string sNew = assembleAndCompare(s, v);
					lines[i] = sNew;
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
				string sNew = assembleAndCompare(s, v);
				auto sLines = stringToLines(sNew.c_str(), sNew.size());
				size_t startLine = i - sLines.size() + 1;
				for (size_t j = 0; j < sLines.size(); j++) {
					lines[startLine + j] = sLines[j];
				}
				//lines[i] = sNew;
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
				string sNew;
				if (s == "undecipherable custom data") {
					string prev = lines[i - 1];
					if (prev == "ret ")
						v.clear();
					if (v.size() == 1) {
						ins = (shader_ins*)++codeStart;
						while (ins->length == 0) {
							ins = (shader_ins*)++codeStart;
						}
					}
					sNew = "";
				} else {
					sNew = assembleAndCompare(s, v);
				}
				lines[i] = sNew;
			}
		}
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

DWORD strToDWORD(string s) {
	if (s == "-1.#IND0000")
		return 0xFFC00000;
	if (s == "1.#INF0000")
		return 0x7F800000;
	if (s == "-1.#INF0000")
		return 0xFF800000;
	if (s == "-1.#QNAN000")
		return 0xFFC10000;
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

vector<DWORD> assembleOp(string s, bool special = false) {
	vector<DWORD> v;
	DWORD op = 0;
	DWORD ext = 0;
	DWORD num = 0;
	DWORD index = 0;
	DWORD value = 0;
	token_operand* tOp = (token_operand*)&op;
	tOp->comps_enum = 2; // 4
	string bPoint;
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
	if (tOp->extended) {
		v.push_back(ext);
	}
	if (s.find('.') != string::npos)
		bPoint = s.substr(0, s.find('.'));
	else
		bPoint = s;
	if (s == "null") {
		v.push_back(0xD000);
		return v;
	}
	if (s == "oDepth") {
		v.push_back(0xC001);
		return v;
	}
	if (s == "oDepthLE") {
		v.push_back(0x27001);
		return v;
	}
	if (s == "oDepthGE") {
		v.push_back(0x00026001);
		return v;
	}
	if (s == "vOutputControlPointID") {
		v.push_back(0x16001);
		return v;
	}
	if (s == "oMask") {
		v.push_back(0xF001);
		return v;
	}
	if (s == "vPrim") {
		v.push_back(0xB001);
		return v;
	}
	if (bPoint == "vForkInstanceID") {
		bool ext = tOp->extended;
		op = 0x17002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		if (bPoint == s)
			op = 0x17001;
		if (ext) tOp->extended = 1;
		v.insert(v.begin(), op);
		return v;
	}
	if (bPoint == "vCoverage") {
		op = 0x23002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		v.push_back(op);
		return v;
	}
	if (bPoint == "vDomain") {
		bool ext = tOp->extended;
		op = 0x1C002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		if (ext) tOp->extended = 1;
		v.insert(v.begin(), op);
		return v;
	}
	if (bPoint == "rasterizer") {
		op = 0xE002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		v.push_back(op);
		return v;
	}
	if (bPoint == "vThreadGroupID") {
		op = 0x21002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		v.push_back(op);
		return v;
	}
	if (bPoint == "vThreadIDInGroup") {
		bool ext = tOp->extended;
		op = 0x22002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		if (ext) tOp->extended = 1;
		v.insert(v.begin(), op);
		return v;
	}
	if (bPoint == "vThreadID") {
		bool ext = tOp->extended;
		op = 0x20002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		if (ext) tOp->extended = 1;
		v.insert(v.begin(), op);
		return v;
	}
	if (bPoint == "vThreadIDInGroupFlattened") {
		bool ext = tOp->extended;
		op = 0x24002;
		handleSwizzle(s.substr(s.find('.') + 1), tOp, special);
		if (bPoint == s)
			op = 0x24001;
		if (ext) tOp->extended = 1;
		v.insert(v.begin(), op);
		return v;
	}
	if (s[0] == 'i' && s[1] == 'c' && s[2] == 'b' || s[0] == 'c' && s[1] == 'b' || s[0] == 'x' || s[0] == 'o' || s[0] == 'v') {
		tOp->num_indices = 2;
		if (s[0] == 'x') {
			tOp->file = 3;
			s.erase(s.begin());
		} else if (s[0] == 'o') {
			tOp->file = 2;
			tOp->num_indices = 1;
			s.erase(s.begin());
		} else if (s[0] == 'v') {
			tOp->file = 1;
			if (s.size() > 4 && s[1] == 'i' && s[2] == 'c' && s[3] == 'p')  { // vicp
				tOp->file = 0x19;
				s.erase(s.begin());
				s.erase(s.begin());
				s.erase(s.begin());
			} else if (s.size() > 4 && s[1] == 'o' && s[2] == 'c' && s[3] == 'p') { // vocp
				tOp->file = 0x1A;
				s.erase(s.begin());
				s.erase(s.begin());
				s.erase(s.begin());
			} else if (s[1] == 'p' && s[2] == 'c') {
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
						v.insert(v.begin(), op);
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
					v.insert(v.begin(), op);
					if (iAdd) v.push_back(iAdd);
					v.push_back(reg[0]);
					v.push_back(reg[1]);
					v.push_back(atoi(index1.c_str()));
					return v;
				}
				tOp->num_indices = 2;
				string swizzle = s.substr(s.find('.') + 1);
				handleSwizzle(swizzle, tOp, special);
				v.insert(v.begin(), op);
				v.push_back(atoi(index0.c_str()));
				v.push_back(atoi(index1.c_str()));
				return v;
			}
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
		} else {
			end = s.find(' ', start);
			if (s[end + 1] == '+') {
				end = s.find(' ', end + 3);
				if (s.size() > end && s[end + 1] == '+') {
					end = s.find(' ', end + 3);
				}
			}
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
		if (s2[s2.size() - 1] == ',')
			s2.erase(--s2.end());
		words[i] = s2;
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

unordered_map<string, vector<DWORD>> hackMap = {
	{ "dcl_output oMask", { 0x02000065, 0x0000F000 } },
};

unordered_map<string, vector<int>> ldMap = {
	{ "gather4_c_aoffimmi_indexable", { 5, 126, 3 } },
	{ "gather4_c_indexable", { 5, 126, 2 } },
	{ "gather4_aoffimmi_indexable", { 4, 109, 3 } },
	{ "gather4_indexable", { 4, 109, 2 } },
	{ "gather4_po_c_indexable", { 6, 128, 2 } },
	{ "gather4_po_indexable", { 5, 127, 2 } },
	{ "ld_aoffimmi", { 3, 45, 1 } },
	{ "ld_aoffimmi_indexable", { 3, 45, 3 }},
	{ "ld_indexable", { 3, 45, 2 } },
	{ "ld_raw_indexable", { 3, 165, 2 } },
	{ "ldms_indexable", { 4, 46, 2 } },
	{ "ldms_aoffimmi_indexable", { 4, 46, 3 } },
	{ "sample_aoffimmi", { 4, 69, 1 } },
	{ "sample_d_indexable", { 6, 73, 2 } },
	{ "sample_b_indexable", { 5, 74, 2 } },
	{ "sample_c_indexable", { 5, 70, 2 } },
	{ "sample_c_aoffimmi", { 5, 70, 1 } },
	{ "sample_c_lz_indexable", { 5, 71, 2 } },
	{ "sample_c_lz_aoffimmi", { 5, 71, 1 } },
	{ "sample_c_lz_aoffimmi_indexable", { 5, 71, 3 } },
	{ "sample_indexable", { 4, 69, 2 } },
	{ "sample_aoffimmi_indexable", { 4, 69, 3 } },
	{ "sample_l_aoffimmi", { 5, 72, 1 } },
	{ "sample_l_aoffimmi_indexable", { 5, 72, 3 } },
	{ "sample_l_indexable", { 5, 72, 2 } },
	{ "resinfo_indexable", { 3, 61, 2 } },
	{ "ld_structured_indexable", { 4, 167, 2 } },
	{ "ld_uav_typed_indexable", { 3, 163, 2 } },
	{ "bufinfo_indexable", { 2, 121, 2 } },
};

unordered_map<string, vector<int>> insMap = {
	{ "sample_b", { 5, 74 } },
	{ "sample_c", { 5, 70 } },
	{ "sample_d", { 6, 73 } },
	{ "sample_c_lz", { 5, 71 } },
	{ "sample_l", { 5, 72 } },
	{ "eval_sample_index", { 3, 204 } },
	{ "bfi", { 5, 140 } },
	{ "swapc", { 5, 142, 2 } },
	{ "imad", { 4, 35 } },
	{ "imul", { 4, 38, 2 } },
	{ "ldms", { 4, 46 } },
	{ "mad", { 4, 50 } },
	{ "movc", { 4, 55 } },
	{ "sample", { 4, 69 } },
	{ "sampled", { 6, 73 } },
	{ "gather4", { 4, 109 } },
	{ "udiv", { 4, 78, 2 } },
	{ "umul", { 4, 81, 2 } },
	{ "umax", { 3, 83 } },
	{ "ubfe", { 4, 138 } },
	{ "store_structured", { 4, 168 } },
	{ "ld_structured", { 4, 167 } },
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
	{ "bfrev", { 2, 141 } },
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
	{ "round_nz", { 2, 67 } },
	{ "rsq", { 2, 68 } },
	{ "sqrt", { 2, 75 } },
	{ "utof", { 2, 86 } },
	{ "rcp", { 2, 129 } },
	{ "sampleinfo", { 2, 111 } },
	{ "f16tof32", { 2, 131 } },
	{ "f32tof16", { 2, 130 } },
	{ "imm_atomic_alloc", { 2, 178 } },
	{ "breakc_z", { 1, 3, 0 } },
	{ "breakc_nz", { 1, 3, 0 } },
	{ "case", { 1, 6 } },
	{ "discard_z", { 1, 13, 0 } },
	{ "discard_nz", { 1, 13, 0 } },
	{ "if_z", { 1, 31, 0 } },
	{ "if_nz", { 1, 31, 0 } },
	{ "switch", { 1, 76, 0 } },
	{ "break", { 0, 2 } },
	{ "default", { 0, 10 } },
	{ "else", { 0, 18 } },
	{ "endif", { 0, 21 } },
	{ "endloop", { 0, 22 } },
	{ "endswitch", { 0, 23 } },
	{ "loop", { 0, 48 } },
	{ "ret", { 0, 62 } },
	{ "retc_nz", { 1, 63, 0 } },
	{ "retc_z", { 1, 63, 0 } },
	{ "emit", { 0, 19 } },
	{ "continue", { 0, 7 } },
	{ "continuec_z", { 1, 8, 0 } },
	{ "continuec_nz", { 1, 8, 0 } },
	{ "cut", { 0, 9 } },
	{ "imm_atomic_and", { 4, 181 } },
	{ "imm_atomic_exch", { 4, 184 } },
	{ "imm_atomic_cmp_exch", { 5, 185 } },
	{ "imm_atomic_iadd", { 4, 180 } },
	{ "imm_atomic_consume", { 2, 179 } },
	{ "atomic_iadd", { 3, 173, 0 } },
	{ "ld_raw", { 3, 165 } },
	{ "store_raw", { 3, 166 } },
	{ "atomic_imax", { 3, 174, 0 } },
	{ "atomic_imin", { 3, 175, 0 } },
	{ "atomic_umax", { 3, 176, 0 } },
	{ "atomic_umin", { 3, 177, 0 } },
	{ "atomic_or", { 3, 170, 0 } },
	{ "dcl_tgsm_raw", { 2, 159, 0 } },
	{ "dcl_tgsm_structured", { 3, 160, 0 } },
	{ "dcl_thread_group", { 3, 155 } },
	{ "dcl_uav_raw", { 1, 157, 0 } },
	{ "dcl_uav_structured", { 2, 158, 0 } },
	{ "firstbit_lo", { 2, 136 } },
	{ "firstbit_hi", { 2, 135 } },
	{ "ibfe", { 4, 139 } },
	{ "lod", { 4, 108 } },
	{ "samplepos", { 3, 110 } },
};

vector<DWORD> assembleIns(string s) {
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
	pos = s.find("_uint");
	if (pos != string::npos) {
		s.erase(pos, 5);
		ins->_11_23 = 2;
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

	if (o == "hs_decls") {
		ins->opcode = 113;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_fork_phase") {
		ins->opcode = 115;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_join_phase") {
		ins->opcode = 116;
		ins->length = 1;
		v.push_back(op);
	} else if (o == "hs_control_point_phase") {
		ins->opcode = 114;
		ins->length = 1;
		v.push_back(op);
	} else if (o.substr(0, 3) == "ps_") {
		op = 0x00000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "vs_") {
		op = 0x10000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "gs_") {
		op = 0x20000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "hs_") {
		op = 0x30000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "ds_") {
		op = 0x40000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (o.substr(0, 3) == "cs_") {
		op = 0x50000;
		op |= 16 * atoi(o.substr(3, 1).c_str());
		op |= atoi(o.substr(5, 1).c_str());
		v.push_back(op);
	} else if (w[0] == "sync_g_t") {
		ins->opcode = 190;
		ins->_11_23 = 3;
		ins->length = 1;
		v.push_back(op);
	} else if (w[0] == "sync_uglobal") {
		ins->opcode = 190;
		ins->_11_23 = 8;
		ins->length = 1;
		v.push_back(op);
	} else if (w[0] == "store_uav_typed") {
		ins->opcode = 134;
		if (w[1][0] == 'u') {
			ins->opcode = 164;
		}
		int numOps = 3;
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
		vector<vector<DWORD>> Os;
		int numSpecial = 1;
		if (vIns.size() > 2)
			numSpecial = vIns[2];
		for (int i = 0; i < numOps; i++)
			Os.push_back(assembleOp(w[i + 1], i < numSpecial));
		ins->opcode = vIns[1];
		if (bSat)
			ins->_11_23 |= 4;
		if (bNZ)
			ins->_11_23 |= 128;
		if (bZ)
			ins->_11_23 |= 0;
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
		}
		for (int i = 0; i < numOps; i++)
			v.insert(v.end(), Os[i].begin(), Os[i].end());
	} else if (o == "dcl_input") {
		vector<DWORD> os = assembleOp(w[1], 1);
		ins->opcode = 95;
		ins->length = 1 + os.size();
		// Should sort special value for text constants.
		if ((os[0] & 0xFF0) == 0)
			os[0] -= 1;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_output") {
		vector<DWORD> os = assembleOp(w[1], 1);
		ins->opcode = 101;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_resource_raw") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 161;
		ins->length = 3;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_resource_buffer") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 1;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
		if (w[1] == "(sint,sint,sint,sint)")
			v.push_back(0x3333);
	} else if (o == "dcl_resource_texture1d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 2;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture1darray") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 7;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_uav_typed_texture1d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 156;
		ins->_11_23 = 2;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture2d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 3;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
		if (w[1] == "(sint,sint,sint,sint)")
			v.push_back(0x3333);
	} else if (o == "dcl_uav_typed_buffer") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 156;
		ins->_11_23 = 1;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture3d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 5;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_uav_typed_texture3d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 156;
		ins->_11_23 = 5;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texturecube") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 6;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texturecubearray") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 10;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture2darray") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 88;
		ins->_11_23 = 8;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_uav_typed_texture2d") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 156;
		ins->_11_23 = 3;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_uav_typed_texture2darray") {
		vector<DWORD> os = assembleOp(w[2]);
		ins->opcode = 156;
		ins->_11_23 = 8;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[1] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[1] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture2dms") {
		vector<DWORD> os = assembleOp(w[3]);
		ins->opcode = 88;
		if (w[1] == "(0)")
			ins->_11_23 = 4;
		if (w[1] == "(2)")
			ins->_11_23 = 68;
		if (w[1] == "(4)")
			ins->_11_23 = 132;
		if (w[1] == "(6)")
			ins->_11_23 = 196;
		if (w[1] == "(8)")
			ins->_11_23 = 260;
		if (w[1] == "(16)")
			ins->_11_23 = 516;
		if (w[1] == "(32)")
			ins->_11_23 = 1028;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[2] == "(float,float,float,float)")
			v.push_back(0x5555);
		if (w[2] == "(uint,uint,uint,uint)")
			v.push_back(0x4444);
	} else if (o == "dcl_resource_texture2dmsarray") {
		vector<DWORD> os = assembleOp(w[3]);
		ins->opcode = 88;
		if (w[1] == "(2)")
			ins->_11_23 = 73;
		if (w[1] == "(4)")
			ins->_11_23 = 137;
		if (w[1] == "(8)")
			ins->_11_23 = 265;
		ins->length = 4;
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
		if (w[2] == "(unorm,unorm,unorm,unorm)")
			v.push_back(0x1111);
	} else if (o == "dcl_indexrange") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 91;
		ins->length = 2 + os.size();
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
		ins->opcode = 106;
		ins->length = 1;
		ins->_11_23 = 0;
		if (w.size() > 1) {
			string s = w[1];
			if (s == "refactoringAllowed")
				ins->_11_23 |= 1;
			if (s == "forceEarlyDepthStencil")
				ins->_11_23 |= 4;
			if (s == "enableRawAndStructuredBuffers")
				ins->_11_23 |= 8;
		}
		if (w.size() > 3) {
			string s = w[3];
			if (s == "refactoringAllowed")
				ins->_11_23 |= 1;
			if (s == "forceEarlyDepthStencil")
				ins->_11_23 |= 4;
			if (s == "enableRawAndStructuredBuffers")
				ins->_11_23 |= 8;
		}
		if (w.size() > 5) {
			string s = w[5];
			if (s == "refactoringAllowed")
				ins->_11_23 |= 1;
			if (s == "forceEarlyDepthStencil")
				ins->_11_23 |= 4;
			if (s == "enableRawAndStructuredBuffers")
				ins->_11_23 |= 8;
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
	} else if (o == "dcl_output_siv") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 103;
		if (w[2] == "position")
			os.push_back(1);
		else if (w[2] == "clip_distance")
			os.push_back(2);
		else if (w[2] == "cull_distance")
			os.push_back(3);
		else if (w[2] == "rendertarget_array_index")
			os.push_back(4);
		else if (w[2] == "viewport_array_index")
			os.push_back(5);
		else if (w[2] == "finalQuadUeq0EdgeTessFactor")
			os.push_back(11);
		else if (w[2] == "finalQuadVeq0EdgeTessFactor")
			os.push_back(12);
		else if (w[2] == "finalQuadUeq1EdgeTessFactor")
			os.push_back(13);
		else if (w[2] == "finalQuadVeq1EdgeTessFactor")
			os.push_back(14);
		else if (w[2] == "finalQuadUInsideTessFactor")
			os.push_back(15);
		else if (w[2] == "finalQuadVInsideTessFactor")
			os.push_back(16);
		else if (w[2] == "finalTriUeq0EdgeTessFactor")
			os.push_back(17);
		else if (w[2] == "finalTriVeq0EdgeTessFactor")
			os.push_back(18);
		else if (w[2] == "finalTriWeq0EdgeTessFactor")
			os.push_back(19);
		else if (w[2] == "finalTriInsideTessFactor")
			os.push_back(20);
		else if (w[2] == "finalLineDetailTessFactor")
			os.push_back(21);
		else if (w[2] == "finalLineDensityTessFactor")
			os.push_back(22);
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_input_siv") {
		vector<DWORD> os = assembleOp(w[1], true);
		ins->opcode = 97;
		if (w[2] == "position")
			os.push_back(1);
		else if (w[2] == "clip_distance")
			os.push_back(2);
		else if (w[2] == "cull_distance")
			os.push_back(3);
		else if (w[2] == "finalLineDetailTessFactor")
			os.push_back(0x15);
		else if (w[2] == "finalLineDensityTessFactor")
			os.push_back(0x16);
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
			} else if (w[2] == "sample") {
				ins->_11_23 = 6;
				os = assembleOp(w[3], true);
			} else {
				ins->_11_23 = 2;
				os = assembleOp(w[2], true);
			}
		}
		if (w[1] == "constant") {
			ins->_11_23 = 1;
			os = assembleOp(w[2], true);
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
			} else if (w[2] == "primitive_id") {
				os.push_back(0x7);
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
			} else if (w[3] == "clip_distance") {
				os = assembleOp(w[2], true);
				ins->_11_23 = 2;
				os.push_back(2);
			}
		} else if (w[1] == "constant") {
			ins->_11_23 = 1;
			os = assembleOp(w[2], true);
			if (w[3] == "rendertarget_array_index")
				os.push_back(4);
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
	} else if (o == "dcl_tessellator_partitioning") {
		ins->opcode = 150;
		ins->length = 1;
		if (w[1] == "partitioning_integer")
			ins->_11_23 = 1;
		else if (w[1] == "partitioning_fractional_odd")
			ins->_11_23 = 3;
		else if (w[1] == "partitioning_fractional_even")
			ins->_11_23 = 4;
		v.push_back(op);
	} else if (o == "dcl_tessellator_output_primitive") {
		ins->opcode = 151;
		ins->length = 1;
		if (w[1] == "output_line")
			ins->_11_23 = 2;
		else if (w[1] == "output_triangle_cw")
			ins->_11_23 = 3;
		else if (w[1] == "output_triangle_ccw")
			ins->_11_23 = 4;
		v.push_back(op);
	} else if (o == "dcl_tessellator_domain") {
		ins->opcode = 149;
		ins->length = 1;
		if (w[1] == "domain_isoline")
			ins->_11_23 = 1;
		else if (w[1] == "domain_tri")
			ins->_11_23 = 2;
		else if (w[1] == "domain_quad")
			ins->_11_23 = 3;
		v.push_back(op);
	} else if (o == "dcl_stream") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 143;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "emit_stream") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 117;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "cut_stream") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 118;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_outputtopology") {
		ins->opcode = 92;
		ins->length = 1;
		if (w[1] == "trianglestrip")
			ins->_11_23 = 5;
		else if (w[1] == "linestrip")
			ins->_11_23 = 3;
		v.push_back(op);
	} else if (o == "dcl_output_control_point_count") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 148;
		ins->_11_23 = os[0];
		ins->length = 1;
		v.push_back(op);
	} else if (o == "dcl_input_control_point_count") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 147;
		ins->_11_23 = os[0];
		ins->length = 1;
		v.push_back(op);
	} else if (o == "dcl_maxout") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 94;
		ins->length = 1 + os.size();
		v.push_back(op);
		v.insert(v.end(), os.begin(), os.end());
	} else if (o == "dcl_inputprimitive") {
		ins->opcode = 93;
		ins->length = 1;
		if (w[1] == "point")
			ins->_11_23 = 1;
		else if (w[1] == "line")
			ins->_11_23 = 2;
		else if (w[1] == "triangle")
			ins->_11_23 = 3;
		else if (w[1] == "triangleadj")
			ins->_11_23 = 7;
		v.push_back(op);
	} else if (o == "dcl_hs_max_tessfactor") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 152;
		ins->length = 1 + os.size() - 1;
		v.push_back(op);
		v.insert(v.end(), os.begin() + 1, os.end());
	} else if (o == "dcl_hs_fork_phase_instance_count") {
		vector<DWORD> os = assembleOp(w[1]);
		ins->opcode = 153;
		ins->length = 1 + os.size();
		v.push_back(op);
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
		size_t numRead = fread(buffer.data(), 1, buffer.size(), f);
		fclose(f);
	}
	return buffer;
}

vector<string> stringToLines(const char* start, size_t size) {
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
		if (s.size() > 1 && s[s.size() - 1] == '\r')
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

// Dead code (any reason to keep this?)
string shaderModel(byte* buffer) {
	DWORD numChunks;
	vector<DWORD> chunkOffsets;

	byte* pPosition = buffer;
	pPosition += 28;
	numChunks = *(DWORD*)pPosition;
	if (numChunks < 1)
		throw std::invalid_argument("shaderModel: Bad shader binary");
	pPosition += 4;
	chunkOffsets.resize(numChunks);
	std::memcpy(chunkOffsets.data(), pPosition, 4 * numChunks);

	byte* codeByteStart;
	int codeChunk = 0;
	for (DWORD i = 1; i <= numChunks; i++) {
		codeChunk = numChunks - i;
		codeByteStart = buffer + chunkOffsets[codeChunk];
		if (memcmp(codeByteStart, "SHEX", 4) == 0 || memcmp(codeByteStart, "SHDR", 4) == 0)
			break;
	}
	// FIXME: If neither SHEX or SHDR was found in the shader, codeByteStart will be garbage
	DWORD* codeStart = (DWORD*)(codeByteStart + 8);
	int major = (*codeStart & 0xF0) >> 4;
	int minor = (*codeStart & 0xF000) >> 12;
	int shaderType = (*codeStart & 0xFFFF0000) >> 16;
	string shaderModel = "ps_";
	if (shaderType == 0x1)
		shaderModel = "vs_";
	switch (major) {
	case 5:
		shaderModel.append("5_");
		break;
	case 4:
		shaderModel.append("4_");
		break;
	case 3:
		shaderModel.append("3_");
		break;
	}
	switch (minor) {
	case 0:
		shaderModel.append("0");
		break;
	case 1:
		shaderModel.append("1");
		break;
	case 2:
		shaderModel.append("2");
		break;
	}
	return shaderModel;
}

vector<byte> assembler(vector<byte> asmFile, vector<byte> buffer) {
	byte fourcc[4];
	DWORD fHash[4];
	DWORD one;
	DWORD fSize;
	DWORD numChunks;
	vector<DWORD> chunkOffsets;

	// TODO: Add robust error checking here (buffer is at least as large as
	// the header, etc). I've added a check for numChunks < 1 as that
	// would lead to codeByteStart being used uninitialised
	byte* pPosition = buffer.data();
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
	asmBuffer = (char*)asmFile.data();
	asmSize = asmFile.size();
	byte* codeByteStart;
	int codeChunk = 0;
	for (DWORD i = 1; i <= numChunks; i++) {
		codeChunk = numChunks - i;
		codeByteStart = buffer.data() + chunkOffsets[numChunks - i];
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
			} else if (s.size() > 0) {
				vector<DWORD> ins = assembleIns(s);
				o.insert(o.end(), ins.begin(), ins.end());
			}
		}
	}
	codeStart = (DWORD*)(codeByteStart); // Endian bug, not that we care
	auto it = buffer.begin() + chunkOffsets[codeChunk] + 8;
	size_t codeSize = codeStart[1];
	buffer.erase(it, it + codeSize);
	size_t newCodeSize = 4 * o.size();
	codeStart[1] = (DWORD)newCodeSize;
	vector<byte> newCode(newCodeSize);
	o[1] = (DWORD)o.size();
	memcpy(newCode.data(), o.data(), newCodeSize);
	it = buffer.begin() + chunkOffsets[codeChunk] + 8;
	buffer.insert(it, newCode.begin(), newCode.end());
	DWORD* dwordBuffer = (DWORD*)buffer.data();
	for (DWORD i = codeChunk + 1; i < numChunks; i++) {
		dwordBuffer[8 + i] += (DWORD)(newCodeSize - codeSize);
	}
	dwordBuffer[6] = (DWORD)buffer.size();
	vector<DWORD> hash = ComputeHash((byte const*)buffer.data() + 20, (DWORD)buffer.size() - 20);
	dwordBuffer[1] = hash[0];
	dwordBuffer[2] = hash[1];
	dwordBuffer[3] = hash[2];
	dwordBuffer[4] = hash[3];
	return buffer;
}
