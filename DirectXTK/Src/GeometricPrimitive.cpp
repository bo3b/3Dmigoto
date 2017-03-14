//--------------------------------------------------------------------------------------
// File: GeometricPrimitive.cpp
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
#include "GeometricPrimitive.h"
#include "Effects.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "VertexTypes.h"
#include "SharedResourcePool.h"
#include "Bezier.h"
#include <vector>
#include <map>

using namespace DirectX;
using namespace Microsoft::WRL;


namespace
{
    static const float SQRT2 = 1.41421356237309504880f;
    static const float SQRT3 = 1.73205080756887729352f;
    static const float SQRT6 = 2.44948974278317809820f;


    void CheckIndexOverflow(size_t value)
    {
        // Use >=, not > comparison, because some D3D level 9_x hardware does not support 0xFFFF index values.
        if (value >= USHRT_MAX)
            throw std::exception("Index value out of range: cannot tesselate primitive so finely");
    }


    // Temporary collection types used when generating the geometry.
    typedef std::vector<VertexPositionNormalTexture> VertexCollection;
    
    
    class IndexCollection : public std::vector<uint16_t>
    {
    public:
        // Sanity check the range of 16 bit index values.
        void push_back(size_t value)
        {
            CheckIndexOverflow(value);
            vector::push_back((uint16_t)value);
        }
    };


    // Helper for flipping winding of geometric primitives for LH vs. RH coords
    static void ReverseWinding( IndexCollection& indices, VertexCollection& vertices )
    {
        assert( (indices.size() % 3) == 0 );
        for( auto it = indices.begin(); it != indices.end(); it += 3 )
        {
            std::swap( *it, *(it+2) );
        }

        for( auto it = vertices.begin(); it != vertices.end(); ++it )
        {
            it->textureCoordinate.x = ( 1.f - it->textureCoordinate.x );
        }
    }


    // Helper for creating a D3D vertex or index buffer.
    template<typename T>
    static void CreateBuffer(_In_ ID3D11Device* device, T const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer)
    {
        assert( pBuffer != 0 );

        D3D11_BUFFER_DESC bufferDesc = { 0 };

        bufferDesc.ByteWidth = (UINT)data.size() * sizeof(T::value_type);
        bufferDesc.BindFlags = bindFlags;
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA dataDesc = { 0 };

        dataDesc.pSysMem = &data.front();

        ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, &dataDesc, pBuffer)
        );

        SetDebugObjectName(*pBuffer, "DirectXTK:GeometricPrimitive");
    }


    // Helper for creating a D3D input layout.
    static void CreateInputLayout(_In_ ID3D11Device* device, IEffect* effect, _Outptr_ ID3D11InputLayout** pInputLayout)
    {
        assert( pInputLayout != 0 );

        void const* shaderByteCode;
        size_t byteCodeLength;

        effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        ThrowIfFailed(
            device->CreateInputLayout(VertexPositionNormalTexture::InputElements,
                                      VertexPositionNormalTexture::InputElementCount,
                                      shaderByteCode, byteCodeLength,
                                      pInputLayout)
        );

        SetDebugObjectName(*pInputLayout, "DirectXTK:GeometricPrimitive");
    }
}


// Internal GeometricPrimitive implementation class.
class GeometricPrimitive::Impl
{
public:
    void Initialize(_In_ ID3D11DeviceContext* deviceContext, VertexCollection& vertices, IndexCollection& indices, bool rhcoords );

    void XM_CALLCONV Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection, FXMVECTOR color, _In_opt_ ID3D11ShaderResourceView* texture, bool wireframe, _In_opt_ std::function<void()> setCustomState);

    void Draw(_In_ IEffect* effect, _In_ ID3D11InputLayout* inputLayout, bool alpha, bool wireframe, _In_opt_ std::function<void()> setCustomState);

    void CreateInputLayout(_In_ IEffect* effect, _Outptr_ ID3D11InputLayout** inputLayout);

private:
    ComPtr<ID3D11Buffer> mVertexBuffer;
    ComPtr<ID3D11Buffer> mIndexBuffer;

    UINT mIndexCount;

    // Only one of these helpers is allocated per D3D device context, even if there are multiple GeometricPrimitive instances.
    class SharedResources
    {
    public:
        SharedResources(_In_ ID3D11DeviceContext* deviceContext);

        void PrepareForRendering(bool alpha, bool wireframe);

        ComPtr<ID3D11DeviceContext> deviceContext;
        std::unique_ptr<BasicEffect> effect;

        ComPtr<ID3D11InputLayout> inputLayoutTextured;
        ComPtr<ID3D11InputLayout> inputLayoutUntextured;

        std::unique_ptr<CommonStates> stateObjects;
    };


    // Per-device-context data.
    std::shared_ptr<SharedResources> mResources;

    static SharedResourcePool<ID3D11DeviceContext*, SharedResources> sharedResourcesPool;
};


// Global pool of per-device-context GeometricPrimitive resources.
SharedResourcePool<ID3D11DeviceContext*, GeometricPrimitive::Impl::SharedResources> GeometricPrimitive::Impl::sharedResourcesPool;


// Per-device-context constructor.
GeometricPrimitive::Impl::SharedResources::SharedResources(_In_ ID3D11DeviceContext* deviceContext)
  : deviceContext(deviceContext)
{
    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    // Create the BasicEffect.
    effect.reset(new BasicEffect(device.Get()));

    effect->EnableDefaultLighting();

    // Create state objects.
    stateObjects.reset(new CommonStates(device.Get()));

    // Create input layouts.
    effect->SetTextureEnabled(true);
    ::CreateInputLayout(device.Get(), effect.get(), &inputLayoutTextured);

    effect->SetTextureEnabled(false);
    ::CreateInputLayout(device.Get(), effect.get(), &inputLayoutUntextured);
}


