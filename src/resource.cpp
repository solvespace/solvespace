//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

std::string LoadString(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    return std::string(static_cast<const char *>(data), size);
}

}
