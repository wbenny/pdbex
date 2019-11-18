#include "PDBSymbolVisitor.h"
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <typename MEMBER_DEFINITION_TYPE>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PDBSymbolVisitor(PDBReconstructorBase* ReconstructVisitor)
{
	m_ReconstructVisitor = ReconstructVisitor;
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Run(const SYMBOL* Symbol)
{
	Visit(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Visit(const SYMBOL* Symbol)
{
	PDBSymbolVisitorBase::Visit(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitBaseType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitBaseType(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumType(const SYMBOL* Symbol)
{
	if (m_MemberContextStack.size())
	{
		m_MemberContextStack.top()->VisitEnumType(Symbol);
	} else
	if (m_ReconstructVisitor->OnEnumType(Symbol))
	{
		m_ReconstructVisitor->OnEnumTypeBegin(Symbol);
		PDBSymbolVisitorBase::VisitEnumType(Symbol);
		m_ReconstructVisitor->OnEnumTypeEnd(Symbol);
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitTypedefType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitTypedefTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitTypedefType(Symbol);
	m_MemberContextStack.top()->VisitTypedefTypeEnd(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitPointerType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitPointerTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitPointerType(Symbol);
	m_MemberContextStack.top()->VisitPointerTypeEnd(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitArrayType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitArrayTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitArrayType(Symbol);
	m_MemberContextStack.top()->VisitArrayTypeEnd(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitFunctionTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitFunctionType(Symbol);
	m_MemberContextStack.top()->VisitFunctionTypeEnd(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionArgType(const SYMBOL* Symbol)
{
	m_MemberContextStack.top()->VisitFunctionArgTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitFunctionArgType(Symbol);
	m_MemberContextStack.top()->VisitFunctionArgTypeEnd(Symbol);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdt(const SYMBOL* Symbol)
{
	if (!PDB::IsUnnamedSymbol(Symbol) && m_MemberContextStack.size())
	{
		m_MemberContextStack.top()->VisitUdtType(Symbol);
	} else
	if (m_ReconstructVisitor->OnUdt(Symbol))
	{
		if (Symbol->Size > 0)
		{
			AnonymousUdtStack AnonymousUDTStackBackup;
			AnonymousUdtStack AnonymousUnionStackBackup;
			AnonymousUdtStack AnonymousStructStackBackup;
			m_AnonymousUdtStack.swap(AnonymousUDTStackBackup);
			m_AnonymousUnionStack.swap(AnonymousUnionStackBackup);
			m_AnonymousStructStack.swap(AnonymousStructStackBackup);

			{
				m_MemberContextStack.push(MemberDefinitionFactory());

				m_ReconstructVisitor->OnUdtBegin(Symbol);
				PDBSymbolVisitorBase::VisitUdt(Symbol);
				m_ReconstructVisitor->OnUdtEnd(Symbol);

				m_MemberContextStack.pop();
			}

			m_AnonymousStructStack.swap(AnonymousStructStackBackup);
			m_AnonymousUnionStack.swap(AnonymousUnionStackBackup);
			m_AnonymousUdtStack.swap(AnonymousUDTStackBackup);
		}
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitOtherType(const SYMBOL* Symbol)
{

}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumField(const SYMBOL_ENUM_FIELD* EnumField)
{
	m_ReconstructVisitor->OnEnumField(EnumField);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtField(const SYMBOL_UDT_FIELD* UdtField)
{
	if (UdtField->Type->Tag == SymTagVTable ||
	    UdtField->IsBaseClass == TRUE)
	{
		return;
	}

	BOOL IsBitFieldMember = UdtField->Bits != 0;
	BOOL IsFirstBitFieldMember = IsBitFieldMember && !m_PreviousBitFieldField;

	m_MemberContextStack.push(MemberDefinitionFactory());
	m_MemberContextStack.top()->SetMemberName(UdtField->Name);

	if (!IsBitFieldMember || IsFirstBitFieldMember)
	{
		CheckForDataFieldPadding(UdtField);
		CheckForAnonymousUnion(UdtField);
		CheckForAnonymousStruct(UdtField);
	}

	if (IsFirstBitFieldMember)
	{
		BOOL IsFirstBitFieldMemberPadding = UdtField->BitPosition != 0;

		assert(m_CurrentBitField.HasValue() == false);

		m_CurrentBitField.FirstUdtFieldBitField = IsFirstBitFieldMemberPadding ? nullptr : UdtField;
		m_CurrentBitField.LastUdtFieldBitField = GetNextUdtFieldWithRespectToBitFields(UdtField) - 1;

		m_ReconstructVisitor->OnUdtFieldBitFieldBegin(
			m_CurrentBitField.FirstUdtFieldBitField,
			m_CurrentBitField.LastUdtFieldBitField
			);
	}

	if (IsBitFieldMember)
	{
		CheckForBitFieldFieldPadding(UdtField);
	}

	m_ReconstructVisitor->OnUdtFieldBegin(UdtField);
	Visit(UdtField->Type);
	m_ReconstructVisitor->OnUdtField(UdtField, m_MemberContextStack.top().get());
	m_ReconstructVisitor->OnUdtFieldEnd(UdtField);

	m_MemberContextStack.pop();

	if (IsBitFieldMember)
	{
		m_PreviousBitFieldField = UdtField;
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtFieldEnd(const SYMBOL_UDT_FIELD* UdtField)
{
	CheckForEndOfAnonymousUdt(UdtField);
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUdtFieldBitFieldEnd(const SYMBOL_UDT_FIELD* UdtField)
{
	assert(m_CurrentBitField.HasValue() == true);
	assert(m_CurrentBitField.LastUdtFieldBitField == UdtField);

	m_ReconstructVisitor->OnUdtFieldBitFieldEnd(
		m_CurrentBitField.FirstUdtFieldBitField,
		m_CurrentBitField.LastUdtFieldBitField
		);

	m_CurrentBitField.Clear();

	VisitUdtFieldEnd(UdtField);

	m_PreviousBitFieldField = nullptr;
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForDataFieldPadding(const SYMBOL_UDT_FIELD* UdtField)
{
	UdtFieldContext UdtFieldCtx(UdtField);
	DWORD PreviousUdtFieldOffset = 0;
	DWORD SizeOfPreviousUdtField = 0;

	if (UdtFieldCtx.IsFirst() == false)
	{
		PreviousUdtFieldOffset = m_PreviousUdtField->Offset;
		SizeOfPreviousUdtField = m_SizeOfPreviousUdtField;
	}

	if (PreviousUdtFieldOffset + SizeOfPreviousUdtField < UdtField->Offset)
	{
		DWORD Difference = UdtField->Offset - (PreviousUdtFieldOffset + SizeOfPreviousUdtField);

		BOOL DifferenceIsDivisibleBy4 = !(Difference % 4);

		m_ReconstructVisitor->OnPaddingMember(
			UdtField,
			DifferenceIsDivisibleBy4 ?     btLong     :   btChar  ,
			DifferenceIsDivisibleBy4 ?       4        :     1     ,
			DifferenceIsDivisibleBy4 ? Difference / 4 : Difference
			);
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForBitFieldFieldPadding(const SYMBOL_UDT_FIELD* UdtField)
{
	BOOL WasPreviousBitFieldMember = m_PreviousBitFieldField ? m_PreviousBitFieldField->Bits != 0 : FALSE;

	if (
	  (UdtField->BitPosition != 0 && !WasPreviousBitFieldMember) ||
	  (WasPreviousBitFieldMember &&
	   UdtField->BitPosition != m_PreviousBitFieldField->BitPosition + m_PreviousBitFieldField->Bits)
	  )
	{
		m_ReconstructVisitor->OnPaddingBitFieldField(UdtField, m_PreviousBitFieldField);
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousUnion(const SYMBOL_UDT_FIELD* UdtField)
{
	UdtFieldContext UdtFieldCtx(UdtField);
	if (UdtFieldCtx.IsLast())
	{
		return;
	}

	if (!m_AnonymousUdtStack.empty() &&
	     m_AnonymousUdtStack.top()->Kind == UdtUnion)
	{
		return;
	}

	do {
		if (UdtFieldCtx.NextUdtField->Offset == UdtField->Offset)
		{
			if (m_AnonymousStructStack.empty() ||
			  (!m_AnonymousStructStack.empty() && UdtFieldCtx.NextUdtField <= m_AnonymousStructStack.top()->LastUdtField))
			{
				PushAnonymousUdt(std::make_shared<AnonymousUdt>(UdtUnion, UdtField, nullptr, UdtField->Type->Size));
				m_ReconstructVisitor->OnAnonymousUdtBegin(UdtUnion, UdtField);
				break;
			}
		}
	} while (UdtFieldCtx.GetNext());
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousStruct(const SYMBOL_UDT_FIELD* UdtField)
{
	UdtFieldContext UdtFieldCtx(UdtField);
	if (UdtFieldCtx.IsLast())
	{
		return;
	}

	if (!m_AnonymousUdtStack.empty() &&
	     m_AnonymousUdtStack.top()->Kind != UdtUnion)
	{
		return;
	}

	if (UdtFieldCtx.NextUdtField->Offset <= UdtField->Offset)
	{
		return;
	}

	do {
		if (
		     UdtFieldCtx.NextUdtField->Offset == UdtField->Offset ||
		     (
		       !m_AnonymousUdtStack.empty() &&
		       UdtFieldCtx.NextUdtField->Offset < m_AnonymousUdtStack.top()->FirstUdtField->Offset + m_AnonymousUdtStack.top()->Size
		     )
		)
		{

			do {
				bool IsEndOfAnonymousStruct =
					UdtFieldCtx.IsLast() ||
					UdtFieldCtx.NextUdtField->Offset <= UdtField->Offset;

				if (IsEndOfAnonymousStruct)
				{
					break;
				}
			} while (UdtFieldCtx.GetNext());

			PushAnonymousUdt(std::make_shared<AnonymousUdt>(UdtStruct, UdtField, UdtFieldCtx.CurrentUdtField));
			m_ReconstructVisitor->OnAnonymousUdtBegin(UdtStruct, UdtField);
			break;
		}
	} while (UdtFieldCtx.GetNext());
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForEndOfAnonymousUdt(const SYMBOL_UDT_FIELD* UdtField)
{
	m_PreviousUdtField       = UdtField;
	m_SizeOfPreviousUdtField = UdtField->Type->Size;

	if (m_AnonymousUdtStack.empty())
	{
		return;
	}

	UdtFieldContext UdtFieldCtx(UdtField, FALSE);

	AnonymousUdt* LastAnonymousUdt;

	do {
		LastAnonymousUdt = m_AnonymousUdtStack.top().get();
		LastAnonymousUdt->MemberCount += 1;

		bool IsEndOfAnonymousUdt = false;

		if (LastAnonymousUdt->Kind == UdtUnion)
		{
			LastAnonymousUdt->Size = max(LastAnonymousUdt->Size, m_SizeOfPreviousUdtField);

			IsEndOfAnonymousUdt =
			   UdtFieldCtx.IsLast() ||
			   UdtFieldCtx.NextUdtField->Offset <  UdtField->Offset ||
			  (UdtFieldCtx.NextUdtField->Offset == UdtField->Offset + LastAnonymousUdt->Size) ||
			  (UdtFieldCtx.NextUdtField->Offset == UdtField->Offset + 8 && Is64BitBasicType(UdtFieldCtx.NextUdtField->Type)) ||
			  (UdtFieldCtx.NextUdtField->Offset >  UdtField->Offset && UdtField->Bits != 0) ||
			  (UdtFieldCtx.NextUdtField->Offset >  UdtField->Offset && UdtField->Offset + UdtField->Type->Size != UdtFieldCtx.NextUdtField->Offset);
		} else
		{
			LastAnonymousUdt->Size += m_SizeOfPreviousUdtField;

			IsEndOfAnonymousUdt =
				UdtFieldCtx.IsLast() ||
				UdtFieldCtx.NextUdtField->Offset <= UdtField->Offset;

			AnonymousUdt* LastAnonymousUnion =
				m_AnonymousUnionStack.empty()
				? nullptr
				: m_AnonymousUnionStack.top().get();

			IsEndOfAnonymousUdt = IsEndOfAnonymousUdt || (
			    LastAnonymousUnion != nullptr &&
			   (LastAnonymousUnion->FirstUdtField->Offset + LastAnonymousUnion->Size == UdtField->Offset + UdtField->Type->Size ||
			    LastAnonymousUnion->FirstUdtField->Offset + LastAnonymousUnion->Size == UdtFieldCtx.NextUdtField->Offset) &&
			    LastAnonymousUdt->MemberCount >= 2
			);
		}

		if (IsEndOfAnonymousUdt)
		{
			m_SizeOfPreviousUdtField = LastAnonymousUdt->Size;
			LastAnonymousUdt->LastUdtField = UdtField;

			m_ReconstructVisitor->OnAnonymousUdtEnd(
				LastAnonymousUdt->Kind,
				LastAnonymousUdt->FirstUdtField,
				LastAnonymousUdt->LastUdtField,
				LastAnonymousUdt->Size
				);

			PopAnonymousUdt();

			LastAnonymousUdt = nullptr;
		}

		if (!m_AnonymousUdtStack.empty())
		{
			if (m_AnonymousUdtStack.top()->Kind == UdtUnion)
			{
				UdtField = m_AnonymousUdtStack.top()->FirstUdtField;
				m_PreviousUdtField = UdtField;
			} else
			{
				UdtField = UdtFieldCtx.CurrentUdtField;
				m_PreviousUdtField = UdtField;
			}
		}
	} while (LastAnonymousUdt == nullptr && !m_AnonymousUdtStack.empty());
}

template <typename MEMBER_DEFINITION_TYPE>
std::shared_ptr<UdtFieldDefinitionBase> PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::MemberDefinitionFactory()
{
	auto MemberDefinition = std::make_shared<MEMBER_DEFINITION_TYPE>();
	return MemberDefinition;
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PushAnonymousUdt(std::shared_ptr<AnonymousUdt> Item)
{
	m_AnonymousUdtStack.push(Item);

	if (Item->Kind == UdtUnion)
	{
		m_AnonymousUnionStack.push(Item);
	} else
	{
		m_AnonymousStructStack.push(Item);
	}
}

template <typename MEMBER_DEFINITION_TYPE>
void PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PopAnonymousUdt()
{
	if (m_AnonymousUdtStack.top()->Kind == UdtUnion)
	{
		m_AnonymousUnionStack.pop();
	} else
	{
		m_AnonymousStructStack.pop();
	}

	m_AnonymousUdtStack.pop();
}

template <typename MEMBER_DEFINITION_TYPE>
const SYMBOL_UDT_FIELD*
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::GetNextUdtFieldWithRespectToBitFields(const SYMBOL_UDT_FIELD* UdtField)
{
	const SYMBOL_UDT* ParentUdt = &UdtField->Parent->u.Udt;
	DWORD UdtFieldCount = ParentUdt->FieldCount;

	const SYMBOL_UDT_FIELD* NextUdtField = UdtField + 1;
	const SYMBOL_UDT_FIELD* EndOfUdtField = &ParentUdt->Fields[UdtFieldCount];

	if (NextUdtField >= EndOfUdtField)
	{
		return EndOfUdtField;
	}

	do {
		if (NextUdtField->BitPosition == 0)
		{
			break;
		}
	} while (++NextUdtField < EndOfUdtField);

	return NextUdtField;
}

template <typename MEMBER_DEFINITION_TYPE>
bool PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Is64BitBasicType(const SYMBOL* Symbol)
{
	return (Symbol->Tag == SymTagBaseType && Symbol->Size == 8);
}
