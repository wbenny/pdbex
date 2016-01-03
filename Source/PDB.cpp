#include "PDB.h"

#include <dia2.h>       // IDia* interfaces

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
			IN CONST CHAR* Path
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
	IN CONST CHAR* Path
	)
{
	//
	// Obtain access to the provider
	//

	HRESULT HResult;

	HResult = CoCreateInstance(
		__uuidof(DiaSource),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IDiaDataSource),
		(void**)&m_DataSource
		);

	if (FAILED(HResult))
	{
		return FALSE;
	}

	HResult = m_DataSource->loadDataFromPdb(
		string_converter.from_bytes(Path).c_str()
		);

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
			IN CONST CHAR* Path
			);

		BOOL
		IsOpened() const;

		CONST CHAR*
		GetPath() const;

		VOID
		Close();

		SYMBOL*
		GetSymbolByName(
			IN CONST CHAR* SymbolName
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
		BuildSymbolMap();

		const SymbolMap&
		GetSymbolMap() const;

		const SymbolNameMap&
		GetSymbolNameMap() const;

	private:
		VOID
		InitSymbol(
			IN IDiaSymbol* DiaSymbol,
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
		ProcessSymbolUserDataType(
			IN IDiaSymbol* DiaSymbol,
			IN SYMBOL* Symbol
			);

		VOID
		DestroySymbol(
			IN SYMBOL* Symbol
			);

	private:
		std::string   m_Path;
		SymbolMap     m_SymbolMap;
		SymbolNameMap m_SymbolNameMap;
		SymbolSet     m_SymbolSet;
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
	IN CONST CHAR* Path
	)
{
	BOOL Result;

	Result = SymbolModuleBase::Open(Path);

	if (Result == FALSE)
	{
		return FALSE;
	}

	BuildSymbolMap();

	return TRUE;
}

BOOL
SymbolModule::IsOpened() const
{
	return SymbolModuleBase::IsOpened();
}

CONST CHAR*
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

		return NULL;
	}

	//
	// BSTR is essentially a wide char string.
	// Since we work in multibyte character set,
	// we need to convert it.
	//

	CHAR*  SymbolNameMb;
	size_t SymbolNameLength;

	SymbolNameLength = SysStringLen(SymbolNameBstr) + 1;
	SymbolNameMb = (CHAR*)malloc(SymbolNameLength);
	wcstombs(SymbolNameMb, SymbolNameBstr, SymbolNameLength);

	//
	// BSTR is supposed to be freed by this call.
	//

	SysFreeString(SymbolNameBstr);

	return SymbolNameMb;
}

SYMBOL*
SymbolModule::GetSymbolByName(
	IN CONST CHAR* SymbolName
	)
{
	auto it = m_SymbolNameMap.find(SymbolName);
	return it == m_SymbolNameMap.end() ? NULL : it->second;
}

SYMBOL*
SymbolModule::GetSymbolByTypeId(
	IN DWORD TypeId
	)
{
	auto it = m_SymbolMap.find(TypeId);
	return it == m_SymbolMap.end() ? NULL : it->second;
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
SymbolModule::BuildSymbolMap()
{
	IDiaEnumSymbols* DiaSymbolEnumerator;

	m_GlobalSymbol->findChildren(SymTagEnum, NULL, nsNone, &DiaSymbolEnumerator);
	BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);

	m_GlobalSymbol->findChildren(SymTagUDT, NULL, nsNone, &DiaSymbolEnumerator);
	BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);

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
	Symbol->Tag = (enum SymTagEnum)DwordResult;

	DiaSymbol->get_baseType(&DwordResult);
	Symbol->BaseType = (BasicType)DwordResult;

	DiaSymbol->get_typeId(&DwordResult);
	Symbol->TypeId = DwordResult;

	DiaSymbol->get_length(&UlonglongResult);
	Symbol->Size = (DWORD)UlonglongResult;

	DiaSymbol->get_constType(&BoolResult);
	Symbol->IsConst = (BOOL)BoolResult;

	DiaSymbol->get_volatileType(&BoolResult);
	Symbol->IsVolatile = (BOOL)BoolResult;

	Symbol->Name = GetSymbolName(DiaSymbol);

	switch (Symbol->Tag)
	{
		case SymTagUDT:             ProcessSymbolUserDataType(DiaSymbol, Symbol); break;
		case SymTagEnum:            ProcessSymbolEnum        (DiaSymbol, Symbol); break;
		case SymTagFunctionType:    ProcessSymbolFunction    (DiaSymbol, Symbol); break;
		case SymTagPointerType:     ProcessSymbolPointer     (DiaSymbol, Symbol); break;
		case SymTagArrayType:       ProcessSymbolArray       (DiaSymbol, Symbol); break;
		case SymTagBaseType:        ProcessSymbolBase        (DiaSymbol, Symbol); break;
		case SymTagTypedef:         ProcessSymbolTypedef     (DiaSymbol, Symbol); break;
		case SymTagFunctionArgType: ProcessSymbolFunctionArg (DiaSymbol, Symbol); break;
		default:                                                                  break;
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
	Symbol->u.Enum.Fields = (SYMBOL_ENUM_FIELD*)calloc(ChildCount, sizeof(SYMBOL_ENUM_FIELD));

	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	for (DWORD Index = 0;
	     SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1);
	     Index++)
	{
		SYMBOL_ENUM_FIELD* EnumValue = &Symbol->u.Enum.Fields[Index];

		EnumValue->Parent = Symbol;

		EnumValue->Name = GetSymbolName(DiaChildSymbol);
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
	Symbol->u.Function.Arguments = (PSYMBOL*)calloc(ChildCount, sizeof(SYMBOL*));

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
}

