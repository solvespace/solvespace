//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_H
#define SOLVESPACE_H

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ss_util.h"
#include "platform.h"
#include "list.h"
#include "solvespaceui.h"
#include "globals.h"
#include "dsc.h"
#include "sketch.h"
#include "resource.h"
#include "polygon.h"
#include "srf/surface.h"
#include "filewriters.h"
#include "expr.h"
#include "render/render.h"
#include "platform/gui.h"
#include "ui.h"
#include "ttf.h"
#include "util.h"


namespace SolveSpace {



//================
// From the platform-specific code.




// End of platform-specific functions
//================

}

#include "namespace.h"

#endif
