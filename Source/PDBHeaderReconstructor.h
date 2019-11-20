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
		None,
		InlineUnnamed,
		InlineAll,
	};

	struct Settings
	{
		Settings()
		{
			MemberStructExpansion       = MemberStructExpansionType::InlineUnnamed;
			OutputFile                  = &std::cout;
			PaddingMemberPrefix         = "Padding_";
			BitFieldPaddingMemberPrefix = "";
			UnnamedTypePrefix           = "TAG_UNNAMED_";
			AnonymousStructPrefix       = "s";  // DUMMYSTRUCTNAME (up to 6)
			AnonymousUnionPrefix        = "u";  // DUMMYUNIONNAME  (up to 9)
			CreatePaddingMembers        = true;
			ShowOffsets                 = true;
			MicrosoftTypedefs           = true;
			AllowBitFieldsInUnion       = false;
			AllowAnonymousDataTypes     = true;
		}

		MemberStructExpansionType MemberStructExpansion;
		std::ostream*             OutputFile;
		std::string               PaddingMemberPrefix;
		std::string               BitFieldPaddingMemberPrefix;
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

	PDBHeaderReconstructor(Settings* VisitorSettings = nullptr);

	void Clear();

	const std::string& GetCorrectedSymbolName(const SYMBOL* Symbol) const;

protected:
	bool OnEnumType(const SYMBOL* Symbol) override;
	void OnEnumTypeBegin(const SYMBOL* Symbol) override;
	void OnEnumTypeEnd(const SYMBOL* Symbol) override;
	void OnEnumField(const SYMBOL_ENUM_FIELD* EnumField) override;

	bool OnUdt(const SYMBOL* Symbol) override;
	void OnUdtBegin(const SYMBOL* Symbol) override;
	void OnUdtEnd(const SYMBOL* Symbol) override;

	void OnUdtFieldBegin(const SYMBOL_UDT_FIELD* UdtField) override;
	void OnUdtFieldEnd(const SYMBOL_UDT_FIELD* UdtField) override;
	void OnUdtField(const SYMBOL_UDT_FIELD* UdtField, UdtFieldDefinitionBase* MemberDefinition) override;

	void OnAnonymousUdtBegin(UdtKind Kind, const SYMBOL_UDT_FIELD* First) override;
	void OnAnonymousUdtEnd(UdtKind Kind, const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last, DWORD Size) override;

	void OnUdtFieldBitFieldBegin(const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last) override;
	void OnUdtFieldBitFieldEnd(const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last) override;

	void OnPaddingMember(const SYMBOL_UDT_FIELD* UdtField, BasicType PaddingBasicType, DWORD PaddingBasicTypeSize, DWORD PaddingSize) override;

	void OnPaddingBitFieldField(const SYMBOL_UDT_FIELD* UdtField, const SYMBOL_UDT_FIELD* PreviousUdtField) override;
private:
	void Write(const char* Format, ...);
	void WriteIndent();
	void WriteVariant(const VARIANT* v);
	void WriteUnnamedDataType(UdtKind Kind);
	void WriteTypedefBegin(const SYMBOL* Symbol);
	void WriteTypedefEnd(const SYMBOL* Symbol);
	void WriteConstAndVolatile(const SYMBOL* Symbol);
	void WriteOffset(const SYMBOL_UDT_FIELD* UdtField, int PaddingOffset);
	bool HasBeenVisited(const SYMBOL* Symbol) const;
	void MarkAsVisited(const SYMBOL* Symbol);
	DWORD GetParentOffset() const;
	bool ShouldExpand(const SYMBOL* Symbol) const;

private:
	Settings* m_Settings;

	std::vector<DWORD> m_OffsetStack;
	DWORD m_Depth = 0;
	DWORD m_AnonymousDataTypeCounter = 0;
	DWORD m_PaddingMemberCounter = 0;

	mutable std::set<const SYMBOL*> m_UnnamedSymbols;
	mutable std::map<const SYMBOL*, std::string> m_CorrectedSymbolNames;

	std::set<std::string> m_VisitedSymbols;
};
