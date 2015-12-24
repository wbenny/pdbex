#include "PDB.h"

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
		IsValid() const;

		BOOL
		GetSymbolTypeInfo(
			IN  ULONG TypeId,
			IN  IMAGEHLP_SYMBOL_TYPE_INFO GetType,
			OUT PVOID Info
			) const;

	protected:
		HANDLE        m_ProcessHandle;
		DWORD64       m_ModuleAddress;
};

//////////////////////////////////////////////////////////////////////////
// SymbolModuleBase - implementation
//

SymbolModuleBase::SymbolModuleBase()
	: m_ProcessHandle(0)
	, m_ModuleAddress(0)
{

}

BOOL
SymbolModuleBase::Open(
	IN CONST CHAR* Path
	)
{
	static ULONG_PTR     FakeProcessHandleCounter = 2;
	static const DWORD64 FakeBaseAddress = 0x10000000;

	HANDLE FileHandle;
	DWORD  FileSize;
	BOOL   Result;

	m_ProcessHandle = (HANDLE)FakeProcessHandleCounter++;

	//
	// First get the file size.
	//

	FileHandle = CreateFileA(
		Path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
		);

	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	FileSize = GetFileSize(FileHandle, NULL);
	CloseHandle(FileHandle);

	//
	// Initialize the symbol handler. The passed handle does not need
	// to be neccessarily an actual process handle, so we fake it
	// in order to distinguish each PDB file.
	//

	Result = SymInitialize(
		m_ProcessHandle,
		NULL,
		FALSE
		);

	if (!Result)
	{
		return FALSE;
	}

	m_ModuleAddress = SymLoadModuleEx(
		m_ProcessHandle,
		NULL,
		Path,
		NULL,
		FakeBaseAddress,
		FileSize,
		NULL,
		0
		);

	if (m_ModuleAddress == 0)
	{
		return FALSE;
	}

	return TRUE;
}

VOID
SymbolModuleBase::Close()
{
	if (IsValid())
	{
		SymUnloadModule64(m_ProcessHandle, m_ModuleAddress);
		SymCleanup(m_ProcessHandle);
	}

	m_ProcessHandle = 0;
	m_ModuleAddress = 0;
}

BOOL
SymbolModuleBase::IsValid() const
{
	return m_ProcessHandle != 0;
}

