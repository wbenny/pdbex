#include "PDBExtractor.h"
#include "PDBHeaderReconstructor.h"
#include "PDBSymbolVisitor.h"
#include "PDBSymbolSorter.h"
#include "UdtFieldDefinition.h"

#include <iostream>
#include <fstream>
#include <stdexcept>

namespace
{
	static const char* MESSAGE_INVALID_PARAMETERS = "Invalid parameters";
	static const char* MESSAGE_FILE_NOT_FOUND = "File not found";
	static const char* MESSAGE_SYMBOL_NOT_FOUND = "Symbol not found";

	class PDBDumperException
		: public std::runtime_error
	{
	public:
		PDBDumperException(const char* Message)
			: std::runtime_error(Message)
		{

		}
	};
}

int PDBExtractor::Run(int argc, char** argv)
{
	int Result = ERROR_SUCCESS;

	try {
		ParseParameters(argc, argv);
		OpenPDBFile();
		DumpAllSymbols();
	} catch (const PDBDumperException& e)
	{
		std::cerr << e.what() << std::endl;
		Result = EXIT_FAILURE;
	}

	CloseOpenFiles();

	return Result;
}

void PDBExtractor::PrintUsage()
{
	printf("Extracts types and structures from PDB (Program database).\n");
	printf("\n");
	printf("pdbex <path> [-o <filename>] [-e <type>]\n");
	printf("                     [-u <prefix>] [-s prefix] [-r prefix] [-g suffix]\n");
	printf("                     [-p] [-x] [-b] [-d]\n");
	printf("\n");
	printf("<path>               Path to the PDB file.\n");
	printf(" -o filename         Specifies the output file.                       (stdout)\n");
	printf(" -e [n,i,a]          Specifies expansion of nested structures/unions. (i)\n");
	printf("                       n = none            Only top-most type is printed.\n");
	printf("                       i = inline unnamed  Unnamed types are nested.\n");
	printf("                       a = inline all      All types are nested.\n");
	printf(" -u prefix           Unnamed union prefix  (in combination with -d).\n");
	printf(" -s prefix           Unnamed struct prefix (in combination with -d).\n");
	printf(" -r prefix           Prefix for all symbols.\n");
	printf(" -g suffix           Suffix for all symbols.\n");
	printf("\n");
	printf("Following options can be explicitly turned off by adding trailing '-'.\n");
	printf("Example: -p-\n");
	printf(" -p                  Create padding members.                          (T)\n");
	printf(" -x                  Show offsets.                                    (T)\n");
	printf(" -b                  Allow bitfields in union.                        (F)\n");
	printf(" -d                  Allow unnamed data types.                        (T)\n");
	printf("\n");
}

