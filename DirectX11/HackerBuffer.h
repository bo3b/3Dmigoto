#pragma once

#include <d3d11_1.h>

#include "HackerUnknown.h"


// -----------------------------------------------------------------------------

class HackerDeviceChild :
	public HackerUnknown
{
private:
	ID3D11DeviceChild *mOrigDeviceChild;

public:
	HackerDeviceChild(ID3D11DeviceChild *pChild);
	~HackerDeviceChild();

	virtual void STDMETHODCALLTYPE GetDevice(
		/* [annotation] */
		_Out_  ID3D11Device **ppDevice);

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_Inout_  UINT *pDataSize,
		/* [annotation] */
		_Out_writes_bytes_opt_(*pDataSize)  void *pData);

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_In_  UINT DataSize,
		/* [annotation] */
		_In_reads_bytes_opt_(DataSize)  const void *pData);

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_In_opt_  const IUnknown *pData);
};


// -----------------------------------------------------------------------------

class HackerResource :
	public HackerDeviceChild
{
private:
	ID3D11Resource *mOrigResource;

public:
	HackerResource(ID3D11Resource *pResource);
	~HackerResource();
	ID3D11Resource* GetOrigResource();

	virtual void STDMETHODCALLTYPE GetType(
		/* [annotation] */
		_Out_  D3D11_RESOURCE_DIMENSION *pResourceDimension);

	virtual void STDMETHODCALLTYPE SetEvictionPriority(
		/* [annotation] */
		_In_  UINT EvictionPriority);

	virtual UINT STDMETHODCALLTYPE GetEvictionPriority(void);
};


// -----------------------------------------------------------------------------

class HackerBuffer :
	public HackerResource
{
private:
	ID3D11Buffer *mOrigBuffer;

public:
	HackerBuffer(ID3D11Buffer *pBuffer);
	~HackerBuffer();

	HRESULT STDMETHODCALLTYPE HackerBuffer::QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;
		
	virtual void STDMETHODCALLTYPE GetDesc(
	/* [annotation] */
	_Out_  D3D11_BUFFER_DESC *pDesc);
};


// -----------------------------------------------------------------------------

class HackerTexture1D :
	public HackerResource
{
private:
	ID3D11Texture1D *mOrigTexture1D;

public:
	HackerTexture1D(ID3D11Texture1D *pTexture1D);
	~HackerTexture1D();

	virtual void STDMETHODCALLTYPE GetDesc(
		/* [annotation] */
		_Out_  D3D11_TEXTURE1D_DESC *pDesc);
};


// -----------------------------------------------------------------------------

class HackerTexture2D :
	public HackerResource
{
private:
	ID3D11Texture2D *mOrigTexture2D;

public:
	HackerTexture2D(ID3D11Texture2D *pTexture2D);
	~HackerTexture2D();

	virtual void STDMETHODCALLTYPE GetDesc(
		/* [annotation] */
		_Out_  D3D11_TEXTURE2D_DESC *pDesc);
};


// -----------------------------------------------------------------------------

class HackerTexture3D :
	public HackerResource
{
private:
	ID3D11Texture3D *mOrigTexture3D;

public:
	HackerTexture3D(ID3D11Texture3D *pTexture3D);
	~HackerTexture3D();

	virtual void STDMETHODCALLTYPE GetDesc(
		/* [annotation] */
		_Out_  D3D11_TEXTURE3D_DESC *pDesc);
};



