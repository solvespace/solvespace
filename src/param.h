#ifndef SOLVESPACE_PARAM_H
#define SOLVESPACE_PARAM_H

#include <cstdint>
#include <unordered_set>

#include "dsc.h"

namespace SolveSpace {

class hRequest;

class hParam {
public:
    // bits 15: 0   -- param index
    //      31:16   -- request index
    uint32_t v;

    inline hRequest request() const;
};

template<>
struct IsHandleOracle<hParam> : std::true_type {};

class Param {
public:
    int         tag;
    hParam      h;

    double      val;
    bool        known;
    bool        free;

    // Used only in the solver
    Param       *substd;

    static const hParam NO_PARAM;

    void Clear() {}
};

using ParamList = IdList<Param,hParam>;

using ParamSet = std::unordered_set<hParam, HandleHasher<hParam>>;

} // namespace SolveSpace

#endif // !SOLVESPACE_PARAM_H