void PDBExtractor::ParseParameters(int argc, char** argv)
{
	if ( argc == 1 ||
	    (argc == 2 && strcmp(argv[1], "-h") == 0) ||
	    (argc == 2 && strcmp(argv[1], "--help") == 0))
	{
		PrintUsage();
		exit(EXIT_SUCCESS);
	}

	int ArgumentPointer = 0;

	m_Settings.PdbPath = argv[++ArgumentPointer];

	while (++ArgumentPointer < argc)
	{
		const char* CurrentArgument = argv[ArgumentPointer];
		size_t CurrentArgumentLength = strlen(CurrentArgument);

		const char* NextArgument = ArgumentPointer < argc ? argv[ArgumentPointer + 1] : nullptr;
		size_t NextArgumentLength = NextArgument ? strlen(CurrentArgument) : 0;

		if ((CurrentArgumentLength != 2 && CurrentArgumentLength != 3) ||
		    (CurrentArgumentLength == 2 && CurrentArgument[0] != '-') ||
		    (CurrentArgumentLength == 3 && CurrentArgument[0] != '-' && CurrentArgument[2] != '-'))
		{
			throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
		}

		bool OffSwitch = CurrentArgumentLength == 3 && CurrentArgument[2] == '-';

		switch (CurrentArgument[1])
		{
		case 'o':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			m_Settings.OutputFilename = NextArgument;

			m_Settings.PdbHeaderReconstructorSettings.OutputFile = new std::ofstream(
				NextArgument, std::ios::out);
			break;

		case 'e':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			switch (NextArgument[0])
			{
			case 'n':
				m_Settings.PdbHeaderReconstructorSettings.MemberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::None;
				break;

			case 'i':
				m_Settings.PdbHeaderReconstructorSettings.MemberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed;
				break;

			case 'a':
				m_Settings.PdbHeaderReconstructorSettings.MemberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineAll;
				break;

			default:
				m_Settings.PdbHeaderReconstructorSettings.MemberStructExpansion =
					PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed;
				break;
			}
			break;

		case 'u':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			m_Settings.PdbHeaderReconstructorSettings.AnonymousUnionPrefix = NextArgument;
			break;

		case 's':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			m_Settings.PdbHeaderReconstructorSettings.AnonymousStructPrefix = NextArgument;
			break;

		case 'r':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			m_Settings.PdbHeaderReconstructorSettings.SymbolPrefix = NextArgument;
			break;

		case 'g':
			if (!NextArgument)
				throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);

			++ArgumentPointer;
			m_Settings.PdbHeaderReconstructorSettings.SymbolSuffix = NextArgument;
			break;

		case 'p':
			m_Settings.PdbHeaderReconstructorSettings.CreatePaddingMembers = !OffSwitch;
			break;

		case 'x':
			m_Settings.PdbHeaderReconstructorSettings.ShowOffsets = !OffSwitch;
			break;

		case 'b':
			m_Settings.PdbHeaderReconstructorSettings.AllowBitFieldsInUnion = !OffSwitch;
			break;

		case 'd':
			m_Settings.PdbHeaderReconstructorSettings.AllowAnonymousDataTypes = !OffSwitch;
			break;

		default:
			throw PDBDumperException(MESSAGE_INVALID_PARAMETERS);
		}
	}

	m_HeaderReconstructor = std::make_unique<PDBHeaderReconstructor>(&m_Settings.PdbHeaderReconstructorSettings);
	m_SymbolVisitor = std::make_unique<PDBSymbolVisitor<UdtFieldDefinition>>(m_HeaderReconstructor.get());
	m_SymbolSorter = std::make_unique<PDBSymbolSorter>();
}

void PDBExtractor::OpenPDBFile()
{
	if (m_PDB.Open(m_Settings.PdbPath.c_str()) == FALSE)
		throw PDBDumperException(MESSAGE_FILE_NOT_FOUND);
}

void PDBExtractor::PrintPDBDeclarations()
{
	for (auto&& e : m_SymbolSorter->GetSortedSymbols())
	{
		if (e->Tag == SymTagUDT && !PDB::IsUnnamedSymbol(e))
		{
			*m_Settings.PdbHeaderReconstructorSettings.OutputFile
				<< PDB::GetUdtKindString(e->u.Udt.Kind)
				<< " " << m_HeaderReconstructor->GetCorrectedSymbolName(e) << ";"
				<< std::endl;
		} else if (e->Tag == SymTagEnum && !PDB::IsUnnamedSymbol(e))
		{
			*m_Settings.PdbHeaderReconstructorSettings.OutputFile
				<< "enum"
				<< " " << m_HeaderReconstructor->GetCorrectedSymbolName(e) << ";"
				<< std::endl;
		}
	}

	*m_Settings.PdbHeaderReconstructorSettings.OutputFile << std::endl;
}

void PDBExtractor::PrintPDBDefinitions()
{
	for (auto&& e : m_SymbolSorter->GetSortedSymbols())
	{
		bool Expand = true;

		if (m_Settings.PdbHeaderReconstructorSettings.MemberStructExpansion == PDBHeaderReconstructor::MemberStructExpansionType::InlineUnnamed &&
		    e->Tag == SymTagUDT &&
		    PDB::IsUnnamedSymbol(e))
		{
			Expand = false;
		}

		if (Expand)
		{
			m_SymbolVisitor->Run(e);
		}
	}
}

void PDBExtractor::PrintPDBFunctions()
{
	*m_Settings.PdbHeaderReconstructorSettings.OutputFile << "/*" << std::endl;

	for (auto&& e : m_PDB.GetFunctionSet())
	{
		*m_Settings.PdbHeaderReconstructorSettings.OutputFile << e << std::endl;
	}

	*m_Settings.PdbHeaderReconstructorSettings.OutputFile << "*/" << std::endl;
}

void PDBExtractor::DumpAllSymbols()
{
	for (auto&& e : m_PDB.GetSymbolMap())
	{
		m_SymbolSorter->Visit(e.second);
	}

	PrintPDBDeclarations();
	PrintPDBDefinitions();
	PrintPDBFunctions();
}

void PDBExtractor::CloseOpenFiles()
{
	if (m_Settings.OutputFilename)
	{
		delete m_Settings.PdbHeaderReconstructorSettings.OutputFile;
	}
}
