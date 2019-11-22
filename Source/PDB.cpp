#include "PDB.h"
#include "PDBCallback.h"

#include <dia2.h>       // IDia* interfaces

#include <cassert>

#include <string>
#include <memory>

class SymbolModuleBase
{
public:
	SymbolModuleBase();

	BOOL Open(IN const CHAR* Path);
	VOID Close();
	BOOL IsOpen() const { return m_DataSource && m_Session && m_GlobalSymbol; }

private:
	HRESULT LoadDiaViaCoCreateInstance();
	HRESULT LoadDiaViaLoadLibrary();

protected:
	IDiaDataSource *m_DataSource;
	IDiaSession    *m_Session;
	IDiaSymbol     *m_GlobalSymbol;
};

SymbolModuleBase::SymbolModuleBase()
{
	HRESULT hr = CoInitialize(nullptr);
	assert(hr == S_OK);
}

HRESULT SymbolModuleBase::LoadDiaViaCoCreateInstance()
{
	return CoCreateInstance(
		__uuidof(DiaSource),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IDiaDataSource),
		(void**)& m_DataSource
		);
}

HRESULT SymbolModuleBase::LoadDiaViaLoadLibrary()
{
	HRESULT Result;
	HMODULE Module = LoadLibrary(TEXT("msdia140.dll"));
	if (!Module)
	{
		Result = HRESULT_FROM_WIN32(GetLastError());
		return Result;
	}

	using PDLLGETCLASSOBJECT_ROUTINE = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID);
	auto DllGetClassObject = reinterpret_cast<PDLLGETCLASSOBJECT_ROUTINE>(GetProcAddress(Module, "DllGetClassObject"));
	if (!DllGetClassObject)
	{
		Result = HRESULT_FROM_WIN32(GetLastError());
		return Result;
	}

	IClassFactory *ClassFactory;
	Result = DllGetClassObject(__uuidof(DiaSource), __uuidof(IClassFactory), &ClassFactory);
	if (FAILED(Result))
	{
		return Result;
	}

	return ClassFactory->CreateInstance(nullptr, __uuidof(IDiaDataSource), (void**)& m_DataSource);
}

BOOL SymbolModuleBase::Open(IN const CHAR* Path)
{
	HRESULT   Result            = S_OK;
	LPCOLESTR PDBSearchPath     = L"srv*.\\Symbols*https://msdl.microsoft.com/download/symbols";

	if (FAILED(Result = LoadDiaViaCoCreateInstance()) &&
	    FAILED(Result = LoadDiaViaLoadLibrary()))
	{
		return FALSE;
	}

	int PathUnicodeLength = MultiByteToWideChar(CP_UTF8, 0, Path, -1, NULL, 0);
	auto PathUnicode       = std::make_unique<WCHAR[]>(PathUnicodeLength);
	MultiByteToWideChar(CP_UTF8, 0, Path, -1, PathUnicode.get(), PathUnicodeLength);

	WCHAR FileExtension[8] = { 0 };
	_wsplitpath_s(PathUnicode.get(),
		nullptr, 0,
		nullptr, 0,
		nullptr, 0, FileExtension,
		_countof(FileExtension));

	if (_wcsicmp(FileExtension, L".pdb") == 0)
	{
		Result = m_DataSource->loadDataFromPdb(PathUnicode.get());
	} else
	{
		PDBCallback Callback;
		Callback.AddRef();

		Result = m_DataSource->loadDataForExe(PathUnicode.get(), PDBSearchPath, &Callback);
	}

	if (FAILED(Result))
	{
		Close();
		return FALSE;
	}

	Result = m_DataSource->openSession(&m_Session);
	if (FAILED(Result))
	{
		Close();
		return FALSE;
	}

	Result = m_Session->get_globalScope(&m_GlobalSymbol);
	if (FAILED(Result))
	{
		Close();
		return FALSE;
	}

	return TRUE;
}

