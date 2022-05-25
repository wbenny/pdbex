#pragma once
#include "UdtFieldDefinitionBase.h"

#include <string>
#include <stack>
#include <vector>

class UdtFieldDefinition
	: public UdtFieldDefinitionBase
{
public:
	void VisitBaseType(const SYMBOL* Symbol) override
	{
		if (Symbol->BaseType == btFloat && Symbol->Size == 10)
			m_Comment += " /* 80-bit float */";

		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";
		m_TypePrefix += PDB::GetBasicTypeString(Symbol);
	}

	void VisitTypedefTypeEnd(const SYMBOL* Symbol) override
	{
		m_TypePrefix = "typedef " + m_TypePrefix;
	}

	void VisitEnumType(const SYMBOL* Symbol) override
	{
		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";
		m_TypePrefix += Symbol->Name;
	}

	void VisitUdtType(const SYMBOL* Symbol) override
	{
		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";

		//
		// If this returns null, it probably means BasicTypeMapMSVC and/or BasicTypeMapStdInt are out of date compared to MS DIA.
		// Output the (non-compilable) type "<unknown_type>" instead of crashing.
		//

		const CHAR* BasicTypeString = PDB::GetBasicTypeString(Symbol, m_Settings->UseStdInt);
		m_TypePrefix += (BasicTypeString != nullptr ? BasicTypeString : "<unknown_type>");
	}


	void VisitPointerTypeEnd(const SYMBOL* Symbol) override
	{
		if (Symbol->u.Pointer.Type->Tag == SymTagFunctionType)
		{
			if (Symbol->u.Pointer.IsReference)
				m_MemberName = "& " + m_MemberName;
			else	m_MemberName = "* " + m_MemberName;

			if (Symbol->IsConst)	m_MemberName += " const";
			if (Symbol->IsVolatile)	m_MemberName += " volatile";

			m_MemberName = "(" + m_MemberName + ")";

			return;
		}

		if (Symbol->u.Pointer.IsReference)
			m_TypePrefix += "&";
		else	m_TypePrefix += "*";

		if (Symbol->IsConst)	m_TypePrefix += " const";
		if (Symbol->IsVolatile)	m_TypePrefix += " volatile";
	}

	void VisitArrayTypeEnd(const SYMBOL* Symbol) override
	{
		if (Symbol->u.Array.ElementCount == 0)
		{
			const_cast<SYMBOL*>(Symbol)->Size = 1;
			m_TypeSuffix += "[]";
		} else
		{
			m_TypeSuffix += "[" + std::to_string(Symbol->u.Array.ElementCount) + "]";
		}
	}

	void VisitFunctionTypeBegin(const SYMBOL* Symbol) override
	{
		if (m_Funcs.size())
		{
			if (m_Funcs.top().Name == m_MemberName)
				m_MemberName = "";
		}

		m_Funcs.push(Function{m_MemberName, m_Args});
		m_MemberName = "";
	}

	void VisitFunctionTypeEnd(const SYMBOL* Symbol) override
	{
		if (Symbol->u.Function.IsStatic)
		{
			m_TypePrefix = "static " + m_TypePrefix;
		} else
		if (Symbol->u.Function.IsVirtual)
		{
			m_TypePrefix = "virtual " + m_TypePrefix;
		}
	#if 0
		std::string Access;
		switch (Symbol->u.Function.Access)
		{
		case 1: Access = "private "; break;
		case 2: Access = "protected "; break;
		case 3: Access = "public "; break;
		}
		m_TypePrefix = Access + m_TypePrefix;
	#endif
		if (Symbol->u.Function.IsConst)		m_Comment += " const";
		if (Symbol->u.Function.IsOverride)	m_Comment += " override";
		if (Symbol->u.Function.IsPure)		m_Comment += " = 0";

		if (Symbol->u.Function.IsVirtual)
		{
			char hexbuf[16];
			snprintf(hexbuf, sizeof(hexbuf), " 0x%02x ", Symbol->u.Function.VirtualOffset);
			m_Comment += " /*" + std::string(hexbuf) + "*/";
		}

		if (m_TypeSuffix.size())
			m_TypePrefix = GetPrintableDefinition();

		m_TypeSuffix = "";

		for (auto && e : m_Args)
		{
			if (m_TypeSuffix.size()) m_TypeSuffix += ", ";
			m_TypeSuffix += e;
		}
		m_TypeSuffix = "(" + m_TypeSuffix + ")";

		Function func = m_Funcs.top();

		m_Funcs.pop();

		m_Args = func.Args;
		m_MemberName = func.Name;
	}

	void VisitFunctionArgTypeBegin(const SYMBOL* Symbol) override
	{
		Function func = m_Funcs.top();
		//TODO
	}

	void VisitFunctionArgTypeEnd(const SYMBOL* Symbol) override
	{
		std::string ArgName;
		if (Symbol->Name) ArgName = std::string(" ") + Symbol->Name;

		if (m_MemberName.find('(') == std::string::npos)
		{
			m_Args.push_back(m_TypePrefix + ArgName);
			m_TypeSuffix = "";
			m_TypePrefix = "";
		} else
		{
			m_TypeSuffix = GetPrintableDefinition();
			m_Args.push_back(m_TypeSuffix + ArgName);
			m_TypeSuffix = "";
			m_TypePrefix = "";
		}

		Function func = m_Funcs.top();

		m_MemberName = func.Name;
		//TODO
	}

	void SetMemberName(const CHAR* MemberName) override
	{
		m_MemberName = MemberName ? MemberName : std::string();
	}

	std::string GetPrintableDefinition() const override
	{
		return m_TypePrefix + " " + m_MemberName + m_TypeSuffix + m_Comment;
	}

private:
	struct Function
	{
		std::string Name;
		std::vector<std::string> Args;
	};

	std::string m_TypePrefix;
	std::string m_MemberName;
	std::string m_TypeSuffix;
	std::string m_Comment;
	std::stack<Function> m_Funcs;
	std::vector<std::string> m_Args;
};
