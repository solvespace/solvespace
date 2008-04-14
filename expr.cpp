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

Expr *Expr::PartialWrt(hParam p) {
    Expr *da, *db;

    switch(op) {
        case PARAM_PTR: oops();
        case PARAM:     return FromConstant(p.v == x.parh.v ? 1 : 0);

        case CONSTANT:  return FromConstant(0);

        case PLUS:      return (a->PartialWrt(p))->Plus(b->PartialWrt(p));
        case MINUS:     return (a->PartialWrt(p))->Minus(b->PartialWrt(p));

        case TIMES:
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return (a->Times(db))->Plus(b->Times(da));

        case DIV:           
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return ((da->Times(b))->Minus(a->Times(db)))->Div(b->Square());

        case SQRT:
            return (FromConstant(0.5)->Div(a->Sqrt()))->Times(a->PartialWrt(p));

        case SQUARE:
            return (FromConstant(2.0)->Times(a))->Times(a->PartialWrt(p));

        case NEGATE:    return (a->PartialWrt(p))->Negate();
        case SIN:       return (a->Cos())->Times(a->PartialWrt(p));
        case COS:       return ((a->Sin())->Times(a->PartialWrt(p)))->Negate();

        default: oops();
    }
}

static char StringBuffer[4096];
void Expr::App(char *s, ...) {
    va_list f;
    va_start(f, s);
    vsprintf(StringBuffer+strlen(StringBuffer), s, f);
}
char *Expr::Print(void) {
    StringBuffer[0] = '\0';
    PrintW();
    return StringBuffer;
}

void Expr::PrintW(void) {
    switch(op) {
        case PARAM:     App("(param %08x)", x.parh.v); break;
        case PARAM_PTR: App("(paramp %08x)", x.parp->h.v); break;

        case CONSTANT:  App("%.3f", x.v);

        case PLUS:      App("(+ "); a->PrintW(); b->PrintW(); App(")"); break;
        case MINUS:     App("(- "); a->PrintW(); b->PrintW(); App(")"); break;
        case TIMES:     App("(* "); a->PrintW(); b->PrintW(); App(")"); break;
        case DIV:       App("(/ "); a->PrintW(); b->PrintW(); App(")"); break;

        case NEGATE:    App("(- "); a->PrintW(); App(")"); break;
        case SQRT:      App("(sqrt "); a->PrintW(); App(")"); break;
        case SQUARE:    App("(square "); a->PrintW(); App(")"); break;
        case SIN:       App("(sin "); a->PrintW(); App(")"); break;
        case COS:       App("(cos "); a->PrintW(); App(")"); break;

        default: oops();
    }
}

