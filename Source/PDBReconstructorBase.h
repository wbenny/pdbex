#pragma once

#include "PDB.h"
#include "UserDataFieldDefinitionBase.h"

class PDBReconstructorBase
{
	public:
		//
		// Called when reached the 'enum' type.
		// If the return value is true, the enum will be expanded.
		//
		virtual
		bool
		OnEnumType(
			const SYMBOL* Symbol
			)
		{
			return false;
		}

		//
		// Called when entering into the 'enum' type
		// which will be expanded.
		//
		virtual
		void
		OnEnumTypeBegin(
			const SYMBOL* Symbol
			)
		{

		}

		//
		// Called when leaving from the 'enum' type.
		//
		virtual
		void
		OnEnumTypeEnd(
			const SYMBOL* Symbol
			)
		{

		}

		//
		// Called for each field of the curent 'enum' type.
		//
		virtual
		void
		OnEnumField(
			const SYMBOL_ENUM_FIELD* EnumField
			)
		{

		}

		//
		// Called when reached the user data type (struct/class/union)
		// If the return value is true, the user data type will be expanded.
		//
		virtual
		bool
		OnUserDataType(
			const SYMBOL* Symbol
			)
		{
			return false;
		}

		//
		// Called when entering into the user data type (struct/class/union)
		// which will be expanded.
		//
		virtual
		void
		OnUserDataTypeBegin(
			const SYMBOL* Symbol
			)
		{

		}

		//
		// Called when leaving from the current user data type.
		//
		virtual
		void
		OnUserDataTypeEnd(
			const SYMBOL* Symbol
			)
		{

		}

		//
		// Called when entering into the field of the current user data type.
		//
		virtual
		void
		OnUserDataFieldBegin(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		//
		// Called when leaving from the field of the current user data type.
		//
		virtual
		void
		OnUserDataFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			)
		{

		}

		//
		// Called for each field in the current user data type.
		//
		virtual
		void
		OnUserDataField(
			const SYMBOL_USERDATA_FIELD* UserDataField,
			UserDataFieldDefinitionBase* MemberDefinition
			)
		{

		}

		//
		// Called when entering into the nested anonymous user data type (struct/class/union)
		// which will be expanded.
		//
		virtual
		void
		OnAnonymousUserDataTypeBegin(
			UdtKind UserDataTypeKind,
			const SYMBOL_USERDATA_FIELD* FirstUserDataField
			)
		{

		}

		//
		// Called when leaving from the current nested anonymous user data type.
		//
		virtual
		void
		OnAnonymousUserDataTypeEnd(
			UdtKind UserDataTypeKind,
			const SYMBOL_USERDATA_FIELD* FirstUserDataField,
			const SYMBOL_USERDATA_FIELD* LastUserDataField,
			DWORD Size
			)
		{

		}

		//
		// Called when entering the bitfield.
		//
		virtual
		void
		OnUserDataFieldBitFieldBegin(
			const SYMBOL_USERDATA_FIELD* FirstUserDataFieldBitField,
			const SYMBOL_USERDATA_FIELD* LastUserDataFieldBitField
			)
		{

		}

		//
		// Called when leaving the bitfield.
		//
		virtual
		void
		OnUserDataFieldBitFieldEnd(
			const SYMBOL_USERDATA_FIELD* FirstUserDataFieldBitField,
			const SYMBOL_USERDATA_FIELD* LastUserDataFieldBitField
			)
		{

		}

		//
		// Called when a padding member should be created.
		//
		virtual
		void
		OnPaddingMember(
			const SYMBOL_USERDATA_FIELD* UserDataField,
			BasicType PaddingBasicType,
			DWORD PaddingBasicTypeSize,
			DWORD PaddingSize
			)
		{

		}
};