BOOL
SymbolModuleBase::GetSymbolTypeInfo(
	IN  ULONG TypeId,
	IN  IMAGEHLP_SYMBOL_TYPE_INFO GetType,
	OUT PVOID Info
	) const
{
	return SymGetTypeInfo(
		m_ProcessHandle,
		m_ModuleAddress,
		TypeId,
		GetType,
		Info
		);
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

		CHAR*
		GetSymbolNameByTypeId(
			IN DWORD TypeId
			);

		BOOL
		BuildSymbolMap();

		const SymbolMap&
		GetSymbolMap() const;

		const SymbolNameMap&
		GetSymbolNameMap() const;

	private:
		static
		BOOL
		CALLBACK
		EnumSymbolsCallbackStaticImpl(
			IN PSYMBOL_INFO SymbolInfo,
			IN ULONG SymbolSize,
			IN PVOID UserContext
			);

		VOID
		EnumSymbolsCallbackImpl(
			IN PSYMBOL_INFO SymbolInfo,
			IN ULONG SymbolSize
			);

	private:
		VOID
		InitSymbol(
			IN SYMBOL* Symbol,
			IN DWORD TypeId
			);

		VOID
		ProcessSymbolBase(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolEnum(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolTypedef(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolPointer(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolArray(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolFunction(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolFunctionArg(
			IN SYMBOL* Symbol
			);

		VOID
		ProcessSymbolUserDataType(
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
	if (SymbolModuleBase::Open(Path) == FALSE)
	{
		return FALSE;
	}

	return BuildSymbolMap();
}

BOOL
SymbolModule::IsOpened() const
{
	return IsValid();
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
SymbolModule::GetSymbolNameByTypeId(
	IN DWORD TypeId
	)
{
	WCHAR* SymbolName;

	if (GetSymbolTypeInfo(TypeId, TI_GET_SYMNAME, &SymbolName))
	{
		CHAR*  SymbolNameMb;
		size_t SymbolNameLength;

		SymbolNameLength = wcslen(SymbolName) + 1;
		SymbolNameMb     = (CHAR*)malloc(SymbolNameLength);
		wcstombs(SymbolNameMb, SymbolName, SymbolNameLength);

		//
		// Result of GetSymbolTypeInfo() call is supposed to be freed by this call.
		//

		LocalFree(SymbolName);

		return SymbolNameMb;
	}

	return NULL;
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
	if (m_SymbolMap.find(TypeId) != m_SymbolMap.end())
	{
		return m_SymbolMap[TypeId];
	}
	
	SYMBOL* NewSymbol = new SYMBOL;
	m_SymbolMap[TypeId] = NewSymbol;
	m_SymbolSet.insert(NewSymbol);

	InitSymbol(NewSymbol, TypeId);

	return NewSymbol;
}

BOOL
SymbolModule::BuildSymbolMap()
{
	return SymEnumTypes(
		m_ProcessHandle,
		m_ModuleAddress,
		&EnumSymbolsCallbackStaticImpl,
		this
		);
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

BOOL
CALLBACK
SymbolModule::EnumSymbolsCallbackStaticImpl(
	IN PSYMBOL_INFO SymbolInfo,
	IN ULONG SymbolSize,
	IN PVOID UserContext
	)
{
	if (SymbolInfo != NULL)
	{
		SymbolModule* Pdb = reinterpret_cast<SymbolModule*>(UserContext);
		Pdb->EnumSymbolsCallbackImpl(SymbolInfo, SymbolSize);
	}

	return TRUE;
}

VOID
SymbolModule::EnumSymbolsCallbackImpl(
	IN PSYMBOL_INFO SymbolInfo,
	IN ULONG SymbolSize
	)
{
	SYMBOL* CreatedSymbol = GetSymbolByTypeId(SymbolInfo->TypeIndex);
	m_SymbolNameMap[CreatedSymbol->Name] = CreatedSymbol;
}

VOID
SymbolModule::InitSymbol(
	IN SYMBOL* Symbol,
	IN DWORD TypeId
	)
{
	GetSymbolTypeInfo(TypeId, TI_GET_SYMTAG,   &Symbol->Tag);
	GetSymbolTypeInfo(TypeId, TI_GET_BASETYPE, &Symbol->BaseType);
	GetSymbolTypeInfo(TypeId, TI_GET_LENGTH,   &Symbol->Size);

	Symbol->Name   = GetSymbolNameByTypeId(TypeId);
	Symbol->TypeId = TypeId;

	switch (Symbol->Tag)
	{
		case SymTagUDT:             ProcessSymbolUserDataType(Symbol); break;
		case SymTagEnum:            ProcessSymbolEnum        (Symbol); break;
		case SymTagFunctionType:    ProcessSymbolFunction    (Symbol); break;
		case SymTagPointerType:     ProcessSymbolPointer     (Symbol); break;
		case SymTagArrayType:       ProcessSymbolArray       (Symbol); break;
		case SymTagBaseType:        ProcessSymbolBase        (Symbol); break;
		case SymTagTypedef:         ProcessSymbolTypedef     (Symbol); break;
		case SymTagFunctionArgType: ProcessSymbolFunctionArg (Symbol); break;
		default:                                                       break;
	}
}

VOID
SymbolModule::ProcessSymbolBase(
	IN SYMBOL* Symbol
	)
{

}

VOID
SymbolModule::ProcessSymbolEnum(
	IN SYMBOL* Symbol
	)
{
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_CHILDRENCOUNT, &Symbol->u.Enum.FieldCount);

	TI_FINDCHILDREN_PARAMS* FindChildrenParams = (TI_FINDCHILDREN_PARAMS*)alloca(offsetof(TI_FINDCHILDREN_PARAMS, ChildId[Symbol->u.Enum.FieldCount]));
	FindChildrenParams->Start = 0;
	FindChildrenParams->Count = Symbol->u.Enum.FieldCount;
	GetSymbolTypeInfo(Symbol->TypeId, TI_FINDCHILDREN, FindChildrenParams);

	Symbol->u.Enum.Fields = (SYMBOL_ENUM_FIELD*)calloc(Symbol->u.Enum.FieldCount, sizeof(SYMBOL_ENUM_FIELD));

	for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; i++)
	{
		SYMBOL_ENUM_FIELD* EnumValue = &Symbol->u.Enum.Fields[i];

		EnumValue->Parent = Symbol;

		EnumValue->Name = GetSymbolNameByTypeId(FindChildrenParams->ChildId[i]);
		GetSymbolTypeInfo(FindChildrenParams->ChildId[i], TI_GET_VALUE, &EnumValue->Value);
	}
}

VOID
SymbolModule::ProcessSymbolTypedef(
	IN SYMBOL* Symbol
	)
{
	DWORD TypedefTypeId;
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_TYPE, &TypedefTypeId);
	Symbol->u.Typedef.Type = GetSymbolByTypeId(TypedefTypeId);
}

VOID
SymbolModule::ProcessSymbolPointer(
	IN SYMBOL* Symbol
	)
{
	DWORD PointerTypeId;
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_TYPE, &PointerTypeId);
	Symbol->u.Pointer.Type = GetSymbolByTypeId(PointerTypeId);
}

VOID
SymbolModule::ProcessSymbolArray(
	IN SYMBOL* Symbol
	)
{
	DWORD DataTypeId;
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_TYPE, &DataTypeId);
	Symbol->u.Array.ElementType = GetSymbolByTypeId(DataTypeId);

	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_COUNT, &Symbol->u.Array.ElementCount);
}

VOID
SymbolModule::ProcessSymbolFunction(
	IN SYMBOL* Symbol
	)
{
	//
	// Calling convention.
	//

	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_CALLING_CONVENTION, &Symbol->u.Function.CallingConvention);

	//
	// Return type.
	//

	DWORD ReturnTypeId;
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_TYPE, &ReturnTypeId);
	Symbol->u.Function.ReturnType = GetSymbolByTypeId(ReturnTypeId);

	//
	// Arguments.
	//

	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_CHILDRENCOUNT, &Symbol->u.Function.ArgumentCount);
	
	Symbol->u.Function.Arguments = (PSYMBOL*)calloc(sizeof(SYMBOL*), Symbol->u.Function.ArgumentCount);

	TI_FINDCHILDREN_PARAMS* FindChildrenParams = (TI_FINDCHILDREN_PARAMS*)alloca(offsetof(TI_FINDCHILDREN_PARAMS, ChildId[Symbol->u.Function.ArgumentCount]));
	FindChildrenParams->Start = 0;
	FindChildrenParams->Count = Symbol->u.Function.ArgumentCount;
	GetSymbolTypeInfo(Symbol->TypeId, TI_FINDCHILDREN, FindChildrenParams);

	for (DWORD i = 0; i < Symbol->u.Function.ArgumentCount; i++)
	{
		SYMBOL* Argument;
		Argument = GetSymbolByTypeId(FindChildrenParams->ChildId[i]);
		Symbol->u.Function.Arguments[i] = Argument;
	}
}

