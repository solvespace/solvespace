#include "diagnostics.h"

namespace SlvsStepCode {
namespace {
thread_local std::ostream *sink = nullptr;
class NullBuffer : public std::streambuf {
    int_type overflow(int_type c) override { return traits_type::not_eof(c); }
};
}

std::ostream &Log() {
    if(sink) return *sink;
    thread_local NullBuffer buffer;
    thread_local std::ostream discard(&buffer);
    return discard;
}

std::ostream *SetLog(std::ostream *stream) {
    auto previous = sink;
    sink = stream;
    return previous;
}
}
