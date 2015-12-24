#pragma once
#include "PDBSymbolSorter.h"
#include "PDBHeaderReconstructor.h"
#include "PDBSymbolVisitor.h"
#include "UserDataFieldDefinition.h"

#include <memory>
#include <string>

#define PDBEX_VERSION_MAJOR 0
#define PDBEX_VERSION_MINOR 1

#define PDBEX_VERSION_STRING "0.1"

class PDBExtractor
{
	public:
		struct Settings
		{
			PDBHeaderReconstructor::Settings PdbHeaderReconstructorSettings;
			UserDataFieldDefinition::Settings UserDataFieldDefinitionSettings;

			std::string SymbolName;
			std::string PdbPath;

			const char* OutputFilename = nullptr;
			const char* TestFilename = nullptr;

			bool PrintReferencedTypes = true;
			bool PrintHeader = true;
			bool PrintDeclarations = true;
			bool PrintDefinitions = true;
		};

		int Run(
			int argc,
			char** argv
			);

	private:
		void
		PrintUsage();

		void
		ParseParameters(
			int argc,
			char** argv
			);

		void
		OpenPDBFile();

		void
		PrintTestHeader();
		
		void 
		PrintTestFooter();

		void
		PrintPDBHeader();

		void
		PrintPDBDeclarations();

		void
		PrintPDBDefinitions();

		void
		GetArchitecture();

		void
		DumpAllSymbols();

		void
		DumpOneSymbol();

		void
		CloseOpenedFiles();

	private:
		PDB m_PDB;
		Settings m_Settings;

		ImageArchitecture m_Architecture = ImageArchitecture::None;

		std::unique_ptr<PDBSymbolSorter> m_SymbolSorter;
		std::unique_ptr<PDBHeaderReconstructor> m_HeaderReconstructor;
		std::unique_ptr<PDBSymbolVisitor<UserDataFieldDefinition>> m_SymbolVisitor;
};

