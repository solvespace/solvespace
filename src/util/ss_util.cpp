//
// Created by benjamin on 9/24/18.
//
#include <stdarg.h>

#include "ss_util.h"

//int64_t SolveSpace::GetMilliseconds()
//{
//    auto timestamp = std::chrono::steady_clock::now().time_since_epoch();
//    return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count();
//}

void SolveSpace::AssertFailure(const char *file, unsigned line, const char *function,
                               const char *condition, const char *message) {
    std::string formattedMsg;
    formattedMsg += ssprintf("File %s, line %u, function %s:\n", file, line, function);
    formattedMsg += ssprintf("Assertion failed: %s.\n", condition);
    formattedMsg += ssprintf("Message: %s.\n", message);
    // TODO : get the real version of this in here
//    SolveSpace::Platform::FatalError(formattedMsg);
    fprintf(stderr, "%s", formattedMsg.c_str());
    abort();
}

std::string SolveSpace::ssprintf(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    int size = vsnprintf(NULL, 0, fmt, va);
    ssassert(size >= 0, "vsnprintf could not encode string");
    va_end(va);

    std::string result;
    result.resize(size + 1);

    va_start(va, fmt);
    vsnprintf(&result[0], size + 1, fmt, va);
    va_end(va);

    result.resize(size);
    return result;
}
