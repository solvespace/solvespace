// Explicit native-conversion diagnostic; never dispatched by file linking.
#include "solvespace.h"
#include "stepreader.h"
#include "stepgeometry.h"

#include <iostream>
#include <sstream>

using namespace SolveSpace;

int main(int argc, char **argv) {
    auto args = Platform::InitCli(argc, argv);
    if(args.size() < 2) {
        std::cout << "Experimental native STEP B-rep probe (no file linking).\n"
                     "Usage: solvespace-step-geometry-probe file.step [file2.stp ...]\n"
                     "Exit codes: 0 = complete, 1 = rejected, 2 = some whole solids omitted.\n";
        return 0;
    }
    SS.chordTolCalculated = 0.01;
    SS.maxSegments = 64;
    int status = 0;
    for(size_t i = 1; i < args.size(); ++i) {
        std::cout << args[i] << ":\n";
        std::string text;
        // Use the existing Unicode-aware path/argument handling. The parser
        // still receives only a seekable stream, never a vendor-owned filename.
        if(!Platform::ReadFile(Platform::Path::From(args[i]), &text)) {
            std::cerr << "Cannot read STEP input.\n";
            status = 1;
            continue;
        }
        std::istringstream input(text);
        auto read = Step::Read(input);
        if(!read.document) {
            std::cerr << read.error << "\n" << read.readerLog;
            status = 1;
            continue;
        }
        if(!read.readerLog.empty()) std::cerr << read.readerLog;
        auto geometry = Step::ConvertGeometry(*read.document);
        read.document.reset();
        std::cout << "  native solids: " << geometry.solids.size() << "\n";
        for(const auto &solid : geometry.solids) {
            Vector max = {VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE};
            Vector min = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE};
            for(const auto &vertex : solid->vertices) vertex.position.MakeMaxMin(&max, &min);
            Vector size = max.Minus(min);
            std::cout << "  solid #" << solid->solid << " in representation #" << solid->representation
                      << ": " << solid->shell.surface.n << " surfaces, " << solid->shell.curve.n
                      << " exact curves, " << solid->vertices.size() << " source vertices\n"
                      << "    length factor: " << solid->lengthToMm << " mm per source unit\n"
                      << "    source-vertex bounds size: " << size.x << ", " << size.y << ", " << size.z << " mm\n";
            // Triangulation is a diagnostic on the already-created SShell,
            // never a conversion path or a fallback for unsupported geometry.
            SMesh mesh = {};
            solid->shell.TriangulateInto(&mesh);
            double volume = 0;
            Vector anchor = solid->vertices.front().position;
            for(const auto &triangle : mesh.l) {
                volume += triangle.a.Minus(anchor).Dot(triangle.b.Minus(anchor).Cross(
                          triangle.c.Minus(anchor))) / 6.0;
            }
            std::cout << "    native triangulation: " << mesh.l.n << " triangles, "
                      << volume << " mm^3 signed volume\n";
            mesh.Clear();
        }
        for(const auto &diagnostic : geometry.diagnostics) {
            std::cerr << "  omitted/rejected: representation #" << diagnostic.representation
                      << ", solid #" << diagnostic.solid << ", entity #" << diagnostic.entity
                      << ": " << diagnostic.message << "\n";
        }
        if(geometry.status == Step::GeometryResult::Status::Rejected) status = 1;
        else if(geometry.status == Step::GeometryResult::Status::Partial && status == 0) status = 2;
        Platform::FreeAllTemporary();
    }
    return status;
}