VOID
SymbolModule::ProcessSymbolFunctionArg(
	IN SYMBOL* Symbol
	)
{
	DWORD ArgumentTypeId;
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_TYPE, &ArgumentTypeId);
	Symbol->u.FunctionArg.Type = GetSymbolByTypeId(ArgumentTypeId);
}

VOID
SymbolModule::ProcessSymbolUserDataType(
	IN SYMBOL* Symbol
	)
{
	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_UDTKIND, &Symbol->u.UserData.Kind);

	GetSymbolTypeInfo(Symbol->TypeId, TI_GET_CHILDRENCOUNT, &Symbol->u.UserData.FieldCount);

	Symbol->u.UserData.Fields = (SYMBOL_USERDATA_FIELD*)calloc(sizeof(SYMBOL_USERDATA_FIELD), Symbol->u.UserData.FieldCount + 1);

	TI_FINDCHILDREN_PARAMS* FindChildrenParams = (TI_FINDCHILDREN_PARAMS*)alloca(offsetof(TI_FINDCHILDREN_PARAMS, ChildId[Symbol->u.UserData.FieldCount]));
	FindChildrenParams->Start = 0;
	FindChildrenParams->Count = Symbol->u.UserData.FieldCount;
	GetSymbolTypeInfo(Symbol->TypeId, TI_FINDCHILDREN, FindChildrenParams);

	SYMBOL_USERDATA_FIELD* PreviousMember = NULL;
	DWORD PreviousBitPosition = 0;
	for (DWORD i = 0; i < Symbol->u.UserData.FieldCount; i++)
	{
		SYMBOL_USERDATA_FIELD* Member = &Symbol->u.UserData.Fields[i];

		Member->Parent = Symbol;

		Member->Name = GetSymbolNameByTypeId(FindChildrenParams->ChildId[i]);

		GetSymbolTypeInfo(FindChildrenParams->ChildId[i], TI_GET_OFFSET,      &Member->Offset);
		GetSymbolTypeInfo(FindChildrenParams->ChildId[i], TI_GET_BITPOSITION, &Member->BitPosition);

		DWORD MemberType;
		GetSymbolTypeInfo(FindChildrenParams->ChildId[i], TI_GET_TYPE,        &MemberType);

		Member->Type = GetSymbolByTypeId(MemberType);

		if (Member->BitPosition != 0 && PreviousMember != NULL)
		{
			PreviousMember->Bits = Member->BitPosition - PreviousBitPosition;
			PreviousBitPosition = Member->BitPosition;
		}
		else if (PreviousBitPosition != 0)
		{
			PreviousMember->Bits = (DWORD)PreviousMember->Type->Size * CHAR_BIT - PreviousBitPosition;
			PreviousBitPosition = 0;
		}

		PreviousMember = Member;
	}

	if (PreviousBitPosition != 0)
	{
		PreviousMember->Bits = (DWORD)PreviousMember->Type->Size * CHAR_BIT - PreviousMember->BitPosition;
	}

	//
	// Padding.
	//
	if (Symbol->u.UserData.Kind == UdtStruct && Symbol->u.UserData.FieldCount > 0)
	{
		SYMBOL_USERDATA_FIELD* LastUserDataField = &Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount - 1];
		SYMBOL_USERDATA_FIELD* PaddingUserDataField = &Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount];
		DWORD PaddingSize = (DWORD)Symbol->Size - (LastUserDataField->Offset + (DWORD)LastUserDataField->Type->Size);

		if (PaddingSize > 0)
		{
			SYMBOL* PaddingSymbolArrayElement = new SYMBOL;
			PaddingSymbolArrayElement->Tag = SymTagBaseType;
			PaddingSymbolArrayElement->BaseType = !(PaddingSize % 4) ? btLong : btChar;
			PaddingSymbolArrayElement->Size = PaddingSymbolArrayElement->BaseType == btLong ? 4 : 1;
			PaddingSymbolArrayElement->TypeId = 0;
			PaddingSymbolArrayElement->Name = NULL;


			SYMBOL* PaddingSymbolArray = new SYMBOL;
			PaddingSymbolArray->Tag = SymTagArrayType;
			PaddingSymbolArray->BaseType = btNoType;
			PaddingSymbolArray->Size = PaddingSize;
			PaddingSymbolArray->TypeId = 0;
			PaddingSymbolArray->Name = NULL;
			PaddingSymbolArray->u.Array.ElementType = PaddingSymbolArrayElement;
			PaddingSymbolArray->u.Array.ElementCount = PaddingSymbolArrayElement->BaseType == btLong ? PaddingSize / 4 : PaddingSize;

			PaddingUserDataField->Name = (CHAR*)malloc(64);
			PaddingUserDataField->Type = PaddingSymbolArray;
			PaddingUserDataField->Offset = LastUserDataField->Offset + (DWORD)LastUserDataField->Type->Size;

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
	ULONG64     Length;
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
	IN ULONG64 Size,
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
