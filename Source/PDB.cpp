#include "PDB.h"
#include "PDBCallback.h"

#include <dia2.h>       // IDia* interfaces
#include <atlcomcli.h>

#include <cassert>

//
// For string converting:
// std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter
//
// Ref: http://stackoverflow.com/a/18597384
//
#include <locale>
#include <codecvt>
#include <string>

namespace
{
	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter;
}

//////////////////////////////////////////////////////////////////////////
// SymbolModuleBase
//

class SymbolModuleBase
{
	public:
		SymbolModuleBase();

		BOOL
		Open(
			IN const CHAR* Path
			);

		VOID
		Close();

		BOOL
		IsOpened() const;

	protected:
		IDiaDataSource* m_DataSource;
		IDiaSession*    m_Session;
		IDiaSymbol*     m_GlobalSymbol;
};

//////////////////////////////////////////////////////////////////////////
// SymbolModuleBase - implementation
//

SymbolModuleBase::SymbolModuleBase()
	: m_DataSource(nullptr)
	, m_Session(nullptr)
	, m_GlobalSymbol(nullptr)
{
	HRESULT hr = CoInitialize(NULL);

	assert(hr == S_OK);
}

BOOL
SymbolModuleBase::Open(
	IN const CHAR* Path
	)
{
	//
	// Obtain access to the provider
	//

	HRESULT HResult             = S_OK;
    auto    PDBSearchPath       = L"Srv*.\\Symbols*https://msdl.microsoft.com/download/symbols";
    char    FileExt[MAX_PATH]   = { 0 };

	HResult = CoCreateInstance(
		__uuidof(DiaSource),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IDiaDataSource),
		(void**)&m_DataSource
		);

    // Retry with direct export call
    if (HResult == REGDB_E_CLASSNOTREG)
    {
        HMODULE HMod = LoadLibraryW(L"msdia140.dll");
        if (!HMod)
        {
            HResult = HRESULT_FROM_WIN32(GetLastError());
            return FALSE;
        }

        auto DllGetClassObject = reinterpret_cast<BOOL(WINAPI*)(REFCLSID, REFIID, LPVOID)>(GetProcAddress(HMod, "DllGetClassObject"));
        if (!DllGetClassObject)
        {
            HResult = HRESULT_FROM_WIN32(GetLastError());
            return FALSE;
        }

        CComPtr<IClassFactory> ClassFactory;
        HResult = DllGetClassObject(__uuidof(DiaSource), __uuidof(IClassFactory), &ClassFactory);
        if (FAILED(HResult))
        {
            return FALSE;
        }

        HResult = ClassFactory->CreateInstance(nullptr, __uuidof(IDiaDataSource), (void**)&m_DataSource);
    }

	if (FAILED(HResult))
	{
		return FALSE;
	}

    _splitpath_s(Path, nullptr, 0, nullptr, 0, nullptr, 0, FileExt, _countof(FileExt));
    if (0 == _stricmp(FileExt, ".pdb"))
    {
        HResult = m_DataSource->loadDataFromPdb(
            string_converter.from_bytes(Path).c_str()
        );
    }
    else
    {
        PDBCallback Callback;
        Callback.AddRef();

        HResult = m_DataSource->loadDataForExe(
            string_converter.from_bytes(Path).c_str(), PDBSearchPath, &Callback);
    }

	if (FAILED(HResult))
	{
		Close();
		return FALSE;
	}

	HResult = m_DataSource->openSession(&m_Session);

	if (FAILED(HResult))
	{
		Close();
		return FALSE;
	}

	HResult = m_Session->get_globalScope(&m_GlobalSymbol);

	if (FAILED(HResult))
	{
		Close();
		return FALSE;
	}

	return TRUE;
}

VOID
SymbolModuleBase::Close()
{
	if (m_GlobalSymbol != nullptr)
	{
		m_GlobalSymbol->Release();
		m_GlobalSymbol = nullptr;
	}

	if (m_Session != nullptr)
	{
		m_Session->Release();
		m_Session = nullptr;
	}

	if (m_DataSource != nullptr)
	{
		m_DataSource->Release();
		m_DataSource = nullptr;
	}

	CoUninitialize();
}

