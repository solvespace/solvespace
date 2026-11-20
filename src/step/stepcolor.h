// Source presentation colours only; no geometry or reference identities change.
#ifndef SOLVESPACE_STEP_COLOR_H
#define SOLVESPACE_STEP_COLOR_H

namespace SolveSpace {
namespace Step {

class Document;
struct GeometryResult;

// Resolve non-contextual surface styles after geometric conversion. Unsupported
// or conflicting appearance falls back to neutral grey, never an omitted solid.
void ApplySurfaceColors(const Document &document, GeometryResult *geometry);

} // namespace Step
} // namespace SolveSpace

#endif
