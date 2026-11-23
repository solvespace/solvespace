// Private to the pinned STEPcode build and the reader adapter. Never redirect
// the application's standard streams to collect third-party diagnostics.
#ifndef SLVS_STEPCODE_DIAGNOSTICS_H
#define SLVS_STEPCODE_DIAGNOSTICS_H

#include <ostream>

namespace SlvsStepCode {
std::ostream &Log();
std::ostream *SetLog(std::ostream *stream);
}

#endif
