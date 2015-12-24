#include "PDBSymbolVisitor.h"

#pragma once
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <
	typename MEMBER_DEFINITION_TYPE
>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PDBSymbolVisitor(
	PDBReconstructorBase* ReconstructVisitor,
	void* MemberDefinitionSettings = nullptr
	)
{
	m_ReconstructVisitor = ReconstructVisitor;
	m_MemberDefinitionSettings = MemberDefinitionSettings;
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Run(
	const SYMBOL* Symbol
	)
{
	Visit(Symbol);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Visit(
	const SYMBOL* Symbol
	)
{
	PDBSymbolVisitorBase::Visit(Symbol);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitBaseType(
	const SYMBOL* Symbol
	)
{
	//
	// BaseType:
	// short/int/long/...
	//

	m_MemberContextStack.top()->VisitBaseType(Symbol);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumType(
	const SYMBOL* Symbol
	)
{
	//
	// EnumType:
	// enum XYZ
	// {
	//   XYZ_1,
	//   XYZ_2,
	// };
	//

	//
	// enum XYZ ...
	//

	if (m_ReconstructVisitor->OnEnumType(Symbol))
	{
		//
		// ...
		// {
		//   XYZ_1,
		//   XYZ_2,
		// }
		//

		m_ReconstructVisitor->OnEnumTypeBegin(Symbol);
		PDBSymbolVisitorBase::VisitEnumType(Symbol);
		m_ReconstructVisitor->OnEnumTypeEnd(Symbol);
	}
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitTypedefType(
	const SYMBOL* Symbol
	)
{

}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitPointerType(
	const SYMBOL* Symbol
	)
{
	//
	// PointerType:
	// short*/int*/long*/...
	//

	m_MemberContextStack.top()->VisitPointerTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitPointerType(Symbol);
	m_MemberContextStack.top()->VisitPointerTypeEnd(Symbol);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitArrayType(
	const SYMBOL* Symbol
	)
{
	//
	// ArrayType:
	// int XYZ[8];
	//

	m_MemberContextStack.top()->VisitArrayTypeBegin(Symbol);
	PDBSymbolVisitorBase::VisitArrayType(Symbol);
	m_MemberContextStack.top()->VisitArrayTypeEnd(Symbol);

}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionType(
	const SYMBOL* Symbol
	)
{
	//
	// #TODO:
	// Currently, show void* instead of functions.
	//

	m_MemberContextStack.top()->VisitFunctionTypeBegin(Symbol);
	// PDBSymbolVisitorBase::VisitFunctionType(Symbol);
	m_MemberContextStack.top()->VisitFunctionTypeEnd(Symbol);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitFunctionArgType(
	const SYMBOL* Symbol
	)
{

}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUserDataType(
	const SYMBOL* Symbol
	)
{
	//
	// UserDataType:
	// struct XYZ
	// {
	//   int XYZ_1;
	//   char XYZ_2;
	// };
	//

	//
	// struct XYZ ...
	//

	if (m_ReconstructVisitor->OnUserDataType(Symbol))
	{
		//
		// ...
		// {
		//   int XYZ_1;
		//   char XYZ_2;
		// }
		//

		if (Symbol->Size > 0)
		{
			//
			// Save the current stacks of anonymous UDTs.
			// This prevents interferencing of members
			// of nested UDTs.
			//
			// Stacks are restored after visiting of the current UDT.
			//
			AnonymousUserDataTypeStack AnonymousUDTStackBackup;
			AnonymousUserDataTypeStack AnonymousUnionStackBackup;
			AnonymousUserDataTypeStack AnonymousStructStackBackup;
			m_AnonymousUserDataTypeStack.swap(AnonymousUDTStackBackup);
			m_AnonymousUnionStack.swap(AnonymousUnionStackBackup);
			m_AnonymousStructStack.swap(AnonymousStructStackBackup);
			
			{
				m_MemberContextStack.push(MemberDefinitionFactory());

				m_ReconstructVisitor->OnUserDataTypeBegin(Symbol);
				PDBSymbolVisitorBase::VisitUserDataType(Symbol);
				m_ReconstructVisitor->OnUserDataTypeEnd(Symbol);

				m_MemberContextStack.pop();
			}

			m_AnonymousStructStack.swap(AnonymousStructStackBackup);
			m_AnonymousUnionStack.swap(AnonymousUnionStackBackup);
			m_AnonymousUserDataTypeStack.swap(AnonymousUDTStackBackup);
		}
	}
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitOtherType(
	const SYMBOL* Symbol
	)
{

}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitEnumField(
	const SYMBOL_ENUM_FIELD* EnumField
	)
{
	m_ReconstructVisitor->OnEnumField(EnumField);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUserDataField(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	BOOL IsBitFieldMember = UserDataField->Bits != 0;
	BOOL IsFirstBitFieldMember = IsBitFieldMember && UserDataField->BitPosition == 0;

	//
	// Push new member context.
	//

	m_MemberContextStack.push(MemberDefinitionFactory());
	m_MemberContextStack.top()->SetMemberName(UserDataField->Name);
	
	if (IsFirstBitFieldMember || !IsBitFieldMember)
	{
		//
		// Handling of inlined user data types.
		//
		// These checks are performed when the current member
		// is not a bitfield member (except the first one).
		//
		// Note that calling these inside of the bitfield
		// would not make sense.
		//

		CheckForDataFieldPadding(UserDataField);
		CheckForAnonymousUnion(UserDataField);
		CheckForAnonymousStruct(UserDataField);
	}

	if (IsFirstBitFieldMember)
	{
		//
		// This is the first bitfield member.
		//

		m_ReconstructVisitor->OnUserDataFieldBitFieldBegin(UserDataField);
	}

	//
	// Dump the field.
	//

	m_ReconstructVisitor->OnUserDataFieldBegin(UserDataField);
	Visit(UserDataField->Type);
	m_ReconstructVisitor->OnUserDataField(UserDataField, m_MemberContextStack.top().get());
	m_ReconstructVisitor->OnUserDataFieldEnd(UserDataField);

	m_MemberContextStack.pop();
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUserDataFieldEnd(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	CheckForEndOfAnonymousUserDataType(UserDataField);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::VisitUserDataFieldBitFieldEnd(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	m_ReconstructVisitor->OnUserDataFieldBitFieldEnd(UserDataField, UserDataField);

	VisitUserDataFieldEnd(UserDataField);
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForDataFieldPadding(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	//
	// Members are sometimes not properly aligned.
	// Example (original definition):
	//   struct XYZ
	//   {
	//     char XYZ_1;
	//     int  XYZ_2;  // This member actually begins at offset 4 (if packing was not applied),
	//                  // resulting in 3 spare bytes before this field.
	//   };
	//
	// This routine creates a "padding" member to fill the empty space, so the final reconstructed
	// structure would look like following:
	//   struct XYZ
	//   {
	//     char XYZ_1;
	//     char Padding_0[3]; // Padding member.
	//     int  XYZ_2;
	//   };
	//

	//
	// Take previous member, sum the size of the field and its offset
	// and compare it to the current member offset.
	// If the sum is less than the current member offset, there is a spare space
	// which will be filled by padding member.
	//

	UserDataFieldContext UserDataFieldCtx(UserDataField);

	if (UserDataFieldCtx.IsFirst() == false &&
	    m_PreviousUserDataField->Offset + (DWORD)m_SizeOfPreviousUserDataField < UserDataField->Offset)
	{
		DWORD Difference = UserDataField->Offset - (m_PreviousUserDataField->Offset + (DWORD)m_SizeOfPreviousUserDataField);

		//
		// We can use !(Difference & 3) if we want to be clever.
		//

		BOOL DifferenceIsDivisibleBy4 = !(Difference % 4);

		m_ReconstructVisitor->OnPaddingMember(
			UserDataField,
			DifferenceIsDivisibleBy4 ?     btLong     :   btChar  ,
			DifferenceIsDivisibleBy4 ?       4        :     1     ,
			DifferenceIsDivisibleBy4 ? Difference / 4 : Difference
			);
	}
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousUnion(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	//
	// When some user data type contains anonymous unions, they are not projected
	// into the PDB file - they are part of the user data type (ie. struct).
	// Anonymous unions can be detected through checking of starting offsets
	// of members in the structure - if there exist more than 1 member (DataField)
	// which start at the same offset, they are placed inside of the union.
	//

	UserDataFieldContext UserDataFieldCtx(UserDataField);

	if (UserDataFieldCtx.IsLast())
	{
		//
		// If current member is the last member of the current
		// user data type, there won't be any anonymous unions.
		//

		return;
	}

	if (!m_AnonymousUserDataTypeStack.empty() &&
	     m_AnonymousUserDataTypeStack.top()->UserDataTypeKind == UdtUnion)
	{
		//
		// Don't start an anonymous union while we're still inside of one.
		//

		return;
	}

	//
	// Iterate members starting from the current one.
	// If any following member which starts at the same offset
	// as the current member does exist, then they must be wrapped
	// inside of the union.
	//

	do
	{
		if (UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset)
		{

			//
			// Do not try to wrap in the union
			// those members, which are out of bounds
			// of the anonymous struct we're currently in.
			//
			// In other words, this prevents creating meaningless unions
			// which have only one member - because it detected
			// that there exist member, which has the same offset -
			// - but the member is already in another struct.
			//

			if (m_AnonymousStructStack.empty() ||
			  (!m_AnonymousStructStack.empty() && UserDataFieldCtx.NextUserDataField <= m_AnonymousStructStack.top()->LastUserDataField))
			{
				PushAnonymousUserDataType(std::make_shared<AnonymousUserDataType>(UdtUnion, UserDataField, nullptr, UserDataField->Type->Size));
				m_ReconstructVisitor->OnAnonymousUserDataTypeBegin(UdtUnion, UserDataField);
				break;
			}
		}
	} while (UserDataFieldCtx.GetNext());
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForAnonymousStruct(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{

	//
	// When some user data type contains anonymous structs, they are not projected
	// into the PDB file - they are part of the structure (UserDataType, respectively).
	// This dumper creates anonymous structs where it's obvious
	// that an anonmous structure is present in the union.
	// Consider following snippet:
	//
	// 0: kd> dt ntdll!_KTHREAD
	// ...
	//   +0x190 StackBase        : Ptr32 Void
	//   +0x194 SuspendApc       : _KAPC
	//   +0x194 SuspendApcFill0  : [1] UChar
	//   +0x195 ResourceIndex    : UChar
	//   +0x194 SuspendApcFill1  : [3] UChar
	//   +0x197 QuantumReset     : UChar
	//   +0x194 SuspendApcFill2  : [4] UChar
	//   +0x198 KernelTime       : Uint4B
	//   +0x194 SuspendApcFill3  : [36] UChar
	//   +0x1b8 WaitPrcb         : Ptr32 _KPRCB
	// ...
	//
	// Note that offset 0x194 is shared among many members, even though after those members
	// is placed another member which starts at another offset than 0x194.
	// This is effectively done by structs placed inside unions. The above snipped could be represented
	// as:
	//
	// struct _KTHREAD {
	// ...
	//   /* 0x0190 */ void* StackBase;
	//   union {
	//     /* 0x0194 */ struct _KAPC SuspendApc;
	//     struct {
	//       /* 0x0194 */ unsigned char SuspendApcFill0[1];
	//       /* 0x0195 */ unsigned char ResourceIndex;
	//     };
	//     struct {
	//       /* 0x0194 */ unsigned char SuspendApcFill1[3];
	//       /* 0x0197 */ unsigned char QuantumReset;
	//     };
	//     struct {
	//       /* 0x0194 */ unsigned char SuspendApcFill2[4];
	//       /* 0x0198 */ unsigned long KernelTime;
	//     };
	//     struct {
	//       /* 0x0194 */ unsigned char SuspendApcFill3[36];
	//       /* 0x01b8 */ KPRCB* WaitPrcb;
	//     };
	// ...
	// };
	//

	UserDataFieldContext UserDataFieldCtx(UserDataField);

	if (UserDataFieldCtx.IsLast())
	{
		//
		// If current member is the last member of the current
		// user data type, there won't be any anonymous structs.
		//

		return;
	}
	
	if (!m_AnonymousUserDataTypeStack.empty() &&
	     m_AnonymousUserDataTypeStack.top()->UserDataTypeKind != UdtUnion)
	{
		//
		// Don't start an anonymous struct while we're still inside of one.
		//

		return;
	}

	if (UserDataFieldCtx.NextUserDataField->Offset <= UserDataField->Offset)
	{
		//
		// If the offset of the next member is less than or equals to the offset
		// of the actual member, we cannot create a struct here.
		//

		return;
	}

	do
	{

		//
		// If offsets of next member and current member equal
		// or the offset of the next member is less than the offset
		// of the end of the last anonymous user data type,
		// we will create an anonymous struct.
		//

		if (
		     UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset ||
		     (
		       !m_AnonymousUserDataTypeStack.empty() &&
		       UserDataFieldCtx.NextUserDataField->Offset < m_AnonymousUserDataTypeStack.top()->FirstUserDataField->Offset + m_AnonymousUserDataTypeStack.top()->Size
		     )
		)
		{

			//
			// Guess the last member of this anonymous struct.
			// Note that this guess is not required to be correct.
			// It only serves as a break for creation of anonymous unions.
			//

			do
			{
				bool IsEndOfAnonymousStruct =
					UserDataFieldCtx.IsLast() ||
					UserDataFieldCtx.NextUserDataField->Offset <= UserDataField->Offset;

				if (IsEndOfAnonymousStruct)
				{
					break;
				}
			} while (UserDataFieldCtx.GetNext());

			//
			// UserDataFieldCtx.CurrentUserDataField now holds the last member
			// of this anonymous struct.
			//

			PushAnonymousUserDataType(std::make_shared<AnonymousUserDataType>(UdtStruct, UserDataField, UserDataFieldCtx.CurrentUserDataField));
			m_ReconstructVisitor->OnAnonymousUserDataTypeBegin(UdtStruct, UserDataField);
			break;
		}
	} while (UserDataFieldCtx.GetNext());
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::CheckForEndOfAnonymousUserDataType(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	//
	// This method is called after each user data field
	// and after the last member of the bitfield,
	// so this is the best place to refresh
	// these two properties.
	//

	m_PreviousUserDataField       = UserDataField;
	m_SizeOfPreviousUserDataField = UserDataField->Type->Size;

	if (m_AnonymousUserDataTypeStack.empty())
	{
		//
		// No user data type to check.
		//

		return;
	}

	UserDataFieldContext UserDataFieldCtx(UserDataField, FALSE);

	//
	// The current member could be nested more than once
	// and at this point more anonymous user data types
	// could be closed, so the code is wrapped inside of the loop.
	//

	AnonymousUserDataType* LastAnonymousUserDataType;

	do
	{
		LastAnonymousUserDataType = m_AnonymousUserDataTypeStack.top().get();
		LastAnonymousUserDataType->MemberCount += 1;

		bool IsEndOfAnonymousUserDataType = false;

		if (LastAnonymousUserDataType->UserDataTypeKind == UdtUnion)
		{
			//
			// Update the size of the current nested union.
			// The size of the union is as big as its biggest member.
			//

			LastAnonymousUserDataType->Size = max(LastAnonymousUserDataType->Size, m_SizeOfPreviousUserDataField);

			//
			// Determination if this is the end of the anonymous union.
			//
			//   - UserDataFieldCtx.IsLast()
			//     - If the current member is last in the root structure.
			//
			//       This check covers all opened anonymous user data types before
			//       top root structure ends.
			//
			//   - UserDataFieldCtx.NextUserDataField->Offset < UserDataField->Offset
			//     - If the offset of the next member is less than to the offset of the current member.
			//
			//   - (UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset + LastAnonymousUserDataType->Size)
			//     - If the offset of the next member equals to the sum of
			//       * the offset of the current member and
			//       * the computed size of the current nested union.
			//
			//   - (UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset + 8 && Is64BitBasicType(UserDataFieldCtx.NextUserDataField->Type))
			//     - If the offset of the next member equals to the offset of current member + 8 and
			//       the next member is of type [u]int64_t.
			//       This is the cause of the alignment.
			//
			//   - (UserDataFieldCtx.NextUserDataField->Offset >  UserDataField->Offset && UserDataField->Bits != 0)
			//     - If the offset of the next member is bigger than the offset of the current member and
			//       current member is not a part of the bitfield.
			//
			//   - (UserDataFieldCtx.NextUserDataField->Offset >  UserDataField->Offset && UserDataField->Offset + UserDataField->Type->Size != UserDataFieldCtx.NextUserDataField->Offset)
			//     - If the offset of the next member is bigger than the offset of the current member and
			//       the offset of the end of the current member is not equal to the offset of the next member.
			//

			IsEndOfAnonymousUserDataType =
			   UserDataFieldCtx.IsLast() ||
			   UserDataFieldCtx.NextUserDataField->Offset <  UserDataField->Offset ||
			  (UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset + LastAnonymousUserDataType->Size) ||
			  (UserDataFieldCtx.NextUserDataField->Offset == UserDataField->Offset + 8 && Is64BitBasicType(UserDataFieldCtx.NextUserDataField->Type)) ||
			  (UserDataFieldCtx.NextUserDataField->Offset >  UserDataField->Offset && UserDataField->Bits != 0) ||
			  (UserDataFieldCtx.NextUserDataField->Offset >  UserDataField->Offset && UserDataField->Offset + UserDataField->Type->Size != UserDataFieldCtx.NextUserDataField->Offset);
		}
		else
		{
			//
			// Update the size of the current nested structure/class.
			// The total size increases by the size of previous member.
			// Because the previous member could be non-trivial member (ie. union),
			// we will use the variable m_SizeOfPreviousUserDataField.
			//

			LastAnonymousUserDataType->Size += m_SizeOfPreviousUserDataField;

			//
			// Determination if this is the end of the anonymous struct.
			//
			//   - UserDataFieldCtx.IsLast()
			//     - If the current member is last in the root structure.
			//
			//       This check covers all opened anonymous user data types before
			//       top root structure ends.
			//
			//   - UserDataFieldCtx.NextUserDataField->Offset <= UserDataField->Offset
			//     - If the offset of the next member is less than or equal to the offset of the current member.
			//

			IsEndOfAnonymousUserDataType =
				UserDataFieldCtx.IsLast() ||
				UserDataFieldCtx.NextUserDataField->Offset <= UserDataField->Offset;

			//
			// Special condition for closing anonymous structs
			// which are placed inside of the anonymous unions.
			//
			// This prevents structs to be longer than it's actually needed.
			//
			// If the offset of the first member after the parent union
			// would be equal to the actual offset of the next member,
			// we can close this struct.
			// Also, in this struct must be at least 2 members.
			//

			AnonymousUserDataType* LastAnonymousUnion =
				m_AnonymousUnionStack.empty()
				? nullptr
				: m_AnonymousUnionStack.top().get();

			IsEndOfAnonymousUserDataType = IsEndOfAnonymousUserDataType || (
				LastAnonymousUnion != nullptr &&
				LastAnonymousUnion->FirstUserDataField->Offset + LastAnonymousUnion->Size == UserDataField->Offset + UserDataField->Type->Size &&
				LastAnonymousUserDataType->MemberCount >= 2
			);
		}

		if (IsEndOfAnonymousUserDataType)
		{
			//
			// Close the anonymous user data type.
			//

			m_SizeOfPreviousUserDataField = LastAnonymousUserDataType->Size;
			LastAnonymousUserDataType->LastUserDataField = UserDataField;

			m_ReconstructVisitor->OnAnonymousUserDataTypeEnd(
				LastAnonymousUserDataType->UserDataTypeKind,
				LastAnonymousUserDataType->FirstUserDataField,
				LastAnonymousUserDataType->LastUserDataField
				);

			PopAnonymousUserDataType();

			LastAnonymousUserDataType = nullptr;
		}

		if (!m_AnonymousUserDataTypeStack.empty())
		{
			if (m_AnonymousUserDataTypeStack.top()->UserDataTypeKind == UdtUnion)
			{
				//
				// If the AnonymousUserDataTypeStack is still not empty
				// and an anonymous union is at the top of it,
				// we must set the first member of the anonymous union
				// as the current member.
				//
				// The reason behind is that the first member of the union
				// is guaranteed to be at the starting offset of the union.
				// This not might be true for another members, as they
				// can be part of another anonymous struct.
				//
				// Example:
				//
				// union {
				//   int a;    /* 0x10 */
				//   int b;    /* 0x10 */
				//   struct {
				//     int c;  /* 0x10 */
				//     int d;  /* 0x14 */
				//             /*
				//              * This is where we are now. We end the struct here,
				//              * and the current offset is 0x14,
				//              * but the union starts at the offset 0x10, so we set
				//              * the current member to the first member of the unnamed union
				//              * which is "int a".
				//              */
				//   };
				// };

				UserDataField = m_AnonymousUserDataTypeStack.top()->FirstUserDataField;
				m_PreviousUserDataField = UserDataField;
			}
			else
			{
				//
				// If at the top of the AnonymousUserDataTypeStack is the struct or class,
				// set the current member back to the actual current member
				// which has been provided.
				//

				UserDataField = UserDataFieldCtx.CurrentUserDataField;
				m_PreviousUserDataField = UserDataField;
			}
		}
	} while (LastAnonymousUserDataType == nullptr && !m_AnonymousUserDataTypeStack.empty());
}

template <
	typename MEMBER_DEFINITION_TYPE
>
std::shared_ptr<UserDataFieldDefinitionBase>
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::MemberDefinitionFactory()
{
	auto MemberDefinition = std::make_shared<MEMBER_DEFINITION_TYPE>();
	MemberDefinition->SetSettings(m_MemberDefinitionSettings);

	return MemberDefinition;
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PushAnonymousUserDataType(
	std::shared_ptr<AnonymousUserDataType> Item
	)
{
	m_AnonymousUserDataTypeStack.push(Item);

	if (Item->UserDataTypeKind == UdtUnion)
	{
		m_AnonymousUnionStack.push(Item);
	}
	else
	{
		m_AnonymousStructStack.push(Item);
	}
}

template <
	typename MEMBER_DEFINITION_TYPE
>
void
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::PopAnonymousUserDataType()
{
	if (m_AnonymousUserDataTypeStack.top()->UserDataTypeKind == UdtUnion)
	{
		m_AnonymousUnionStack.pop();
	}
	else
	{
		m_AnonymousStructStack.pop();
	}

	m_AnonymousUserDataTypeStack.pop();
}

template <
	typename MEMBER_DEFINITION_TYPE
>
const SYMBOL_USERDATA_FIELD*
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::GetNextUserDataFieldWithRespectToBitFields(
	const SYMBOL_USERDATA_FIELD* UserDataField
	)
{
	const SYMBOL_USERDATA* ParentUserData = &UserDataField->Parent->u.UserData;
	DWORD UserDataFieldCount = ParentUserData->FieldCount;

	const SYMBOL_USERDATA_FIELD* NextUserDataField = UserDataField;
	const SYMBOL_USERDATA_FIELD* EndOfUserDataField = &ParentUserData->Fields[UserDataFieldCount];
	DWORD UserDataFieldOffset = UserDataField->Offset;

	if (UserDataField->Bits == 0)
	{
		//
		// Provided member is not a bitfield.
		// Increment to the next user data field.
		//
		NextUserDataField++;
	}
	else
	{
		//
		// If provided member is a part of a bitfield,
		// we will try to iterate until end of that bitfield.
		//
		// This is achieved through summing all bits in the bitfield members.
		//

		DWORD BitSum = 0;

		do
		{
			if (NextUserDataField->Offset != UserDataFieldOffset)
			{
				//
				// If offsets don't match up before appropriate sum was reached,
				// it means that the UserDataField provided was actually
				// already (non-first) member of some bitfield.
				//

				if (NextUserDataField->Bits == 0)
				{
					//
					// If next user data field is not a part of the bitfield,
					// just break here.
					//

					break;
				}
				else
				{
					//
					// If next user data field is a part of the bitfield,
					// it means that new bitfield starts after the current one.
					// Just reset the counter and start counting from the scratch.
					//

					BitSum = 0;
				}
			}

			BitSum += NextUserDataField->Bits;

			if (BitSum == NextUserDataField->Type->Size * 8)
			{
				//
				// If the sum now equals to the bit size of the data type
				// of the bitfield, break the loop.
				// The NextUserDataField currently points to the last member
				// of the bitfield, so we increment it here so it points
				// to the first member after the current bitfield.
				//

				NextUserDataField++;
				break;
			}
		} while (++NextUserDataField < EndOfUserDataField);
	}

	if (NextUserDataField >= EndOfUserDataField)
	{
		return EndOfUserDataField;
	}

	return NextUserDataField;
}

template <
	typename MEMBER_DEFINITION_TYPE
>
bool
PDBSymbolVisitor<MEMBER_DEFINITION_TYPE>::Is64BitBasicType(
	const SYMBOL* Symbol
	)
{
	return (Symbol->Tag == SymTagBaseType && Symbol->Size == 8);
}