// Sets up D3D device state ready for drawing a primitive.
void GeometricPrimitive::Impl::SharedResources::PrepareForRendering(bool alpha, bool wireframe)
{
    // Set the blend and depth stencil state.
    ID3D11BlendState* blendState;
    ID3D11DepthStencilState* depthStencilState;

    if (alpha)
    {
        // Alpha blended rendering.
        blendState = stateObjects->AlphaBlend();
        depthStencilState = stateObjects->DepthRead();
    }
    else
    {
        // Opaque rendering.
        blendState = stateObjects->Opaque();
        depthStencilState = stateObjects->DepthDefault();
    }

    deviceContext->OMSetBlendState(blendState, nullptr, 0xFFFFFFFF);
    deviceContext->OMSetDepthStencilState(depthStencilState, 0);

    // Set the rasterizer state.
    if ( wireframe )
        deviceContext->RSSetState( stateObjects->Wireframe() );
    else
        deviceContext->RSSetState( stateObjects->CullCounterClockwise() );

    ID3D11SamplerState* samplerState = stateObjects->LinearClamp();
         
    deviceContext->PSSetSamplers(0, 1, &samplerState);
}


// Initializes a geometric primitive instance that will draw the specified vertex and index data.
_Use_decl_annotations_
void GeometricPrimitive::Impl::Initialize(ID3D11DeviceContext* deviceContext, VertexCollection& vertices, IndexCollection& indices, bool rhcoords)
{
    if ( vertices.size() >= USHRT_MAX )
        throw std::exception("Too many vertices for 16-bit index buffer");

    if ( !rhcoords )
        ReverseWinding( indices, vertices );

    mResources = sharedResourcesPool.DemandCreate(deviceContext);

    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    CreateBuffer(device.Get(), vertices, D3D11_BIND_VERTEX_BUFFER, &mVertexBuffer);
    CreateBuffer(device.Get(), indices, D3D11_BIND_INDEX_BUFFER, &mIndexBuffer);

    mIndexCount = static_cast<UINT>( indices.size() );
}


// Draws the primitive.
_Use_decl_annotations_
void XM_CALLCONV GeometricPrimitive::Impl::Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection, FXMVECTOR color,
                                                ID3D11ShaderResourceView* texture, bool wireframe, std::function<void()> setCustomState)
{
    assert( mResources != 0 );
    auto effect = mResources->effect.get();
    assert( effect != 0 );

    ID3D11InputLayout *inputLayout;
    if ( texture )
    {
        effect->SetTextureEnabled(true);
        effect->SetTexture(texture);

        inputLayout = mResources->inputLayoutTextured.Get();
    }
    else
    {
        effect->SetTextureEnabled(false);
        
        inputLayout = mResources->inputLayoutUntextured.Get();
    }

    float alpha = XMVectorGetW(color);

    // Set effect parameters.
    effect->SetWorld(world);
    effect->SetView(view);
    effect->SetProjection(projection);

    effect->SetDiffuseColor(color);
    effect->SetAlpha(alpha);

    Draw( effect, inputLayout, (alpha < 1.f), wireframe, setCustomState );
}


// Draw the primitive using a custom effect.
_Use_decl_annotations_
void GeometricPrimitive::Impl::Draw(IEffect* effect, ID3D11InputLayout* inputLayout, bool alpha, bool wireframe, std::function<void()> setCustomState )
{
    assert( mResources != 0 );
    auto deviceContext = mResources->deviceContext.Get();
    assert( deviceContext != 0 );

    // Set state objects.
    mResources->PrepareForRendering(alpha, wireframe);

    // Set input layout.
    assert( inputLayout != 0 );
    deviceContext->IASetInputLayout(inputLayout);

    // Activate our shaders, constant buffers, texture, etc.
    assert(effect != 0);
    effect->Apply(deviceContext);

    // Set the vertex and index buffer.
    auto vertexBuffer = mVertexBuffer.Get();
    UINT vertexStride = sizeof(VertexPositionNormalTexture);
    UINT vertexOffset = 0;

    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexStride, &vertexOffset);

    deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Hook lets the caller replace our shaders or state settings with whatever else they see fit.
    if (setCustomState)
    {
        setCustomState();
    }

    // Draw the primitive.
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->DrawIndexed(mIndexCount, 0, 0);
}


// Create input layout for drawing with a custom effect.
_Use_decl_annotations_
void GeometricPrimitive::Impl::CreateInputLayout( IEffect* effect, ID3D11InputLayout** inputLayout )
{
    assert( effect != 0 );
    assert( inputLayout != 0 );

    assert( mResources != 0 );
    auto deviceContext = mResources->deviceContext.Get();
    assert( deviceContext != 0 );

    ComPtr<ID3D11Device> device;
    deviceContext->GetDevice(&device);

    ::CreateInputLayout( device.Get(), effect, inputLayout );
}


//--------------------------------------------------------------------------------------
// GeometricPrimitive
//--------------------------------------------------------------------------------------

// Constructor.
GeometricPrimitive::GeometricPrimitive()
  : pImpl(new Impl())
{
}


// Destructor.
GeometricPrimitive::~GeometricPrimitive()
{
}


// Public entrypoints.
_Use_decl_annotations_
void XM_CALLCONV GeometricPrimitive::Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection, FXMVECTOR color, ID3D11ShaderResourceView* texture, bool wireframe, std::function<void()> setCustomState)
{
    pImpl->Draw(world, view, projection, color, texture, wireframe, setCustomState);
}


_Use_decl_annotations_
void GeometricPrimitive::Draw(IEffect* effect, ID3D11InputLayout* inputLayout, bool alpha, bool wireframe, std::function<void()> setCustomState )
{
    pImpl->Draw(effect, inputLayout, alpha, wireframe, setCustomState);
}


_Use_decl_annotations_
void GeometricPrimitive::CreateInputLayout(IEffect* effect, ID3D11InputLayout** inputLayout )
{
    pImpl->CreateInputLayout(effect, inputLayout);
}


//--------------------------------------------------------------------------------------
// Cube (aka a Hexahedron)
//--------------------------------------------------------------------------------------