VOID SymbolModuleBase::Close()
{
	if (m_GlobalSymbol != NULL)
	{
		m_GlobalSymbol->Release();
		m_GlobalSymbol = NULL;
	}

	if (m_Session != NULL)
	{
		m_Session->Release();
		m_Session = NULL;
	}

	if (m_DataSource != NULL)
	{
		m_DataSource->Release();
		m_DataSource = NULL;
	}

	CoUninitialize();
}

class SymbolModule
	: public SymbolModuleBase
{
public:
	SymbolModule();
	~SymbolModule();

	BOOL Open(IN const CHAR* Path);
	BOOL IsOpen() const { return SymbolModuleBase::IsOpen(); }
	const CHAR* GetPath() const { return m_Path.c_str(); }
	VOID Close();
	DWORD GetMachineType() const { return m_MachineType; }
	CV_CFL_LANG GetLanguage() const { return m_Language; }
	SYMBOL*	GetSymbolByName(IN const CHAR* SymbolName);
	SYMBOL* GetSymbolByTypeId(IN DWORD TypeId);
	SYMBOL* GetSymbol(IN IDiaSymbol* DiaSymbol);
	CHAR* GetSymbolName(IN IDiaSymbol* DiaSymbol);
	VOID BuildSymbolMapFromEnumerator(IN IDiaEnumSymbols* DiaSymbolEnumerator);
	VOID BuildFunctionSetFromEnumerator(IN IDiaEnumSymbols* DiaSymbolEnumerator);
	VOID BuildSymbolMap();
	const SymbolMap& GetSymbolMap() const { return m_SymbolMap; }
	const SymbolNameMap& GetSymbolNameMap() const { return m_SymbolNameMap; }
	const FunctionSet& GetFunctionSet() const { return m_FunctionSet; }

private:
	VOID InitSymbol(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID DestroySymbol(IN SYMBOL* Symbol);
	VOID ProcessSymbolBase(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolEnum(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolTypedef(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolPointer(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolArray(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolFunction(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolFunctionArg(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolUdt(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);
	VOID ProcessSymbolFunctionEx(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol);

private:
	std::string   m_Path;
	SymbolMap     m_SymbolMap;
	SymbolNameMap m_SymbolNameMap;
	SymbolSet     m_SymbolSet;
	FunctionSet   m_FunctionSet;

	DWORD         m_MachineType = 0;
	CV_CFL_LANG   m_Language = CV_CFL_C;
};

SymbolModule::SymbolModule()
{

}

SymbolModule::~SymbolModule()
{
	Close();
}

BOOL SymbolModule::Open(IN const CHAR* Path)
{
	BOOL Result;

	Result = SymbolModuleBase::Open(Path);
	if (Result == FALSE)
		return FALSE;

	m_GlobalSymbol->get_machineType(&m_MachineType);

	DWORD Language;
	m_GlobalSymbol->get_language(&Language);
	m_Language = static_cast<CV_CFL_LANG>(Language);

	BuildSymbolMap();

	return TRUE;
}

VOID SymbolModule::Close()
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

CHAR* SymbolModule::GetSymbolName(IN IDiaSymbol* DiaSymbol)
{
	BSTR SymbolNameBstr;

	//if (DiaSymbol->get_undecoratedName(&SymbolNameBstr) != S_OK) {
		if (DiaSymbol->get_name(&SymbolNameBstr) != S_OK)
			return nullptr;
	//}

	CHAR*  SymbolNameMb;
	size_t SymbolNameLength;

	SymbolNameLength = (size_t)SysStringLen(SymbolNameBstr) + 1;
	SymbolNameMb = new CHAR[SymbolNameLength];
	wcstombs(SymbolNameMb, SymbolNameBstr, SymbolNameLength);

	SysFreeString(SymbolNameBstr);

	return SymbolNameMb;
}

SYMBOL* SymbolModule::GetSymbolByName(IN const CHAR* SymbolName)
{
	auto it = m_SymbolNameMap.find(SymbolName);
	return it == m_SymbolNameMap.end() ? nullptr : it->second;
}

SYMBOL* SymbolModule::GetSymbolByTypeId(IN DWORD TypeId)
{
	auto it = m_SymbolMap.find(TypeId);
	return it == m_SymbolMap.end() ? nullptr : it->second;
}

SYMBOL* SymbolModule::GetSymbol(IN IDiaSymbol* DiaSymbol)
{
	DWORD TypeId;
	DiaSymbol->get_symIndexId(&TypeId);

	auto it = m_SymbolMap.find(TypeId);
	if (it != m_SymbolMap.end())
		return it->second;

	SYMBOL* Symbol;
	Symbol = new SYMBOL;
	m_SymbolMap[TypeId] = Symbol;
	m_SymbolSet.insert(Symbol);

	InitSymbol(DiaSymbol, Symbol);

	if (Symbol->Name)
		m_SymbolNameMap[Symbol->Name] = Symbol;

	return Symbol;
}

VOID SymbolModule::BuildSymbolMapFromEnumerator(IN IDiaEnumSymbols* DiaSymbolEnumerator)
{
	IDiaSymbol* Result;
	ULONG FetchedSymbolCount = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &Result, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		IDiaSymbol *DiaChildSymbol(Result);
		GetSymbol(DiaChildSymbol);
		DiaChildSymbol->Release();
	}
}

VOID SymbolModule::BuildFunctionSetFromEnumerator(IN IDiaEnumSymbols* DiaSymbolEnumerator)
{
	IDiaSymbol* Result;
	ULONG FetchedSymbolCount = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &Result, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		IDiaSymbol *DiaChildSymbol(Result);

		BOOL IsFunction;
		DiaChildSymbol->get_function(&IsFunction);

		if (IsFunction)
		{
			CHAR* FunctionName = GetSymbolName(DiaChildSymbol);

			DWORD DwordResult;
			DiaChildSymbol->get_symTag(&DwordResult);
			// auto Tag = static_cast<enum SymTagEnum>(DwordResult);

			m_FunctionSet.insert(FunctionName);
			delete[] FunctionName;
		}
		DiaChildSymbol->Release();
	}
	}

VOID SymbolModule::BuildSymbolMap()
{
	IDiaEnumSymbols *DiaSymbolEnumerator;

	if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagPublicSymbol, nullptr, nsNone, &DiaSymbolEnumerator)))
	{
		BuildFunctionSetFromEnumerator(DiaSymbolEnumerator);
	}
	if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagEnum, nullptr, nsNone, &DiaSymbolEnumerator)))
	{
		BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);
	}
	if (SUCCEEDED(m_GlobalSymbol->findChildren(SymTagUDT, nullptr, nsNone, &DiaSymbolEnumerator)))
	{
		BuildSymbolMapFromEnumerator(DiaSymbolEnumerator);
	}
	DiaSymbolEnumerator->Release();
}

