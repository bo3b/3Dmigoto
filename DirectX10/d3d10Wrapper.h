#pragma once

#include <D3D10.h>

class IDirect3DUnknown
{
protected:
    IUnknown*   m_pUnk;
    ULONG       m_ulRef;

public:
    IDirect3DUnknown(IUnknown* pUnk)
    {
        m_pUnk = pUnk;
        m_ulRef = 1;
    }

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);

    STDMETHOD_(ULONG,AddRef)(THIS)
    {
        m_pUnk->AddRef();
        return ++m_ulRef;
    }

    STDMETHOD_(ULONG,Release)(THIS)
    {
        m_pUnk->Release();

        ULONG ulRef = --m_ulRef;
        if( 0 == ulRef )
        {
            delete this;
            return 0;
        }
        return ulRef;
    }
};

#include "d3d10WrapperDevice.h"
