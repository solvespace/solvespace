//-----------------------------------------------------------------------------
// Our entry point for exposing various internal mechanisms.
//
// Copyright 2017 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

int main(int argc, char **argv) {
    std::vector<std::string> args = InitPlatform(argc, argv);

    if(args.size() == 3 && args[1] == "expr") {
        std::string expr = args[2], err;
        Expr *e = Expr::Parse(expr.c_str(), &err);
        if(e == NULL) {
            fprintf(stderr, "cannot parse: %s\n", err.c_str());
        } else {
            fprintf(stderr, "%g\n", e->Eval());
        }
        FreeAllTemporary();
    } else {
        fprintf(stderr, "Usage: %s <command> <options>\n", args[0].c_str());
//-----------------------------------------------------------------------------> 80 col */
        fprintf(stderr, R"(
Commands:
    expr [expr]
        Evaluate an expression.
)");
    }

    return 0;
}