BOOL
SymbolModuleBase::IsOpened() const
{
	return m_DataSource && m_Session && m_GlobalSymbol;
}

//////////////////////////////////////////////////////////////////////////
// SymbolModule
//

class SymbolModule
	: public SymbolModuleBase
{
	public:
		SymbolModule();

		~SymbolModule();

		BOOL
		Open(
			IN const CHAR* Path
			);

		BOOL
		IsOpened() const;

		const CHAR*
		GetPath() const;

		VOID
		Close();

		DWORD
		GetMachineType() const;

		CV_CFL_LANG
		GetLanguage() const;

		SYMBOL*
		GetSymbolByName(
			IN const CHAR* SymbolName
			);

		SYMBOL*
		GetSymbolByTypeId(
			IN DWORD TypeId
			);

		SYMBOL*
		GetSymbol(
			IN IDiaSymbol* DiaSymbol
			);

		CHAR*
		GetSymbolName(
			IN IDiaSymbol* DiaSymbol
			);

		VOID
		BuildSymbolMapFromEnumerator(
			IN IDiaEnumSymbols* DiaSymbolEnumerator
			);

		VOID
		BuildFunctionSetFromEnumerator(
			IN IDiaEnumSymbols* DiaSymbolEnumerator
			);

		VOID
		BuildSymbolMap();

		const SymbolMap&
		GetSymbolMap() const;

		const SymbolNameMap&
		GetSymbolNameMap() const;

		const FunctionSet&
		GetFunctionSet() const;

	private:
		VOID
		InitSymbol(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		DestroySymbol(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolBase(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolEnum(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolTypedef(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolPointer(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolArray(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolFunction(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolFunctionArg(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolUdt(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

	private:
		std::string   m_Path;
		SymbolMap     m_SymbolMap;
		SymbolNameMap m_SymbolNameMap;
		SymbolSet     m_SymbolSet;
		FunctionSet   m_FunctionSet;

		DWORD         m_MachineType;
		CV_CFL_LANG   m_Language;
};

SymbolModule::SymbolModule()
{

}

SymbolModule::~SymbolModule()
{
	Close();
}

BOOL
SymbolModule::Open(
	IN const CHAR* Path
	)
{
	BOOL Result;

	Result = SymbolModuleBase::Open(Path);

	if (Result == FALSE)
	{
		return FALSE;
	}

	m_GlobalSymbol->get_machineType(&m_MachineType);

	DWORD Language;
	m_GlobalSymbol->get_language(&Language);
	m_Language = static_cast<CV_CFL_LANG>(Language);

	BuildSymbolMap();

	return TRUE;
}

BOOL
SymbolModule::IsOpened() const
{
	return SymbolModuleBase::IsOpened();
}

const CHAR*
SymbolModule::GetPath() const
{
	return m_Path.c_str();
}

VOID
SymbolModule::Close()
{
	SymbolModuleBase::Close();

	for (auto&& Symbol : m_SymbolSet)
	{
		DestroySymbol(Symbol);
		delete Symbol;
	}

	m_Path.clear();
	m_SymbolMap.clear();
	m_SymbolNameMap.clear();
	m_SymbolSet.clear();
}

DWORD
SymbolModule::GetMachineType() const
{
	return m_MachineType;
}

CV_CFL_LANG
SymbolModule::GetLanguage() const
{
	return m_Language;
}

CHAR*
SymbolModule::GetSymbolName(
	IN IDiaSymbol* DiaSymbol
	)
{
	BSTR SymbolNameBstr;

	if (DiaSymbol->get_name(&SymbolNameBstr) != S_OK)
	{
		//
		// Not all symbols have the name.
		//

		return nullptr;
	}

	//
	// BSTR is essentially a wide char string.
	// Since we work in multibyte character set,
	// we need to convert it.
	//

	CHAR*  SymbolNameMb;
	size_t SymbolNameLength;

	SymbolNameLength = SysStringLen(SymbolNameBstr) + 1;
	SymbolNameMb = new CHAR[SymbolNameLength];
	wcstombs(SymbolNameMb, SymbolNameBstr, SymbolNameLength);

	//
	// BSTR is supposed to be freed by this call.
	//

	SysFreeString(SymbolNameBstr);

	return SymbolNameMb;
}

SYMBOL*
SymbolModule::GetSymbolByName(
	IN const CHAR* SymbolName
	)
{
	auto it = m_SymbolNameMap.find(SymbolName);
	return it == m_SymbolNameMap.end() ? nullptr : it->second;
}

SYMBOL*
SymbolModule::GetSymbolByTypeId(
	IN DWORD TypeId
	)
{
	auto it = m_SymbolMap.find(TypeId);
	return it == m_SymbolMap.end() ? nullptr : it->second;
}

SYMBOL*
SymbolModule::GetSymbol(
	IN IDiaSymbol* DiaSymbol
	)
{
	DWORD TypeId;
	DiaSymbol->get_symIndexId(&TypeId);

	auto it = m_SymbolMap.find(TypeId);

	if (it != m_SymbolMap.end())
	{
		return it->second;
	}

	SYMBOL* Symbol;
	Symbol = new SYMBOL;
	m_SymbolMap[TypeId] = Symbol;
	m_SymbolSet.insert(Symbol);

	InitSymbol(DiaSymbol, Symbol);

	if (Symbol->Name)
	{
		m_SymbolNameMap[Symbol->Name] = Symbol;
	}

	return Symbol;
}

VOID
SymbolModule::BuildSymbolMapFromEnumerator(
	IN IDiaEnumSymbols* DiaSymbolEnumerator
	)
{
	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		GetSymbol(DiaChildSymbol);

		DiaChildSymbol->Release();
	}
}

VOID
SymbolModule::BuildFunctionSetFromEnumerator(
	IN IDiaEnumSymbols* DiaSymbolEnumerator
	)
{
	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		BOOL IsFunction;
		DiaChildSymbol->get_function(&IsFunction);

		if (IsFunction)
		{
			CHAR* FunctionName = GetSymbolName(DiaChildSymbol);

			DWORD DwordResult;
			DiaChildSymbol->get_symTag(&DwordResult);
			auto Tag = static_cast<enum SymTagEnum>(DwordResult);

			m_FunctionSet.insert(std::string(FunctionName));
			delete[] FunctionName;
		}

		DiaChildSymbol->Release();
	}
}

VOID
SymbolModule::BuildSymbolMap()
{
	IDiaEnumSymbols* DiaSymbolEnumerator;

	if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagPublicSymbol, NULL, nsNone, &DiaSymbolEnumerator)))
	    BuildFunctionSetFromEnumerator(DiaSymbolEnumerator);

    if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagEnum, NULL, nsNone, &DiaSymbolEnumerator)))
	    BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);

    if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagUDT, NULL, nsNone, &DiaSymbolEnumerator)))
	    BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);

	if (DiaSymbolEnumerator) 
        DiaSymbolEnumerator->Release();
}

