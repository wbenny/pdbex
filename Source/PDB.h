#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#include <dia2.h>

#include <unordered_set>
#include <unordered_map>

typedef struct _SYMBOL SYMBOL, *PSYMBOL;

//
// Representation of the enum field.
//
// enum XYZ
// {
//   // Name       Value
//   // -----------------
//      XYZ_1   =   2,
//      XYZ_2   =   4,
// };
//
// Note that 'Value' is represented as the VARIANT type.
//
typedef struct _SYMBOL_ENUM_FIELD
{
	//
	// Name of the enumeration field.
	//
	CHAR*                   Name;

	//
	// Assigned value of the enumeration field.
	//
	VARIANT                 Value;

	//
	// Parent enumeration.
	//
	SYMBOL*                 Parent;

} SYMBOL_ENUM_FIELD, *PSYMBOL_ENUM_FIELD;

//
// Representation of the struct/class/union field.
//
// struct XYZ
// {
//    // Type     Name    Bits    Offset     BitPosition
//    // -------------------------------------------------
//       int     XYZ_1;        //  0            0
//       short   XYZ_2  :  3;  //  4            0
//       short   XYZ_3  :  13; //  4            3
// };
//
typedef struct _SYMBOL_USERDATA_FIELD
{
	//
	// Name of the user data field.
	//
	CHAR*                   Name;

	//
	// Type of the field.
	//
	SYMBOL*                 Type;

	//
	// Offset from the start of the structure/class/union.
	//
	DWORD                   Offset;

	//
	// Amount of bits this field takes.
	// If this value is 0, this field takes
	// all of the space of the field type (Type->Size bytes).
	//
	DWORD                   Bits;

	//
	// Which bit this fields starts at (relative to the Offset).
	//
	DWORD                   BitPosition;

	//
	// Parent User Data symbol.
	//
	SYMBOL*                 Parent;

} SYMBOL_USERDATA_FIELD, *PSYMBOL_USERDATA_FIELD;

//
// Representation of the enumeration.
//
// Example for FieldCount = 3
//
// enum XYZ
// {
//   XYZ_1,
//   XYZ_2,
//   XYZ_3,
// };
//
typedef struct _SYMBOL_ENUM
{
	//
	// Count of fields in the enumeration.
	//
	DWORD                   FieldCount;

	//
	// Pointer to the continuous array of structures of the enumeration fields.
	//
	SYMBOL_ENUM_FIELD*      Fields;

} SYMBOL_ENUM, *PSYMBOL_ENUM;

//
// Representation of the typedef statement.
//
typedef struct _SYMBOL_TYPEDEF
{
	//
	// Underlying type of the type definition.
	//
	SYMBOL*                 Type;

} SYMBOL_TYPEDEF, *PSYMBOL_TYPEDEF;

//
// Representation of the pointer statement.
//
typedef struct _SYMBOL_POINTER
{
	//
	// Underlying type of the pointer definition.
	//
	SYMBOL*                 Type;

	//
	// Specifies if the pointer represents the reference.
	//
	BOOL                    IsReference;

} SYMBOL_POINTER, *PSYMBOL_POINTER;

//
// Representation of the array.
//
typedef struct _SYMBOL_ARRAY
{
	//
	// Type of the array element.
	//
	SYMBOL*                 ElementType;

	//
	// Array size in elements.
	//
	DWORD                   ElementCount;

} SYMBOL_ARRAY, *PSYMBOL_ARRAY;

//
// Representation of the function.
//
typedef struct _SYMBOL_FUNCTION
{
	//
	// Return type of the function.
	//
	SYMBOL*                 ReturnType;

	//
	// Calling convention of the function.
	//
	CV_call_e               CallingConvention;

	//
	// Number of arguments.
	//
	DWORD                   ArgumentCount;

	//
	// Pointer to the continuous array of pointers to the symbol structure for arguments.
	// These symbols are of type SYMBOL_FUNCTIONARG and has tag SymTagFunctionArgType.
	//
	SYMBOL**                Arguments;

} SYMBOL_FUNCTION, *PSYMBOL_FUNCTION;

//
// Representation of the function argument type.
//
typedef struct _SYMBOL_FUNCTIONARG
{
	//
	// Underlying type of the argument.
	//
	PSYMBOL                 Type;

} SYMBOL_FUNCTIONARG, *PSYMBOL_FUNCTIONARG;

