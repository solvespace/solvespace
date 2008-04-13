#include "solvespace.h"

Expr *Expr::FromParam(hParam p) {
    Expr *r = AllocExpr();
    r->op = PARAM;
    r->x.parh = p;
    return r;
}

Expr *Expr::FromConstant(double v) {
    Expr *r = AllocExpr();
    r->op = CONSTANT;
    r->x.v = v;
    return r;
}

Expr *Expr::AnyOp(int newOp, Expr *b) {
    Expr *r = AllocExpr();
    r->op = newOp;
    r->a = this;
    r->b = b;
    return r;
}

double Expr::Eval(void) {
    switch(op) {
        case PARAM:         return SS.GetParam(x.parh)->val;
        case PARAM_PTR:     return (x.parp)->val;

        case CONSTANT:      return x.v;

        case PLUS:          return a->Eval() + b->Eval();
        case MINUS:         return a->Eval() - b->Eval();
        case TIMES:         return a->Eval() * b->Eval();
        case DIV:           return a->Eval() / b->Eval();

        case NEGATE:        return -(a->Eval());
        case SQRT:          return sqrt(a->Eval());
        case SQUARE:        { double r = a->Eval(); return r*r; }
        case SIN:           return sin(a->Eval());
        case COS:           return cos(a->Eval());

        default: oops();
    }
}

