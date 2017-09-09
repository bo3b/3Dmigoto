// Shaders.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <direct.h>

using namespace std;

FILE *LogFile = NULL;
bool gLogDebug = false;

static vector<string> enumerateFiles(string pathName, string filter = "") {
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

static vector<byte> readFile(string fileName) {
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

int _tmain(int argc, _TCHAR* argv[])
{
	int shaderNo = 1;
	vector<string> gameNames;
	string pathName;
	vector<string> files;
	FILE* f;
	char cwd[MAX_PATH];
	char gamebuffer[10000];

	if (!_getcwd(cwd, MAX_PATH))
		return 1;
	vector<string> lines;
	fopen_s(&f, "gamelist.txt", "rb");
	if (f) {
		size_t fr = ::fread(gamebuffer, 1, 10000, f);
		fclose(f);
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
			cout << "bin->asm: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];

				vector<byte> ASM;
				disassembler(&readFile(fileName), &ASM, NULL);

				fileName.erase(fileName.size() - 3, 3);
				fileName.append("txt");
				FILE* f;
				fopen_s(&f, fileName.c_str(), "wb");
				fwrite(ASM.data(), 1, ASM.size(), f);
				fclose(f);
				
				int newProgress = (int)(50.0 * i / files.size());
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;

		progress = 0;
		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.txt");
		if (files.size() > 0) {
			cout << "asm->cbo: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];

				auto ASM = readFile(fileName);
				fileName.erase(fileName.size() - 3, 3);
				fileName.append("bin");
				auto BIN = readFile(fileName);
				
				auto CBO = assembler((vector<char>*)&ASM, BIN);

				fileName.erase(fileName.size() - 3, 3);
				fileName.append("cbo");
				FILE* f;
				fopen_s(&f, fileName.c_str(), "wb");
				if (f) {
					fwrite(CBO.data(), 1, CBO.size(), f);
					fclose(f);
				}

				int newProgress = (int)(50.0 * i / files.size());
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
			cout << "bin->asm validate: ";
			for (DWORD i = 0; i < files.size(); i++) {
				string fileName = files[i];

				vector<byte> ASM;
				disassembler(&readFile(fileName), &ASM, NULL);

				fileName.erase(fileName.size() - 3, 3);
				fileName.append("txt");
				FILE* f;
				fopen_s(&f, fileName.c_str(), "wb");
				if (f) {
					fwrite(ASM.data(), 1, ASM.size(), f);
					fclose(f);
				}

				int newProgress = (int)(50.0 * i / files.size());
				if (newProgress > progress) {
					cout << ".";
					progress++;
				}
			}
		}
		cout << endl;

		writeLUT();
	}
	return 0;
}
