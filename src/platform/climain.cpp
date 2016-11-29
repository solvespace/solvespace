//-----------------------------------------------------------------------------
// Our main() function for the command-line interface.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {
    // These are defined in headless.cpp, and aren't exposed in solvespace.h.
    extern std::shared_ptr<Pixmap> framebuffer;
}

static void ShowUsage(const char *argv0) {
    fprintf(stderr, "Usage: %s <command> <options> <filename> [filename...]", argv0);
//-----------------------------------------------------------------------------> 80 col */
    fprintf(stderr, R"(
    When run, performs an action specified by <command> on every <filename>.

    Common options:
    -o, --output <pattern>
        For an input file <basename>.slvs, replaces the '%%' symbol in <pattern>
        with <basename> and uses it as output file. For example, when using
        --output %%-2d.png for input files a.slvs and b.slvs, output files
        a-2d.png and b-2d.png will be written.
    -v, --view <direction>
        Selects the camera direction. <direction> can be one of "top", "bottom",
        "left", "right", "front", "back", or "isometric".
    -t, --chord-tol <tolerance>
        Selects the chord tolerance, used for converting exact curves to
        piecewise linear, and exact surfaces into triangle meshes.
        For export commands, the unit is mm, and the default is 1.0 mm.
        For non-export commands, the unit is %%, and the default is 1.0 %%.

    Commands:
    thumbnail --output <pattern> --size <size> --view <direction>
              [--chord-tol <tolerance>]
        Outputs a rendered view of the sketch, like the SolveSpace GUI would.
        <size> is <width>x<height>, in pixels. Graphics acceleration is
        not used, and the output may look slightly different from the GUI.
    export-view --output <pattern> --view <direction> [--chord-tol <tolerance>]
        Exports a view of the sketch, in a 2d vector format.
    export-wireframe --output <pattern> [--chord-tol <tolerance>]
        Exports a wireframe of the sketch, in a 3d vector format.
    export-mesh --output <pattern> [--chord-tol <tolerance>]
        Exports a triangle mesh of solids in the sketch, with exact surfaces
        being triangulated first.
    export-surfaces --output <pattern>
        Exports exact surfaces of solids in the sketch, if any.
)");

    auto FormatListFromFileFilter = [](const FileFilter *filter) {
        std::string descr;
        while(filter->name) {
            descr += "\n        ";
            descr += filter->name;
            descr += " (";
            const char *const *patterns = filter->patterns;
            while(*patterns) {
                descr += *patterns;
                if(*++patterns) {
                    descr += ", ";
                }
            }
            descr += ")";
            filter++;
        }
        return descr;
    };

    fprintf(stderr, R"(
    File formats:
    thumbnail:%s
    export-view:%s
    export-wireframe:%s
    export-mesh:%s
    export-surfaces:%s
)", FormatListFromFileFilter(PngFileFilter).c_str(),
    FormatListFromFileFilter(VectorFileFilter).c_str(),
    FormatListFromFileFilter(Vector3dFileFilter).c_str(),
    FormatListFromFileFilter(MeshFileFilter).c_str(),
    FormatListFromFileFilter(SurfaceFileFilter).c_str());
}

