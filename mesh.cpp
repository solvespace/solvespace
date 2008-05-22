#include "solvespace.h"

void SMesh::Clear(void) {
    l.Clear();
}

void SMesh::AddTriangle(Vector a, Vector b, Vector c) {
    STriangle t; ZERO(&t);
    t.a = a;
    t.b = b;
    t.c = c;
    l.Add(&t);
}

