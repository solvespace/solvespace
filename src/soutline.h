//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SOUTLINE_H
#define SOLVESPACE_SOUTLINE_H

#include "dsc.h"
#include "sedge.h"

namespace SolveSpace{

class SOutline {
public:
    int    tag;
    Vector a, b, nl, nr;

    bool IsVisible(Vector projDir) const;
};

class SOutlineList {
public:
    List<SOutline> l;

    void Clear();
    void AddEdge(Vector a, Vector b, Vector nl, Vector nr, int tag = 0);
    void ListTaggedInto(SEdgeList *el, int auxA = 0, int auxB = 0);

    void MakeFromCopyOf(SOutlineList *ol);
};

}

#endif //SOLVESPACE_SOUTLINE_H
