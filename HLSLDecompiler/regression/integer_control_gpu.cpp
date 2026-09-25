#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <cstring>
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D failure " + std::to_string(hr)); }
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected generated HLSL path");
        std::ifstream file(argv[1]);
        if (!file) throw std::runtime_error("Cannot read HLSL");
        std::string source((std::istreambuf_iterator<char>(file)), {});
        // Keep the generated function body; invoke it from a one-thread compute
        // wrapper to inspect raw results without render-target float handling.
        source = std::regex_replace(source, std::regex("void main\\("), "void evaluate(");
        source = std::regex_replace(source, std::regex(": SV_POSITION0|: SV_Target0"), "");
        source += "\nRWStructuredBuffer<uint4> result : register(u0);\n"
                  "[numthreads(1,1,1)] void main() { float4 value; evaluate(0, value); result[0] = asuint(value); }\n";
        ComPtr<ID3DBlob> code, errors;
        HRESULT hr = D3DCompile(source.data(), source.size(), argv[1], nullptr, nullptr, "main", "cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
        if (FAILED(hr) && errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
        check(hr);
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &device, nullptr, &context));
        ComPtr<ID3D11ComputeShader> shader;
        check(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader));
        context->CSSetShader(shader.Get(), nullptr, 0);
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; bd.StructureByteStride = 16;
        ComPtr<ID3D11Buffer> output, staging;
        check(device->CreateBuffer(&bd, nullptr, &output));
        ComPtr<ID3D11UnorderedAccessView> uav;
        check(device->CreateUnorderedAccessView(output.Get(), nullptr, &uav));
        ID3D11UnorderedAccessView *views[] = {uav.Get()};
        context->CSSetUnorderedAccessViews(0, 1, views, nullptr);
        bd.Usage = D3D11_USAGE_STAGING; bd.BindFlags = 0; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        bd.MiscFlags = 0; bd.StructureByteStride = 0;
        check(device->CreateBuffer(&bd, nullptr, &staging));
        const UINT cases[][4] = {
            {0x0100000fu, 0x80000000u, 0xffffffffu, 0x7fc00001u},
            {0xffffffffu, 1u, 0x80000001u, 0u},
            {0x01000001u, 0u, 0x80000000u, 0u},
            {0x01000002u, 0xffffffffu, 0x7fffffffu, 0u}
        };
        for (const auto &input : cases) {
            ComPtr<ID3D11Texture2D> textures[2];
            ComPtr<ID3D11ShaderResourceView> srvs[2];
            for (unsigned i = 0; i < 2; ++i) {
                D3D11_TEXTURE2D_DESC td = {};
                td.Width = td.Height = td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
                td.Format = i ? DXGI_FORMAT_R32G32B32A32_SINT : DXGI_FORMAT_R32G32B32A32_UINT;
                td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA initial = {}; initial.pSysMem = input; initial.SysMemPitch = 16;
                check(device->CreateTexture2D(&td, &initial, &textures[i]));
                check(device->CreateShaderResourceView(textures[i].Get(), nullptr, &srvs[i]));
            }
            ID3D11ShaderResourceView *resources[] = {srvs[0].Get(), srvs[1].Get()};
            context->CSSetShaderResources(10, 2, resources);
            context->Dispatch(1, 1, 1);
            context->CopyResource(staging.Get(), output.Get());
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
            UINT actual[4]; std::memcpy(actual, mapped.pData, 16);
            context->Unmap(staging.Get(), 0);
            float branch = (input[0] & 15) == 1 ? 11.f : (input[0] & 15) == 15 ? 15.f : 99.f;
            float condition = input[1] ? 21.f : 22.f;
            UINT expected[4] = {0, 0, input[0], input[2]};
            std::memcpy(expected, &branch, 4); std::memcpy(expected + 1, &condition, 4);
            if (std::memcmp(actual, expected, 16)) throw std::runtime_error("Integer control/load bit mismatch");
        }
        std::cout << "PASS integer switch, raw if, scalar/vector UINT and scalar SINT Load (4 GPU cases)\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