VOID
SymbolModule::ProcessSymbolUserDataType(
	IN IDiaSymbol* DiaSymbol,
	IN SYMBOL* Symbol
	)
{
	DiaSymbol->get_udtKind((DWORD*)&Symbol->u.UserData.Kind);

	IDiaEnumSymbols* DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, NULL, nsNone, &DiaSymbolEnumerator)))
	{
		return;
	}

	LONG ChildCount;

	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.UserData.FieldCount = static_cast<DWORD>(ChildCount);
	Symbol->u.UserData.Fields = (SYMBOL_USERDATA_FIELD*)calloc(ChildCount + 1, sizeof(SYMBOL_USERDATA_FIELD));

	IDiaSymbol* DiaChildSymbol;
	ULONG FetchedSymbolCount = 0;

	for (DWORD Index = 0;
	     SUCCEEDED(DiaSymbolEnumerator->Next(1, &DiaChildSymbol, &FetchedSymbolCount)) && (FetchedSymbolCount == 1);
	     Index++)
	{
		SYMBOL_USERDATA_FIELD* Member = &Symbol->u.UserData.Fields[Index];

		Member->Parent = Symbol;

		Member->Name = GetSymbolName(DiaChildSymbol);

		Member->BitPosition = 0;
		Member->Bits = 0;

		DiaChildSymbol->get_offset((LONG*)&Member->Offset);
		DiaChildSymbol->get_bitPosition(&Member->BitPosition);

		ULONGLONG Bits;
		DiaChildSymbol->get_length(&Bits);
		Member->Bits = (DWORD)Bits;

		IDiaSymbol* MemberTypeDiaSymbol;

		if (SUCCEEDED(DiaChildSymbol->get_type(&MemberTypeDiaSymbol)))
		{
			Member->Type = GetSymbol(MemberTypeDiaSymbol);

			MemberTypeDiaSymbol->Release();
		}

		DiaChildSymbol->Release();
	}

	DiaSymbolEnumerator->Release();

	//
	// Padding.
	//
	if (Symbol->u.UserData.Kind == UdtStruct && Symbol->u.UserData.FieldCount > 0 && Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount - 1].Type != nullptr)
	{
		SYMBOL_USERDATA_FIELD* LastUserDataField = &Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount - 1];
		SYMBOL_USERDATA_FIELD* PaddingUserDataField = &Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount];
		DWORD PaddingSize = Symbol->Size - (LastUserDataField->Offset + LastUserDataField->Type->Size);

		if (PaddingSize > 0)
		{
			SYMBOL* PaddingSymbolArrayElement = new SYMBOL;
			PaddingSymbolArrayElement->Tag = SymTagBaseType;
			PaddingSymbolArrayElement->BaseType = !(PaddingSize % 4) ? btLong : btChar;
			PaddingSymbolArrayElement->TypeId = 0;
			PaddingSymbolArrayElement->Size = PaddingSymbolArrayElement->BaseType == btLong ? 4 : 1;
			PaddingSymbolArrayElement->IsConst = FALSE;
			PaddingSymbolArrayElement->IsVolatile = FALSE;
			PaddingSymbolArrayElement->Name = NULL;


			SYMBOL* PaddingSymbolArray = new SYMBOL;
			PaddingSymbolArray->Tag = SymTagArrayType;
			PaddingSymbolArray->BaseType = btNoType;
			PaddingSymbolArray->TypeId = 0;
			PaddingSymbolArray->Size = PaddingSize;
			PaddingSymbolArray->IsConst = FALSE;
			PaddingSymbolArray->IsVolatile = FALSE;
			PaddingSymbolArray->Name = NULL;
			PaddingSymbolArray->u.Array.ElementType = PaddingSymbolArrayElement;
			PaddingSymbolArray->u.Array.ElementCount = PaddingSymbolArrayElement->BaseType == btLong ? PaddingSize / 4 : PaddingSize;

			PaddingUserDataField->Name = (CHAR*)malloc(64);
			PaddingUserDataField->Type = PaddingSymbolArray;
			PaddingUserDataField->Offset = LastUserDataField->Offset + LastUserDataField->Type->Size;

			PaddingUserDataField->Bits = 0;
			PaddingUserDataField->BitPosition = 0;
			PaddingUserDataField->Parent = Symbol;

			strcpy(PaddingUserDataField->Name, "__PADDING__");

			Symbol->u.UserData.FieldCount++;

			m_SymbolSet.insert(PaddingSymbolArray);
			m_SymbolSet.insert(PaddingSymbolArrayElement);
		}
	}
}