const SymbolMap&
SymbolModule::GetSymbolMap() const
{
	return m_SymbolMap;
}

const SymbolNameMap&
SymbolModule::GetSymbolNameMap() const
{
	return m_SymbolNameMap;
}

const FunctionSet&
SymbolModule::GetFunctionSet() const
{
	return m_FunctionSet;
}

VOID
SymbolModule::InitSymbol(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	DWORD DwordResult;
	ULONGLONG UlonglongResult;
	BOOL BoolResult;

	DiaSymbol->get_symTag(&DwordResult);
	Symbol->Tag = static_cast<enum SymTagEnum>(DwordResult);

	DiaSymbol->get_baseType(&DwordResult);
	Symbol->BaseType = static_cast<BasicType>(DwordResult);

	DiaSymbol->get_typeId(&DwordResult);
	Symbol->TypeId = DwordResult;

	DiaSymbol->get_length(&UlonglongResult);
	Symbol->Size = static_cast<DWORD>(UlonglongResult);

	DiaSymbol->get_constType(&BoolResult);
	Symbol->IsConst = static_cast<BOOL>(BoolResult);

	DiaSymbol->get_volatileType(&BoolResult);
	Symbol->IsVolatile = static_cast<BOOL>(BoolResult);

	Symbol->Name = GetSymbolName(DiaSymbol);

	switch (Symbol->Tag)
	{
		case SymTagUDT:             ProcessSymbolUdt        (DiaSymbol, Symbol); break;
		case SymTagEnum:            ProcessSymbolEnum       (DiaSymbol, Symbol); break;
		case SymTagFunctionType:    ProcessSymbolFunction   (DiaSymbol, Symbol); break;
		case SymTagPointerType:     ProcessSymbolPointer    (DiaSymbol, Symbol); break;
		case SymTagArrayType:       ProcessSymbolArray      (DiaSymbol, Symbol); break;
		case SymTagBaseType:        ProcessSymbolBase       (DiaSymbol, Symbol); break;
		case SymTagTypedef:         ProcessSymbolTypedef    (DiaSymbol, Symbol); break;
		case SymTagFunctionArgType: ProcessSymbolFunctionArg(DiaSymbol, Symbol); break;
		default:                                                                 break;
	}
}

