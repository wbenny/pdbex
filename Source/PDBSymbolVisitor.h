#pragma once
#include "PDB.h"
#include "PDBSymbolVisitorBase.h"
#include "PDBReconstructorBase.h"

#include <memory>
#include <stack>

template <
	typename MEMBER_DEFINITION_TYPE
>
class PDBSymbolVisitor
	: public PDBSymbolVisitorBase
{
	public:
		//
		// Public methods.
		//

		PDBSymbolVisitor(
			PDBReconstructorBase* ReconstructVisitor,
			void* MemberDefinitionSettings = nullptr
			);

		void
		Run(
			const SYMBOL* Symbol
			);

	protected:
		//
		// Protected methods.
		//

		void
		Visit(
			const SYMBOL* Symbol
			) override;

		void
		VisitBaseType(
			const SYMBOL* Symbol
			) override;

		void
		VisitEnumType(
			const SYMBOL* Symbol
			) override;

		void
		VisitTypedefType(
			const SYMBOL* Symbol
			) override;

		void
		VisitPointerType(
			const SYMBOL* Symbol
			) override;

		void
		VisitArrayType(
			const SYMBOL* Symbol
			) override;

		void
		VisitFunctionType(
			const SYMBOL* Symbol
			) override;

		void
		VisitFunctionArgType(
			const SYMBOL* Symbol
			) override;

		void
		VisitUserDataType(
			const SYMBOL* Symbol
			) override;

		void
		VisitOtherType(
			const SYMBOL* Symbol
			) override;

		void
		VisitEnumField(
			const SYMBOL_ENUM_FIELD* EnumField
			) override;

		void
		VisitUserDataField(
			const SYMBOL_USERDATA_FIELD* UserDataField
			) override;

		void
		VisitUserDataFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			) override;

		void
		VisitUserDataFieldBitFieldEnd(
			const SYMBOL_USERDATA_FIELD* UserDataField
			) override;

	private:
		//
		// Private data types.
		//

		struct AnonymousUserDataType
		{
			//
			// This structure holds information about
			// nested anonymous user data types.
			// Anonymous user data type (ie. anonymous struct)
			// is a type which members are in fact members
			// of a parent user data type.
			//
			// struct Foo
			// {
			//   struct
			//   {
			//     int hi;
			//     int bye;
			//   }; // <--- no member name!
			// };
			//
			// Visit http://stackoverflow.com/a/14248127 for more information about differences
			// between unnamed and anonymous data types.
			//

			AnonymousUserDataType(
				UdtKind UserDataTypeKind,
				const SYMBOL_USERDATA_FIELD* FirstUserDataField,
				const SYMBOL_USERDATA_FIELD* LastUserDataField,
				DWORD Size = 0,
				DWORD MemberCount = 0
				)
			{
				this->UserDataTypeKind   = UserDataTypeKind;
				this->FirstUserDataField = FirstUserDataField;
				this->LastUserDataField  = LastUserDataField;
				this->Size               = Size;
				this->MemberCount        = MemberCount;
			}

			//
			// First member of the anonymous user data type.
			//
			const SYMBOL_USERDATA_FIELD* FirstUserDataField;

			//
			// Last member of the anonymous user data type.
			//
			const SYMBOL_USERDATA_FIELD* LastUserDataField;

			//
			// Size of the anonymous user data type.
			//
			DWORD Size;

			//
			// Current count of members in this anonymous user data type.
			//
			DWORD MemberCount;

			//
			// User data type kind.
			//
			UdtKind UserDataTypeKind;
		};

		struct BitFieldRange
		{
			const SYMBOL_USERDATA_FIELD* FirstUserDataFieldBitField;
			const SYMBOL_USERDATA_FIELD* LastUserDataFieldBitField;

			BitFieldRange()
				: FirstUserDataFieldBitField(nullptr)
				, LastUserDataFieldBitField(nullptr)
			{

			}

			void
			Clear()
			{
				FirstUserDataFieldBitField = nullptr;
				LastUserDataFieldBitField = nullptr;
			}

			bool
			HasValue() const
			{
				return FirstUserDataFieldBitField != nullptr &&
				       LastUserDataFieldBitField  != nullptr;
			}
		};

		struct UserDataFieldContext
		{
			UserDataFieldContext(
				const SYMBOL_USERDATA_FIELD* UserDataField, 
				BOOL RespectBitFields = TRUE
				)
			{
				SYMBOL_USERDATA* ParentUserData = &UserDataField->Parent->u.UserData;
				DWORD UserDataFieldCount = ParentUserData->FieldCount;

				FirstUserDataField    = &ParentUserData->Fields[0];
				EndOfUserDataField    = &ParentUserData->Fields[UserDataFieldCount];

				PreviousUserDataField = &UserDataField[-1];
				CurrentUserDataField  = &UserDataField[ 0];
				NextUserDataField     = &UserDataField[ 1];

				this->RespectBitFields = RespectBitFields;

				if (RespectBitFields)
				{
					NextUserDataField   = GetNextUserDataFieldWithRespectToBitFields(UserDataField);
				}
			}

			bool
			IsFirst() const
			{
				return PreviousUserDataField < FirstUserDataField;
			}

			bool
			IsLast() const
			{
				return NextUserDataField == EndOfUserDataField;
			}

			bool
			GetNext()
			{
				PreviousUserDataField = CurrentUserDataField;
				CurrentUserDataField  = NextUserDataField;
				NextUserDataField     = &CurrentUserDataField[1];

				if (RespectBitFields && IsLast() == false)
				{
					NextUserDataField = GetNextUserDataFieldWithRespectToBitFields(CurrentUserDataField);
				}

				return IsLast() == false;
			}

			const SYMBOL_USERDATA_FIELD* FirstUserDataField;
			const SYMBOL_USERDATA_FIELD* EndOfUserDataField;

			const SYMBOL_USERDATA_FIELD* PreviousUserDataField;
			const SYMBOL_USERDATA_FIELD* CurrentUserDataField;
			const SYMBOL_USERDATA_FIELD* NextUserDataField;

			BOOL RespectBitFields;
		};

		using AnonymousUserDataTypeStack = std::stack<std::shared_ptr<AnonymousUserDataType>>;
		using ContextStack               = std::stack<std::shared_ptr<UserDataFieldDefinitionBase>>;

	private:
		//
		// Private methods.
		//

		void
		CheckForDataFieldPadding(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		void
		CheckForAnonymousUnion(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		void
		CheckForAnonymousStruct(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		void
		CheckForEndOfAnonymousUserDataType(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		std::shared_ptr<UserDataFieldDefinitionBase>
		MemberDefinitionFactory();

		void
		PushAnonymousUserDataType(
			std::shared_ptr<AnonymousUserDataType> Item
			);

		void
		PopAnonymousUserDataType();

	private:
		//
		// Static methods.
		//

		static
		const SYMBOL_USERDATA_FIELD*
		GetNextUserDataFieldWithRespectToBitFields(
			const SYMBOL_USERDATA_FIELD* UserDataField
			);

		static
		bool
		Is64BitBasicType(
			const SYMBOL* Symbol
			);

	private:
		//
		// Class properties.
		//

		//
		// These two properties are used for padding.
		// m_SizeOfPreviousUserDataField holds the size of the previous
		// user data field with respect to nested unnamed and anonymous
		// user data types.
		//
		// m_PreviousUserDataField just holds pointer to the previous
		// user data field.
		//
		DWORD m_SizeOfPreviousUserDataField = 0;
		const SYMBOL_USERDATA_FIELD* m_PreviousUserDataField = nullptr;

		//
		// This stack holds information about anonymous user data types.
		// More information about anonymous user data types are in documentation
		// of the AnonymousUserDataType struct.
		//
		AnonymousUserDataTypeStack m_AnonymousUserDataTypeStack;

		AnonymousUserDataTypeStack m_AnonymousUnionStack;
		AnonymousUserDataTypeStack m_AnonymousStructStack;

		//
		// Holds information about current bitfield.
		//
		BitFieldRange m_CurrentBitField;

		//
		// This stack holds instance of a class which will be responsible
		// for the formatting of the current member (user data field) -
		// - its type, member name, ...
		//
		ContextStack m_MemberContextStack;

		//
		// Settings for this Visit.
		//
		PDBReconstructorBase* m_ReconstructVisitor;

		//
		// Settigs for constructing member definitions.
		//
		void* m_MemberDefinitionSettings;
};

#include "PDBSymbolVisitor.inl"