// Creates a cube primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCube(_In_ ID3D11DeviceContext* deviceContext, float size, bool rhcoords)
{
    // A cube has six faces, each one pointing in a different direction.
    const int FaceCount = 6;

    static const XMVECTORF32 faceNormals[FaceCount] =
    {
        {  0,  0,  1 },
        {  0,  0, -1 },
        {  1,  0,  0 },
        { -1,  0,  0 },
        {  0,  1,  0 },
        {  0, -1,  0 },
    };

    static const XMVECTORF32 textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    VertexCollection vertices;
    IndexCollection indices;

    size /= 2;

    // Create each face in turn.
    for (int i = 0; i < FaceCount; i++)
    {
        XMVECTOR normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

        XMVECTOR side1 = XMVector3Cross(normal, basis);
        XMVECTOR side2 = XMVector3Cross(normal, side1);

        // Six indices (two triangles) per face.
        size_t vbase = vertices.size();
        indices.push_back(vbase + 0);
        indices.push_back(vbase + 1);
        indices.push_back(vbase + 2);

        indices.push_back(vbase + 0);
        indices.push_back(vbase + 2);
        indices.push_back(vbase + 3);

        // Four vertices per face.
        vertices.push_back(VertexPositionNormalTexture((normal - side1 - side2) * size, normal, textureCoordinates[0]));
        vertices.push_back(VertexPositionNormalTexture((normal - side1 + side2) * size, normal, textureCoordinates[1]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 + side2) * size, normal, textureCoordinates[2]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 - side2) * size, normal, textureCoordinates[3]));
    }

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}



//--------------------------------------------------------------------------------------
// Sphere
//--------------------------------------------------------------------------------------

// Creates a sphere primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateSphere(_In_ ID3D11DeviceContext* deviceContext, float diameter, size_t tessellation, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    size_t verticalSegments = tessellation;
    size_t horizontalSegments = tessellation * 2;

    float radius = diameter / 2;

    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        float v = 1 - (float)i / verticalSegments;

        float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
        float dy, dxz;

        XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            float u = (float)j / horizontalSegments;

            float longitude = j * XM_2PI / horizontalSegments;
            float dx, dz;

            XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            XMVECTOR normal = XMVectorSet(dx, dy, dz, 0);
            XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

            vertices.push_back(VertexPositionNormalTexture(normal * radius, normal, textureCoordinate));
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    size_t stride = horizontalSegments + 1;

    for (size_t i = 0; i < verticalSegments; i++)
    {
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            size_t nextI = i + 1;
            size_t nextJ = (j + 1) % stride;

            indices.push_back(i * stride + j);
            indices.push_back(nextI * stride + j);
            indices.push_back(i * stride + nextJ);

            indices.push_back(i * stride + nextJ);
            indices.push_back(nextI * stride + j);
            indices.push_back(nextI * stride + nextJ);
        }
    }

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Geodesic sphere
//--------------------------------------------------------------------------------------

