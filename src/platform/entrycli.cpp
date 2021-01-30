//-----------------------------------------------------------------------------
// Our main() function for the command-line interface.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "config.h"

int main(int argc, char **argv) {
    std::vector<std::string> args = Platform::InitCli(argc, argv);

    if(args.size() == 1) {
        SolveSpace::ShowUsage(args[0]);
        return 0;
    }

    if(!SolveSpace::RunCommand(args)) {
        return 1;
    } else {
        return 0;
    }
}
