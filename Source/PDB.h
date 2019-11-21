#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#include <dia2.h>

#include <set>
#include <unordered_set>
#include <unordered_map>

typedef struct _SYMBOL SYMBOL, *PSYMBOL;

typedef struct _SYMBOL_ENUM_FIELD
{
	CHAR*                Name;
	VARIANT              Value;
	SYMBOL*              Parent;

} SYMBOL_ENUM_FIELD, *PSYMBOL_ENUM_FIELD;

typedef struct _SYMBOL_UDT_FIELD
{
	enum SymTagEnum      Tag;
	CHAR*                Name;
	SYMBOL*              Type;
	DWORD                Offset;
	DWORD                Bits;
	DWORD                BitPosition;
	SYMBOL*              Parent;
	BOOL                 IsBaseClass;

} SYMBOL_UDT_FIELD, *PSYMBOL_UDT_FIELD;

typedef struct _SYMBOL_ENUM
{
	DWORD                FieldCount;
	SYMBOL_ENUM_FIELD*   Fields;

} SYMBOL_ENUM, *PSYMBOL_ENUM;

typedef struct _SYMBOL_TYPEDEF
{
	SYMBOL*              Type;

} SYMBOL_TYPEDEF, *PSYMBOL_TYPEDEF;

typedef struct _SYMBOL_POINTER
{
	SYMBOL*              Type;
	BOOL                 IsReference;

} SYMBOL_POINTER, *PSYMBOL_POINTER;

typedef struct _SYMBOL_ARRAY
{
	SYMBOL*              ElementType;
	DWORD                ElementCount;

} SYMBOL_ARRAY, *PSYMBOL_ARRAY;

typedef struct _SYMBOL_FUNCTION
{
	SYMBOL*              ReturnType;
	CV_call_e            CallingConvention;
	BOOL                 IsStatic;
	BOOL                 IsVirtual;
	DWORD                VirtualOffset;
	BOOL                 IsOverride;
	BOOL                 IsConst;
	BOOL                 IsPure;
	DWORD                ArgumentCount;
	SYMBOL**             Arguments;

} SYMBOL_FUNCTION, *PSYMBOL_FUNCTION;

typedef struct _SYMBOL_FUNCTIONARG
{
	SYMBOL*              Type;

} SYMBOL_FUNCTIONARG, *PSYMBOL_FUNCTIONARG;

typedef struct _SYMBOL_UDT_BASECLASS
{
	SYMBOL*              Type;
	DWORD                Access;
	BOOL                 IsVirtual;
} SYMBOL_UDT_BASECLASS;

typedef struct _SYMBOL_UDT
{
	UdtKind              Kind;
	DWORD                FieldCount;
	SYMBOL_UDT_FIELD*    Fields;
	DWORD                BaseClassCount;
	SYMBOL_UDT_BASECLASS *BaseClassFields;

} SYMBOL_UDT, *PSYMBOL_UDT;

struct _SYMBOL
{
	enum SymTagEnum      Tag;
	enum DataKind        DataKind;
	BasicType            BaseType;
	DWORD                TypeId;
	DWORD                Size;
	BOOL                 IsConst;
	BOOL                 IsVolatile;
	CHAR*                Name;

	union
	{
		SYMBOL_ENUM        Enum;
		SYMBOL_TYPEDEF     Typedef;
		SYMBOL_POINTER     Pointer;
		SYMBOL_ARRAY       Array;
		SYMBOL_FUNCTION    Function;
		SYMBOL_FUNCTIONARG FunctionArg;
		SYMBOL_UDT         Udt;
	} u;
};

class SymbolModule;

using SymbolMap     = std::unordered_map<DWORD, SYMBOL*>;
using SymbolNameMap = std::unordered_map<std::string, SYMBOL*>;
using SymbolSet     = std::unordered_set<SYMBOL*>;
using FunctionSet   = std::set<std::string>;

class PDB
{
public:
	PDB();
	PDB(IN const CHAR* Path);
	~PDB();
	BOOL Open(IN const CHAR* Path);
	BOOL IsOpened() const;
	const CHAR* GetPath() const;
	VOID Close();
	DWORD GetMachineType() const;
	CV_CFL_LANG GetLanguage() const;
	const SYMBOL* GetSymbolByName(IN const CHAR* SymbolName);
	const SYMBOL* GetSymbolByTypeId(IN DWORD TypeId);
	const SymbolMap& GetSymbolMap() const;
	const SymbolNameMap& GetSymbolNameMap() const;
	const FunctionSet& GetFunctionSet() const;
	static const CHAR* GetBasicTypeString(IN BasicType BaseType, IN DWORD Size);
	static const CHAR* GetBasicTypeString(IN const SYMBOL* Symbol);
	static const CHAR* GetUdtKindString(IN UdtKind Kind);
	static BOOL IsUnnamedSymbol(const SYMBOL* Symbol);

private:
	SymbolModule* m_Impl;
};