// Creates a geosphere primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateGeoSphere(_In_ ID3D11DeviceContext* deviceContext, float diameter, size_t tessellation, bool rhcoords)
{
    // An undirected edge between two vertices, represented by a pair of indexes into a vertex array.
    // Becuse this edge is undirected, (a,b) is the same as (b,a).
    typedef std::pair<uint16_t, uint16_t> UndirectedEdge;

    // Makes an undirected edge. Rather than overloading comparison operators to give us the (a,b)==(b,a) property,
    // we'll just ensure that the larger of the two goes first. This'll simplify things greatly.
    auto makeUndirectedEdge = [](uint16_t a, uint16_t b)
    {
        return std::make_pair(std::max(a, b), std::min(a, b));
    };

    // Key: an edge
    // Value: the index of the vertex which lies midway between the two vertices pointed to by the key value
    // This map is used to avoid duplicating vertices when subdividing triangles along edges.
    typedef std::map<UndirectedEdge, uint16_t> EdgeSubdivisionMap;


    static const XMFLOAT3 OctahedronVertices[] =
    {
                              // when looking down the negative z-axis (into the screen)
        XMFLOAT3( 0,  1,  0), // 0 top
        XMFLOAT3( 0,  0, -1), // 1 front
        XMFLOAT3( 1,  0,  0), // 2 right
        XMFLOAT3( 0,  0,  1), // 3 back
        XMFLOAT3(-1,  0,  0), // 4 left
        XMFLOAT3( 0, -1,  0), // 5 bottom
    };
    static const uint16_t OctahedronIndices[] =
    {
        0, 1, 2, // top front-right face
        0, 2, 3, // top back-right face
        0, 3, 4, // top back-left face
        0, 4, 1, // top front-left face
        5, 1, 4, // bottom front-left face
        5, 4, 3, // bottom back-left face
        5, 3, 2, // bottom back-right face
        5, 2, 1, // bottom front-right face
    };

    const float radius = diameter / 2.0f;
    
    // Start with an octahedron; copy the data into the vertex/index collection.

    std::vector<XMFLOAT3> vertexPositions(std::begin(OctahedronVertices), std::end(OctahedronVertices));

    IndexCollection indices;
    indices.insert(indices.begin(), std::begin(OctahedronIndices), std::end(OctahedronIndices));

    // We know these values by looking at the above index list for the octahedron. Despite the subdivisions that are
    // about to go on, these values aren't ever going to change because the vertices don't move around in the array.
    // We'll need these values later on to fix the singularities that show up at the poles.
    const uint16_t northPoleIndex = 0;
    const uint16_t southPoleIndex = 5;
    
    for (size_t iSubdivision = 0; iSubdivision < tessellation; ++iSubdivision)
    {
        assert(indices.size() % 3 == 0); // sanity

        // We use this to keep track of which edges have already been subdivided.
        EdgeSubdivisionMap subdividedEdges;

        // The new index collection after subdivision.
        IndexCollection newIndices;

        const size_t triangleCount = indices.size() / 3;
        for (size_t iTriangle = 0; iTriangle < triangleCount; ++iTriangle)
        {
            // For each edge on this triangle, create a new vertex in the middle of that edge.
            // The winding order of the triangles we output are the same as the winding order of the inputs.

            // Indices of the vertices making up this triangle
            uint16_t iv0 = indices[iTriangle*3+0];
            uint16_t iv1 = indices[iTriangle*3+1];
            uint16_t iv2 = indices[iTriangle*3+2];
            
            // Get the new vertices
            XMFLOAT3 v01; // vertex on the midpoint of v0 and v1
            XMFLOAT3 v12; // ditto v1 and v2
            XMFLOAT3 v20; // ditto v2 and v0
            uint16_t iv01; // index of v01
            uint16_t iv12; // index of v12
            uint16_t iv20; // index of v20

            // Function that, when given the index of two vertices, creates a new vertex at the midpoint of those vertices.
            auto divideEdge = [&](uint16_t i0, uint16_t i1, XMFLOAT3& outVertex, uint16_t& outIndex)
            {
                const UndirectedEdge edge = makeUndirectedEdge(i0, i1);

                // Check to see if we've already generated this vertex
                auto it = subdividedEdges.find(edge);
                if (it != subdividedEdges.end())
                {
                    // We've already generated this vertex before
                    outIndex = it->second; // the index of this vertex
                    outVertex = vertexPositions[outIndex]; // and the vertex itself
                }
                else
                {
                    // Haven't generated this vertex before: so add it now

                    // outVertex = (vertices[i0] + vertices[i1]) / 2
                    XMStoreFloat3(
                        &outVertex,
                        XMVectorScale(
                            XMVectorAdd(XMLoadFloat3(&vertexPositions[i0]), XMLoadFloat3(&vertexPositions[i1])),
                            0.5f
                        )
                    );

                    outIndex = static_cast<uint16_t>( vertexPositions.size() );
                    CheckIndexOverflow(outIndex);
                    vertexPositions.push_back(outVertex);

                    // Now add it to the map.
                    subdividedEdges.insert(std::make_pair(edge, outIndex));
                }
            };

            // Add/get new vertices and their indices
            divideEdge(iv0, iv1, v01, iv01);
            divideEdge(iv1, iv2, v12, iv12);
            divideEdge(iv0, iv2, v20, iv20);

            // Add the new indices. We have four new triangles from our original one:
            //        v0
            //        o
            //       /a\
            //  v20 o---o v01
            //     /b\c/d\
            // v2 o---o---o v1
            //       v12
            const uint16_t indicesToAdd[] =
            {
                 iv0, iv01, iv20, // a
                iv20, iv12,  iv2, // b
                iv20, iv01, iv12, // c
                iv01,  iv1, iv12, // d
            };
            newIndices.insert(newIndices.end(), std::begin(indicesToAdd), std::end(indicesToAdd));
        }

        indices = std::move(newIndices);
    }

    // Now that we've completed subdivision, fill in the final vertex collection
    VertexCollection vertices;
    vertices.reserve(vertexPositions.size());
    for (auto it = vertexPositions.begin(); it != vertexPositions.end(); ++it)
    {
        auto vertexValue = *it;

        auto normal = XMVector3Normalize(XMLoadFloat3(&vertexValue));
        auto pos = XMVectorScale(normal, radius);

        XMFLOAT3 normalFloat3;
        XMStoreFloat3(&normalFloat3, normal);

        // calculate texture coordinates for this vertex
        float longitude = atan2(normalFloat3.x, -normalFloat3.z);
        float latitude = acos(normalFloat3.y);

        float u = longitude / XM_2PI + 0.5f;
        float v = latitude / XM_PI;

        auto texcoord = XMVectorSet(1.0f - u, v, 0.0f, 0.0f);
        vertices.push_back(VertexPositionNormalTexture(pos, normal, texcoord));
    }

    // There are a couple of fixes to do. One is a texture coordinate wraparound fixup. At some point, there will be
    // a set of triangles somewhere in the mesh with texture coordinates such that the wraparound across 0.0/1.0
    // occurs across that triangle. Eg. when the left hand side of the triangle has a U coordinate of 0.98 and the
    // right hand side has a U coordinate of 0.0. The intent is that such a triangle should render with a U of 0.98 to
    // 1.0, not 0.98 to 0.0. If we don't do this fixup, there will be a visible seam across one side of the sphere.
    // 
    // Luckily this is relatively easy to fix. There is a straight edge which runs down the prime meridian of the
    // completed sphere. If you imagine the vertices along that edge, they circumscribe a semicircular arc starting at
    // y=1 and ending at y=-1, and sweeping across the range of z=0 to z=1. x stays zero. It's along this edge that we
    // need to duplicate our vertices - and provide the correct texture coordinates.
    size_t preFixupVertexCount = vertices.size();
    for (size_t i = 0; i < preFixupVertexCount; ++i)
    {
        // This vertex is on the prime meridian if position.x and texcoord.u are both zero (allowing for small epsilon).
        bool isOnPrimeMeridian = XMVector2NearEqual(
            XMVectorSet(vertices[i].position.x, vertices[i].textureCoordinate.x, 0.0f, 0.0f),
            XMVectorZero(),
            XMVectorSplatEpsilon());

        if (isOnPrimeMeridian)
        {
            size_t newIndex = vertices.size(); // the index of this vertex that we're about to add
            CheckIndexOverflow(newIndex);

            // copy this vertex, correct the texture coordinate, and add the vertex
            VertexPositionNormalTexture v = vertices[i];
            v.textureCoordinate.x = 1.0f;
            vertices.push_back(v);

            // Now find all the triangles which contain this vertex and update them if necessary
            for (size_t j = 0; j < indices.size(); j += 3)
            {
                uint16_t* triIndex0 = &indices[j+0];
                uint16_t* triIndex1 = &indices[j+1];
                uint16_t* triIndex2 = &indices[j+2];

                if (*triIndex0 == i)
                {
                    // nothing; just keep going
                }
                else if (*triIndex1 == i)
                {
                    std::swap(triIndex0, triIndex1); // swap the pointers (not the values)
                }
                else if (*triIndex2 == i)
                {
                    std::swap(triIndex0, triIndex2); // swap the pointers (not the values)
                }
                else
                {
                    // this triangle doesn't use the vertex we're interested in
                    continue;
                }

                // If we got to this point then triIndex0 is the pointer to the index to the vertex we're looking at
                assert(*triIndex0 == i);
                assert(*triIndex1 != i && *triIndex2 != i); // assume no degenerate triangles
                
                const VertexPositionNormalTexture& v0 = vertices[*triIndex0];
                const VertexPositionNormalTexture& v1 = vertices[*triIndex1];
                const VertexPositionNormalTexture& v2 = vertices[*triIndex2];

                // check the other two vertices to see if we might need to fix this triangle

                if (abs(v0.textureCoordinate.x - v1.textureCoordinate.x) > 0.5f ||
                    abs(v0.textureCoordinate.x - v2.textureCoordinate.x) > 0.5f)
                {
                    // yep; replace the specified index to point to the new, corrected vertex
                    *triIndex0 = static_cast<uint16_t>(newIndex);
                }
            }
        }
    }

    // And one last fix we need to do: the poles. A common use-case of a sphere mesh is to map a rectangular texture onto
    // it. If that happens, then the poles become singularities which map the entire top and bottom rows of the texture
    // onto a single point. In general there's no real way to do that right. But to match the behavior of non-geodesic
    // spheres, we need to duplicate the pole vertex for every triangle that uses it. This will introduce seams near the
    // poles, but reduce stretching.
    auto fixPole = [&](size_t poleIndex)
    {
        auto poleVertex = vertices[poleIndex];
        bool overwrittenPoleVertex = false; // overwriting the original pole vertex saves us one vertex

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            // These pointers point to the three indices which make up this triangle. pPoleIndex is the pointer to the
            // entry in the index array which represents the pole index, and the other two pointers point to the other
            // two indices making up this triangle.
            uint16_t* pPoleIndex;
            uint16_t* pOtherIndex0;
            uint16_t* pOtherIndex1;
            if (indices[i + 0] == poleIndex)
            {
                pPoleIndex = &indices[i + 0];
                pOtherIndex0 = &indices[i + 1];
                pOtherIndex1 = &indices[i + 2];
            }
            else if (indices[i + 1] == poleIndex)
            {
                pPoleIndex = &indices[i + 1];
                pOtherIndex0 = &indices[i + 2];
                pOtherIndex1 = &indices[i + 0];
            }
            else if (indices[i + 2] == poleIndex)
            {
                pPoleIndex = &indices[i + 2];
                pOtherIndex0 = &indices[i + 0];
                pOtherIndex1 = &indices[i + 1];
            }
            else
            {
                continue;
            }

            const auto& otherVertex0 = vertices[*pOtherIndex0];
            const auto& otherVertex1 = vertices[*pOtherIndex1];

            // Calculate the texcoords for the new pole vertex, add it to the vertices and update the index
            VertexPositionNormalTexture newPoleVertex = poleVertex;
            newPoleVertex.textureCoordinate.x = (otherVertex0.textureCoordinate.x + otherVertex1.textureCoordinate.x) / 2;
            newPoleVertex.textureCoordinate.y = poleVertex.textureCoordinate.y;

            if (!overwrittenPoleVertex)
            {
                vertices[poleIndex] = newPoleVertex;
                overwrittenPoleVertex = true;
            }
            else
            {
                CheckIndexOverflow(vertices.size());

                *pPoleIndex = static_cast<uint16_t>(vertices.size());
                vertices.push_back(newPoleVertex);
            }
        }
    };

    fixPole(northPoleIndex);
    fixPole(southPoleIndex);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);
    return primitive;
}


