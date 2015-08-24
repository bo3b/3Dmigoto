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
