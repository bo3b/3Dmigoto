// Shaders.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <direct.h>

#include "DecompileHLSL.h"

using namespace std;

string replace(string str, string before, string after) {
	int pos = str.find(before);
	if (pos < str.size()) {
		str = str.erase(pos, before.size());
		str.insert(str.begin() + pos, after.begin(), after.end());
	}
	return str;
}

static map<string, vector<DWORD>> codeBin;
static int processedBin = 0;

string convertF(DWORD original) {
	char buf[80];
	char buf2[80];

	float fOriginal = reinterpret_cast<float &>(original);
	sprintf(buf2, "%.9E", fOriginal);
	int len = strlen(buf2);
	if (buf2[len - 4] == '-') {
		int exp = atoi(buf2 + len - 3);
		switch (exp) {
		case 1:
			sprintf(buf, "%.9f", fOriginal);
			break;
		case 2:
			sprintf(buf, "%.10f", fOriginal);
			break;
		case 3:
			sprintf(buf, "%.11f", fOriginal);
			break;
		case 4:
			sprintf(buf, "%.12f", fOriginal);
			break;
		case 5:
			sprintf(buf, "%.13f", fOriginal);
			break;
		case 6:
			sprintf(buf, "%.14f", fOriginal);
			break;
		default:
			sprintf(buf, "%.9E", fOriginal);
			break;
		}
	} else {
		int exp = atoi(buf2 + len - 3);
		switch (exp) {
		case 0:
			sprintf(buf, "%.8f", fOriginal);
			break;
		default:
			sprintf(buf, "%.8f", fOriginal);
			break;
		}
	}
	string sLiteral(buf);
	DWORD newDWORD = strToDWORD(sLiteral);
	if (newDWORD != original) {
		FILE* f = fopen("debug.txt", "wb");;
		fwrite("Hej", 1, 3, f);
		fclose(f);
	}
	return sLiteral;
}