//--------------------------------------------------------------------------------------
// Cylinder / Cone
//--------------------------------------------------------------------------------------

// Helper computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
static inline XMVECTOR GetCircleVector(size_t i, size_t tessellation)
{
    float angle = i * XM_2PI / tessellation;
    float dx, dz;

    XMScalarSinCos(&dx, &dz, angle);

    XMVECTORF32 v = { dx, 0, dz, 0 };
    return v;
}

static inline XMVECTOR GetCircleTangent(size_t i, size_t tessellation)
{
    float angle = ( i * XM_2PI / tessellation ) + XM_PIDIV2;
    float dx, dz;

    XMScalarSinCos(&dx, &dz, angle);

    XMVECTORF32 v = { dx, 0, dz, 0 };
    return v;
}


// Helper creates a triangle fan to close the end of a cylinder / cone
static void CreateCylinderCap(VertexCollection& vertices, IndexCollection& indices, size_t tessellation, float height, float radius, bool isTop)
{
    // Create cap indices.
    for (size_t i = 0; i < tessellation - 2; i++)
    {
        size_t i1 = (i + 1) % tessellation;
        size_t i2 = (i + 2) % tessellation;

        if (isTop)
        {
            std::swap(i1, i2);
        }

        size_t vbase = vertices.size();
        indices.push_back(vbase);
        indices.push_back(vbase + i1);
        indices.push_back(vbase + i2);
    }

    // Which end of the cylinder is this?
    XMVECTOR normal = g_XMIdentityR1;
    XMVECTOR textureScale = g_XMNegativeOneHalf;

    if (!isTop)
    {
        normal = -normal;
        textureScale *= g_XMNegateX;
    }

    // Create cap vertices.
    for (size_t i = 0; i < tessellation; i++)
    {
        XMVECTOR circleVector = GetCircleVector(i, tessellation);

        XMVECTOR position = (circleVector * radius) + (normal * height);

        XMVECTOR textureCoordinate = XMVectorMultiplyAdd(XMVectorSwizzle<0, 2, 3, 3>(circleVector), textureScale, g_XMOneHalf);

        vertices.push_back(VertexPositionNormalTexture(position, normal, textureCoordinate));
    }
}