VOID SymbolModule::InitSymbol(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
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
	case SymTagFunctionType:
	{
					Symbol->u.Function.IsOverride = FALSE;
					Symbol->u.Function.IsPure = FALSE;
					Symbol->u.Function.IsConst = FALSE;
					Symbol->u.Function.IsVirtual = FALSE;
					Symbol->u.Function.IsStatic = FALSE;
				    ProcessSymbolFunction   (DiaSymbol, Symbol); 
	} break;
	case SymTagPointerType:     ProcessSymbolPointer    (DiaSymbol, Symbol); break;
	case SymTagArrayType:       ProcessSymbolArray      (DiaSymbol, Symbol); break;
	case SymTagBaseType:        ProcessSymbolBase       (DiaSymbol, Symbol); break;
	case SymTagTypedef:         ProcessSymbolTypedef    (DiaSymbol, Symbol); break;
	case SymTagFunctionArgType: ProcessSymbolFunctionArg(DiaSymbol, Symbol); break;
	case SymTagFunction:        ProcessSymbolFunctionEx (DiaSymbol, Symbol); break;
	default:                                                                 break;
	}
}

VOID SymbolModule::ProcessSymbolBase(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
}

VOID SymbolModule::ProcessSymbolEnum(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	IDiaEnumSymbols *DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, nullptr, nsNone, &DiaSymbolEnumerator)))
		return;

	LONG ChildCount;
	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Enum.FieldCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Enum.Fields = new SYMBOL_ENUM_FIELD[ChildCount];

	IDiaSymbol* Result;
	ULONG FetchedSymbolCount = 0;
	DWORD Index = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &Result, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		IDiaSymbol *DiaChildSymbol(Result);

		SYMBOL_ENUM_FIELD* EnumValue = &Symbol->u.Enum.Fields[Index];

		EnumValue->Parent = Symbol;
		EnumValue->Name = GetSymbolName(DiaChildSymbol);

		VariantInit(&EnumValue->Value);
		DiaChildSymbol->get_value(&EnumValue->Value);

		DiaChildSymbol->Release();
		Index += 1;
	}
}

