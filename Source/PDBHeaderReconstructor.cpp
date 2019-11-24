#include "PDBHeaderReconstructor.h"
#include "PDBReconstructorBase.h"

#include <iostream>
#include <numeric> // std::accumulate
#include <string>
#include <map>
#include <set>

#include <cassert>

PDBHeaderReconstructor::PDBHeaderReconstructor(Settings* VisitorSettings)
{
	static Settings DefaultSettings;

	if (VisitorSettings == nullptr)
	{
		VisitorSettings = &DefaultSettings;
	}
	m_Settings = VisitorSettings;
}

void PDBHeaderReconstructor::Clear()
{
	assert(m_Depth == 0);

	m_AnonymousDataTypeCounter = 0;
	m_PaddingMemberCounter = 0;

	m_UnnamedSymbols.clear();
	m_CorrectedSymbolNames.clear();
	m_VisitedSymbols.clear();
}

const std::string& PDBHeaderReconstructor::GetCorrectedSymbolName(const SYMBOL* Symbol) const
{
	auto CorrectedNameIt = m_CorrectedSymbolNames.find(Symbol);
	if (CorrectedNameIt == m_CorrectedSymbolNames.end())
	{
		std::string CorrectedName;

		CorrectedName += m_Settings->SymbolPrefix;

		if (PDB::IsUnnamedSymbol(Symbol))
		{
			m_UnnamedSymbols.insert(Symbol);

			CorrectedName += m_Settings->UnnamedTypePrefix + std::to_string(m_UnnamedSymbols.size());
		} else
		{
			CorrectedName += Symbol->Name;
		}

		CorrectedName += m_Settings->SymbolSuffix;

		m_CorrectedSymbolNames[Symbol] = CorrectedName;
	}

	return m_CorrectedSymbolNames[Symbol];
}

bool PDBHeaderReconstructor::OnEnumType(const SYMBOL* Symbol)
{
	std::string CorrectedName = GetCorrectedSymbolName(Symbol);

	bool Expand = ShouldExpand(Symbol);

	MarkAsVisited(Symbol);

	if (!Expand)
	{
		Write("enum %s", CorrectedName.c_str());
	}

	return Expand;
}

void PDBHeaderReconstructor::OnEnumTypeBegin(const SYMBOL* Symbol)
{
	std::string CorrectedName = GetCorrectedSymbolName(Symbol);

	WriteTypedefBegin(Symbol);

	Write("enum");

	Write(" %s", CorrectedName.c_str());

	Write("\n");

	WriteIndent();
	Write("{\n");

	m_Depth += 1;
}

void PDBHeaderReconstructor::OnEnumTypeEnd(const SYMBOL* Symbol)
{
	m_Depth -= 1;

	WriteIndent();
	Write("}");

	WriteTypedefEnd(Symbol);

	if (m_Depth == 0)
	{
		Write(";\n\n");
	}
}

void PDBHeaderReconstructor::OnEnumField(const SYMBOL_ENUM_FIELD* EnumField)
{
	WriteIndent();
	Write("%s = ", EnumField->Name);

	WriteVariant(&EnumField->Value);
	Write(",\n");
}

bool PDBHeaderReconstructor::OnUdt(const SYMBOL* Symbol)
{
	bool Expand = ShouldExpand(Symbol);

	MarkAsVisited(Symbol);

	if (!Expand)
	{
		std::string CorrectedName = GetCorrectedSymbolName(Symbol);

		WriteConstAndVolatile(Symbol);

		Write("%s %s", PDB::GetUdtKindString(Symbol->u.Udt.Kind), CorrectedName.c_str());

		if (m_Depth == 0)
		{
			Write(";\n\n");
		}
	}

	return Expand;
}

void PDBHeaderReconstructor::OnUdtBegin(const SYMBOL* Symbol)
{
	WriteTypedefBegin(Symbol);

	WriteConstAndVolatile(Symbol);

	Write("%s", PDB::GetUdtKindString(Symbol->u.Udt.Kind));

	if (!PDB::IsUnnamedSymbol(Symbol))
	{
		std::string CorrectedName = GetCorrectedSymbolName(Symbol);
		Write(" %s", CorrectedName.c_str());

		if (Symbol->u.Udt.BaseClassCount)
		{
			std::string ClassName;
			for (DWORD i = 0; i < Symbol->u.Udt.BaseClassCount; ++i)
			{
				std::string Access;
				switch (Symbol->u.Udt.BaseClassFields[i].Access)
				{
				case 1: Access = "private "; break;
				case 2: Access = "protected "; break;
				case 3: Access = "public "; break;
				}
				std::string Virtual;
				if (Symbol->u.Udt.BaseClassFields[i].IsVirtual) Virtual = "virtual ";
				std::string CorrectFuncName = GetCorrectedSymbolName(Symbol->u.Udt.BaseClassFields[i].Type);
				if (ClassName.size()) ClassName += ", ";
				ClassName += Access + Virtual + CorrectFuncName;
			}
			Write(" : %s", ClassName.c_str());
		}
	}

	Write("\n");

	WriteIndent();
	Write("{\n");

	m_Depth += 1;
}