VOID
SymbolModule::ProcessSymbolBase(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{

}

VOID
SymbolModule::ProcessSymbolEnum(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	IDiaEnumSymbols* DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, NULL, nsNone, &DiaSymbolEnumerator)))
	{
		return;
	}

	LONG ChildCount;
	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Enum.FieldCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Enum.Fields = new SYMBOL_ENUM_FIELD[ChildCount];

	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	for (DWORD Index = 0;
	     SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1);
	     Index++)
	{
		SYMBOL_ENUM_FIELD* EnumValue = &Symbol->u.Enum.Fields[Index];

		EnumValue->Parent = Symbol;

		EnumValue->Name = GetSymbolName(DiaChildSymbol);

		VariantInit(&EnumValue->Value);
		DiaChildSymbol->get_value(&EnumValue->Value);

		DiaChildSymbol->Release();
	}

	DiaSymbolEnumerator->Release();
}

VOID
SymbolModule::ProcessSymbolTypedef(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	IDiaSymbol* DiaTypedefSymbol;

	DiaSymbol->get_type(&DiaTypedefSymbol);

	Symbol->u.Typedef.Type = GetSymbol(DiaTypedefSymbol);

	DiaTypedefSymbol->Release();
}

VOID
SymbolModule::ProcessSymbolPointer(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	IDiaSymbol* DiaPointerSymbol;

	DiaSymbol->get_type(&DiaPointerSymbol);
	DiaSymbol->get_reference(&Symbol->u.Pointer.IsReference);

	Symbol->u.Pointer.Type = GetSymbol(DiaPointerSymbol);

	DiaPointerSymbol->Release();

	if (m_MachineType == 0)
	{

		//
		// Sometimes the Machine type is not stored in the PDB.
		// If this is our case, try to guess the machine type
		// by pointer size.
		//

		switch (Symbol->Size)
		{
			case 4:  m_MachineType = IMAGE_FILE_MACHINE_I386;  break;
			case 8:  m_MachineType = IMAGE_FILE_MACHINE_AMD64; break;
			default: m_MachineType = 0; break;
		}
	}
}

VOID
SymbolModule::ProcessSymbolArray(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	IDiaSymbol* DiaDataTypeSymbol;

	DiaSymbol->get_type(&DiaDataTypeSymbol);
	Symbol->u.Array.ElementType = GetSymbol(DiaDataTypeSymbol);

	DiaSymbol->get_count(&Symbol->u.Array.ElementCount);

	DiaDataTypeSymbol->Release();
}

VOID
SymbolModule::ProcessSymbolFunction(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	//
	// Calling convention.
	//

	DWORD CallingConvention;
	DiaSymbol->get_callingConvention(&CallingConvention);

	Symbol->u.Function.CallingConvention = static_cast<CV_call_e>(CallingConvention);

	//
	// Return type.
	//

	IDiaSymbol* DiaReturnTypeSymbol;
	DiaSymbol->get_type(&DiaReturnTypeSymbol);
	Symbol->u.Function.ReturnType = GetSymbol(DiaReturnTypeSymbol);

	DiaReturnTypeSymbol->Release();

	//
	// Arguments.
	//

	IDiaEnumSymbols* DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, NULL, nsNone, &DiaSymbolEnumerator)))
	{
		return;
	}

	LONG ChildCount;

	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Function.ArgumentCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Function.Arguments = new SYMBOL*[ChildCount];

	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	for (DWORD Index = 0;
	     SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1);
	     Index++)
	{
		SYMBOL* Argument;
		Argument = GetSymbol(DiaChildSymbol);
		Symbol->u.Function.Arguments[Index] = Argument;

		DiaChildSymbol->Release();
	}

	DiaSymbolEnumerator->Release();
}