VOID SymbolModule::ProcessSymbolTypedef(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	IDiaSymbol *DiaTypedefSymbol;

	DiaSymbol->get_type(&DiaTypedefSymbol);

	Symbol->u.Typedef.Type = GetSymbol(DiaTypedefSymbol);

	DiaTypedefSymbol->Release();
}

VOID SymbolModule::ProcessSymbolPointer(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	IDiaSymbol *DiaPointerSymbol;

	DiaSymbol->get_type(&DiaPointerSymbol);
	DiaSymbol->get_reference(&Symbol->u.Pointer.IsReference);

	Symbol->u.Pointer.Type = GetSymbol(DiaPointerSymbol);

	DiaPointerSymbol->Release();

	if (m_MachineType == 0)
	{
		switch (Symbol->Size)
		{
			case 4:  m_MachineType = IMAGE_FILE_MACHINE_I386;  break;
			case 8:  m_MachineType = IMAGE_FILE_MACHINE_AMD64; break;
			default: m_MachineType = 0; break;
		}
	}
}

VOID SymbolModule::ProcessSymbolArray(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	IDiaSymbol *DiaDataTypeSymbol;

	DiaSymbol->get_type(&DiaDataTypeSymbol);
	Symbol->u.Array.ElementType = GetSymbol(DiaDataTypeSymbol);

	DiaSymbol->get_count(&Symbol->u.Array.ElementCount);

	DiaDataTypeSymbol->Release();
}

VOID SymbolModule::ProcessSymbolFunction(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	DWORD CallingConvention;
	DiaSymbol->get_callingConvention(&CallingConvention);

	Symbol->u.Function.CallingConvention = static_cast<CV_call_e>(CallingConvention);

	IDiaSymbol *DiaReturnTypeSymbol;
	DiaSymbol->get_type(&DiaReturnTypeSymbol);
	Symbol->u.Function.ReturnType = GetSymbol(DiaReturnTypeSymbol);
	DiaReturnTypeSymbol->Release();

	IDiaEnumSymbols *DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, nullptr, nsNone, &DiaSymbolEnumerator)))
		return;

	LONG ChildCount;

	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Function.ArgumentCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Function.Arguments = new SYMBOL*[ChildCount];

	IDiaSymbol* Result;
	ULONG FetchedSymbolCount = 0;
	DWORD Index = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &Result, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		IDiaSymbol *DiaChildSymbol(Result);

		SYMBOL* Argument;
		Argument = GetSymbol(DiaChildSymbol);
		Symbol->u.Function.Arguments[Index] = Argument;
		DiaChildSymbol->Release();
		Index += 1;
	}
	DiaSymbolEnumerator->Release();
}

VOID SymbolModule::ProcessSymbolFunctionArg(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	IDiaSymbol *DiaArgumentTypeSymbol;

	DiaSymbol->get_type(&DiaArgumentTypeSymbol);
	Symbol->u.FunctionArg.Type = GetSymbol(DiaArgumentTypeSymbol);
	DiaArgumentTypeSymbol->Release();
}

