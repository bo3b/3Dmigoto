#include "HackerUnknown.h"

#include "log.h"
#include "util.h"


// This is the base class for a couple of different "Hacker" style objects.
// This is the root of the normal DX11 tree, which implements the QueryInterface
// COM calls.
//
// We want to keep the object hierarchy as intact as possible, because it allows
// us to override methods at more optimal spots with less duplication.


HackerUnknown::HackerUnknown(IUnknown *pUnknown)
{
	mOrigUnknown = pUnknown;
}

// -----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) HackerUnknown::AddRef(THIS)
{
	ULONG ulRef = mOrigUnknown->AddRef();
	LogInfo("HackerUnknown::AddRef(%s@%p), counter=%d, this=%p \n", typeid(*this).name(), this, ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerUnknown::Release(THIS)
{
	ULONG ulRef = mOrigUnknown->Release();
	LogInfo("HackerUnknown::Release(%s@%p), counter=%d, this=%p \n", typeid(*this).name(), this, ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  counter=%d, this=%p, deleting self. \n", ulRef, this);

		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP HackerUnknown::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerUnknown::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());
	HRESULT hr = mOrigUnknown->QueryInterface(riid, ppvObject);
	LogDebug("  returns result = %x for %p \n", hr, ppvObject);
	return hr;
}


