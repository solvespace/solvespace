#include "solvespace.h"

void SShell::MakeFromUnionOf(SShell *a, SShell *b) {
    MakeFromCopyOf(b);
}

void SShell::MakeFromDifferenceOf(SShell *a, SShell *b) {
    MakeFromCopyOf(b);
}

