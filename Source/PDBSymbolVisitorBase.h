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
			const SYMBOL_USERDATA_FIELD* EndOfUserDataField;

			if (Symbol->u.UserData.FieldCount == 0)
			{
				//
				// Early return on empty UDTs.
				//

				return;
			}

			UserDataField = Symbol->u.UserData.Fields;
			EndOfUserDataField = &Symbol->u.UserData.Fields[Symbol->u.UserData.FieldCount];

			do
			{
				if (UserDataField->Bits == 0)
				{
					//
					// Non-bitfield member.
					//
					VisitUserDataFieldBegin(UserDataField);
					VisitUserDataField(UserDataField);
					VisitUserDataFieldEnd(UserDataField);
				}
				else
				{
					//
					// UserDataField now points to the first member of the bitfield.
					//
					VisitUserDataFieldBitFieldBegin(UserDataField);

					do
					{
						//
						// Visit all bitfield members
						//
						VisitUserDataFieldBitField(UserDataField);
					} while (++UserDataField < EndOfUserDataField &&
					           UserDataField->BitPosition != 0);

					//
					// UserDataField now points behind the last bitfield member.
					// So decrement the iterator and call VisitUserDataFieldBitFieldEnd().
					//
					VisitUserDataFieldBitFieldEnd(--UserDataField);
				}
			} while (++UserDataField < EndOfUserDataField);
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

