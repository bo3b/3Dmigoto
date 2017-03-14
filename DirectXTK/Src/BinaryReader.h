//--------------------------------------------------------------------------------------
// File: BinaryReader.h
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

#pragma once

#include <memory>
#include <exception>
#include <type_traits>

#include "PlatformHelpers.h"


namespace DirectX
{
    // Helper for reading binary data, either from the filesystem a memory buffer.
    class BinaryReader
    {
    public:
        explicit BinaryReader(_In_z_ wchar_t const* fileName);
        BinaryReader(_In_reads_bytes_(dataSize) uint8_t const* dataBlob, size_t dataSize);

        
        // Reads a single value.
        template<typename T> T const& Read()
        {
            return *ReadArray<T>(1);
        }


        // Reads an array of values.
        template<typename T> T const* ReadArray(size_t elementCount)
        {
            static_assert(std::is_pod<T>::value, "Can only read plain-old-data types");

            uint8_t const* newPos = mPos + sizeof(T) * elementCount;

            if (newPos > mEnd)
                throw std::exception("End of file");

            auto result = reinterpret_cast<T const*>(mPos);

            mPos = newPos;

            return result;
        }


        // Lower level helper reads directly from the filesystem into memory.
        static HRESULT ReadEntireFile(_In_z_ wchar_t const* fileName, _Inout_ std::unique_ptr<uint8_t[]>& data, _Out_ size_t* dataSize);


    private:
        // The data currently being read.
        uint8_t const* mPos;
        uint8_t const* mEnd;

        std::unique_ptr<uint8_t[]> mOwnedData;


        // Prevent copying.
        BinaryReader(BinaryReader const&) DIRECTX_CTOR_DELETE
        BinaryReader& operator= (BinaryReader const&) DIRECTX_CTOR_DELETE
    };
}