void SymbolModule::DestroySymbol(
	IN SYMBOL* Symbol
	)
{
	free(Symbol->Name);

	switch (Symbol->Tag)
	{
		case SymTagUDT:
			for (DWORD i = 0; i < Symbol->u.UserData.FieldCount; i++)
			{
				free(Symbol->u.UserData.Fields[i].Name);
			}

			free(Symbol->u.UserData.Fields);
			break;

		case SymTagEnum:
			for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; i++)
			{
				free(Symbol->u.Enum.Fields[i].Name);
			}

			free(Symbol->u.Enum.Fields);
			break;

		case SymTagFunctionType:
			free(Symbol->u.Function.Arguments);
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
	{ btNoType,       0,  "btNoType",         NULL               },
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
	{ btCurrency,     0,  "btCurrency",       NULL               },
	{ btDate,         0,  "btDate",           "DATE"             },
	{ btVariant,      0,  "btVariant",        "VARIANT"          },
	{ btComplex,      0,  "btComplex",        NULL               },
	{ btBit,          0,  "btBit",            NULL               },
	{ btBSTR,         0,  "btBSTR",           "BSTR"             },
	{ btHresult,      4,  "btHresult",        "HRESULT"          },
	{ (BasicType)0,   0,  NULL,               NULL               },
};

BasicTypeMapElement BasicTypeMapStdInt[] = {
	{ btNoType,       0,  "btNoType",         NULL               },
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
	{ btCurrency,     0,  "btCurrency",       NULL               },
	{ btDate,         0,  "btDate",           "DATE"             },
	{ btVariant,      0,  "btVariant",        "VARIANT"          },
	{ btComplex,      0,  "btComplex",        NULL               },
	{ btBit,          0,  "btBit",            NULL               },
	{ btBSTR,         0,  "btBSTR",           "BSTR"             },
	{ btHresult,      4,  "btHresult",        "HRESULT"          },
	{ (BasicType)0,   0,  NULL,               NULL               },
};

PDB::PDB()
{
	m_Impl = new SymbolModule();
}

PDB::PDB(
	IN CONST CHAR* Path
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
	IN CONST CHAR* Path
	)
{
	return m_Impl->Open(Path);
}

BOOL
PDB::IsOpened() const
{
	return m_Impl->IsOpened();
}

CONST CHAR*
PDB::GetPath() const
{
	return m_Impl->GetPath();
}

VOID
PDB::Close()
{
	m_Impl->Close();
}

CONST SYMBOL*
PDB::GetSymbolByName(
	IN CONST CHAR* SymbolName
	)
{
	return m_Impl->GetSymbolByName(SymbolName);
}

CONST SYMBOL*
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

CONST CHAR*
PDB::GetBasicTypeString(
	IN BasicType BaseType,
	IN DWORD Size,
	IN BOOL UseStdInt
	)
{
	BasicTypeMapElement* TypeMap = UseStdInt ? BasicTypeMapStdInt : BasicTypeMapMSVC;

	for (int n = 0; TypeMap[n].BasicTypeString != NULL; n++)
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

	return NULL;
}

CONST CHAR*
PDB::GetBasicTypeString(
	IN CONST SYMBOL* Symbol,
	IN BOOL UseStdInt
	)
{
	return GetBasicTypeString(Symbol->BaseType, Symbol->Size, UseStdInt);
}

CONST CHAR*
PDB::GetUdtKindString(
	IN UdtKind UdtKind
	)
{
	static CONST CHAR* UdtKindStrings[] = {
		"struct",
		"class",
		"union",
	};

	if (UdtKind >= UdtStruct && UdtKind <= UdtUnion)
	{
		return UdtKindStrings[UdtKind];
	}

	return NULL;
}

BOOL
PDB::IsUnnamedSymbol(
	CONST SYMBOL* Symbol
	)
{
	return strstr(Symbol->Name, "<unnamed-") != NULL ||
	       strstr(Symbol->Name, "__unnamed") != NULL;
}