VOID
SymbolModule::ProcessSymbolFunctionArg(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	IDiaSymbol* DiaArgumentTypeSymbol;

	DiaSymbol->get_type(&DiaArgumentTypeSymbol);
	Symbol->u.FunctionArg.Type = GetSymbol(DiaArgumentTypeSymbol);

	DiaArgumentTypeSymbol->Release();
}

VOID
SymbolModule::ProcessSymbolUdt(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	DWORD Kind;
	DiaSymbol->get_udtKind(&Kind);
	Symbol->u.Udt.Kind = static_cast<UdtKind>(Kind);

	IDiaEnumSymbols* DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagData, NULL, nsNone, &DiaSymbolEnumerator)))
	{
		return;
	}

	LONG ChildCount;

	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Udt.FieldCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Udt.Fields = new SYMBOL_UDT_FIELD[ChildCount + 1];

	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	for (DWORD Index = 0;
	     SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1);
	     Index++)
	{
		SYMBOL_UDT_FIELD* Member = &Symbol->u.Udt.Fields[Index];

		Member->Name = GetSymbolName(DiaChildSymbol);
		Member->Parent = Symbol;

		LONG Offset = 0;
		DiaChildSymbol->get_offset(&Offset);
		Member->Offset = static_cast<DWORD>(Offset);

		ULONGLONG Bits = 0;
		DiaChildSymbol->get_length(&Bits);
		Member->Bits = static_cast<DWORD>(Bits);

		DiaChildSymbol->get_bitPosition(&Member->BitPosition);

		IDiaSymbol* MemberTypeDiaSymbol;
		DiaChildSymbol->get_type(&MemberTypeDiaSymbol);
		Member->Type = GetSymbol(MemberTypeDiaSymbol);

		MemberTypeDiaSymbol->Release();

		DiaChildSymbol->Release();
	}

	DiaSymbolEnumerator->Release();

	//
	// Padding.
	//
	if (Symbol->u.Udt.Kind == UdtStruct && Symbol->u.Udt.FieldCount > 0 && Symbol->u.Udt.Fields[Symbol->u.Udt.FieldCount - 1].Type != nullptr)
	{
		SYMBOL_UDT_FIELD* LastUdtField = &Symbol->u.Udt.Fields[Symbol->u.Udt.FieldCount - 1];
		SYMBOL_UDT_FIELD* PaddingUdtField = &Symbol->u.Udt.Fields[Symbol->u.Udt.FieldCount];
		DWORD PaddingSize = Symbol->Size - (LastUdtField->Offset + LastUdtField->Type->Size);

		if (PaddingSize > 0)
		{
			SYMBOL* PaddingSymbolArrayElement = new SYMBOL;
			PaddingSymbolArrayElement->Tag = SymTagBaseType;
			PaddingSymbolArrayElement->BaseType = !(PaddingSize % 4) ? btLong : btChar;
			PaddingSymbolArrayElement->TypeId = 0;
			PaddingSymbolArrayElement->Size = PaddingSymbolArrayElement->BaseType == btLong ? 4 : 1;
			PaddingSymbolArrayElement->IsConst = FALSE;
			PaddingSymbolArrayElement->IsVolatile = FALSE;
			PaddingSymbolArrayElement->Name = nullptr;


			SYMBOL* PaddingSymbolArray = new SYMBOL;
			PaddingSymbolArray->Tag = SymTagArrayType;
			PaddingSymbolArray->BaseType = btNoType;
			PaddingSymbolArray->TypeId = 0;
			PaddingSymbolArray->Size = PaddingSize;
			PaddingSymbolArray->IsConst = FALSE;
			PaddingSymbolArray->IsVolatile = FALSE;
			PaddingSymbolArray->Name = nullptr;
			PaddingSymbolArray->u.Array.ElementType = PaddingSymbolArrayElement;
			PaddingSymbolArray->u.Array.ElementCount = PaddingSymbolArrayElement->BaseType == btLong ? PaddingSize / 4 : PaddingSize;

			PaddingUdtField->Name = new CHAR[64];
			PaddingUdtField->Type = PaddingSymbolArray;
			PaddingUdtField->Offset = LastUdtField->Offset + LastUdtField->Type->Size;

			PaddingUdtField->Bits = 0;
			PaddingUdtField->BitPosition = 0;
			PaddingUdtField->Parent = Symbol;

			strcpy(PaddingUdtField->Name, "__PADDING__");

			Symbol->u.Udt.FieldCount++;

			m_SymbolSet.insert(PaddingSymbolArray);
			m_SymbolSet.insert(PaddingSymbolArrayElement);
		}
	}
}