// Creates a cylinder primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCylinder(_In_ ID3D11DeviceContext* deviceContext, float height, float diameter, size_t tessellation, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    height /= 2;

    XMVECTOR topOffset = g_XMIdentityR1 * height;

    float radius = diameter / 2;
    size_t stride = tessellation + 1;

    // Create a ring of triangles around the outside of the cylinder.
    for (size_t i = 0; i <= tessellation; i++)
    {
        XMVECTOR normal = GetCircleVector(i, tessellation);

        XMVECTOR sideOffset = normal * radius;

        float u = (float)i / tessellation;

        XMVECTOR textureCoordinate = XMLoadFloat(&u);

        vertices.push_back(VertexPositionNormalTexture(sideOffset + topOffset, normal, textureCoordinate));
        vertices.push_back(VertexPositionNormalTexture(sideOffset - topOffset, normal, textureCoordinate + g_XMIdentityR1));

        indices.push_back(i * 2);
        indices.push_back((i * 2 + 2) % (stride * 2));
        indices.push_back(i * 2 + 1);

        indices.push_back(i * 2 + 1);
        indices.push_back((i * 2 + 2) % (stride * 2));
        indices.push_back((i * 2 + 3) % (stride * 2));
    }

    // Create flat triangle fan caps to seal the top and bottom.
    CreateCylinderCap(vertices, indices, tessellation, height, radius, true);
    CreateCylinderCap(vertices, indices, tessellation, height, radius, false);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}


// Creates a cone primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateCone(_In_ ID3D11DeviceContext* deviceContext, float diameter, float height, size_t tessellation, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    height /= 2;

    XMVECTOR topOffset = g_XMIdentityR1 * height;

    float radius = diameter / 2;
    size_t stride = tessellation + 1;

    // Create a ring of triangles around the outside of the cone.
    for (size_t i = 0; i <= tessellation; i++)
    {
        XMVECTOR circlevec = GetCircleVector(i, tessellation);

        XMVECTOR sideOffset = circlevec * radius;

        float u = (float)i / tessellation;

        XMVECTOR textureCoordinate = XMLoadFloat(&u);

        XMVECTOR pt = sideOffset - topOffset;

        XMVECTOR normal = XMVector3Cross( GetCircleTangent( i, tessellation ), topOffset - pt );
        normal = XMVector3Normalize( normal );

        // Duplicate the top vertex for distinct normals
        vertices.push_back(VertexPositionNormalTexture(topOffset, normal, g_XMZero));
        vertices.push_back(VertexPositionNormalTexture(pt, normal, textureCoordinate + g_XMIdentityR1 ));

        indices.push_back(i * 2);
        indices.push_back((i * 2 + 3) % (stride * 2));
        indices.push_back((i * 2 + 1) % (stride * 2));
    }

    // Create flat triangle fan caps to seal the bottom.
    CreateCylinderCap(vertices, indices, tessellation, height, radius, false);

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Torus
//--------------------------------------------------------------------------------------

// Creates a torus primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTorus(_In_ ID3D11DeviceContext* deviceContext, float diameter, float thickness, size_t tessellation, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    size_t stride = tessellation + 1;

    // First we loop around the main ring of the torus.
    for (size_t i = 0; i <= tessellation; i++)
    {
        float u = (float)i / tessellation;

        float outerAngle = i * XM_2PI / tessellation - XM_PIDIV2;

        // Create a transform matrix that will align geometry to
        // slice perpendicularly though the current ring position.
        XMMATRIX transform = XMMatrixTranslation(diameter / 2, 0, 0) * XMMatrixRotationY(outerAngle);

        // Now we loop along the other axis, around the side of the tube.
        for (size_t j = 0; j <= tessellation; j++)
        {
            float v = 1 - (float)j / tessellation;

            float innerAngle = j * XM_2PI / tessellation + XM_PI;
            float dx, dy;

            XMScalarSinCos(&dy, &dx, innerAngle);

            // Create a vertex.
            XMVECTOR normal = XMVectorSet(dx, dy, 0, 0);
            XMVECTOR position = normal * thickness / 2;
            XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

            position = XMVector3Transform(position, transform);
            normal = XMVector3TransformNormal(normal, transform);

            vertices.push_back(VertexPositionNormalTexture(position, normal, textureCoordinate));

            // And create indices for two triangles.
            size_t nextI = (i + 1) % stride;
            size_t nextJ = (j + 1) % stride;

            indices.push_back(i * stride + j);
            indices.push_back(i * stride + nextJ);
            indices.push_back(nextI * stride + j);

            indices.push_back(i * stride + nextJ);
            indices.push_back(nextI * stride + nextJ);
            indices.push_back(nextI * stride + j);
        }
    }

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Tetrahedron
//--------------------------------------------------------------------------------------

