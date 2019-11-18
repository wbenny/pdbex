#pragma once
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <typename MEMBER_DEFINITION_TYPE>
class PDBSymbolVisitor
	: public PDBSymbolVisitorBase
{
public:
	PDBSymbolVisitor(PDBReconstructorBase* ReconstructVisitor);

	void Run(const SYMBOL* Symbol);
protected:

	void Visit(const SYMBOL* Symbol) override;
	void VisitBaseType(const SYMBOL* Symbol) override;
	void VisitEnumType(const SYMBOL* Symbol) override;
	void VisitTypedefType(const SYMBOL* Symbol) override;
	void VisitPointerType(const SYMBOL* Symbol) override;
	void VisitArrayType(const SYMBOL* Symbol) override;
	void VisitFunctionType(const SYMBOL* Symbol) override;
	void VisitFunctionArgType(const SYMBOL* Symbol) override;
	void VisitUdt(const SYMBOL* Symbol) override;
	void VisitOtherType(const SYMBOL* Symbol) override;
	void VisitEnumField(const SYMBOL_ENUM_FIELD* EnumField) override;
	void VisitUdtField(const SYMBOL_UDT_FIELD* UdtField) override;
	void VisitUdtFieldEnd(const SYMBOL_UDT_FIELD* UdtField) override;
	void VisitUdtFieldBitFieldEnd(const SYMBOL_UDT_FIELD* UdtField) override;

private:

	struct AnonymousUdt
	{
		AnonymousUdt(
			UdtKind Kind,
			const SYMBOL_UDT_FIELD* FirstUdtField,
			const SYMBOL_UDT_FIELD* LastUdtField,
			DWORD Size = 0,
			DWORD MemberCount = 0
			)
		{
			this->Kind          = Kind;
			this->FirstUdtField = FirstUdtField;
			this->LastUdtField  = LastUdtField;
			this->Size          = Size;
			this->MemberCount   = MemberCount;
		}

		const SYMBOL_UDT_FIELD* FirstUdtField;
		const SYMBOL_UDT_FIELD* LastUdtField;
		DWORD Size;
		DWORD MemberCount;
		UdtKind Kind;
	};

	struct BitFieldRange
	{
		const SYMBOL_UDT_FIELD* FirstUdtFieldBitField;
		const SYMBOL_UDT_FIELD* LastUdtFieldBitField;

		BitFieldRange()
			: FirstUdtFieldBitField(nullptr)
			, LastUdtFieldBitField(nullptr)
		{

		}

		void Clear()
		{
			FirstUdtFieldBitField = nullptr;
			LastUdtFieldBitField = nullptr;
		}

		bool HasValue() const
		{
			return /*FirstUdtFieldBitField != nullptr &&*/
			       LastUdtFieldBitField  != nullptr;
		}
	};

	struct UdtFieldContext
	{
		UdtFieldContext(const SYMBOL_UDT_FIELD* UdtField, BOOL RespectBitFields = TRUE)
		{
			SYMBOL_UDT* ParentUdt = &UdtField->Parent->u.Udt;
			DWORD UdtFieldCount = ParentUdt->FieldCount;

			FirstUdtField    = &ParentUdt->Fields[0];
			EndOfUdtField    = &ParentUdt->Fields[UdtFieldCount];

			PreviousUdtField = &UdtField[-1];
			CurrentUdtField  = &UdtField[ 0];
			NextUdtField     = &UdtField[ 1];

			this->RespectBitFields = RespectBitFields;

			if (RespectBitFields)
			{
				NextUdtField   = GetNextUdtFieldWithRespectToBitFields(UdtField);
			}
		}

		bool IsFirst() const { return PreviousUdtField < FirstUdtField;	}
		bool IsLast() const { return NextUdtField == EndOfUdtField; }

		bool GetNext()
		{
			PreviousUdtField = CurrentUdtField;
			CurrentUdtField  = NextUdtField;
			NextUdtField     = &CurrentUdtField[1];

			if (RespectBitFields && IsLast() == false)
			{
				NextUdtField = GetNextUdtFieldWithRespectToBitFields(CurrentUdtField);
			}

			return IsLast() == false;
		}

		const SYMBOL_UDT_FIELD* FirstUdtField;
		const SYMBOL_UDT_FIELD* EndOfUdtField;

		const SYMBOL_UDT_FIELD* PreviousUdtField;
		const SYMBOL_UDT_FIELD* CurrentUdtField;
		const SYMBOL_UDT_FIELD* NextUdtField;

		BOOL RespectBitFields;
	};

	using AnonymousUdtStack = std::stack<std::shared_ptr<AnonymousUdt>>;
	using ContextStack      = std::stack<std::shared_ptr<UdtFieldDefinitionBase>>;

private:
	void CheckForDataFieldPadding(const SYMBOL_UDT_FIELD* UdtField);
	void CheckForBitFieldFieldPadding(const SYMBOL_UDT_FIELD* UdtField);
	void CheckForAnonymousUnion(const SYMBOL_UDT_FIELD* UdtField);
	void CheckForAnonymousStruct(const SYMBOL_UDT_FIELD* UdtField);
	void CheckForEndOfAnonymousUdt(const SYMBOL_UDT_FIELD* UdtField);

	std::shared_ptr<UdtFieldDefinitionBase> MemberDefinitionFactory();

	void PushAnonymousUdt(std::shared_ptr<AnonymousUdt> Item);
	void PopAnonymousUdt();

private:
	static const SYMBOL_UDT_FIELD* GetNextUdtFieldWithRespectToBitFields(const SYMBOL_UDT_FIELD* UdtField);
	static bool Is64BitBasicType(const SYMBOL* Symbol);

private:
	DWORD m_SizeOfPreviousUdtField = 0;
	const SYMBOL_UDT_FIELD* m_PreviousUdtField = nullptr;
	const SYMBOL_UDT_FIELD* m_PreviousBitFieldField = nullptr;
	AnonymousUdtStack m_AnonymousUdtStack;
	AnonymousUdtStack m_AnonymousUnionStack;
	AnonymousUdtStack m_AnonymousStructStack;
	BitFieldRange m_CurrentBitField;
	ContextStack m_MemberContextStack;
	PDBReconstructorBase* m_ReconstructVisitor;
};

#include "PDBSymbolVisitor.inl"