static bool RunCommand(size_t argc, char **argv) {
    if(argc < 2) return false;

    std::function<void(const std::string &)> runner;

    std::vector<std::string> inputFiles;
    auto ParseInputFile = [&](size_t &argn) {
        std::string arg = argv[argn];
        if(arg[0] != '-') {
            inputFiles.push_back(arg);
            return true;
        } else return false;
    };

    std::string outputPattern;
    auto ParseOutputPattern = [&](size_t &argn) {
        if(argn + 1 < argc && (!strcmp(argv[argn], "--output") ||
                               !strcmp(argv[argn], "-o"))) {
            argn++;
            outputPattern = argv[argn];
            return true;
        } else return false;
    };

    Vector projUp, projRight;
    auto ParseViewDirection = [&](size_t &argn) {
        if(argn + 1 < argc && (!strcmp(argv[argn], "--view") ||
                               !strcmp(argv[argn], "-v"))) {
            argn++;
            if(!strcmp(argv[argn], "top")) {
                projRight = Vector::From(1, 0, 0);
                projUp    = Vector::From(0, 1, 0);
            } else if(!strcmp(argv[argn], "bottom")) {
                projRight = Vector::From(-1, 0, 0);
                projUp    = Vector::From(0, 1, 0);
            } else if(!strcmp(argv[argn], "left")) {
                projRight = Vector::From(0, 1, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(!strcmp(argv[argn], "right")) {
                projRight = Vector::From(0, -1, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(!strcmp(argv[argn], "front")) {
                projRight = Vector::From(-1, 0, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(!strcmp(argv[argn], "back")) {
                projRight = Vector::From(1, 0, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(!strcmp(argv[argn], "isometric")) {
                projRight = Vector::From(0.707,  0.000, -0.707);
                projUp    = Vector::From(-0.408, 0.816, -0.408);
            } else {
                fprintf(stderr, "Unrecognized view direction '%s'\n", argv[argn]);
            }
            return true;
        } else return false;
    };

    double chordTol = 1.0;
    auto ParseChordTolerance = [&](size_t &argn) {
        if(argn + 1 < argc && (!strcmp(argv[argn], "--chord-tol") ||
                               !strcmp(argv[argn], "-t"))) {
            argn++;
            if(sscanf(argv[argn], "%lf", &chordTol) == 1) {
                return true;
            } else return false;
        } else return false;
    };

    if(!strcmp(argv[1], "thumbnail")) {
        unsigned width, height;
        auto ParseSize = [&](size_t &argn) {
            if(argn + 1 < argc && !strcmp(argv[argn], "--size")) {
                argn++;
                if(sscanf(argv[argn], "%ux%u", &width, &height) == 2) {
                    return true;
                } else return false;
            } else return false;
        };

        for(size_t argn = 2; argn < argc; argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseViewDirection(argn) ||
                 ParseChordTolerance(argn) ||
                 ParseSize(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", argv[argn]);
                return false;
            }
        }

        if(width == 0 || height == 0) {
            fprintf(stderr, "Non-zero viewport size must be specified.\n");
            return false;
        }

        if(EXACT(projUp.Magnitude() == 0 || projRight.Magnitude() == 0)) {
            fprintf(stderr, "View direction must be specified.\n");
            return false;
        }

        runner = [&](const std::string &output) {
            SS.GW.width     = width;
            SS.GW.height    = height;
            SS.GW.projRight = projRight;
            SS.GW.projUp    = projUp;
            SS.chordTol     = chordTol;

            SS.GW.ZoomToFit(/*includingInvisibles=*/false);
            SS.GenerateAll();
            PaintGraphics();
            framebuffer->WritePng(output, /*flip=*/true);
        };
    } else if(!strcmp(argv[1], "export-view")) {
        for(size_t argn = 2; argn < argc; argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseViewDirection(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", argv[argn]);
                return false;
            }
        }

        if(EXACT(projUp.Magnitude() == 0 || projRight.Magnitude() == 0)) {
            fprintf(stderr, "View direction must be specified.\n");
            return false;
        }

        runner = [&](const std::string &output) {
            SS.GW.projRight   = projRight;
            SS.GW.projUp      = projUp;
            SS.exportChordTol = chordTol;

            SS.ExportViewOrWireframeTo(output, /*exportWireframe=*/false);
        };
    } else if(!strcmp(argv[1], "export-wireframe")) {
        for(size_t argn = 2; argn < argc; argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", argv[argn]);
                return false;
            }
        }

        runner = [&](const std::string &output) {
            SS.exportChordTol = chordTol;

            SS.ExportViewOrWireframeTo(output, /*exportWireframe=*/true);
        };
    } else if(!strcmp(argv[1], "export-mesh")) {
        for(size_t argn = 2; argn < argc; argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", argv[argn]);
                return false;
            }
        }

        runner = [&](const std::string &output) {
            SS.exportChordTol = chordTol;

            SS.ExportMeshTo(output);
        };
    } else if(!strcmp(argv[1], "export-surfaces")) {
        for(size_t argn = 2; argn < argc; argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", argv[argn]);
                return false;
            }
        }

        runner = [&](const std::string &output) {
            StepFileWriter sfw = {};
            sfw.ExportSurfacesTo(output);
        };
    } else {
        fprintf(stderr, "Unrecognized command '%s'.\n", argv[1]);
        return false;
    }

    if(outputPattern.empty()) {
        fprintf(stderr, "An output pattern must be specified.\n");
        return false;
    } else if(outputPattern.find('%') == std::string::npos && inputFiles.size() > 1) {
        fprintf(stderr,
                "Output pattern must include a %% symbol when using multiple inputs!\n");
        return false;
    }

    if(inputFiles.size() == 0) {
        fprintf(stderr, "At least one input file must be specified.\n");
        return false;
    }

    for(const std::string &inputFile : inputFiles) {
        std::string outputFile = outputPattern;
        size_t replaceAt = outputFile.find('%');
        if(replaceAt != std::string::npos) {
            outputFile.replace(replaceAt, 1, Basename(inputFile, /*stripExtension=*/true));
        }

        SS.Init();
        if(!SS.LoadFromFile(inputFile)) {
            fprintf(stderr, "Cannot load '%s'!\n", inputFile.c_str());
            return false;
        }
        SS.AfterNewFile();
        runner(outputFile);
        SK.Clear();
        SS.Clear();

        fprintf(stderr, "Written '%s'.\n", outputFile.c_str());
    }

    return true;
}

int main(int argc, char **argv) {
    InitPlatform();

    if(argc == 1) {
        ShowUsage(argv[0]);
        return 0;
    }

    if(!RunCommand(argc, argv)) {
        return 1;
    } else {
        return 0;
    }
}