std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTetrahedron(_In_ ID3D11DeviceContext* deviceContext, float size, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    static const XMVECTORF32 verts[4] =
    {
        {            0.f,        0.f,      1.f },
        {  2.f*SQRT2/3.f,        0.f, -1.f/3.f },
        {     -SQRT2/3.f,  SQRT6/3.f, -1.f/3.f },
        {     -SQRT2/3.f, -SQRT6/3.f, -1.f/3.f }
    };

    static const uint32_t faces[4*3] = 
    {
        0, 1, 2,
        0, 2, 3,
        0, 3, 1,
        1, 3, 2,
    };

    for( size_t j = 0; j < _countof(faces); j += 3 )
    {
        uint32_t v0 = faces[ j ];
        uint32_t v1 = faces[ j + 1 ];
        uint32_t v2 = faces[ j + 2 ];

        XMVECTOR normal = XMVector3Cross( verts[ v1 ].v - verts[ v0 ].v,
                                          verts[ v2 ].v - verts[ v0 ].v );
        normal = XMVector3Normalize( normal );

        size_t base = vertices.size();
        indices.push_back( base );
        indices.push_back( base + 1 );
        indices.push_back( base + 2 );
 
        // Duplicate vertices to use face normals
        XMVECTOR position = XMVectorScale( verts[ v0 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMZero /* 0, 0 */ ) );

        position = XMVectorScale( verts[ v1 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR0 /* 1, 0 */ ) ); 

        position = XMVectorScale( verts[ v2 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR1 /* 0, 1 */ ) ); 
    }

    assert( vertices.size() == 4*3 );
    assert( indices.size() == 4*3 );

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, !rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Octahedron
//--------------------------------------------------------------------------------------

std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateOctahedron(_In_ ID3D11DeviceContext* deviceContext, float size, bool rhcoords )
{
    VertexCollection vertices;
    IndexCollection indices;

    static const XMVECTORF32 verts[6] =
    {
        {  1,  0,  0 },
        { -1,  0,  0 },
        {  0,  1,  0 },
        {  0, -1,  0 },
        {  0,  0,  1 },
        {  0,  0, -1 }
    };

    static const uint32_t faces[8*3] = 
    {
        4, 0, 2, 
        4, 2, 1,
        4, 1, 3,
        4, 3, 0,
        5, 2, 0,
        5, 1, 2,
        5, 3, 1,
        5, 0, 3
    };

    for( size_t j = 0; j < _countof(faces); j += 3 )
    {
        uint32_t v0 = faces[ j ];
        uint32_t v1 = faces[ j + 1 ];
        uint32_t v2 = faces[ j + 2 ];

        XMVECTOR normal = XMVector3Cross( verts[ v1 ].v - verts[ v0 ].v,
                                          verts[ v2 ].v - verts[ v0 ].v );
        normal = XMVector3Normalize( normal );

        size_t base = vertices.size();
        indices.push_back( base );
        indices.push_back( base + 1 );
        indices.push_back( base + 2 );
 
        // Duplicate vertices to use face normals
        XMVECTOR position = XMVectorScale( verts[ v0 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMZero /* 0, 0 */ ) );

        position = XMVectorScale( verts[ v1 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR0 /* 1, 0 */ ) ); 

        position = XMVectorScale( verts[ v2 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR1 /* 0, 1*/ ) ); 
    }

    assert( vertices.size() == 8*3 );
    assert( indices.size() == 8*3 );

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, !rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Dodecahedron
//--------------------------------------------------------------------------------------

std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateDodecahedron(_In_ ID3D11DeviceContext* deviceContext, float size, bool rhcoords )
{
    VertexCollection vertices;
    IndexCollection indices;

    static const float a = 1.f/SQRT3;
    static const float b = 0.356822089773089931942f; // sqrt( ( 3 - sqrt(5) ) / 6 )
    static const float c = 0.934172358962715696451f; // sqrt( ( 3 + sqrt(5) ) / 6 );

    static const XMVECTORF32 verts[20] =
    {
        {  a,  a,  a },
        {  a,  a, -a },
        {  a, -a,  a },
        {  a, -a, -a },
        { -a,  a,  a },
        { -a,  a, -a },
        { -a, -a,  a },
        { -a, -a, -a },
        {  b,  c,  0 },
        { -b,  c,  0 },
        {  b, -c,  0 },
        { -b, -c,  0 },
        {  c,  0,  b },
        {  c,  0, -b },
        { -c,  0,  b },
        { -c,  0, -b },
        {  0,  b,  c },
        {  0, -b,  c },
        {  0,  b, -c },
        {  0, -b, -c }
    };

    static const uint32_t faces[12*5] = 
    {
        0, 8, 9, 4, 16,
        0, 16, 17, 2, 12,
        12, 2, 10, 3, 13,
        9, 5, 15, 14, 4,
        3, 19, 18, 1, 13,
        7, 11, 6, 14, 15,
        0, 12, 13, 1, 8,
        8, 1, 18, 5, 9,
        16, 4, 14, 6, 17,
        6, 11, 10, 2, 17,
        7, 15, 5, 18, 19,
        7, 19, 3, 10, 11,
    };

    static const XMVECTORF32 textureCoordinates[5] =
    {
        {  0.654508f, 0.0244717f },
        { 0.0954915f,  0.206107f },
        { 0.0954915f,  0.793893f },
        {  0.654508f,  0.975528f },
        {        1.f,       0.5f }
    };

    static const uint32_t textureIndex[12][5] =
    {
        { 0, 1, 2, 3, 4 },
        { 2, 3, 4, 0, 1 },
        { 4, 0, 1, 2, 3 },
        { 1, 2, 3, 4, 0 },
        { 2, 3, 4, 0, 1 },
        { 0, 1, 2, 3, 4 },
        { 1, 2, 3, 4, 0 },
        { 4, 0, 1, 2, 3 },
        { 4, 0, 1, 2, 3 },
        { 1, 2, 3, 4, 0 },
        { 0, 1, 2, 3, 4 },
        { 2, 3, 4, 0, 1 },
    };

    size_t t = 0;
    for( size_t j = 0; j < _countof(faces); j += 5, ++t )
    {
        uint32_t v0 = faces[ j ];
        uint32_t v1 = faces[ j + 1 ];
        uint32_t v2 = faces[ j + 2 ];
        uint32_t v3 = faces[ j + 3 ];
        uint32_t v4 = faces[ j + 4 ];

        XMVECTOR normal = XMVector3Cross( verts[ v1 ].v - verts[ v0 ].v,
                                          verts[ v2 ].v - verts[ v0 ].v );
        normal = XMVector3Normalize( normal );

        size_t base = vertices.size();

        indices.push_back( base );
        indices.push_back( base + 1 );
        indices.push_back( base + 2 );

        indices.push_back( base );
        indices.push_back( base + 2 );
        indices.push_back( base + 3 );

        indices.push_back( base );
        indices.push_back( base + 3 );
        indices.push_back( base + 4 );

        // Duplicate vertices to use face normals
        XMVECTOR position = XMVectorScale( verts[ v0 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, textureCoordinates[ textureIndex[t][0] ] ) );

        position = XMVectorScale( verts[ v1 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, textureCoordinates[ textureIndex[t][1] ] ) ); 

        position = XMVectorScale( verts[ v2 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, textureCoordinates[ textureIndex[t][2] ] ) ); 

        position = XMVectorScale( verts[ v3 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, textureCoordinates[ textureIndex[t][3] ] ) ); 

        position = XMVectorScale( verts[ v4 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, textureCoordinates[ textureIndex[t][4] ] ) ); 
    }

    assert( vertices.size() == 12*5 );
    assert( indices.size() == 12*3*3 );

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, !rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Icosahedron
//--------------------------------------------------------------------------------------

std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateIcosahedron(_In_ ID3D11DeviceContext* deviceContext, float size, bool rhcoords )
{
    VertexCollection vertices;
    IndexCollection indices;

    static const float  t = 1.618033988749894848205f; // (1 + sqrt(5)) / 2
    static const float t2 = 1.519544995837552493271f; // sqrt( 1 + sqr( (1 + sqrt(5)) / 2 ) )

    static const XMVECTORF32 verts[12] =
    {
        {    t/t2,  1.f/t2,       0 },
        {   -t/t2,  1.f/t2,       0 },
        {    t/t2, -1.f/t2,       0 },
        {   -t/t2, -1.f/t2,       0 },
        {  1.f/t2,       0,    t/t2 },
        {  1.f/t2,       0,   -t/t2 },
        { -1.f/t2,       0,    t/t2 },
        { -1.f/t2,       0,   -t/t2 },
        {       0,    t/t2,  1.f/t2 },
        {       0,   -t/t2,  1.f/t2 },
        {       0,    t/t2, -1.f/t2 },
        {       0,   -t/t2, -1.f/t2 }
    };
 
    static const uint32_t faces[20*3] = 
    {
        0, 8, 4,
        0, 5, 10,
        2, 4, 9,
        2, 11, 5,
        1, 6, 8,
        1, 10, 7,
        3, 9, 6,
        3, 7, 11,
        0, 10, 8,
        1, 8, 10,
        2, 9, 11,
        3, 11, 9,
        4, 2, 0,
        5, 0, 2,
        6, 1, 3,
        7, 3, 1,
        8, 6, 4,
        9, 4, 6,
        10, 5, 7,
        11, 7, 5
    };

    for( size_t j = 0; j < _countof(faces); j += 3 )
    {
        uint32_t v0 = faces[ j ];
        uint32_t v1 = faces[ j + 1 ];
        uint32_t v2 = faces[ j + 2 ];

        XMVECTOR normal = XMVector3Cross( verts[ v1 ].v - verts[ v0 ].v,
                                          verts[ v2 ].v - verts[ v0 ].v );
        normal = XMVector3Normalize( normal );

        size_t base = vertices.size();
        indices.push_back( base );
        indices.push_back( base + 1 );
        indices.push_back( base + 2 );
 
        // Duplicate vertices to use face normals
        XMVECTOR position = XMVectorScale( verts[ v0 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMZero /* 0, 0 */ ) );

        position = XMVectorScale( verts[ v1 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR0 /* 1, 0 */ ) ); 

        position = XMVectorScale( verts[ v2 ], size );
        vertices.push_back( VertexPositionNormalTexture( position, normal, g_XMIdentityR1 /* 0, 1 */ ) ); 
    }

    assert( vertices.size() == 20*3 );
    assert( indices.size() == 20*3 );

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, !rhcoords);

    return primitive;
}


//--------------------------------------------------------------------------------------
// Teapot
//--------------------------------------------------------------------------------------

// Include the teapot control point data.
namespace
{
    #include "TeapotData.inc"
}


// Tessellates the specified bezier patch.
static void XM_CALLCONV TessellatePatch(VertexCollection& vertices, IndexCollection& indices, TeapotPatch const& patch, size_t tessellation, FXMVECTOR scale, bool isMirrored)
{
    // Look up the 16 control points for this patch.
    XMVECTOR controlPoints[16];

    for (int i = 0; i < 16; i++)
    {
        controlPoints[i] = TeapotControlPoints[patch.indices[i]] * scale;
    }

    // Create the index data.
    size_t vbase = vertices.size();
    Bezier::CreatePatchIndices(tessellation, isMirrored, [&](size_t index)
    {
        indices.push_back(vbase + index);
    });

    // Create the vertex data.
    Bezier::CreatePatchVertices(controlPoints, tessellation, isMirrored, [&](FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
    {
        vertices.push_back(VertexPositionNormalTexture(position, normal, textureCoordinate));
    });
}

        
// Creates a teapot primitive.
std::unique_ptr<GeometricPrimitive> GeometricPrimitive::CreateTeapot(_In_ ID3D11DeviceContext* deviceContext, float size, size_t tessellation, bool rhcoords)
{
    VertexCollection vertices;
    IndexCollection indices;

    if (tessellation < 1)
        throw std::out_of_range("tesselation parameter out of range");

    XMVECTOR scaleVector = XMVectorReplicate(size);

    XMVECTOR scaleNegateX = scaleVector * g_XMNegateX;
    XMVECTOR scaleNegateZ = scaleVector * g_XMNegateZ;
    XMVECTOR scaleNegateXZ = scaleVector * g_XMNegateX * g_XMNegateZ;

    for (int i = 0; i < sizeof(TeapotPatches) / sizeof(TeapotPatches[0]); i++)
    {
        TeapotPatch const& patch = TeapotPatches[i];

        // Because the teapot is symmetrical from left to right, we only store
        // data for one side, then tessellate each patch twice, mirroring in X.
        TessellatePatch(vertices, indices, patch, tessellation, scaleVector, false);
        TessellatePatch(vertices, indices, patch, tessellation, scaleNegateX, true);

        if (patch.mirrorZ)
        {
            // Some parts of the teapot (the body, lid, and rim, but not the
            // handle or spout) are also symmetrical from front to back, so
            // we tessellate them four times, mirroring in Z as well as X.
            TessellatePatch(vertices, indices, patch, tessellation, scaleNegateZ, true);
            TessellatePatch(vertices, indices, patch, tessellation, scaleNegateXZ, false);
        }
    }

    // Create the primitive object.
    std::unique_ptr<GeometricPrimitive> primitive(new GeometricPrimitive());
    
    primitive->pImpl->Initialize(deviceContext, vertices, indices, rhcoords);

    return primitive;
}
