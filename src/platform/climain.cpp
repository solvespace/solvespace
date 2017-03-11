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

static void ShowUsage(const std::string &cmd) {
    fprintf(stderr, "Usage: %s <command> <options> <filename> [filename...]", cmd.c_str());
//-----------------------------------------------------------------------------> 80 col */
    fprintf(stderr, R"(
    When run, performs an action specified by <command> on every <filename>.

Common options:
    -o, --output <pattern>
        For an input file <name>.slvs, replaces the '%%' symbol in <pattern>
        with <name> and uses it as output file. For example, when using
        --output %%-2d.png for input files f/a.slvs and f/b.slvs, output files
        f/a-2d.png and f/b-2d.png will be written.
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
    regenerate
        Reloads all imported files, regenerates the sketch, and saves it.
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

static bool RunCommand(const std::vector<std::string> args) {
    if(args.size() < 2) return false;

    for(const std::string &arg : args) {
        if(arg == "--help" || arg == "-h") {
            ShowUsage(args[0]);
            return true;
        }
    }

    std::function<void(const Platform::Path &)> runner;

    std::vector<Platform::Path> inputFiles;
    auto ParseInputFile = [&](size_t &argn) {
        std::string arg = args[argn];
        if(arg[0] != '-') {
            inputFiles.push_back(Platform::Path::From(arg));
            return true;
        } else return false;
    };

    std::string outputPattern;
    auto ParseOutputPattern = [&](size_t &argn) {
        if(argn + 1 < args.size() && (args[argn] == "--output" ||
                                      args[argn] == "-o")) {
            argn++;
            outputPattern = args[argn];
            return true;
        } else return false;
    };

    Vector projUp = {}, projRight = {};
    auto ParseViewDirection = [&](size_t &argn) {
        if(argn + 1 < args.size() && (args[argn] == "--view" ||
                                      args[argn] == "-v")) {
            argn++;
            if(args[argn] == "top") {
                projRight = Vector::From(1, 0, 0);
                projUp    = Vector::From(0, 1, 0);
            } else if(args[argn] == "bottom") {
                projRight = Vector::From(-1, 0, 0);
                projUp    = Vector::From(0, 1, 0);
            } else if(args[argn] == "left") {
                projRight = Vector::From(0, 1, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(args[argn] == "right") {
                projRight = Vector::From(0, -1, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(args[argn] == "front") {
                projRight = Vector::From(-1, 0, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(args[argn] == "back") {
                projRight = Vector::From(1, 0, 0);
                projUp    = Vector::From(0, 0, 1);
            } else if(args[argn] == "isometric") {
                projRight = Vector::From(0.707,  0.000, -0.707);
                projUp    = Vector::From(-0.408, 0.816, -0.408);
            } else {
                fprintf(stderr, "Unrecognized view direction '%s'\n", args[argn].c_str());
            }
            return true;
        } else return false;
    };

    double chordTol = 1.0;
    auto ParseChordTolerance = [&](size_t &argn) {
        if(argn + 1 < args.size() && (args[argn] == "--chord-ol" ||
                                      args[argn] == "-t")) {
            argn++;
            if(sscanf(args[argn].c_str(), "%lf", &chordTol) == 1) {
                return true;
            } else return false;
        } else return false;
    };

    unsigned width = 0, height = 0;
    if(args[1] == "thumbnail") {
        auto ParseSize = [&](size_t &argn) {
            if(argn + 1 < args.size() && args[argn] == "--size") {
                argn++;
                if(sscanf(args[argn].c_str(), "%ux%u", &width, &height) == 2) {
                    return true;
                } else return false;
            } else return false;
        };

        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseViewDirection(argn) ||
                 ParseChordTolerance(argn) ||
                 ParseSize(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
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

        runner = [&](const Platform::Path &output) {
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
    } else if(args[1] == "export-view") {
        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseViewDirection(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
                return false;
            }
        }

        if(EXACT(projUp.Magnitude() == 0 || projRight.Magnitude() == 0)) {
            fprintf(stderr, "View direction must be specified.\n");
            return false;
        }

        runner = [&](const Platform::Path &output) {
            SS.GW.projRight   = projRight;
            SS.GW.projUp      = projUp;
            SS.exportChordTol = chordTol;

            SS.ExportViewOrWireframeTo(output, /*exportWireframe=*/false);
        };
    } else if(args[1] == "export-wireframe") {
        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
                return false;
            }
        }

        runner = [&](const Platform::Path &output) {
            SS.exportChordTol = chordTol;

            SS.ExportViewOrWireframeTo(output, /*exportWireframe=*/true);
        };
    } else if(args[1] == "export-mesh") {
        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn) ||
                 ParseChordTolerance(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
                return false;
            }
        }

        runner = [&](const Platform::Path &output) {
            SS.exportChordTol = chordTol;

            SS.ExportMeshTo(output);
        };
    } else if(args[1] == "export-surfaces") {
        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn) ||
                 ParseOutputPattern(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
                return false;
            }
        }

        runner = [&](const Platform::Path &output) {
            StepFileWriter sfw = {};
            sfw.ExportSurfacesTo(output);
        };
    } else if(args[1] == "regenerate") {
        for(size_t argn = 2; argn < args.size(); argn++) {
            if(!(ParseInputFile(argn))) {
                fprintf(stderr, "Unrecognized option '%s'.\n", args[argn].c_str());
                return false;
            }
        }

        outputPattern = "%.slvs";

        runner = [&](const Platform::Path &output) {
            SS.SaveToFile(output);
        };
    } else {
        fprintf(stderr, "Unrecognized command '%s'.\n", args[1].c_str());
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

    for(const Platform::Path &inputFile : inputFiles) {
        Platform::Path absInputFile = inputFile.Expand(/*fromCurrentDirectory=*/true);

        Platform::Path outputFile = Platform::Path::From(outputPattern);
        size_t replaceAt = outputFile.raw.find('%');
        if(replaceAt != std::string::npos) {
            Platform::Path outputSubst = inputFile.Parent();
            outputSubst = outputSubst.Join(inputFile.FileStem());
            outputFile.raw.replace(replaceAt, 1, outputSubst.raw);
        }
        Platform::Path absOutputFile = outputFile.Expand(/*fromCurrentDirectory=*/true);

        SS.Init();
        if(!SS.LoadFromFile(absInputFile)) {
            fprintf(stderr, "Cannot load '%s'!\n", inputFile.raw.c_str());
            return false;
        }
        SS.AfterNewFile();
        runner(absOutputFile);
        SK.Clear();
        SS.Clear();

        fprintf(stderr, "Written '%s'.\n", outputFile.raw.c_str());
    }

    return true;
}

int main(int argc, char **argv) {
    std::vector<std::string> args = InitPlatform(argc, argv);

    if(args.size() == 1) {
        ShowUsage(args[0]);
        return 0;
    }

    if(!RunCommand(args)) {
        return 1;
    } else {
        return 0;
    }
}
