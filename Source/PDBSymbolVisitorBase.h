#pragma once
#include "PDB.h"

class PDBSymbolVisitorBase
{
	public:
		virtual
		~PDBSymbolVisitorBase() = default;

		virtual
		void
		Visit(
			const SYMBOL* Symbol
			)
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

				case SymTagFunctionType:
					VisitFunctionType(Symbol);
					break;

				case SymTagFunctionArgType:
					VisitFunctionArgType(Symbol);
					break;

				case SymTagUDT:
					VisitUserDataType(Symbol);
					break;

				default:
					VisitOtherType(Symbol);
					break;
			}
		}

	protected:
		virtual
		void
		VisitBaseType(
			const SYMBOL* Symbol
			)
		{
		
		}

		virtual
		void
		VisitEnumType(
			const SYMBOL* Symbol
			)
		{
			for (DWORD i = 0; i < Symbol->u.Enum.FieldCount; i++)
			{
				VisitEnumField(&Symbol->u.Enum.Fields[i]);
			}
		}

		virtual
		void
		VisitTypedefType(
			const SYMBOL* Symbol
			)
		{
			Visit(Symbol->u.Typedef.Type);
		}

		virtual
		void
		VisitPointerType(
			const SYMBOL* Symbol
			)
		{
			Visit(Symbol->u.Pointer.Type);
		}

		virtual
		void
		VisitArrayType(
			const SYMBOL* Symbol
			)
		{
			Visit(Symbol->u.Array.ElementType);
		}

		virtual
		void
		VisitFunctionType(
			const SYMBOL* Symbol
			)
		{
			for (DWORD i = 0; i < Symbol->u.Function.ArgumentCount; i++)
			{
				Visit(Symbol->u.Function.Arguments[i]);
			}
		}

		virtual
		void
		VisitFunctionArgType(
			const SYMBOL* Symbol
			)
		{
			Visit(Symbol->u.FunctionArg.Type);
		}

		virtual
		void
		VisitUserDataType(
			const SYMBOL* Symbol
			)
		{
			const SYMBOL_USERDATA_FIELD* UserDataField;
			DWORD UserDataFieldCount;
			DWORD UserDataFieldOffset;
			DWORD BitSum = 0;

			UserDataField = Symbol->u.UserData.Fields;
			UserDataFieldCount = Symbol->u.UserData.FieldCount;

			for (DWORD i = 0; i < UserDataFieldCount; i++)
			{
				UserDataField = &Symbol->u.UserData.Fields[i];
				UserDataFieldOffset = UserDataField->Offset;

				if (UserDataField->Bits == 0)
				{
					VisitUserDataFieldBegin(UserDataField);
					VisitUserDataField(UserDataField);
					VisitUserDataFieldEnd(UserDataField);
					continue;
				}

				VisitUserDataFieldBitFieldBegin(UserDataField);
				for (; i < UserDataFieldCount; i++)
				{
					UserDataField = &Symbol->u.UserData.Fields[i];

					BitSum += UserDataField->Bits;

					VisitUserDataFieldBitField(UserDataField);

					if (BitSum == UserDataField->Type->Size * 8)
					{
						VisitUserDataFieldBitFieldEnd(UserDataField);
						BitSum = 0;
						break;
					}
				}
			}
		}

		virtual
		void
		VisitOtherType(
			const SYMBOL* Symbol
			)
		{

		}

		virtual
		void
		VisitEnumField(
			const SYMBOL_ENUM_FIELD* EnumField
			)
		{

		}

		virtual
		void
		VisitUserDataFieldBegin(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		virtual
		void
		VisitUserDataFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		virtual
		void
		VisitUserDataField(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		virtual
		void
		VisitUserDataFieldBitFieldBegin(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		virtual
		void
		VisitUserDataFieldBitFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		virtual
		void
		VisitUserDataFieldBitField(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{
			//
			// Call VisitUserDataField by default.
			//

			VisitUserDataField(UserDataField);
		}
};

