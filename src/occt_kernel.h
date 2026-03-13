//-----------------------------------------------------------------------------
// OpenCASCADE Technology (OCCT) integration for SolveSpace.
//
// Provides:
//  - Conversion between SolveSpace's internal NURBS/BRep representation
//    (SShell / SSurface) and OCCT's TopoDS topology.
//  - STEP export via OCCT's STEPControl_Writer.
//  - IGES export via OCCT's IGESControl_Writer.
//  - Boolean solid-model operations via OCCT's BRepAlgoAPI family.
//
// All declarations in this file are compiled only when HAVE_OPENCASCADE is
// defined (i.e. -DENABLE_OPENCASCADE=ON at CMake configure time).
//
// Copyright 2024 The SolveSpace contributors.
//-----------------------------------------------------------------------------

#pragma once

#ifdef HAVE_OPENCASCADE

#include "platform/platform.h"

namespace SolveSpace {

class SShell;

//-----------------------------------------------------------------------------
// Export the current shell to a STEP file using OCCT's STEPControl_Writer.
// Returns true on success.
//-----------------------------------------------------------------------------
bool OCCTExportStepFile(const Platform::Path &filename, SShell *shell);

//-----------------------------------------------------------------------------
// Export the current shell to an IGES file using OCCT's IGESControl_Writer.
// Returns true on success.
//-----------------------------------------------------------------------------
bool OCCTExportIgesFile(const Platform::Path &filename, SShell *shell);

//-----------------------------------------------------------------------------
// Perform a Boolean union of shells `a` and `b` using OCCT and store the
// result in `result`.  Returns true on success; on failure the caller should
// fall back to the built-in kernel.
//-----------------------------------------------------------------------------
bool OCCTBooleanUnion(SShell *result, SShell *a, SShell *b);

//-----------------------------------------------------------------------------
// Perform a Boolean difference (a minus b) using OCCT.
// Returns true on success.
//-----------------------------------------------------------------------------
bool OCCTBooleanDifference(SShell *result, SShell *a, SShell *b);

//-----------------------------------------------------------------------------
// Perform a Boolean intersection of shells `a` and `b` using OCCT.
// Returns true on success.
//-----------------------------------------------------------------------------
bool OCCTBooleanIntersection(SShell *result, SShell *a, SShell *b);

} // namespace SolveSpace

#endif // HAVE_OPENCASCADE
