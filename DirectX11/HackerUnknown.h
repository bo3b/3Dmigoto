#pragma once

#include "Unknwn.h"

// -----------------------------------------------------------------------------

class HackerUnknown : public IUnknown
{
private:
	IUnknown *mOrigUnknown;

public:
	HackerUnknown(IUnknown *pUnknown);


	STDMETHOD(QueryInterface)(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	STDMETHOD_(ULONG, AddRef)(THIS);

	STDMETHOD_(ULONG, Release)(THIS);
};

