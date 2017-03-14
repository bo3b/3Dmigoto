//--------------------------------------------------------------------------------------
// File: EffectCommon.h
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

#include "Effects.h"
#include "PlatformHelpers.h"
#include "ConstantBuffer.h"
#include "SharedResourcePool.h"
#include "AlignedNew.h"


// BasicEffect, SkinnedEffect, et al, have many things in common, but also significant
// differences (for instance, not all the effects support lighting). This header breaks
// out common functionality into a set of helpers which can be assembled in different
// combinations to build up whatever subset is needed by each effect.


namespace DirectX
{
    // Bitfield tracks which derived parameter values need to be recomputed.
    namespace EffectDirtyFlags
    {
        const int ConstantBuffer        = 0x01;
        const int WorldViewProj         = 0x02;
        const int WorldInverseTranspose = 0x04;
        const int EyePosition           = 0x08;
        const int MaterialColor         = 0x10;
        const int FogVector             = 0x20;
        const int FogEnable             = 0x40;
        const int AlphaTest             = 0x80;
    }


    // Helper stores matrix parameter values, and computes derived matrices.
    struct EffectMatrices
    {
        EffectMatrices();

        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
        XMMATRIX worldView;

        void SetConstants(_Inout_ int& dirtyFlags, _Inout_ XMMATRIX& worldViewProjConstant);
    };


    // Helper stores the current fog settings, and computes derived shader parameters.
    struct EffectFog
    {
        EffectFog();

        bool enabled;
        float start;
        float end;

        void XM_CALLCONV SetConstants(_Inout_ int& dirtyFlags, _In_ FXMMATRIX worldView, _Inout_ XMVECTOR& fogVectorConstant);
    };


    // Helper stores material color settings, and computes derived parameters for shaders that do not support realtime lighting.
    struct EffectColor
    {
        EffectColor();

        XMVECTOR diffuseColor;
        float alpha;

        void SetConstants(_Inout_ int& dirtyFlags, _Inout_ XMVECTOR& diffuseColorConstant);
    };


    // Helper stores the current light settings, and computes derived shader parameters.
    struct EffectLights : public EffectColor
    {
        EffectLights();

        static const int MaxDirectionalLights = IEffectLights::MaxDirectionalLights;


        // Fields.
        XMVECTOR emissiveColor;
        XMVECTOR ambientLightColor;

        bool lightEnabled[MaxDirectionalLights];
        XMVECTOR lightDiffuseColor[MaxDirectionalLights];
        XMVECTOR lightSpecularColor[MaxDirectionalLights];


        // Methods.
        void InitializeConstants(_Out_ XMVECTOR& specularColorAndPowerConstant, _Out_writes_all_(MaxDirectionalLights) XMVECTOR* lightDirectionConstant, _Out_writes_all_(MaxDirectionalLights) XMVECTOR* lightDiffuseConstant, _Out_writes_all_(MaxDirectionalLights) XMVECTOR* lightSpecularConstant);
        void SetConstants(_Inout_ int& dirtyFlags, _In_ EffectMatrices const& matrices, _Inout_ XMMATRIX& worldConstant, _Inout_updates_(3) XMVECTOR worldInverseTransposeConstant[3], _Inout_ XMVECTOR& eyePositionConstant, _Inout_ XMVECTOR& diffuseColorConstant, _Inout_ XMVECTOR& emissiveColorConstant, bool lightingEnabled);

        int SetLightEnabled(int whichLight, bool value, _Inout_updates_(MaxDirectionalLights) XMVECTOR* lightDiffuseConstant, _Inout_updates_(MaxDirectionalLights) XMVECTOR* lightSpecularConstant);
        int XM_CALLCONV SetLightDiffuseColor(int whichLight, FXMVECTOR value, _Inout_updates_(MaxDirectionalLights) XMVECTOR* lightDiffuseConstant);
        int XM_CALLCONV SetLightSpecularColor(int whichLight, FXMVECTOR value, _Inout_updates_(MaxDirectionalLights) XMVECTOR* lightSpecularConstant);

        static void ValidateLightIndex(int whichLight);
        static void EnableDefaultLighting(_In_ IEffectLights* effect);
    };


    // Points to a precompiled vertex or pixel shader program.
    struct ShaderBytecode
    {
        void const* code;
        size_t length;
    };


    // Factory for lazily instantiating shaders. BasicEffect supports many different
    // shader permutations, so we only bother creating the ones that are actually used.
    class EffectDeviceResources
    {
    public:
        EffectDeviceResources(_In_ ID3D11Device* device)
          : mDevice(device)
        { }

