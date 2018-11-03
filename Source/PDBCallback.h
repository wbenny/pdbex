#pragma once

#include <dia2.h>       // IDia* interfaces

//////////////////////////////////////////////////////////////////////////
// PDBCallback
//

class PDBCallback : public IDiaLoadCallback2
{
    volatile unsigned long   m_RefCount = 0;

public:

    //IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return m_RefCount++;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        if ((--m_RefCount) == 0)
            delete this;
        return m_RefCount;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID rid, void **ppUnk) override
    {
        if (ppUnk == nullptr)
        {
            return E_INVALIDARG;
        }
        if (rid == __uuidof(IDiaLoadCallback2))
        {
            *ppUnk = (IDiaLoadCallback2 *)this;
        }
        else if (rid == __uuidof(IDiaLoadCallback))
        {
            *ppUnk = (IDiaLoadCallback *)this;
        }
        else if (rid == __uuidof(IUnknown))
        {
            *ppUnk = (IUnknown *)this;
        }
        else
        {
            *ppUnk = nullptr;
        }
        if (*ppUnk != nullptr)
        {
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE NotifyDebugDir(
        BOOL fExecutable,
        DWORD cbData,
        BYTE data[]) override // really a const struct _IMAGE_DEBUG_DIRECTORY *
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE NotifyOpenDBG(
        LPCOLESTR dbgPath,
        HRESULT resultCode) override
    {
        // wprintf(L"opening %s...\n", dbgPath);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE NotifyOpenPDB(
        LPCOLESTR pdbPath,
        HRESULT resultCode) override
    {
        // wprintf(L"opening %s...\n", pdbPath);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictRegistryAccess() override
    {
        // return hr != S_OK to prevent querying the registry for symbol search paths
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictSymbolServerAccess() override
    {
      // return hr != S_OK to prevent accessing a symbol server
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictOriginalPathAccess() override
    {
        // return hr != S_OK to prevent querying the registry for symbol search paths
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictReferencePathAccess() override
    {
        // return hr != S_OK to prevent accessing a symbol server
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictDBGAccess() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictSystemRootAccess() override
    {
        return S_OK;
    }
};