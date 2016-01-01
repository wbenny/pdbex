#pragma once
#include "PDBReconstructorBase.h"

#include <iostream>
#include <numeric> // std::accumulate
#include <string>
#include <map>
#include <set>

#include <cassert>

class PDBHeaderReconstructor
	: public PDBReconstructorBase
{
	public:
		enum class MemberStructExpansionType
		{
			//
			// No expansion of nested user data types.
			//
			None,

			//
			// Expand only unnamed user data types.
			//
			InlineUnnamed,

			//
			// Expand all references user data types.
			//
			InlineAll,
		};

		struct Settings
		{
			Settings()
			{
				MemberStructExpansion   = MemberStructExpansionType::InlineUnnamed;
				OutputFile              = &std::cout;
				TestFile                = nullptr;
				PaddingMemberPrefix     = "Padding_";
				UnnamedTypePrefix       = "TAG_UNNAMED_";
				AnonymousStructPrefix   = "s";  // DUMMYSTRUCTNAME (up to 6)
				AnonymousUnionPrefix    = "u";  // DUMMYUNIONNAME  (up to 9)
				CreatePaddingMembers    = true;
				ShowOffsets             = true;
				MicrosoftTypedefs       = true;
				AllowBitFieldsInUnion   = false;
				AllowAnonymousDataTypes = true;
			}

			MemberStructExpansionType MemberStructExpansion;
			std::ostream*             OutputFile;
			std::ostream*             TestFile;
			std::string               PaddingMemberPrefix;
			std::string               UnnamedTypePrefix;
			std::string               SymbolPrefix;
			std::string               SymbolSuffix;
			std::string               AnonymousStructPrefix;
			std::string               AnonymousUnionPrefix;
			bool                      CreatePaddingMembers    : 1;
			bool                      ShowOffsets             : 1;
			bool                      MicrosoftTypedefs       : 1;
			bool                      AllowBitFieldsInUnion   : 1;
			bool                      AllowAnonymousDataTypes : 1;
		};

		PDBHeaderReconstructor(
			Settings* VisitorSettings = nullptr
			);

		void
		Clear();

		const std::string&
		GetCorrectedSymbolName(
			const SYMBOL* Symbol
			) const;

	protected:
		bool
		OnEnumType(
			const SYMBOL* Symbol
			) override;

		void
		OnEnumTypeBegin(
			const SYMBOL* Symbol
			) override;

		void
		OnEnumTypeEnd(
			const SYMBOL* Symbol
			) override;

		void
		OnEnumField(
			const SYMBOL_ENUM_FIELD* EnumField
			) override;

		bool
		OnUserDataType(
			const SYMBOL* Symbol
			) override;

		void
		OnUserDataTypeBegin(
			const SYMBOL* Symbol
			) override;

		void
		OnUserDataTypeEnd(
			const SYMBOL* Symbol
			) override;

		void
		OnUserDataFieldBegin(
			const SYMBOL_USERDATA_FIELD* UserDataField
			) override;

		void
		OnUserDataFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			) override;

		void
		OnUserDataField(
			const SYMBOL_USERDATA_FIELD* UserDataField,
			UserDataFieldDefinitionBase* MemberDefinition
			) override;

		void
		OnAnonymousUserDataTypeBegin(
			UdtKind UserDataTypeKind,
			const SYMBOL_USERDATA_FIELD* FirstUserDataField
			);

		void
		OnAnonymousUserDataTypeEnd(
			UdtKind UserDataTypeKind,
			const SYMBOL_USERDATA_FIELD* FirstUserDataField,
			const SYMBOL_USERDATA_FIELD* LastUserDataField
			) override;

		void
		OnUserDataFieldBitFieldBegin(
			const SYMBOL_USERDATA_FIELD* FirstUserDataFieldBitField,
			const SYMBOL_USERDATA_FIELD* LastUserDataFieldBitField
			) override;

		void
		OnUserDataFieldBitFieldEnd(
			const SYMBOL_USERDATA_FIELD* FirstUserDataFieldBitField,
			const SYMBOL_USERDATA_FIELD* LastUserDataFieldBitField
			) override;

		void
		OnPaddingMember(
			const SYMBOL_USERDATA_FIELD* UserDataField,
			BasicType PaddingBasicType,
			ULONG64 PaddingBasicTypeSize,
			DWORD PaddingSize
			) override;

	private:
		void
		Write(
			const char* Format,
			...
			);

		void
		WriteIndent();

		void
		WriteVariant(
			const VARIANT* v
			);

		void
		WriteUnnamedDataType(
			UdtKind Kind
			);

		void
		WriteTypedefBegin(
			const SYMBOL* Symbol
			);

		void
		WriteTypedefEnd(
			const SYMBOL* Symbol
			);

		void
		WriteOffset(
			const SYMBOL_USERDATA_FIELD* UserDataField,
			int PaddingOffset
			);

		bool
		HasBeenVisited(
			const SYMBOL* Symbol
			) const;

		void
		MarkAsVisited(
			const SYMBOL* Symbol
			);

		DWORD
		GetParentOffset() const;

		void
		AppendToTest(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		bool
		ShouldExpand(
			const SYMBOL* Symbol
			) const;

	private:
		//
		// Settings for this visitor.
		//
		Settings* m_Settings;

		//
		// Everytime visitor enters a new member (user data field),
		// it pushes the current offset here.
		// In case the current member is a new struct (or any other
		// user data type) which will be expanded, this property
		// helps to find the offset of the parent member.
		//
		std::vector<DWORD> m_OffsetStack;

		//
		// Indentation.
		//
		DWORD m_Depth = 0;

		//
		// Counter for anonymous user data types.
		//
		DWORD m_AnonymousDataTypeCounter = 0;

		//
		// Counter of padding members.
		//
		DWORD m_PaddingMemberCounter = 0;

		//
		// Collection of unnamed symbols.
		//
		// Unnamed symbols actually have a special name.
		// See PDB::IsUnnamedSymbol() for more information.
		//
		mutable std::set<const SYMBOL*> m_UnnamedSymbols;

		//
		// Mapping of symbols to their "corrected" names.
		//
		mutable std::map<const SYMBOL*, std::string> m_CorrectedSymbolNames;

		//
		// Collection of symbol names which has already been visited.
		// We save names of the symbols here, because some PDBs
		// has multiple definition of the same symbol.
		//
		// See PDBVisitorSorter::HasBeenVisited() for more information.
		//
		std::set<std::string> m_VisitedSymbols;
};