string assembleAndCompare(string s, vector<DWORD> v) {
	string s2;
	/*
	while (memcmp(s.c_str(), " ", 1) == 0) {
		s.erase(s.begin());
	}
	*/
	int lastLiteral = 0;
	int lastEnd = 0;
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
							string sBegin = sNew.substr(0, lastLiteral + 2);
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
							string sBegin = sNew.substr(0, lastLiteral + 2);
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
							string sBegin = sNew.substr(0, lastLiteral + 2);
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
							string sBegin = sNew.substr(0, lastLiteral + 2);
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
						string sBegin = sNew.substr(0, lastLiteral + 2);
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
						string sBegin = sNew.substr(0, lastLiteral + 2);
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
						string sBegin = sNew.substr(0, lastLiteral + 2);
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
						string sBegin = sNew.substr(0, lastLiteral + 2);
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
						string sBegin = sNew.substr(0, lastLiteral + 2);
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
			s2 = s;
			s2.append(" orig");
			codeBin[s2] = v;
			s2 = s;
			s2.append(" fail");
			codeBin[s2] = v2;
		}
	} else {
		s2 = "!missing ";
		s2.append(s);
		codeBin[s2] = v;
	}
	return sNew;
}
void DXBC(string fileName, bool patch = false) {
	byte fourcc[4];
	DWORD fHash[4];
	DWORD one;
	DWORD fSize;
	DWORD numChunks;
	vector<DWORD> chunkOffsets;
	vector<byte> buffer;

	string asmFile = fileName;
	if (patch) {
		fileName.erase(fileName.size() - 3, 3);
		fileName.append("bin");
	}
	buffer = readFile(fileName);
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
	pPosition += 4;
	chunkOffsets.resize(numChunks);
	std::memcpy(chunkOffsets.data(), pPosition, 4 * numChunks);

	char* asmBuffer;
	int asmSize;
	vector<byte> asmBuf;
	if (patch) {
		asmBuf = readFile(asmFile);
		asmBuffer = (char*)asmBuf.data();
		asmSize = asmBuf.size();
	} else {
		ID3DBlob* pDissassembly;
		HRESULT ok = D3DDisassemble(buffer.data(), buffer.size(), 0, NULL, &pDissassembly);
		if (ok == S_OK) {
			asmBuffer = (char*)pDissassembly->GetBufferPointer();
			asmSize = pDissassembly->GetBufferSize();
			string asmFile = fileName;
			asmFile.erase(asmFile.size() - 3, 3);
			asmFile.append("txt");
			FILE* f;
			fopen_s(&f, asmFile.c_str(), "wb");
			fwrite(pDissassembly->GetBufferPointer(), 1, pDissassembly->GetBufferSize(), f);
			fclose(f);
		}
	}
	byte* codeByteStart;
	int codeChunk = 0;
	for (DWORD i = 1; i <= numChunks; i++) {
		codeChunk = numChunks - i;
		codeByteStart = buffer.data() + chunkOffsets[numChunks - i];
		if (memcmp(codeByteStart, "SHEX", 4) == 0 || memcmp(codeByteStart, "SHDR", 4) == 0)
			break;
	}
	vector<string> lines = stringToLines(asmBuffer, asmSize);
	DWORD* codeStart = (DWORD*)(codeByteStart + 8);
	bool codeStarted = false;
	bool multiLine = false;
	int multiLines = 0;
	string s2;
	vector<DWORD> o;
	for (DWORD i = 0; i < lines.size(); i++) {
		string s = lines[i];
		if (memcmp(s.c_str(), "//", 2) != 0) {
			vector<DWORD> v;
			if (!codeStarted) {
				if (s.size() > 0 && s[0] != ' ') {
					codeStarted = true;
					if (patch) {
						vector<DWORD> ins = assembleIns(s);
						o.insert(o.end(), ins.begin(), ins.end());
						o.push_back(0); // DWORD Size
					} else {
						v.push_back(*codeStart);
						codeStart += 2;
						string sNew = assembleAndCompare(s, v);
						lines[i] = sNew;
					}
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
				if (patch) {
					vector<DWORD> ins = assembleIns(s);
					o.insert(o.end(), ins.begin(), ins.end());
				} else {
					shader_ins* ins = (shader_ins*)codeStart;
					v.push_back(*codeStart);
					codeStart++;
					DWORD length = *codeStart;
					v.push_back(*codeStart);
					codeStart++;
					for (DWORD i = 2; i < length; i++) {
						v.push_back(*codeStart);
						codeStart++;
					}
					string sNew = assembleAndCompare(s, v);
					auto sLines = stringToLines(sNew.c_str(), sNew.size());
					int startLine = i - sLines.size() + 1;
					for (int j = 0; j < sLines.size(); j++) {
						lines[startLine + j] = sLines[j];
					}
					//lines[i] = sNew;
				}
			} else if (multiLine) {
				s2.append("\n");
				s2.append(s);
				multiLines++;
			} else if (s.size() > 0) {
				if (patch) {
					vector<DWORD> ins = assembleIns(s);
					o.insert(o.end(), ins.begin(), ins.end());
				} else {
					shader_ins* ins = (shader_ins*)codeStart;
					v.push_back(*codeStart);
					codeStart++;
					for (DWORD j = 1; j < ins->length; j++) {
						v.push_back(*codeStart);
						codeStart++;
					}
					string sNew = assembleAndCompare(s, v);
					lines[i] = sNew;
				}
			}
		}
	}
	if (!patch) {
		FILE* f;
		string oFile = fileName;
		oFile.erase(oFile.size() - 3, 3);
		oFile.append("asm");
		fopen_s(&f, oFile.c_str(), "wb");
		for (int i = 0; i < lines.size(); i++) {
			fwrite(lines[i].c_str(), 1, lines[i].size(), f);
			fwrite("\n", 1, 1, f);
		}
		fclose(f);
	}
	if (patch) {
		DWORD* codeStart = (DWORD*)(codeByteStart);
		auto it = buffer.begin() + chunkOffsets[codeChunk] + 8;
		DWORD codeSize = codeStart[1];
		buffer.erase(it, it + codeSize);
		DWORD newCodeSize = 4 * o.size();
		codeStart[1] = newCodeSize;
		vector<byte> newCode(newCodeSize);
		o[1] = o.size();
		memcpy(newCode.data(), o.data(), newCodeSize);
		it = buffer.begin() + chunkOffsets[codeChunk] + 8;
		buffer.insert(it, newCode.begin(), newCode.end());
		DWORD* dwordBuffer = (DWORD*)buffer.data();
		for (DWORD i = codeChunk + 1; i < numChunks; i++) {
			dwordBuffer[8 + i] += newCodeSize - codeSize;
		}
		dwordBuffer[6] = buffer.size();
		vector<DWORD> hash = ComputeHash((byte const*)buffer.data() + 20, buffer.size() - 20);
		dwordBuffer[1] = hash[0];
		dwordBuffer[2] = hash[1];
		dwordBuffer[3] = hash[2];
		dwordBuffer[4] = hash[3];
		string oFile = asmFile;
		oFile.erase(oFile.size() - 3, 3);
		oFile.append("cbo");
		FILE* f;
		fopen_s(&f, oFile.c_str(), "wb");
		fwrite(buffer.data(), 1, buffer.size(), f);
		fclose(f);
	}
}

vector<string> enumerateFiles(string pathName, string filter = "") {
	vector<string> files;
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind;
	string sName = pathName;
	sName.append(filter);
	hFind = FindFirstFileA(sName.c_str(), &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)	{
		string fName = pathName;
		fName.append(FindFileData.cFileName);
		files.push_back(fName);
		while (FindNextFileA(hFind, &FindFileData)) {
			fName = pathName;
			fName.append(FindFileData.cFileName);
			files.push_back(fName);
		}
		FindClose(hFind);
	}
	return files;
}

void writeLUT() {
	FILE* f;
	fopen_s(&f, "lut.asm", "wb");
	for (std::map<string, vector<DWORD>>::iterator it = codeBin.begin(); it != codeBin.end(); ++it) {
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

int _tmain(int argc, _TCHAR* argv[])
{
	int shaderNo = 1;
	vector<string> gameNames;
	string pathName;
	vector<string> files;
	char cwd[MAX_PATH];
	char gamebuffer[10000];

	_getcwd(cwd, MAX_PATH);
	vector<string> lines;
	FILE* f = ::fopen("gamelist.txt", "rb");
	if (f) {
		int fr = ::fread(gamebuffer, 1, 10000, f);
		::fclose(f);
		lines = stringToLines(gamebuffer, fr);
	}

	if (lines.size() > 0) {
		for (auto i = lines.begin(); i != lines.end(); i++) {
			gameNames.push_back(*i);
		}
	} else {
		gameNames.push_back(cwd);
	}
	for (DWORD i = 0; i < gameNames.size(); i++) {
		string gameName = gameNames[i];
		cout << gameName << ":" << endl;

		int progress = 0;
		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.bin");
		if (files.size() > 0) {
			cout << "bin->asm validate: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];
				DXBC(fileName);

				int newProgress = 50.0 * i / files.size();
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;

		/*
		progress = 0;
		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.txt");
		if (files.size() > 0) {
			cout << "asm->cbo: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];
				DXBC(fileName, true);

				int newProgress = 50.0 * i / files.size();
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;
		*/

		progress = 0;
		pathName = gameNames[i];
		pathName.append("\\Mark\\");
		files = enumerateFiles(pathName, "*.bin");
		if (files.size() > 0) {
			cout << "bin->asm validate: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];
				DXBC(fileName);

				int newProgress = 50.0 * i / files.size();
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;

		progress = 0;
		pathName = gameNames[i];
		pathName.append("\\Mark\\");
		files = enumerateFiles(pathName, "*.bin");
		if (files.size() > 0) {
			cout << "ValidHLSL: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];
				auto BIN = readFile(fileName);
				fileName.erase(fileName.size() - 3, 3);
				fileName.append("txt");
				auto ASM = readFile(fileName);

				bool patched = false;
				string shaderModel;
				bool errorOccurred = false;

				// Set all to zero, so we only init the ones we are using here.
				ParseParameters p = {};

				p.bytecode = BIN.data();
				p.decompiled = (const char *)ASM.data();
				p.decompiledSize = ASM.size();
				const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);

				if (errorOccurred) {
					fileName.erase(fileName.size() - 4, 4);
					fileName.append("_replace_bad.txt");
					FILE* f;
					fopen_s(&f, fileName.c_str(), "wb");
					fwrite(decompiledCode.data(), 1, decompiledCode.size(), f);
					fclose(f);
					continue;
				}

				fileName.erase(fileName.size() - 4, 4);
				fileName.append("_replace.txt");
				FILE* f;
				fopen_s(&f, fileName.c_str(), "wb");
				fwrite(decompiledCode.data(), 1, decompiledCode.size(), f);
				fclose(f);

				int newProgress = 50.0 * i / files.size();
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;
	}
	writeLUT();
	return 0;
}

