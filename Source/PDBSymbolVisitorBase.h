#pragma once
#include "PDB.h"

class PDBSymbolVisitorBase
{
public:
	virtual ~PDBSymbolVisitorBase() = default;

	virtual	void Visit(const SYMBOL* Symbol)
	{
		switch (Symbol->Tag)
		{
		case SymTagBaseType:
			VisitBaseType(Symbol);
			break;
		case SymTagEnum:
			VisitEnumType(Symbol);
			break;
		case SymTagTypedef:
			VisitTypedefType(Symbol);
			break;
		case SymTagPointerType:
			VisitPointerType(Symbol);
			break;
		case SymTagArrayType:
			VisitArrayType(Symbol);
			break;
		case SymTagFunction:
		case SymTagFunctionType:
			VisitFunctionType(Symbol);
			break;
		case SymTagFunctionArgType:
			VisitFunctionArgType(Symbol);
			break;
		case SymTagUDT:
			VisitUdt(Symbol);
			break;
		default:
			VisitOtherType(Symbol);
			break;
		}
	}

protected:
	virtual	void VisitBaseType(const SYMBOL* Symbol)
	{
	}

	virtual	void VisitEnumType(const SYMBOL* Symbol)
	{
		for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; ++i)
		{
			VisitEnumField(&Symbol->u.Enum.Fields[i]);
		}
	}

	virtual	void VisitTypedefType(const SYMBOL* Symbol)
	{
		Visit(Symbol->u.Typedef.Type);
	}

	virtual void VisitPointerType(const SYMBOL* Symbol)
	{
		Visit(Symbol->u.Pointer.Type);
	}

	virtual void VisitArrayType(const SYMBOL* Symbol)
	{
		Visit(Symbol->u.Array.ElementType);
	}

	virtual void VisitFunctionType(const SYMBOL* Symbol)
	{
		for (DWORD i = 0; i < Symbol->u.Function.ArgumentCount; ++i)
		{
			Visit(Symbol->u.Function.Arguments[i]);
		}
		if (Symbol->u.Function.ReturnType)
			Visit(Symbol->u.Function.ReturnType);
	}

	virtual void VisitFunctionArgType(const SYMBOL* Symbol)
	{
		Visit(Symbol->u.FunctionArg.Type);
	}

	virtual void VisitUdt(const SYMBOL* Symbol)
	{
		if (Symbol->u.Udt.FieldCount == 0)
		{
			return;
		}

		const SYMBOL_UDT_FIELD* UdtField = Symbol->u.Udt.Fields;
		const SYMBOL_UDT_FIELD* EndOfUdtField = &Symbol->u.Udt.Fields[Symbol->u.Udt.FieldCount];

		do {
			if (UdtField->Bits == 0)
			{
				VisitUdtFieldBegin(UdtField);
				VisitUdtField(UdtField);
				VisitUdtFieldEnd(UdtField);
			} else
			{
				VisitUdtFieldBitFieldBegin(UdtField);

				do {
					VisitUdtFieldBitField(UdtField);
				} while (++UdtField < EndOfUdtField && UdtField->BitPosition != 0);

				VisitUdtFieldBitFieldEnd(--UdtField);
			}
		} while (++UdtField < EndOfUdtField);
	}

	virtual void VisitOtherType(const SYMBOL* Symbol) {}

	virtual	void VisitEnumField(const SYMBOL_ENUM_FIELD* EnumField) {}

	virtual void VisitUdtFieldBegin(const SYMBOL_UDT_FIELD* UdtField) {}
	virtual void VisitUdtFieldEnd(const SYMBOL_UDT_FIELD* UdtField) {}

	virtual void VisitUdtField(const SYMBOL_UDT_FIELD* UdtField) {}

	virtual	void VisitUdtFieldBitFieldBegin(const SYMBOL_UDT_FIELD* UdtField) {}
	virtual	void VisitUdtFieldBitFieldEnd(const SYMBOL_UDT_FIELD* UdtField)	{}

	virtual	void VisitUdtFieldBitField(const SYMBOL_UDT_FIELD* UdtField)
	{
		VisitUdtField(UdtField);
	}
};