//
// Representation of the user data type (struct/class/union).
//
typedef struct _SYMBOL_USERDATA
{
	//
	// Kind of the user data type.
	// It may be either UdtStruct, UdtClass or UdtUnion.
	//
	UdtKind                 Kind;

	//
	// Number of fields (members) in the user data type.
	//
	DWORD                   FieldCount;

	//
	// Pointer to the continuous array of structures of the user data fields.
	//
	SYMBOL_USERDATA_FIELD*  Fields;

} SYMBOL_USERDATA, *PSYMBOL_USERDATA;

//
// Representation of the debug symbol.
//
struct _SYMBOL
{
	//
	// Type of the symbol.
	//
	enum SymTagEnum         Tag;

	//
	// Base type.
	// Only set if Tag == SymTagBaseType.
	//
	BasicType               BaseType;

	//
	// Internal ID of the type.
	//
	DWORD                   TypeId;

	//
	// Total size of the type which this symbol represents.
	//
	DWORD                   Size;

	//
	// Specifies constness.
	//
	BOOL                    IsConst;

	//
	// Specifies volatileness.
	//
	BOOL                    IsVolatile;

	//
	// Name of the type.
	//
	CHAR*                   Name;

	union
	{
		SYMBOL_ENUM           Enum;
		SYMBOL_TYPEDEF        Typedef;
		SYMBOL_POINTER        Pointer;
		SYMBOL_ARRAY          Array;
		SYMBOL_FUNCTION       Function;
		SYMBOL_FUNCTIONARG    FunctionArg;
		SYMBOL_USERDATA       UserData;
	} u;
};

class SymbolModule;

using SymbolMap     = std::unordered_map<DWORD, SYMBOL*>;
using SymbolNameMap = std::unordered_map<std::string, SYMBOL*>;
using SymbolSet     = std::unordered_set<SYMBOL*>;

class PDB
{
	public:
		//
		// Default constructor.
		//
		PDB();

		//
		// Instantiates PDB class with particular PDB file.
		//
		PDB(
			IN CONST CHAR* Path
			);

		//
		// Destructor.
		//
		~PDB();

		//
		// Opens particular PDB file and parses it.
		//
		// Returns non-zero value on success.
		//
		BOOL
		Open(
			IN CONST CHAR* Path
			);

		//
		// Returns TRUE if a PDB file is opened.
		//
		BOOL
		IsOpened() const;

		//
		// Returns path of the currently opened PDB file.
		//
		CONST CHAR*
		GetPath() const;

		//
		// Closes all resources which holds the opened PDB file.
		//
		VOID
		Close();

		//
		// Returns a SYMBOL structure of particular name.
		//
		// Returns non-NULL value on success.
		//
		CONST SYMBOL*
		GetSymbolByName(
			IN CONST CHAR* SymbolName
			);

		//
		// Returns a SYMBOL structure of particular Type ID.
		//
		// Returns non-NULL value on success.
		//
		CONST SYMBOL*
		GetSymbolByTypeId(
			IN DWORD TypeId
			);

		//
		// Returns collection of all symbols.
		//
		const SymbolMap&
		GetSymbolMap() const;

		//
		// Returns collection of all named symbols.
		//
		const SymbolNameMap&
		GetSymbolNameMap() const;

		//
		// Returns C-like name of the type of provided symbol.
		// The symbol must be BaseType.
		//
		// Returns non-NULL value on success.
		//
		static
		CONST CHAR*
		PDB::GetBasicTypeString(
			IN BasicType BaseType,
			IN DWORD Size,
			IN BOOL UseStdInt = FALSE
			);

		//
		// Returns C-like name of the type of provided symbol.
		// The symbol must be BaseType.
		//
		// Returns non-NULL value on success.
		//
		static
		CONST CHAR*
		GetBasicTypeString(
			IN CONST SYMBOL* Symbol,
			IN BOOL UseStdInt = FALSE
			);

		//
		// Returns string representing the kind
		// of provided User Data Type.
		//
		static
		CONST CHAR*
		GetUdtKindString(
			IN UdtKind UdtKind
			);

		static
		BOOL
		IsUnnamedSymbol(
			CONST SYMBOL* Symbol
			);

	private:
		SymbolModule* m_Impl;
};