VOID SymbolModule::ProcessSymbolUdt(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	DWORD Kind;
	DiaSymbol->get_udtKind(&Kind);
	Symbol->u.Udt.Kind = static_cast<UdtKind>(Kind);
	Symbol->u.Udt.BaseClassCount = 0;
	Symbol->u.Udt.BaseClassFields = 0;

	IDiaEnumSymbols *DiaSymbolEnumerator;

	if (FAILED(DiaSymbol->findChildren(SymTagNull, nullptr, nsNone, &DiaSymbolEnumerator)))
		return;

	LONG ChildCount;

	DiaSymbolEnumerator->get_Count(&ChildCount);

	Symbol->u.Udt.FieldCount = static_cast<DWORD>(ChildCount);
	Symbol->u.Udt.Fields = new SYMBOL_UDT_FIELD[ChildCount + 1];

	IDiaSymbol* Result;
	ULONG FetchedSymbolCount = 0;
	DWORD Index = 0;

	while (SUCCEEDED(DiaSymbolEnumerator->Next(1, &Result, &FetchedSymbolCount)) && (FetchedSymbolCount == 1))
	{
		IDiaSymbol *DiaChildSymbol(Result);

		DWORD symTag;
		DiaChildSymbol->get_symTag(&symTag);

		SYMBOL_UDT_FIELD* Member = &Symbol->u.Udt.Fields[Index];

		Member->Name = GetSymbolName(DiaChildSymbol);
		Member->Parent = Symbol;
		Member->IsBaseClass = FALSE;

		Member->Tag = static_cast<enum SymTagEnum>(symTag);
		Member->DataKind = static_cast<enum DataKind>(0); //???

		LONG Offset = 0;
		DiaChildSymbol->get_offset(&Offset);
		Member->Offset = static_cast<DWORD>(Offset);

		ULONGLONG Bits = 0;
		if (symTag == SymTagData)
		{
			DiaChildSymbol->get_length(&Bits);
		}
		Member->Bits = static_cast<DWORD>(Bits);

		DiaChildSymbol->get_bitPosition(&Member->BitPosition);

		if (symTag == SymTagData || symTag == SymTagBaseClass)
		{
			DWORD DwordResult;
			DiaChildSymbol->get_dataKind(&DwordResult);
			Member->DataKind = static_cast<enum DataKind>(DwordResult);

			IDiaSymbol *MemberTypeDiaSymbol;
			DiaChildSymbol->get_type(&MemberTypeDiaSymbol);
			Member->Type = GetSymbol(MemberTypeDiaSymbol);
			MemberTypeDiaSymbol->Release();
			if (symTag == SymTagBaseClass)
			{
				++Symbol->u.Udt.BaseClassCount;
				if (Symbol->u.Udt.BaseClassFields == 0)
					Symbol->u.Udt.BaseClassFields = (SYMBOL_UDT_BASECLASS *)malloc(sizeof(SYMBOL_UDT_BASECLASS));
				else	Symbol->u.Udt.BaseClassFields = (SYMBOL_UDT_BASECLASS *)realloc(Symbol->u.Udt.BaseClassFields, sizeof(SYMBOL_UDT_BASECLASS)*Symbol->u.Udt.BaseClassCount);
				Symbol->u.Udt.BaseClassFields[Symbol->u.Udt.BaseClassCount-1].Type = Member->Type;
				DWORD Access = 0;
				DiaChildSymbol->get_access(&Access);
				Symbol->u.Udt.BaseClassFields[Symbol->u.Udt.BaseClassCount-1].Access = Access;
				BOOL IsVirtual = FALSE;
				DiaChildSymbol->get_virtualBaseClass(&IsVirtual);
				Symbol->u.Udt.BaseClassFields[Symbol->u.Udt.BaseClassCount-1].IsVirtual = IsVirtual;
				Member->IsBaseClass = TRUE;
			}
		} else
		{
			Member->Type = GetSymbol(DiaChildSymbol);
		#if 0
			if (symTag == SymTagFunction)
			{
				if (Member->Type->u.Function.IsOverride && Symbol->u.Udt.BaseClassCount)
				{
					for (DWORD i = 0; i < Symbol->u.Udt.BaseClassCount; ++i)
					{
						SYMBOL *TmpSymbol = Symbol->u.Udt.BaseClassFields[i].Type;
						for (DWORD j = 0; j < min(TmpSymbol->u.Udt.FieldCount, Index); ++j)
						{
							if (TmpSymbol->u.Udt.Fields[j].Type->Tag == SymTagFunction &&
							    strcmp(TmpSymbol->u.Udt.Fields[j].Name, Member->Type->Name) == 0 &&
							    TmpSymbol->u.Udt.Fields[j].Type->u.Function.ArgumentCount == Member->Type->u.Function.ArgumentCount)
							{
								Member->Type->u.Function.VirtualOffset = TmpSymbol->u.Udt.Fields[j].Type->u.Function.VirtualOffset;
							}
						}
					}
				}
			} else
		#endif
			if (symTag == SymTagUDT)
			{
				Member->Type->u.Function.IsOverride = TRUE;
			}
		}
		DiaChildSymbol->Release();
		Index += 1;
	}

	DiaSymbolEnumerator->Release();

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

VOID SymbolModule::ProcessSymbolFunctionEx(IN IDiaSymbol* DiaSymbol, IN SYMBOL* Symbol)
{
	BOOL IsStatic = FALSE;
	DiaSymbol->get_isStatic(&IsStatic);
	Symbol->u.Function.IsStatic = IsStatic;

	BOOL IsVirtual = FALSE;
	DiaSymbol->get_virtual(&IsVirtual);
	Symbol->u.Function.IsVirtual = IsVirtual;
	Symbol->u.Function.IsOverride = FALSE;

	BOOL IsIntro = FALSE;
	if (!DiaSymbol->get_intro(&IsIntro) && !IsIntro && IsVirtual)
	{
		Symbol->u.Function.IsOverride = TRUE;
	}

	Symbol->u.Function.VirtualOffset = -1;
	if (IsVirtual == TRUE)
	{
		DWORD VirtualOffset = 0;
		DiaSymbol->get_virtualBaseOffset(&VirtualOffset);
		Symbol->u.Function.VirtualOffset = VirtualOffset;
	}

	BOOL IsConst = FALSE;
	DiaSymbol->get_constType(&IsConst);
	Symbol->u.Function.IsConst = IsConst;

	BOOL IsPure = FALSE;
	if (IsVirtual == TRUE)
	{
		DiaSymbol->get_pure(&IsPure);
	}
	Symbol->u.Function.IsPure = IsPure;

	IDiaSymbol *DiaArgumentTypeSymbol;
	DiaSymbol->get_type(&DiaArgumentTypeSymbol);
	ProcessSymbolFunction(DiaArgumentTypeSymbol, Symbol);
	DiaArgumentTypeSymbol->Release();
}

void SymbolModule::DestroySymbol(IN SYMBOL* Symbol)
{
	delete []Symbol->Name;

	switch (Symbol->Tag)
	{
	case SymTagUDT:
		for (DWORD i = 0; i < Symbol->u.Udt.FieldCount; ++i)
			delete []Symbol->u.Udt.Fields[i].Name;

		delete []Symbol->u.Udt.Fields;
		if (Symbol->u.Udt.BaseClassCount)
			free(Symbol->u.Udt.BaseClassFields);
		break;

	case SymTagEnum:
		for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; ++i)
			delete []Symbol->u.Enum.Fields[i].Name;

		delete []Symbol->u.Enum.Fields;
		break;

	case SymTagFunctionType:
		delete []Symbol->u.Function.Arguments;
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
	{ btNoType,       0,  "btNoType",         "<NoType>"         }, //nullptr
	{ btVoid,         0,  "btVoid",           "void"             },
	{ btChar,         1,  "btChar",           "char"             },
	{ btChar16,       2,  "btChar16",         "char16_t"         },
	{ btChar32,       4,  "btChar32",         "char32_t"         },
	{ btWChar,        2,  "btWChar",          "wchar_t"          },
	{ btInt,          1,  "btInt",            "char"             },
	{ btInt,          2,  "btInt",            "short"            },
	{ btInt,          4,  "btInt",            "int"              },
	{ btInt,          8,  "btInt",            "__int64"          },
	{ btUInt,        16,  "btInt",            "__m128"           },
	{ btUInt,         1,  "btUInt",           "unsigned char"    },
	{ btUInt,         2,  "btUInt",           "unsigned short"   },
	{ btUInt,         4,  "btUInt",           "unsigned int"     },
	{ btUInt,         8,  "btUInt",           "unsigned __int64" },
	{ btUInt,        16,  "btUInt",           "__m128"           },
	{ btFloat,        4,  "btFloat",          "float"            },
	{ btFloat,        8,  "btFloat",          "double"           },
	{ btFloat,       10,  "btFloat",          "long double"      }, // 80-bit float
	{ btBCD,          0,  "btBCD",            "BCD"              },
	{ btBool,         0,  "btBool",           "BOOL"             },
	{ btLong,         4,  "btLong",           "long"             },
	{ btULong,        4,  "btULong",          "unsigned long"    },
	{ btCurrency,     0,  "btCurrency",       "<NoType>"         }, //nullptr
	{ btDate,         0,  "btDate",           "DATE"             },
	{ btVariant,      0,  "btVariant",        "VARIANT"          },
	{ btComplex,      0,  "btComplex",        "<NoType>"         }, //nullptr
	{ btBit,          0,  "btBit",            "<NoType>"         }, //nullptr
	{ btBSTR,         0,  "btBSTR",           "BSTR"             },
	{ btHresult,      4,  "btHresult",        "HRESULT"          },
	{ (BasicType)0,   0,  nullptr,            "<NoType>"         }, //nullptr
};

PDB::PDB()
{
	m_Impl = new SymbolModule();
}

PDB::PDB(IN const CHAR* Path)
{
	m_Impl = new SymbolModule();
	m_Impl->Open(Path);
}

PDB::~PDB()
{
	delete m_Impl;
}

BOOL PDB::Open(IN const CHAR* Path)
{
	return m_Impl->Open(Path);
}

BOOL PDB::IsOpened() const
{
	return m_Impl->IsOpen();
}

const CHAR* PDB::GetPath() const
{
	return m_Impl->GetPath();
}

VOID PDB::Close()
{
	m_Impl->Close();
}

DWORD PDB::GetMachineType() const
{
	return m_Impl->GetMachineType();
}

CV_CFL_LANG PDB::GetLanguage() const
{
	return m_Impl->GetLanguage();
}

const SYMBOL* PDB::GetSymbolByName(IN const CHAR* SymbolName)
{
	return m_Impl->GetSymbolByName(SymbolName);
}

const SYMBOL* PDB::GetSymbolByTypeId(IN DWORD TypeId)
{
	return m_Impl->GetSymbolByTypeId(TypeId);
}

const SymbolMap& PDB::GetSymbolMap() const
{
	return m_Impl->GetSymbolMap();
}

const SymbolNameMap& PDB::GetSymbolNameMap() const
{
	return m_Impl->GetSymbolNameMap();
}

const FunctionSet& PDB::GetFunctionSet() const
{
	return m_Impl->GetFunctionSet();
}

const CHAR* PDB::GetBasicTypeString(IN BasicType BaseType, IN DWORD Size)
{
	BasicTypeMapElement* TypeMap = BasicTypeMapMSVC;

	for (int n = 0; TypeMap[n].BasicTypeString != nullptr; ++n)
	{
		if (TypeMap[n].BaseType == BaseType)
		{
			if (TypeMap[n].Length == Size ||
			    TypeMap[n].Length == 0)
			{
				return TypeMap[n].TypeString; //TODO NULL ???
			}
		}
	}

	return "<NoType>"; //nullptr;
}

const CHAR* PDB::GetBasicTypeString(IN const SYMBOL* Symbol)
{
	return GetBasicTypeString(Symbol->BaseType, Symbol->Size);
}

const CHAR* PDB::GetUdtKindString(IN UdtKind Kind)
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

BOOL PDB::IsUnnamedSymbol(const SYMBOL* Symbol)
{
	return strstr(Symbol->Name, "<anonymous-") != nullptr ||
	       strstr(Symbol->Name, "<unnamed-") != nullptr ||
	       strstr(Symbol->Name, "__unnamed") != nullptr;
}
