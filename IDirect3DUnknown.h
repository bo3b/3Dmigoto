// Base class for all wrapper objects. 
// Not sure, but apparently this is to allow IPC between the objects using QueryInterface.

class IDirect3DUnknown 
{
public:
	IUnknown*   m_pUnk;
	ULONG       m_ulRef;

	IDirect3DUnknown(IUnknown* pUnk)
	{
		m_pUnk = pUnk;
		m_ulRef = 1;
	}

	IDirect3DUnknown GetOrig()
	{
		return m_pUnk;
	}

	ULONG GetRefCount()
	{
		return m_ulRef;
	}


	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);

	STDMETHOD_(ULONG, AddRef)(THIS)
	{
		++m_ulRef;
		return m_pUnk->AddRef();
	}

	STDMETHOD_(ULONG, Release)(THIS)
	{
		--m_ulRef;
		ULONG ulRef = m_pUnk->Release();

		if (ulRef == 0)
		{
			delete this;
			return 0;
		}
		return ulRef;
	}
};

