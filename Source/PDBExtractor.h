#pragma once
#include "PDBSymbolSorterBase.h"
#include "PDBHeaderReconstructor.h"
#include "PDBSymbolVisitor.h"
#include "UdtFieldDefinition.h"

#include <memory>
#include <string>

// From ntimage.h
#ifndef IMAGE_FILE_MACHINE_CHPE_X86
#define IMAGE_FILE_MACHINE_CHPE_X86 0x3A64
#endif

#define PDBEX_VERSION_MAJOR 0
#define PDBEX_VERSION_MINOR 18

#define PDBEX_VERSION_STRING "0.18"

class PDBExtractor
{
public:
	struct Settings
	{
		PDBHeaderReconstructor::Settings PdbHeaderReconstructorSettings;

		std::string PdbPath;

		const char* OutputFilename = nullptr;
	};

	int Run(int argc, char** argv);

private:
	void PrintUsage();
	void ParseParameters(int argc, char** argv);
	void OpenPDBFile();
	void PrintPDBDefinitions();
	void PrintPDBFunctions();
	void DumpAllSymbols();
	void CloseOpenFiles();

private:
	PDB m_PDB;
	Settings m_Settings;

	std::unique_ptr<PDBSymbolSorterBase> m_SymbolSorter;
	std::unique_ptr<PDBHeaderReconstructor> m_HeaderReconstructor;
	std::unique_ptr<PDBSymbolVisitor<UdtFieldDefinition>> m_SymbolVisitor;
};
