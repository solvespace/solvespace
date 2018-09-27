//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_REQUEST_H
#define SOLVESPACE_REQUEST_H

#include "handle.h"
#include "param.h"
#include "entity.h"

namespace SolveSpace {

// A user request for some primitive or derived operation; for example a
// line, or a step and repeat.
class Request {
public:

    int         tag;
    hRequest    h;

    // Types of requests
    enum class Type : uint32_t {
        WORKPLANE              = 100,
        DATUM_POINT            = 101,
        LINE_SEGMENT           = 200,
        CUBIC                  = 300,
        CUBIC_PERIODIC         = 301,
        CIRCLE                 = 400,
        ARC_OF_CIRCLE          = 500,
        TTF_TEXT               = 600,
        IMAGE                  = 700
    };

    Request::Type type;
    int         extraPoints;

    hEntity     workplane; // or Entity::FREE_IN_3D
    hGroup      group;
    hStyle      style;

    bool        construction;

    std::string str;
    std::string font;
    Platform::Path file;
    double      aspectRatio;

    static hParam AddParam(ParamList *param, hParam hp);
    void Generate(EntityList *entity, ParamList *param);

    std::string DescriptionString() const;
    int IndexOfPoint(hEntity he) const;

    void Clear() {}
};


}

#endif //SOLVESPACE_REQUEST_H
