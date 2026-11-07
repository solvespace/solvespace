// Dependency-only probe. It never converts geometry or writes a STEP file.
#include "stepreader.h"

#include <fstream>
#include <iostream>

using namespace SolveSpace;

int main(int argc, char **argv) {
    if(argc < 2) {
        std::cout << "Experimental dependency-only STEP probe (no geometry import).\n"
                     "Usage: solvespace-step-probe file.step [file2.stp ...]\n"
                     "Compiled bindings:\n";
        for(const auto &binding : Step::SupportedBindings()) {
            std::cout << "  " << binding.protocol << ": " << binding.schema << "\n";
        }
        return 0;
    }
    int status = 0;
    for(int i = 1; i < argc; ++i) {
        // The reader accepts streams, not filenames. This standalone diagnostic
        // tool uses ordinary command-line paths; a future importer will use
        // SolveSpace's Platform::Path handling, including Unicode on Windows.
        std::ifstream input(argv[i], std::ios::binary);
        auto result = Step::Read(input);
        std::cout << argv[i] << ":\n";
        if(!result.document) {
            std::cerr << result.error << "\n" << result.readerLog;
            status = 1;
            continue;
        }
        auto &document = *result.document;
        std::cout << "  " << document.Protocol() << " (" << document.EntityTypeCount()
                  << " schema entity types), " << document.EntityCount() << " instances\n"
                  << "  FILE_SCHEMA: " << document.DeclaredSchema() << "\n";
        for(const char *type : { "MANIFOLD_SOLID_BREP", "BREP_WITH_VOIDS", "CLOSED_SHELL",
                                "ADVANCED_FACE", "EDGE_CURVE", "B_SPLINE_SURFACE",
                                "SI_UNIT", "CONVERSION_BASED_UNIT" }) {
            std::cout << "  " << type << ": " << document.EntitiesOfType(type).size() << "\n";
        }
        if(!result.readerLog.empty()) std::cerr << result.readerLog;
    }
    return status;
}
