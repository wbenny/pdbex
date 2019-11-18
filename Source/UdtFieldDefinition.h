#pragma once
#include "UdtFieldDefinitionBase.h"

#include <string>
#include <stack>
#include <vector>

class UdtFieldDefinition
	: public UdtFieldDefinitionBase
{
public:
	struct Settings
	{
		bool UseStdInt = false;
	};

	void VisitBaseType(const SYMBOL* Symbol) override
	{
		if (Symbol->BaseType == btFloat && Symbol->Size == 10)
		{
			m_Comment += " /* 80-bit float */";
		}

		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";

		m_TypePrefix += PDB::GetBasicTypeString(Symbol, m_Settings->UseStdInt);
	}

	void VisitTypedefTypeEnd(const SYMBOL* Symbol) override
	{
		m_TypePrefix = "typedef " + m_TypePrefix;
	}

	void VisitEnumType(const SYMBOL* Symbol) override
	{
		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";

		m_TypePrefix += "enum ";
		m_TypePrefix += Symbol->Name;
	}

	void VisitUdtType(const SYMBOL* Symbol) override
	{
		if (Symbol->IsConst)	m_TypePrefix += "const ";
		if (Symbol->IsVolatile)	m_TypePrefix += "volatile ";

		m_TypePrefix += PDB::GetUdtKindString(Symbol->u.Udt.Kind);
		m_TypePrefix += " ";
		m_TypePrefix += Symbol->Name;
	}


	void VisitPointerTypeEnd(const SYMBOL* Symbol) override
	{
		if (Symbol->u.Pointer.Type->Tag == SymTagFunctionType)
		{
			if (Symbol->u.Pointer.IsReference)
			{
				m_MemberName += "& ";
			} else
			{
				m_MemberName += "* ";
			}

			if (Symbol->IsConst)	m_MemberName += " const";
			if (Symbol->IsVolatile)	m_MemberName += " volatile";

			m_MemberName = "(" + m_MemberName + ")";

			return;
		}

		if (Symbol->u.Pointer.IsReference)
		{
			m_TypePrefix += "&";
		} else
		{
			m_TypePrefix += "*";
		}

		if (Symbol->IsConst)	m_TypePrefix += " const";
		if (Symbol->IsVolatile)	m_TypePrefix += " volatile";
	}

	void VisitArrayTypeEnd(const SYMBOL* Symbol) override
	{
		if (Symbol->u.Array.ElementCount == 0)
		{
			const_cast<SYMBOL*>(Symbol)->Size = 1;
			m_TypePrefix += "*";

			m_Comment += " /* zero-length array */";
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
		if (Symbol->u.Udt.BaseClassCount)
		{
			m_TypePrefix = "virtual " + m_TypePrefix;
		}

		if (Symbol->u.Function.IsConst)		m_Comment += " const";
		if (Symbol->u.Function.IsOverride)	m_Comment += " override";
		if (Symbol->u.Function.IsPure)		m_Comment += " = 0";

		if (Symbol->u.Function.IsVirtual)
		{
			m_Comment += " /* VO: " + std:to_string(Symbol->u.Function.VirtualOffset) + " */"; //hex
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
		if (m_MemberName.find('(') == std::string::npos)
		{
			m_Args.push_back(m_TypePrefix);
			m_TypeSuffix = "";
			m_TypePrefix = "";
		} else
		{
			m_TypeSuffix = 	GetPrintableDefinition();
			m_Args.push_back(m_TypeSuffix);
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

	void SetSettings(void* MemberDefinitionSettings) override
	{
		static Settings DefaultSettings;
		if (MemberDefinitionSettings == nullptr)
		{
			MemberDefinitionSettings = &DefaultSettings;
		}

		m_Settings = static_cast<Settings*>(MemberDefinitionSettings);
	}

	virtual void* GetSettings() override
	{
		return &m_Settings;
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

	Settings* m_Settings;
};
