//--------------------------------------------------------------------------------------
// File: BinaryReader.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "BinaryReader.h"

using namespace DirectX;


// Constructor reads from the filesystem.
BinaryReader::BinaryReader(_In_z_ wchar_t const* fileName)
{
    size_t dataSize;

    HRESULT hr = ReadEntireFile(fileName, mOwnedData, &dataSize);
    if ( FAILED(hr) )
    {
        DebugTrace( "BinaryReader failed (%08X) to load '%ls'\n", hr, fileName );
        throw std::exception( "BinaryReader" );
    }

    mPos = mOwnedData.get();
    mEnd = mOwnedData.get() + dataSize;
}


// Constructor reads from an existing memory buffer.
BinaryReader::BinaryReader(_In_reads_bytes_(dataSize) uint8_t const* dataBlob, size_t dataSize)
{
    mPos = dataBlob;
    mEnd = dataBlob + dataSize;
}


// Reads from the filesystem into memory.
HRESULT BinaryReader::ReadEntireFile(_In_z_ wchar_t const* fileName, _Inout_ std::unique_ptr<uint8_t[]>& data, _Out_ size_t* dataSize)
{
    // Open the file.
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif

    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    // Get the file size.
    LARGE_INTEGER fileSize = { 0 };

#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
    FILE_STANDARD_INFO fileInfo;

    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    fileSize = fileInfo.EndOfFile;
#else
    GetFileSizeEx(hFile.get(), &fileSize);
#endif

    // File is too big for 32-bit allocation, so reject read.
    if (fileSize.HighPart > 0)
        return E_FAIL;

    // Create enough space for the file data.
    data.reset(new uint8_t[fileSize.LowPart]);

    if (!data)
        return E_OUTOFMEMORY;

    // Read the data in.
    DWORD bytesRead = 0;

    if (!ReadFile(hFile.get(), data.get(), fileSize.LowPart, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesRead < fileSize.LowPart)
        return E_FAIL;

    *dataSize = bytesRead;
    
    return S_OK;
}
