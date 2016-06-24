//-----------------------------------------------------------------------------
// A helper class to assemble scattered edges into contiguous polylines,
// as nicely as possible.
//
// Copyright 2016 M-Labs Ltd
//-----------------------------------------------------------------------------
#include "solvespace.h"

bool PolylineBuilder::Vertex::GetNext(uint32_t kind, Vertex **next, Edge **nextEdge) {
    auto it = std::find_if(edges.begin(), edges.end(), [&](const Edge *e) {
        return e->tag == 0 && e->kind == kind;
    });

    if(it != edges.end()) {
        (*it)->tag = 1;
        *next = (*it)->GetOtherVertex(this);
        *nextEdge = *it;
        return true;
    }

    return false;
}

bool PolylineBuilder::Vertex::GetNext(uint32_t kind, Vector plane, double dist,
                                      Vertex **next, Edge **nextEdge) {
    Edge *best = NULL;
    double minD = VERY_POSITIVE;
    for(Edge *e : edges) {
        if(e->tag != 0) continue;
        if(e->kind != kind) continue;

        // We choose the best next edge with minimal distance from the current plane
        Vector nextPos = e->GetOtherVertex(this)->pos;
        double curD = fabs(plane.Dot(nextPos) - dist);
        if(best != NULL && curD > minD) continue;
        best = e;
        minD = curD;
    }

    if(best != NULL) {
        best->tag = 1;
        *next = best->GetOtherVertex(this);
        *nextEdge = best;
        return true;
    }

    return false;
}


size_t PolylineBuilder::Vertex::CountEdgesWithTagAndKind(int tag, uint32_t kind) const {
    return std::count_if(edges.begin(), edges.end(), [&](const Edge *e) {
        return e->tag == tag && e->kind == kind;
    });
}

PolylineBuilder::Vertex *PolylineBuilder::Edge::GetOtherVertex(PolylineBuilder::Vertex *v) const {
    if(a == v) return b;
    if(b == v) return a;
    return NULL;
}

size_t PolylineBuilder::VertexPairHash::operator()(const std::pair<Vertex *, Vertex *> &v) const {
    return ((uintptr_t)v.first / sizeof(Vertex)) ^
           ((uintptr_t)v.second / sizeof(Vertex));
}

bool PolylineBuilder::Edge::GetStartAndNext(PolylineBuilder::Vertex **start,
                                            PolylineBuilder::Vertex **next, bool loop) const {
    size_t numA = a->CountEdgesWithTagAndKind(0, kind);
    size_t numB = b->CountEdgesWithTagAndKind(0, kind);

    if((numA == 1 && numB > 1) || (loop && numA > 1 && numB > 1)) {
        *start = a;
        *next = b;
        return true;
    }

    if(numA > 1 && numB == 1) {
        *start = b;
        *next = a;
        return true;
    }

    return false;
}

PolylineBuilder::~PolylineBuilder() {
    Clear();
}

void PolylineBuilder::Clear() {
    for(Edge *e : edges) {
        delete e;
    }
    edges.clear();

    for(auto &v : vertices) {
        delete v.second;
    }
    vertices.clear();
}

PolylineBuilder::Vertex *PolylineBuilder::AddVertex(const Vector &pos) {
    auto it = vertices.find(pos);
    if(it != vertices.end()) {
        return it->second;
    }

    Vertex *result = new Vertex;
    result->pos = pos;
    vertices.emplace(pos, result);

    return result;
}

PolylineBuilder::Edge *PolylineBuilder::AddEdge(const Vector &p0, const Vector &p1,
                                                uint32_t kind, uintptr_t data) {
    Vertex *v0 = AddVertex(p0);
    Vertex *v1 = AddVertex(p1);
    if(v0 == v1) return NULL;

    auto it = edgeMap.find(std::make_pair(v0, v1));
    if(it != edgeMap.end()) {
        return it->second;
    }

    PolylineBuilder::Edge *edge = new PolylineBuilder::Edge {};
    edge->a = v0;
    edge->b = v1;
    edge->kind = kind;
    edge->tag = 0;
    edge->data = data;
    edges.push_back(edge);
    edgeMap.emplace(std::make_pair(v0, v1), edge);

    v0->edges.push_back(edge);
    v1->edges.push_back(edge);

    return edge;
}

void PolylineBuilder::Generate(
        std::function<void(Vertex *start, Vertex *next, Edge *edge)> startFunc,
        std::function<void(Vertex *next, Edge *edge)> nextFunc,
        std::function<void(Edge *alone)> aloneFunc,
        std::function<void()> endFunc) {
    bool found;
    bool loop = false;
    do {
        found = false;
        for(PolylineBuilder::Edge *e : edges) {
            if(e->tag != 0) continue;

            Vertex *start;
            Vertex *next;
            if(!e->GetStartAndNext(&start, &next, loop)) continue;

            Vector startPos = start->pos;
            Vector nextPos = next->pos;
            found = true;
            e->tag = 1;

            startFunc(start, next, e);

            Edge *nextEdge;
            if(next->GetNext(e->kind, &next, &nextEdge)) {
                Vector plane = nextPos.Minus(startPos).Cross(next->pos.Minus(startPos));
                double dist = plane.Dot(startPos);
                nextFunc(next, nextEdge);
                while(next->GetNext(e->kind, plane, dist, &next, &nextEdge)) {
                    nextFunc(next, nextEdge);
                }
            }

            endFunc();
        }

        if(!found && !loop) {
            loop  = true;
            found = true;
        }
    } while(found);

    for(PolylineBuilder::Edge *e : edges) {
        if(e->tag != 0) continue;
        aloneFunc(e);
    }
}
