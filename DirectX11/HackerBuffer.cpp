#include "HackerBuffer.h"

#include "log.h"
#include "util.h"

// -----------------------------------------------------------------------------

HackerDeviceChild::HackerDeviceChild(ID3D11DeviceChild *pChild)
	: HackerUnknown(pChild)
{
	mOrigDeviceChild = pChild;
}


HackerDeviceChild::~HackerDeviceChild()
{
}

STDMETHODIMP_(void) HackerDeviceChild::GetDevice(
	/* [annotation] */
	_Out_  ID3D11Device **ppDevice)
{
	LogInfo("HackerDeviceChild::GetDevice(%s@%p) called \n", typeid(*this).name(), this);
	mOrigDeviceChild->GetDevice(ppDevice);
}

STDMETHODIMP HackerDeviceChild::GetPrivateData(
	/* [annotation] */
	_In_  REFGUID guid,
	/* [annotation] */
	_Inout_  UINT *pDataSize,
	/* [annotation] */
	_Out_writes_bytes_opt_(*pDataSize)  void *pData)
{
	LogInfo("HackerDeviceChild::GetPrivateData(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());

	HRESULT hr = mOrigDeviceChild->GetPrivateData(guid, pDataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDeviceChild::SetPrivateData(
	/* [annotation] */
	_In_  REFGUID guid,
	/* [annotation] */
	_In_  UINT DataSize,
	/* [annotation] */
	_In_reads_bytes_opt_(DataSize)  const void *pData)
{
	LogInfo("HackerDeviceChild::SetPrivateData(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());
	LogInfo("  DataSize = %d \n", DataSize);

	HRESULT hr = mOrigDeviceChild->SetPrivateData(guid, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDeviceChild::SetPrivateDataInterface(
	/* [annotation] */
	_In_  REFGUID guid,
	/* [annotation] */
	_In_opt_  const IUnknown *pData)
{
	LogInfo("HackerDeviceChild::SetPrivateDataInterface(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());

	HRESULT hr = mOrigDeviceChild->SetPrivateDataInterface(guid, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

HackerResource::HackerResource(ID3D11Resource *pResource)
	: HackerDeviceChild(pResource)
{
	mOrigResource = pResource;
}


HackerResource::~HackerResource()
{
}

ID3D11Resource* HackerResource::GetOrigResource()
{
	return mOrigResource;
}


STDMETHODIMP_(void) HackerResource::GetType(
	/* [annotation] */
	_Out_  D3D11_RESOURCE_DIMENSION *pResourceDimension)
{
	LogInfo("HackerResource::GetType(%s@%p) called \n", typeid(*this).name(), this);
	mOrigResource->GetType(pResourceDimension);
}

STDMETHODIMP_(void) HackerResource::SetEvictionPriority(
	/* [annotation] */
	_In_  UINT EvictionPriority)
{
	LogInfo("HackerResource::SetEvictionPriority(%s@%p) called \n", typeid(*this).name(), this);
	mOrigResource->SetEvictionPriority(EvictionPriority);
}

STDMETHODIMP_(UINT) HackerResource::GetEvictionPriority(void)
{
	LogInfo("HackerResource::GetEvictionPriority(%s@%p) called \n", typeid(*this).name(), this);
	UINT priority = mOrigResource->GetEvictionPriority();
	LogInfo("  returns priority = %d \n", priority);
	return priority;
}


// -----------------------------------------------------------------------------

// We are making an override of all 4 types, Texture1D, Texture2D, Texture3D,
// and Buffer, because those all descend from ID3D11Resource, and we need to
// be able to just use the Resource parent when necessary- like in SetShaderResource.

// -----------------------------------------------------------------------------

HackerBuffer::HackerBuffer(ID3D11Buffer *pBuffer)
	: HackerResource(pBuffer)
{
	mOrigBuffer = pBuffer;
}


HackerBuffer::~HackerBuffer()
{
}

STDMETHODIMP_(void) HackerBuffer::GetDesc(
	/* [annotation] */
	_Out_  D3D11_BUFFER_DESC *pDesc)
{
	LogInfo("HackerBuffer::GetDesc(%s@%p) called \n", typeid(*this).name(), this);

	mOrigBuffer->GetDesc(pDesc);
	
	if (pDesc)
	{
		LogInfo("  returns ByteWidth = %d \n", pDesc->ByteWidth);
		LogInfo("  returns Usage = %d \n", pDesc->Usage);
		LogInfo("  returns BindFlags = %x \n", pDesc->BindFlags);
		LogInfo("  returns CPUAccessFlags = %x \n", pDesc->CPUAccessFlags);
		LogInfo("  returns MiscFlags = %x \n", pDesc->MiscFlags);
		LogInfo("  returns StructureByteStride = %d \n", pDesc->StructureByteStride);
	}
}


// Need to override the QueryInterface for the HackerBuffer, so we can return
// our version, not the original.

HRESULT STDMETHODCALLTYPE HackerBuffer::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerBuffer::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigBuffer->QueryInterface(riid, ppvObject);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p \n", hr, ppvObject);
		return hr;
	}

	// No need for further checks of null ppvObject, as it could not have successfully
	// called the original in that case.

	if (riid == __uuidof(ID3D11Buffer))
	{
		*ppvObject = this;
		LogDebug("  return HackerDevice(%s@%p) wrapper of %p \n", typeid(*this).name(), this, mOrigBuffer);
	}

	LogDebug("  returns result = %x for %p \n", hr, *ppvObject);
	return hr;
}


// -----------------------------------------------------------------------------

HackerTexture1D::HackerTexture1D(ID3D11Texture1D *pTexture1D)
	: HackerResource(pTexture1D)
{
	mOrigTexture1D = pTexture1D;
}


HackerTexture1D::~HackerTexture1D()
{
}

STDMETHODIMP_(void) HackerTexture1D::GetDesc(
	/* [annotation] */
	_Out_  D3D11_TEXTURE1D_DESC *pDesc)
{
	LogInfo("HackerTexture1D::GetDesc(%s@%p) called \n", typeid(*this).name(), this);

	mOrigTexture1D->GetDesc(pDesc);

	//if (pDesc)
	//{
	//	LogInfo("  returns ByteWidth = %d \n", pDesc->ByteWidth);
	//}
}


// -----------------------------------------------------------------------------

HackerTexture2D::HackerTexture2D(ID3D11Texture2D *pTexture2D)
	: HackerResource(pTexture2D)
{
	mOrigTexture2D = pTexture2D;
}


HackerTexture2D::~HackerTexture2D()
{
}

STDMETHODIMP_(void) HackerTexture2D::GetDesc(
	/* [annotation] */
	_Out_  D3D11_TEXTURE2D_DESC *pDesc)
{
	LogInfo("HackerTexture2D::GetDesc(%s@%p) called \n", typeid(*this).name(), this);

	mOrigTexture2D->GetDesc(pDesc);

	//if (pDesc)
	//{
	//	LogInfo("  returns ByteWidth = %d \n", pDesc->ByteWidth);
	//}
}


// -----------------------------------------------------------------------------

HackerTexture3D::HackerTexture3D(ID3D11Texture3D *pTexture3D)
	: HackerResource(pTexture3D)
{
	mOrigTexture3D = pTexture3D;
}


HackerTexture3D::~HackerTexture3D()
{
}

STDMETHODIMP_(void) HackerTexture3D::GetDesc(
	/* [annotation] */
	_Out_  D3D11_TEXTURE3D_DESC *pDesc)
{
	LogInfo("HackerTexture3D::GetDesc(%s@%p) called \n", typeid(*this).name(), this);

	mOrigTexture3D->GetDesc(pDesc);

	//if (pDesc)
	//{
	//	LogInfo("  returns ByteWidth = %d \n", pDesc->ByteWidth);
	//}
}