void SymbolModule::DestroySymbol(
	IN SYMBOL* Symbol
	)
{
	delete[] Symbol->Name;

	switch (Symbol->Tag)
	{
		case SymTagUDT:
			for (DWORD i = 0; i < Symbol->u.Udt.FieldCount; i++)
			{
				delete[] Symbol->u.Udt.Fields[i].Name;
			}

			delete[] Symbol->u.Udt.Fields;
			break;

		case SymTagEnum:
			for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; i++)
			{
				delete[] Symbol->u.Enum.Fields[i].Name;
			}

			delete[] Symbol->u.Enum.Fields;
			break;

		case SymTagFunctionType:
			delete[] Symbol->u.Function.Arguments;
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
// PDB - implementation
//

struct BasicTypeMapElement
{
	BasicType   BaseType;
	DWORD       Length;
	const CHAR* BasicTypeString;
	const CHAR* TypeString;
};

BasicTypeMapElement BasicTypeMapMSVC[] = {
	{ btNoType,       0,  "btNoType",         nullptr            },
	{ btVoid,         0,  "btVoid",           "void"             },
	{ btChar,         1,  "btChar",           "char"             },
	{ btWChar,        2,  "btWChar",          "wchar_t"          },
	{ btInt,          1,  "btInt",            "char"             },
	{ btInt,          2,  "btInt",            "short"            },
	{ btInt,          4,  "btInt",            "int"              },
	{ btInt,          8,  "btInt",            "__int64"          },
	{ btUInt,         1,  "btUInt",           "unsigned char"    },
	{ btUInt,         2,  "btUInt",           "unsigned short"   },
	{ btUInt,         4,  "btUInt",           "unsigned int"     },
	{ btUInt,         8,  "btUInt",           "unsigned __int64" },
	{ btFloat,        4,  "btFloat",          "float"            },
	{ btFloat,        8,  "btFloat",          "double"           },
	{ btFloat,       10,  "btFloat",          "long double"      }, // 80-bit float
	{ btBCD,          0,  "btBCD",            "BCD"              },
	{ btBool,         0,  "btBool",           "BOOL"             },
	{ btLong,         4,  "btLong",           "long"             },
	{ btULong,        4,  "btULong",          "unsigned long"    },
	{ btCurrency,     0,  "btCurrency",       nullptr            },
	{ btDate,         0,  "btDate",           "DATE"             },
	{ btVariant,      0,  "btVariant",        "VARIANT"          },
	{ btComplex,      0,  "btComplex",        nullptr            },
	{ btBit,          0,  "btBit",            nullptr            },
	{ btBSTR,         0,  "btBSTR",           "BSTR"             },
	{ btHresult,      4,  "btHresult",        "HRESULT"          },
	{ (BasicType)0,   0,  nullptr,            nullptr            },
};

BasicTypeMapElement BasicTypeMapStdInt[] = {
	{ btNoType,       0,  "btNoType",         nullptr            },
	{ btVoid,         0,  "btVoid",           "void"             },
	{ btChar,         1,  "btChar",           "char"             },
	{ btWChar,        2,  "btWChar",          "wchar_t"          },
	{ btInt,          1,  "btInt",            "int8_t"           },
	{ btInt,          2,  "btInt",            "int16_t"          },
	{ btInt,          4,  "btInt",            "int32_t"          },
	{ btInt,          8,  "btInt",            "int64_t"          },
	{ btUInt,         1,  "btUInt",           "uint8_t"          },
	{ btUInt,         2,  "btUInt",           "uint16_t"         },
	{ btUInt,         4,  "btUInt",           "uint32_t"         },
	{ btUInt,         8,  "btUInt",           "uint64_t"         },
	{ btFloat,        4,  "btFloat",          "float"            },
	{ btFloat,        8,  "btFloat",          "double"           },
	{ btFloat,       10,  "btFloat",          "long double"      }, // 80-bit float
	{ btBCD,          0,  "btBCD",            "BCD"              },
	{ btBool,         0,  "btBool",           "BOOL"             },
	{ btLong,         4,  "btLong",           "int32_t"          },
	{ btULong,        4,  "btULong",          "uint32_t"         },
	{ btCurrency,     0,  "btCurrency",       nullptr            },
	{ btDate,         0,  "btDate",           "DATE"             },
	{ btVariant,      0,  "btVariant",        "VARIANT"          },
	{ btComplex,      0,  "btComplex",        nullptr            },
	{ btBit,          0,  "btBit",            nullptr            },
	{ btBSTR,         0,  "btBSTR",           "BSTR"             },
	{ btHresult,      4,  "btHresult",        "HRESULT"          },
	{ (BasicType)0,   0,  nullptr,            nullptr            },
};

PDB::PDB()
{
	m_Impl = new SymbolModule();
}

PDB::PDB(
	IN const CHAR* Path
	)
{
	m_Impl = new SymbolModule();
	m_Impl->Open(Path);
}

PDB::~PDB()
{
	delete m_Impl;
}

BOOL
PDB::Open(
	IN const CHAR* Path
	)
{
	return m_Impl->Open(Path);
}

BOOL
PDB::IsOpened() const
{
	return m_Impl->IsOpened();
}

const CHAR*
PDB::GetPath() const
{
	return m_Impl->GetPath();
}

VOID
PDB::Close()
{
	m_Impl->Close();
}

DWORD
PDB::GetMachineType() const
{
	return m_Impl->GetMachineType();
}

CV_CFL_LANG
PDB::GetLanguage() const
{
	return m_Impl->GetLanguage();
}

const SYMBOL*
PDB::GetSymbolByName(
	IN const CHAR* SymbolName
	)
{
	return m_Impl->GetSymbolByName(SymbolName);
}

const SYMBOL*
PDB::GetSymbolByTypeId(
	IN DWORD TypeId
	)
{
	return m_Impl->GetSymbolByTypeId(TypeId);
}

const SymbolMap&
PDB::GetSymbolMap() const
{
	return m_Impl->GetSymbolMap();
}

const SymbolNameMap&
PDB::GetSymbolNameMap() const
{
	return m_Impl->GetSymbolNameMap();
}

const FunctionSet&
PDB::GetFunctionSet() const
{
	return m_Impl->GetFunctionSet();
}

const CHAR*
PDB::GetBasicTypeString(
	IN BasicType BaseType,
	IN DWORD Size,
	IN BOOL UseStdInt
	)
{
	BasicTypeMapElement* TypeMap = UseStdInt ? BasicTypeMapStdInt : BasicTypeMapMSVC;

	for (int n = 0; TypeMap[n].BasicTypeString != nullptr; n++)
	{
		if (TypeMap[n].BaseType == BaseType)
		{
			if (TypeMap[n].Length == Size ||
			    TypeMap[n].Length == 0)
			{
				return TypeMap[n].TypeString;
			}
		}
	}

	return nullptr;
}

const CHAR*
PDB::GetBasicTypeString(
	IN const SYMBOL* Symbol,
	IN BOOL UseStdInt
	)
{
	return GetBasicTypeString(Symbol->BaseType, Symbol->Size, UseStdInt);
}

const CHAR*
PDB::GetUdtKindString(
	IN UdtKind Kind
	)
{
	static const CHAR* UdtKindStrings[] = {
		"struct",
		"class",
		"union",
	};

	if (Kind >= UdtStruct && Kind <= UdtUnion)
	{
		return UdtKindStrings[Kind];
	}

	return nullptr;
}

BOOL
PDB::IsUnnamedSymbol(
	const SYMBOL* Symbol
	)
{
	return strstr(Symbol->Name, "<unnamed-") != nullptr ||
	       strstr(Symbol->Name, "__unnamed") != nullptr;
}