void PDBHeaderReconstructor::OnUdtEnd(const SYMBOL* Symbol)
{
	m_Depth -= 1;

	WriteIndent();
	Write("}");

	WriteTypedefEnd(Symbol);

	if (m_Depth == 0)
	{
		Write(";");
	}

	Write(" /* size: 0x%04x */", Symbol->Size);

	if (m_Depth == 0)
	{
		Write("\n\n");
	}
}

void PDBHeaderReconstructor::OnUdtFieldBegin(const SYMBOL_UDT_FIELD* UdtField)
{
	WriteIndent();

	if (UdtField->Type->Tag != SymTagFunction &&
	    UdtField->Type->Tag != SymTagTypedef &&
	    (UdtField->Type->Tag != SymTagUDT ||
	    ShouldExpand(UdtField->Type) == false))
	{
		WriteOffset(UdtField, GetParentOffset());
	}

	m_OffsetStack.push_back(UdtField->Offset);
}

void PDBHeaderReconstructor::OnUdtFieldEnd(const SYMBOL_UDT_FIELD* UdtField)
{
	m_OffsetStack.pop_back();
}

void PDBHeaderReconstructor::OnUdtField(const SYMBOL_UDT_FIELD* UdtField, UdtFieldDefinitionBase* MemberDefinition)
{
	if (UdtField->DataKind == DataIsStaticMember) //TODO
		Write("static ");

	Write("%s", MemberDefinition->GetPrintableDefinition().c_str());

	if (UdtField->Bits != 0)
	{
		Write(" : %i", UdtField->Bits);
	}

	Write(";");

	if (UdtField->Bits != 0)
	{
		Write(" /* bit position: %i */", UdtField->BitPosition);
	}

	Write("\n");
}

void PDBHeaderReconstructor::OnAnonymousUdtBegin(UdtKind Kind, const SYMBOL_UDT_FIELD* First)
{
	WriteIndent();
	Write("%s\n", PDB::GetUdtKindString(Kind));

	WriteIndent();
	Write("{\n");

	m_Depth += 1;
}

void PDBHeaderReconstructor::OnAnonymousUdtEnd(UdtKind Kind,
	const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last, DWORD Size)
{
	m_Depth -= 1;
	WriteIndent();
	Write("}");

	WriteUnnamedDataType(Kind);

	Write(";");
	Write(" /* size: 0x%04x */", Size);
	Write("\n");
}

void PDBHeaderReconstructor::OnUdtFieldBitFieldBegin(const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last)
{
	if (m_Settings->AllowBitFieldsInUnion == false)
	{
		if (First != Last)
		{
			WriteIndent();
			Write("%s /* bitfield */\n", PDB::GetUdtKindString(UdtStruct));

			WriteIndent();
			Write("{\n");

			m_Depth += 1;
		}
	}
}

void PDBHeaderReconstructor::OnUdtFieldBitFieldEnd(const SYMBOL_UDT_FIELD* First, const SYMBOL_UDT_FIELD* Last)
{
	if (m_Settings->AllowBitFieldsInUnion == false)
	{
		if (First != Last)
		{
			m_Depth -= 1;

			WriteIndent();
			Write("}; /* bitfield */\n");
		}
	}
}

void PDBHeaderReconstructor::OnPaddingMember(const SYMBOL_UDT_FIELD* UdtField,
	BasicType PaddingBasicType, DWORD PaddingBasicTypeSize, DWORD PaddingSize)
{
	if (m_Settings->CreatePaddingMembers)
	{
		WriteIndent();

		WriteOffset(UdtField, -((int)PaddingSize * (int)PaddingBasicTypeSize));

		Write(
			"%s %s%u",
			PDB::GetBasicTypeString(PaddingBasicType, PaddingBasicTypeSize),
			m_Settings->PaddingMemberPrefix.c_str(),
			m_PaddingMemberCounter++
			);

		if (PaddingSize > 1)
		{
			Write("[%u]", PaddingSize);
		}

		Write(";\n");
	}
}

