// Shaders.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <direct.h>

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

void assembleAndCompare(string s, vector<DWORD> v) {
	string s2;
	while (memcmp(s.c_str(), " ", 1) == 0) {
		s.erase(s.begin());
	}
	vector<DWORD> v2 = assembleIns(s);
	bool valid = true;
	if (v2.size() > 0) {
		if (v2.size() == v.size()) {
			for (DWORD i = 0; i < v.size(); i++) {
				if (v[i] == 0x1835) {
					i += v[++i];
				} else if (v[i] == 0x4001) {
					i++;
				} else if (v[i] == 0x4002) {
					i += 4;
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
			asmFile.append("asm");
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
						assembleAndCompare(s, v);
					}
				}
			} else if (s.find("{ {") < s.size()) {
				s2 = s;
				multiLine = true;
			} else if (s.find("} }") < s.size()) {
				s2.append("\n");
				s2.append(s);
				s = s2;
				multiLine = false;
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
					assembleAndCompare(s, v);
				}
			} else if (multiLine) {
				s2.append("\n");
				s2.append(s);
			} else if (s.size() > 0) {
				if (patch) {
					vector<DWORD> ins = assembleIns(s);
					o.insert(o.end(), ins.begin(), ins.end());
				} else {
					shader_ins* ins = (shader_ins*)codeStart;
					v.push_back(*codeStart);
					codeStart++;
					for (DWORD i = 1; i < ins->length; i++) {
						v.push_back(*codeStart);
						codeStart++;
					}
					assembleAndCompare(s, v);
				}
			}
		}
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
		oFile.append("bin");
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
	vector<string> files2;
	char* buffer = NULL;
	buffer = _getcwd(NULL, 0);
	if (true) {
		/*
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\Aliens vs Predator");
		gameNames.push_back("D:\\Spel\\Battlefield Bad Company 2");
		gameNames.push_back("D:\\Spel\\ANNO 2070");
		gameNames.push_back("D:\\Spel\\ANNO 1404 - Gold Edition");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\Assassin's Creed 3");
		gameNames.push_back("D:\\Spel\\Assassin's Creed IV Black Flag Asia");	
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\Batman Arkham City GOTY\\Binaries\\Win32");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\Batman Arkham Origins\\SinglePlayer\\Binaries\\Win32");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\Bioshock\\Builds\\Release");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\BioShock 2\\MP\\Builds\\Binaries");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\BioShock 2\\SP\\Builds\\Binaries");
		gameNames.push_back("D:\\Steam\\SteamApps\\common\\BioShock Infinite\\Binaries\\Win32");
		*/
		gameNames.push_back("F:\\GOG Games\\The Witcher 3 Wild Hunt\\bin\\x64");
	} else {
		gameNames.push_back(buffer);
	}
	for (DWORD i = 0; i < gameNames.size(); i++) {
		string gameName = gameNames[i];
		cout << gameName << ":" << endl;

		int progress = 0;
		pathName = gameName;
		pathName.append("\\Dump\\");
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

		string patchName;
		pathName = gameName;

		progress = 0;
		patchName = pathName;
		patchName.append("\\Dump\\");
		files = enumerateFiles(patchName, "*.asm");
		if (files.size() > 0) {
			cout << "asm->bin: ";
			for (DWORD i = 0; i < files.size(); i++) {
				DXBC(files[i], true);
				int newProgress = 50.0 * i / files.size();
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
			cout << endl;
		}
	}
	if (codeBin.size() > 0)
		writeLUT();
	return 0;
}

