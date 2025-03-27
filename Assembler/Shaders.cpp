// Shaders.cpp : Defines the entry point for the console application.

#include "Assembler.h"

#include <cstdio>
#include <direct.h>
#include <iostream>
#include <string>
#include <tchar.h>
#include <vector>
#include <Windows.h>

using std::cout;
using std::endl;
using std::string;
using std::vector;

// -----------------------------------------------------------------------------

bool  gLogDebug = false;
FILE* LogFile   = nullptr;

static vector<string> enumerate_files(
    string path_name,
    string filter = "")
{
    vector<string>   files;
    WIN32_FIND_DATAA find_file_data;
    HANDLE           h_find;
    string           s_name = path_name;
    s_name.append(filter);
    h_find = FindFirstFileA(s_name.c_str(), &find_file_data);
    if (h_find != INVALID_HANDLE_VALUE)
    {
        string f_name = path_name;
        f_name.append(find_file_data.cFileName);
        files.push_back(f_name);
        while (FindNextFileA(h_find, &find_file_data))
        {
            f_name = path_name;
            f_name.append(find_file_data.cFileName);
            files.push_back(f_name);
        }
        FindClose(h_find);
    }
    return files;
}

static vector<byte> read_file(
    string file_name)
{
    vector<byte> buffer;
    FILE*        f;
    fopen_s(&f, file_name.c_str(), "rb");
    if (f != nullptr)
    {
        fseek(f, 0L, SEEK_END);
        int file_size = ftell(f);
        buffer.resize(file_size);
        fseek(f, 0L, SEEK_SET);
        size_t num_read = fread(buffer.data(), 1, buffer.size(), f);
        fclose(f);
    }
    return buffer;
}

int _tmain(
    int     argc,
    _TCHAR* argv[])
{
    int            shader_no = 1;
    vector<string> game_names;
    string         path_name;
    vector<string> files;
    FILE*          f;
    char           cwd[MAX_PATH];
    char           gamebuffer[10000];

    if (!_getcwd(cwd, MAX_PATH))
        return 1;
    vector<string> lines;
    fopen_s(&f, "gamelist.txt", "rb");
    if (f)
    {
        size_t fr = ::fread(gamebuffer, 1, 10000, f);
        fclose(f);
        lines = string_to_lines(gamebuffer, fr);
    }

    if (lines.size() > 0)
    {
        for (auto i = lines.begin(); i != lines.end(); i++)
        {
            game_names.push_back(*i);
        }
    }
    else
    {
        game_names.push_back(cwd);
    }
    for (DWORD i = 0; i < game_names.size(); i++)
    {
        string game_name = game_names[i];
        cout << game_name << ":" << endl;

        int progress = 0;
        path_name    = game_name;
        path_name.append("\\ShaderCache\\");
        files = enumerate_files(path_name, "????????????????-??.bin");
        if (files.size() > 0)
        {
            cout << "bin->asm: ";
            for (DWORD i = 0; i < files.size(); i++)
            {
                string file_name = files[i];

                vector<byte> ASM;
                disassembler(&read_file(file_name), &ASM, nullptr);

                file_name.erase(file_name.size() - 3, 3);
                file_name.append("txt");
                FILE* f;
                fopen_s(&f, file_name.c_str(), "wb");
                fwrite(ASM.data(), 1, ASM.size(), f);
                fclose(f);

                int new_progress = static_cast<int>(50.0 * i / files.size());
                if (new_progress > progress)
                {
                    cout << ".";
                    progress++;
                }
            }
        }
        cout << endl;

        progress  = 0;
        path_name = game_name;
        path_name.append("\\ShaderCache\\");
        files = enumerate_files(path_name, "????????????????-??.txt");
        if (files.size() > 0)
        {
            cout << "asm->cbo: ";
            for (DWORD i = 0; i < files.size(); i++)
            {
                string file_name = files[i];

                auto ASM = read_file(file_name);
                file_name.erase(file_name.size() - 3, 3);
                file_name.append("bin");
                auto BIN = read_file(file_name);

                auto CBO = assembler(reinterpret_cast<vector<char>*>(&ASM), BIN);

                file_name.erase(file_name.size() - 3, 3);
                file_name.append("cbo");
                FILE* f;
                fopen_s(&f, file_name.c_str(), "wb");
                if (f)
                {
                    fwrite(CBO.data(), 1, CBO.size(), f);
                    fclose(f);
                }

                int new_progress = static_cast<int>(50.0 * i / files.size());
                if (new_progress > progress)
                {
                    cout << ".";
                    progress++;
                }
            }
        }
        cout << endl;

        progress  = 0;
        path_name = game_names[i];
        path_name.append("\\Mark\\");
        files = enumerate_files(path_name, "*.bin");
        if (files.size() > 0)
        {
            cout << "bin->asm validate: ";
            for (DWORD i = 0; i < files.size(); i++)
            {
                string file_name = files[i];

                vector<byte> ASM;
                disassembler(&read_file(file_name), &ASM, nullptr);

                file_name.erase(file_name.size() - 3, 3);
                file_name.append("txt");
                FILE* f;
                fopen_s(&f, file_name.c_str(), "wb");
                if (f)
                {
                    fwrite(ASM.data(), 1, ASM.size(), f);
                    fclose(f);
                }

                int new_progress = static_cast<int>(50.0 * i / files.size());
                if (new_progress > progress)
                {
                    cout << ".";
                    progress++;
                }
            }
        }
        cout << endl;

        write_lut();
    }
    return 0;
}