void PDBHeaderReconstructor::OnPaddingBitFieldField(
	const SYMBOL_UDT_FIELD* UdtField, const SYMBOL_UDT_FIELD* PreviousUdtField)
{
	WriteIndent();

	WriteOffset(UdtField, GetParentOffset());

	if (m_Settings->BitFieldPaddingMemberPrefix.empty())
	{
		Write("%s", PDB::GetBasicTypeString(UdtField->Type));
	} else
	{
		Write("%s %s%u", PDB::GetBasicTypeString(UdtField->Type),
			m_Settings->PaddingMemberPrefix.c_str(), m_PaddingMemberCounter++);
	}

	DWORD Bits = PreviousUdtField
		? UdtField->BitPosition - (PreviousUdtField->BitPosition + PreviousUdtField->Bits)
		: UdtField->BitPosition;

	DWORD BitPosition = PreviousUdtField
		? PreviousUdtField->BitPosition + PreviousUdtField->Bits
		: 0;

	assert(Bits != 0);

	Write(" : %i", Bits);
	Write(";");
	Write(" /* bit position: %i */", BitPosition);
	Write("\n");
}

void PDBHeaderReconstructor::Write(const char* Format, ...)
{
	char TempBuffer[8 * 1024];

	va_list ArgPtr;
	va_start(ArgPtr, Format);
	vsprintf_s(TempBuffer, Format, ArgPtr);
	va_end(ArgPtr);

	m_Settings->OutputFile->write(TempBuffer, strlen(TempBuffer));
}

void PDBHeaderReconstructor::WriteIndent()
{
	for (DWORD i = 0; i < m_Depth; ++i)
	{
		Write("  ");
	}
}

void PDBHeaderReconstructor::WriteVariant(const VARIANT* v)
{
	switch (v->vt)
	{
	case VT_I1:
		Write("%d", (INT)v->cVal);
		break;

	case VT_UI1:
		Write("0x%x", (UINT)v->cVal);
		break;

	case VT_I2:
		Write("%d", (UINT)v->iVal);
		break;

	case VT_UI2:
		Write("0x%x", (UINT)v->iVal);
		break;

	case VT_INT:
		Write("%d", (UINT)v->lVal);
		break;

	case VT_UINT:
	case VT_UI4:
	case VT_I4:
		Write("0x%x", (UINT)v->lVal);
		break;
	}
}

void PDBHeaderReconstructor::WriteUnnamedDataType(UdtKind Kind)
{
	if (m_Settings->AllowAnonymousDataTypes == false)
	{
		switch (Kind)
		{
		case UdtStruct:
		case UdtClass:
			Write(" %s", m_Settings->AnonymousStructPrefix.c_str());
			break;
		case UdtUnion:
			Write(" %s", m_Settings->AnonymousUnionPrefix.c_str());
			break;
		default:
			assert(0);
			break;
		}

		if (m_AnonymousDataTypeCounter++ > 0)
		{
			Write("%u", m_AnonymousDataTypeCounter);
		}
	}
}

void PDBHeaderReconstructor::WriteTypedefBegin(const SYMBOL* Symbol)
{
}

void PDBHeaderReconstructor::WriteTypedefEnd(const SYMBOL* Symbol)
{
}

void PDBHeaderReconstructor::WriteConstAndVolatile(const SYMBOL* Symbol)
{
	if (m_Depth != 0)
	{
		if (Symbol->IsConst)
		{
			Write("const ");
		}

		if (Symbol->IsVolatile)
		{
			Write("volatile ");
		}
	}
}

void PDBHeaderReconstructor::WriteOffset(const SYMBOL_UDT_FIELD* UdtField, int PaddingOffset)
{
	if (m_Settings->ShowOffsets)
	{
		Write("/* 0x%04x */ ", UdtField->Offset + PaddingOffset);
	}
}

bool PDBHeaderReconstructor::HasBeenVisited(const SYMBOL* Symbol) const
{
	std::string CorrectedName = GetCorrectedSymbolName(Symbol);
	return m_VisitedSymbols.find(CorrectedName) != m_VisitedSymbols.end();
}

void PDBHeaderReconstructor::MarkAsVisited(const SYMBOL* Symbol)
{
	std::string CorrectedName = GetCorrectedSymbolName(Symbol);
	m_VisitedSymbols.insert(CorrectedName);
}

DWORD PDBHeaderReconstructor::GetParentOffset() const
{
	return std::accumulate(m_OffsetStack.begin(), m_OffsetStack.end(), (DWORD)0);
}

bool PDBHeaderReconstructor::ShouldExpand(const SYMBOL* Symbol) const
{
	bool Expand = false;

	switch (m_Settings->MemberStructExpansion)
	{
	default:
	case MemberStructExpansionType::None:
		Expand = m_Depth == 0;
		break;

	case MemberStructExpansionType::InlineUnnamed:
		Expand = m_Depth == 0 || (Symbol->Tag == SymTagUDT && PDB::IsUnnamedSymbol(Symbol));
		break;

	case MemberStructExpansionType::InlineAll:
		Expand = !HasBeenVisited(Symbol);
		break;
	}

	return Expand && Symbol->Size > 0;
}