        ID3D11VertexShader* DemandCreateVertexShader(_Inout_ Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertexShader, ShaderBytecode const& bytecode);
        ID3D11PixelShader * DemandCreatePixelShader (_Inout_ Microsoft::WRL::ComPtr<ID3D11PixelShader> & pixelShader,  ShaderBytecode const& bytecode);
        ID3D11ShaderResourceView* GetDefaultTexture();

    protected:
        Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mDefaultTexture;

        std::mutex mMutex;
    };


    // Templated base class provides functionality common to all the built-in effects.
    template<typename Traits>
    class EffectBase : public AlignedNew<typename Traits::ConstantBufferType>
    {
    public:
        // Constructor.
        EffectBase(_In_ ID3D11Device* device)
          : dirtyFlags(INT_MAX),
            mConstantBuffer(device),
            mDeviceResources(deviceResourcesPool.DemandCreate(device))
        {
            ZeroMemory(&constants, sizeof(constants));
        }


        // Fields.
        typename Traits::ConstantBufferType constants;

        EffectMatrices matrices;
        EffectFog fog;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;

        int dirtyFlags;


        // Helper looks up the bytecode for the specified vertex shader permutation.
        // Client code needs this in order to create matching input layouts.
        void GetVertexShaderBytecode(int permutation, _Out_ void const** pShaderByteCode, _Out_ size_t* pByteCodeLength)
        {
            int shaderIndex = VertexShaderIndices[permutation];

            ShaderBytecode const& bytecode = VertexShaderBytecode[shaderIndex];

            *pShaderByteCode = bytecode.code;
            *pByteCodeLength = bytecode.length;
        }


        // Helper sets our shaders and constant buffers onto the D3D device.
        void ApplyShaders(_In_ ID3D11DeviceContext* deviceContext, int permutation)
        {
            // Set shaders.
            auto vertexShader = mDeviceResources->GetVertexShader(permutation);
            auto pixelShader = mDeviceResources->GetPixelShader(permutation);

            deviceContext->VSSetShader(vertexShader, nullptr, 0);
            deviceContext->PSSetShader(pixelShader, nullptr, 0);

            // Make sure the constant buffer is up to date.
            if (dirtyFlags & EffectDirtyFlags::ConstantBuffer)
            {
                mConstantBuffer.SetData(deviceContext, constants);
     
                dirtyFlags &= ~EffectDirtyFlags::ConstantBuffer;
            }

            // Set the constant buffer.
            ID3D11Buffer* buffer = mConstantBuffer.GetBuffer();

            deviceContext->VSSetConstantBuffers(0, 1, &buffer);
            deviceContext->PSSetConstantBuffers(0, 1, &buffer);
        }


        // Helper returns the default texture.
        ID3D11ShaderResourceView* GetDefaultTexture() { return mDeviceResources->GetDefaultTexture(); }


    protected:
        // Static arrays hold all the precompiled shader permutations.
        static const ShaderBytecode VertexShaderBytecode[Traits::VertexShaderCount];
        static const ShaderBytecode PixelShaderBytecode[Traits::PixelShaderCount];

        static const int VertexShaderIndices[Traits::ShaderPermutationCount];
        static const int PixelShaderIndices[Traits::ShaderPermutationCount];

    private:
        // D3D constant buffer holds a copy of the same data as the public 'constants' field.
        ConstantBuffer<typename Traits::ConstantBufferType> mConstantBuffer;

        // Only one of these helpers is allocated per D3D device, even if there are multiple effect instances.
        class DeviceResources : protected EffectDeviceResources
        {
        public:
            DeviceResources(_In_ ID3D11Device* device)
              : EffectDeviceResources(device)
            { }

        
            // Gets or lazily creates the specified vertex shader permutation.
            ID3D11VertexShader* GetVertexShader(int permutation)
            {
                int shaderIndex = VertexShaderIndices[permutation];

                return DemandCreateVertexShader(mVertexShaders[shaderIndex], VertexShaderBytecode[shaderIndex]);
            }


            // Gets or lazily creates the specified pixel shader permutation.
            ID3D11PixelShader* GetPixelShader(int permutation)
            {
                int shaderIndex = PixelShaderIndices[permutation];

                return DemandCreatePixelShader(mPixelShaders[shaderIndex], PixelShaderBytecode[shaderIndex]);
            }


            // Gets or lazily creates the default texture
            ID3D11ShaderResourceView* GetDefaultTexture() { return EffectDeviceResources::GetDefaultTexture(); }


        private:
            Microsoft::WRL::ComPtr<ID3D11VertexShader> mVertexShaders[Traits::VertexShaderCount];
            Microsoft::WRL::ComPtr<ID3D11PixelShader> mPixelShaders[Traits::PixelShaderCount];
        };


        // Per-device resources.
        std::shared_ptr<DeviceResources> mDeviceResources;

        static SharedResourcePool<ID3D11Device*, DeviceResources> deviceResourcesPool;
    };
}
