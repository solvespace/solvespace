//-----------------------------------------------------------------------------
// The symbolic algebra system used to write our constraint equations;
// routines to build expressions in software or from a user-provided string,
// and to compute the partial derivatives that we'll use when write our
// Jacobian matrix.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

ExprVector ExprVector::From(Expr *x, Expr *y, Expr *z) {
    ExprVector r = { x, y, z};
    return r;
}

ExprVector ExprVector::From(Vector vn) {
    ExprVector ve;
    ve.x = Expr::From(vn.x);
    ve.y = Expr::From(vn.y);
    ve.z = Expr::From(vn.z);
    return ve;
}

ExprVector ExprVector::From(hParam x, hParam y, hParam z) {
    ExprVector ve;
    ve.x = Expr::From(x);
    ve.y = Expr::From(y);
    ve.z = Expr::From(z);
    return ve;
}

ExprVector ExprVector::From(double x, double y, double z) {
    ExprVector ve;
    ve.x = Expr::From(x);
    ve.y = Expr::From(y);
    ve.z = Expr::From(z);
    return ve;
}

ExprVector ExprVector::Minus(ExprVector b) const {
    ExprVector r;
    r.x = x->Minus(b.x);
    r.y = y->Minus(b.y);
    r.z = z->Minus(b.z);
    return r;
}

ExprVector ExprVector::Plus(ExprVector b) const {
    ExprVector r;
    r.x = x->Plus(b.x);
    r.y = y->Plus(b.y);
    r.z = z->Plus(b.z);
    return r;
}

Expr *ExprVector::Dot(ExprVector b) const {
    Expr *r;
    r =         x->Times(b.x);
    r = r->Plus(y->Times(b.y));
    r = r->Plus(z->Times(b.z));
    return r;
}

ExprVector ExprVector::Cross(ExprVector b) const {
    ExprVector r;
    r.x = (y->Times(b.z))->Minus(z->Times(b.y));
    r.y = (z->Times(b.x))->Minus(x->Times(b.z));
    r.z = (x->Times(b.y))->Minus(y->Times(b.x));
    return r;
}

ExprVector ExprVector::ScaledBy(Expr *s) const {
    ExprVector r;
    r.x = x->Times(s);
    r.y = y->Times(s);
    r.z = z->Times(s);
    return r;
}

ExprVector ExprVector::WithMagnitude(Expr *s) const {
    Expr *m = Magnitude();
    return ScaledBy(s->Div(m));
}

Expr *ExprVector::Magnitude() const {
    Expr *r;
    r =         x->Square();
    r = r->Plus(y->Square());
    r = r->Plus(z->Square());
    return r->Sqrt();
}

Vector ExprVector::Eval() const {
    Vector r;
    r.x = x->Eval();
    r.y = y->Eval();
    r.z = z->Eval();
    return r;
}

ExprQuaternion ExprQuaternion::From(hParam w, hParam vx, hParam vy, hParam vz) {
    ExprQuaternion q;
    q.w  = Expr::From(w);
    q.vx = Expr::From(vx);
    q.vy = Expr::From(vy);
    q.vz = Expr::From(vz);
    return q;
}

ExprQuaternion ExprQuaternion::From(Expr *w, Expr *vx, Expr *vy, Expr *vz)
{
    ExprQuaternion q;
    q.w = w;
    q.vx = vx;
    q.vy = vy;
    q.vz = vz;
    return q;
}

ExprQuaternion ExprQuaternion::From(Quaternion qn) {
    ExprQuaternion qe;
    qe.w = Expr::From(qn.w);
    qe.vx = Expr::From(qn.vx);
    qe.vy = Expr::From(qn.vy);
    qe.vz = Expr::From(qn.vz);
    return qe;
}

ExprVector ExprQuaternion::RotationU() const {
    ExprVector u;
    Expr *two = Expr::From(2);

    u.x = w->Square();
    u.x = (u.x)->Plus(vx->Square());
    u.x = (u.x)->Minus(vy->Square());
    u.x = (u.x)->Minus(vz->Square());

    u.y = two->Times(w->Times(vz));
    u.y = (u.y)->Plus(two->Times(vx->Times(vy)));

    u.z = two->Times(vx->Times(vz));
    u.z = (u.z)->Minus(two->Times(w->Times(vy)));

    return u;
}

ExprVector ExprQuaternion::RotationV() const {
    ExprVector v;
    Expr *two = Expr::From(2);

    v.x = two->Times(vx->Times(vy));
    v.x = (v.x)->Minus(two->Times(w->Times(vz)));

    v.y = w->Square();
    v.y = (v.y)->Minus(vx->Square());
    v.y = (v.y)->Plus(vy->Square());
    v.y = (v.y)->Minus(vz->Square());

    v.z = two->Times(w->Times(vx));
    v.z = (v.z)->Plus(two->Times(vy->Times(vz)));

    return v;
}

ExprVector ExprQuaternion::RotationN() const {
    ExprVector n;
    Expr *two = Expr::From(2);

    n.x =              two->Times( w->Times(vy));
    n.x = (n.x)->Plus (two->Times(vx->Times(vz)));

    n.y =              two->Times(vy->Times(vz));
    n.y = (n.y)->Minus(two->Times( w->Times(vx)));

    n.z =               w->Square();
    n.z = (n.z)->Minus(vx->Square());
    n.z = (n.z)->Minus(vy->Square());
    n.z = (n.z)->Plus (vz->Square());

    return n;
}

ExprVector ExprQuaternion::Rotate(ExprVector p) const {
    // Express the point in the new basis
    return (RotationU().ScaledBy(p.x)).Plus(
            RotationV().ScaledBy(p.y)).Plus(
            RotationN().ScaledBy(p.z));
}

ExprQuaternion ExprQuaternion::Times(ExprQuaternion b) const {
    Expr *sa = w, *sb = b.w;
    ExprVector va = { vx, vy, vz };
    ExprVector vb = { b.vx, b.vy, b.vz };

    ExprQuaternion r;
    r.w = (sa->Times(sb))->Minus(va.Dot(vb));
    ExprVector vr = vb.ScaledBy(sa).Plus(
                    va.ScaledBy(sb).Plus(
                    va.Cross(vb)));
    r.vx = vr.x;
    r.vy = vr.y;
    r.vz = vr.z;
    return r;
}

Expr *ExprQuaternion::Magnitude() const {
    return ((w ->Square())->Plus(
            (vx->Square())->Plus(
            (vy->Square())->Plus(
            (vz->Square())))))->Sqrt();
}


Expr *Expr::From(hParam p) {
    Expr *r = AllocExpr();
    r->op = Op::PARAM;
    r->parh = p;
    return r;
}

Expr *Expr::From(double v) {
    // Statically allocate common constants.
    // Note: this is only valid because AllocExpr() uses AllocTemporary(),
    // and Expr* is never explicitly freed.

    if(v == 0.0) {
        static Expr zero(0.0);
        return &zero;
    }

    if(v == 1.0) {
        static Expr one(1.0);
        return &one;
    }

    if(v == -1.0) {
        static Expr mone(-1.0);
        return &mone;
    }

    if(v == 0.5) {
        static Expr half(0.5);
        return &half;
    }

    if(v == -0.5) {
        static Expr mhalf(-0.5);
        return &mhalf;
    }

    Expr *r = AllocExpr();
    r->op = Op::CONSTANT;
    r->v = v;
    return r;
}

Expr *Expr::AnyOp(Op newOp, Expr *b) {
    Expr *r = AllocExpr();
    r->op = newOp;
    r->a = this;
    r->b = b;
    return r;
}

int Expr::Children() const {
    switch(op) {
        case Op::PARAM:
        case Op::PARAM_PTR:
        case Op::CONSTANT:
            return 0;

        case Op::PLUS:
        case Op::MINUS:
        case Op::TIMES:
        case Op::DIV:
            return 2;

        case Op::NEGATE:
        case Op::SQRT:
        case Op::SQUARE:
        case Op::SIN:
        case Op::COS:
        case Op::ASIN:
        case Op::ACOS:
            return 1;

        case Op::PAREN:
        case Op::BINARY_OP:
        case Op::UNARY_OP:
        case Op::ALL_RESOLVED:
            break;
    }
    ssassert(false, "Unexpected operation");
}

int Expr::Nodes() const {
    switch(Children()) {
        case 0: return 1;
        case 1: return 1 + a->Nodes();
        case 2: return 1 + a->Nodes() + b->Nodes();
        default: ssassert(false, "Unexpected children count");
    }
}

Expr *Expr::DeepCopy() const {
    Expr *n = AllocExpr();
    *n = *this;
    int c = n->Children();
    if(c > 0) n->a = a->DeepCopy();
    if(c > 1) n->b = b->DeepCopy();
    return n;
}

Expr *Expr::DeepCopyWithParamsAsPointers(IdList<Param,hParam> *firstTry,
    IdList<Param,hParam> *thenTry) const
{
    Expr *n = AllocExpr();
    if(op == Op::PARAM) {
        // A param that is referenced by its hParam gets rewritten to go
        // straight in to the parameter table with a pointer, or simply
        // into a constant if it's already known.
        Param *p = firstTry->FindByIdNoOops(parh);
        if(!p) p = thenTry->FindById(parh);
        if(p->known) {
            n->op = Op::CONSTANT;
            n->v = p->val;
        } else {
            n->op = Op::PARAM_PTR;
            n->parp = p;
        }
        return n;
    }

    *n = *this;
    int c = n->Children();
    if(c > 0) n->a = a->DeepCopyWithParamsAsPointers(firstTry, thenTry);
    if(c > 1) n->b = b->DeepCopyWithParamsAsPointers(firstTry, thenTry);
    return n;
}

double Expr::Eval() const {
    switch(op) {
        case Op::PARAM:         return SK.GetParam(parh)->val;
        case Op::PARAM_PTR:     return parp->val;

        case Op::CONSTANT:      return v;

        case Op::PLUS:          return a->Eval() + b->Eval();
        case Op::MINUS:         return a->Eval() - b->Eval();
        case Op::TIMES:         return a->Eval() * b->Eval();
        case Op::DIV:           return a->Eval() / b->Eval();

        case Op::NEGATE:        return -(a->Eval());
        case Op::SQRT:          return sqrt(a->Eval());
        case Op::SQUARE:        { double r = a->Eval(); return r*r; }
        case Op::SIN:           return sin(a->Eval());
        case Op::COS:           return cos(a->Eval());
        case Op::ACOS:          return acos(a->Eval());
        case Op::ASIN:          return asin(a->Eval());

        case Op::PAREN:
        case Op::BINARY_OP:
        case Op::UNARY_OP:
        case Op::ALL_RESOLVED:
            break;
    }
    ssassert(false, "Unexpected operation");
}

Expr *Expr::PartialWrt(hParam p) const {
    Expr *da, *db;

    switch(op) {
        case Op::PARAM_PTR: return From(p.v == parp->h.v ? 1 : 0);
        case Op::PARAM:     return From(p.v == parh.v ? 1 : 0);

        case Op::CONSTANT:  return From(0.0);

        case Op::PLUS:      return (a->PartialWrt(p))->Plus(b->PartialWrt(p));
        case Op::MINUS:     return (a->PartialWrt(p))->Minus(b->PartialWrt(p));

        case Op::TIMES:
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return (a->Times(db))->Plus(b->Times(da));

        case Op::DIV:
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return ((da->Times(b))->Minus(a->Times(db)))->Div(b->Square());

        case Op::SQRT:
            return (From(0.5)->Div(a->Sqrt()))->Times(a->PartialWrt(p));

        case Op::SQUARE:
            return (From(2.0)->Times(a))->Times(a->PartialWrt(p));

        case Op::NEGATE:    return (a->PartialWrt(p))->Negate();
        case Op::SIN:       return (a->Cos())->Times(a->PartialWrt(p));
        case Op::COS:       return ((a->Sin())->Times(a->PartialWrt(p)))->Negate();

        case Op::ASIN:
            return (From(1)->Div((From(1)->Minus(a->Square()))->Sqrt()))
                        ->Times(a->PartialWrt(p));
        case Op::ACOS:
            return (From(-1)->Div((From(1)->Minus(a->Square()))->Sqrt()))
                        ->Times(a->PartialWrt(p));

        case Op::PAREN:
        case Op::BINARY_OP:
        case Op::UNARY_OP:
        case Op::ALL_RESOLVED:
            break;
    }
    ssassert(false, "Unexpected operation");
}

uint64_t Expr::ParamsUsed() const {
    uint64_t r = 0;
    if(op == Op::PARAM)     r |= ((uint64_t)1 << (parh.v % 61));
    if(op == Op::PARAM_PTR) r |= ((uint64_t)1 << (parp->h.v % 61));

    int c = Children();
    if(c >= 1)          r |= a->ParamsUsed();
    if(c >= 2)          r |= b->ParamsUsed();
    return r;
}

bool Expr::DependsOn(hParam p) const {
    if(op == Op::PARAM)     return (parh.v    == p.v);
    if(op == Op::PARAM_PTR) return (parp->h.v == p.v);

    int c = Children();
    if(c == 1)          return a->DependsOn(p);
    if(c == 2)          return a->DependsOn(p) || b->DependsOn(p);
    return false;
}

bool Expr::Tol(double a, double b) {
    return fabs(a - b) < 0.001;
}
Expr *Expr::FoldConstants() {
    Expr *n = AllocExpr();
    *n = *this;

    int c = Children();
    if(c >= 1) n->a = a->FoldConstants();
    if(c >= 2) n->b = b->FoldConstants();

    switch(op) {
        case Op::PARAM_PTR:
        case Op::PARAM:
        case Op::CONSTANT:
            break;

        case Op::MINUS:
        case Op::TIMES:
        case Op::DIV:
        case Op::PLUS:
            // If both ops are known, then we can evaluate immediately
            if(n->a->op == Op::CONSTANT && n->b->op == Op::CONSTANT) {
                double nv = n->Eval();
                n->op = Op::CONSTANT;
                n->v = nv;
                break;
            }
            // x + 0 = 0 + x = x
            if(op == Op::PLUS && n->b->op == Op::CONSTANT && Tol(n->b->v, 0)) {
                *n = *(n->a); break;
            }
            if(op == Op::PLUS && n->a->op == Op::CONSTANT && Tol(n->a->v, 0)) {
                *n = *(n->b); break;
            }
            // 1*x = x*1 = x
            if(op == Op::TIMES && n->b->op == Op::CONSTANT && Tol(n->b->v, 1)) {
                *n = *(n->a); break;
            }
            if(op == Op::TIMES && n->a->op == Op::CONSTANT && Tol(n->a->v, 1)) {
                *n = *(n->b); break;
            }
            // 0*x = x*0 = 0
            if(op == Op::TIMES && n->b->op == Op::CONSTANT && Tol(n->b->v, 0)) {
                n->op = Op::CONSTANT; n->v = 0; break;
            }
            if(op == Op::TIMES && n->a->op == Op::CONSTANT && Tol(n->a->v, 0)) {
                n->op = Op::CONSTANT; n->v = 0; break;
            }

            break;

        case Op::SQRT:
        case Op::SQUARE:
        case Op::NEGATE:
        case Op::SIN:
        case Op::COS:
        case Op::ASIN:
        case Op::ACOS:
            if(n->a->op == Op::CONSTANT) {
                double nv = n->Eval();
                n->op = Op::CONSTANT;
                n->v = nv;
            }
            break;

        case Op::PAREN:
        case Op::BINARY_OP:
        case Op::UNARY_OP:
        case Op::ALL_RESOLVED:
            ssassert(false, "Unexpected operation");
    }
    return n;
}

void Expr::Substitute(hParam oldh, hParam newh) {
    ssassert(op != Op::PARAM_PTR, "Expected an expression that refer to params via handles");

    if(op == Op::PARAM && parh.v == oldh.v) {
        parh = newh;
    }
    int c = Children();
    if(c >= 1) a->Substitute(oldh, newh);
    if(c >= 2) b->Substitute(oldh, newh);
}

//-----------------------------------------------------------------------------
// If the expression references only one parameter that appears in pl, then
// return that parameter. If no param is referenced, then return NO_PARAMS.
// If multiple params are referenced, then return MULTIPLE_PARAMS.
//-----------------------------------------------------------------------------
const hParam Expr::NO_PARAMS       = { 0 };
const hParam Expr::MULTIPLE_PARAMS = { 1 };
hParam Expr::ReferencedParams(ParamList *pl) const {
    if(op == Op::PARAM) {
        if(pl->FindByIdNoOops(parh)) {
            return parh;
        } else {
            return NO_PARAMS;
        }
    }
    ssassert(op != Op::PARAM_PTR, "Expected an expression that refer to params via handles");

    int c = Children();
    if(c == 0) {
        return NO_PARAMS;
    } else if(c == 1) {
        return a->ReferencedParams(pl);
    } else if(c == 2) {
        hParam pa, pb;
        pa = a->ReferencedParams(pl);
        pb = b->ReferencedParams(pl);
        if(pa.v == NO_PARAMS.v) {
            return pb;
        } else if(pb.v == NO_PARAMS.v) {
            return pa;
        } else if(pa.v == pb.v) {
            return pa; // either, doesn't matter
        } else {
            return MULTIPLE_PARAMS;
        }
    } else ssassert(false, "Unexpected children count");
}


//-----------------------------------------------------------------------------
// Routines to pretty-print an expression. Mostly for debugging.
//-----------------------------------------------------------------------------

std::string Expr::Print() const {
    char c;
    switch(op) {
        case Op::PARAM:     return ssprintf("param(%08x)", parh.v);
        case Op::PARAM_PTR: return ssprintf("param(p%08x)", parp->h.v);

        case Op::CONSTANT:  return ssprintf("%.3f", v);

        case Op::PLUS:      c = '+'; goto p;
        case Op::MINUS:     c = '-'; goto p;
        case Op::TIMES:     c = '*'; goto p;
        case Op::DIV:       c = '/'; goto p;
p:
            return "(" + a->Print() + " " + c + " " + b->Print() + ")";
            break;

        case Op::NEGATE:    return "(- " + a->Print() + ")";
        case Op::SQRT:      return "(sqrt " + a->Print() + ")";
        case Op::SQUARE:    return "(square " + a->Print() + ")";
        case Op::SIN:       return "(sin " + a->Print() + ")";
        case Op::COS:       return "(cos " + a->Print() + ")";
        case Op::ASIN:      return "(asin " + a->Print() + ")";
        case Op::ACOS:      return "(acos " + a->Print() + ")";

        case Op::PAREN:
        case Op::BINARY_OP:
        case Op::UNARY_OP:
        case Op::ALL_RESOLVED:
            break;
    }
    ssassert(false, "Unexpected operation");
}


//-----------------------------------------------------------------------------
// A parser; convert a string to an expression. Infix notation, with the
// usual shift/reduce approach. I had great hopes for user-entered eq
// constraints, but those don't seem very useful, so right now this is just
// to provide calculator type functionality wherever numbers are entered.
//-----------------------------------------------------------------------------

#define MAX_UNPARSED 1024
static Expr *Unparsed[MAX_UNPARSED];
static int UnparsedCnt, UnparsedP;

static Expr *Operands[MAX_UNPARSED];
static int OperandsP;

static Expr *Operators[MAX_UNPARSED];
static int OperatorsP;

void Expr::PushOperator(Expr *e) {
    if(OperatorsP >= MAX_UNPARSED) throw "operator stack full!";
    Operators[OperatorsP++] = e;
}
Expr *Expr::TopOperator() {
    if(OperatorsP <= 0) throw "operator stack empty (get top)";
    return Operators[OperatorsP-1];
}
Expr *Expr::PopOperator() {
    if(OperatorsP <= 0) throw "operator stack empty (pop)";
    return Operators[--OperatorsP];
}
void Expr::PushOperand(Expr *e) {
    if(OperandsP >= MAX_UNPARSED) throw "operand stack full";
    Operands[OperandsP++] = e;
}
Expr *Expr::PopOperand() {
    if(OperandsP <= 0) throw "operand stack empty";
    return Operands[--OperandsP];
}
Expr *Expr::Next() {
    if(UnparsedP >= UnparsedCnt) return NULL;
    return Unparsed[UnparsedP];
}
void Expr::Consume() {
    if(UnparsedP >= UnparsedCnt) throw "no token to consume";
    UnparsedP++;
}

int Expr::Precedence(Expr *e) {
    if(e->op == Op::ALL_RESOLVED) return -1; // never want to reduce this marker
    ssassert(e->op == Op::BINARY_OP || e->op == Op::UNARY_OP, "Unexpected operation");

    switch(e->c) {
        case 'q':
        case 's':
        case 'c':
        case 'n':   return 30;

        case '*':
        case '/':   return 20;

        case '+':
        case '-':   return 10;

        default: ssassert(false, "Unexpected operator");
    }
}

void Expr::Reduce() {
    Expr *a, *b;

    Expr *op = PopOperator();
    Expr *n;
    Op o;
    switch(op->c) {
        case '+': o = Op::PLUS;  goto c;
        case '-': o = Op::MINUS; goto c;
        case '*': o = Op::TIMES; goto c;
        case '/': o = Op::DIV;   goto c;
c:
            b = PopOperand();
            a = PopOperand();
            n = a->AnyOp(o, b);
            break;

        case 'n': n = PopOperand()->Negate(); break;
        case 'q': n = PopOperand()->Sqrt(); break;
        case 's': n = (PopOperand()->Times(Expr::From(PI/180)))->Sin(); break;
        case 'c': n = (PopOperand()->Times(Expr::From(PI/180)))->Cos(); break;

        default: ssassert(false, "Unexpected operator");
    }
    PushOperand(n);
}

void Expr::ReduceAndPush(Expr *n) {
    while(Precedence(n) <= Precedence(TopOperator())) {
        Reduce();
    }
    PushOperator(n);
}

void Expr::Parse() {
    Expr *e = AllocExpr();
    e->op = Op::ALL_RESOLVED;
    PushOperator(e);

    for(;;) {
        Expr *n = Next();
        if(!n) throw "end of expression unexpected";

        if(n->op == Op::CONSTANT) {
            PushOperand(n);
            Consume();
        } else if(n->op == Op::PAREN && n->c == '(') {
            Consume();
            Parse();
            n = Next();
            if(n->op != Op::PAREN || n->c != ')') throw "expected: )";
            Consume();
        } else if(n->op == Op::UNARY_OP) {
            PushOperator(n);
            Consume();
            continue;
        } else if(n->op == Op::BINARY_OP && n->c == '-') {
            // The minus sign is special, because it might be binary or
            // unary, depending on context.
            n->op = Op::UNARY_OP;
            n->c = 'n';
            PushOperator(n);
            Consume();
            continue;
        } else {
            throw "expected expression";
        }

        n = Next();
        if(n && n->op == Op::BINARY_OP) {
            ReduceAndPush(n);
            Consume();
        } else {
            break;
        }
    }

    while(TopOperator()->op != Op::ALL_RESOLVED) {
        Reduce();
    }
    PopOperator(); // discard the ALL_RESOLVED marker
}

void Expr::Lex(const char *in) {
    while(*in) {
        if(UnparsedCnt >= MAX_UNPARSED) throw "too long";

        char c = *in;
        if(isdigit(c) || c == '.') {
            // A number literal
            char number[70];
            int len = 0;
            while((isdigit(*in) || *in == '.') && len < 30) {
                number[len++] = *in;
                in++;
            }
            number[len++] = '\0';
            Expr *e = AllocExpr();
            e->op = Op::CONSTANT;
            e->v = atof(number);
            Unparsed[UnparsedCnt++] = e;
        } else if(isalpha(c) || c == '_') {
            char name[70];
            int len = 0;
            while(isforname(*in) && len < 30) {
                name[len++] = *in;
                in++;
            }
            name[len++] = '\0';

            Expr *e = AllocExpr();
            if(strcmp(name, "sqrt")==0) {
                e->op = Op::UNARY_OP;
                e->c = 'q';
            } else if(strcmp(name, "cos")==0) {
                e->op = Op::UNARY_OP;
                e->c = 'c';
            } else if(strcmp(name, "sin")==0) {
                e->op = Op::UNARY_OP;
                e->c = 's';
            } else if(strcmp(name, "pi")==0) {
                e->op = Op::CONSTANT;
                e->v = PI;
            } else {
                throw "unknown name";
            }
            Unparsed[UnparsedCnt++] = e;
        } else if(strchr("+-*/()", c)) {
            Expr *e = AllocExpr();
            e->op = (c == '(' || c == ')') ? Op::PAREN : Op::BINARY_OP;
            e->c = c;
            Unparsed[UnparsedCnt++] = e;
            in++;
        } else if(isspace(c)) {
            // Ignore whitespace
            in++;
        } else {
            // This is a lex error.
            throw "unexpected characters";
        }
    }
}

Expr *Expr::From(const char *in, bool popUpError) {
    UnparsedCnt = 0;
    UnparsedP = 0;
    OperandsP = 0;
    OperatorsP = 0;

    Expr *r;
    try {
        Lex(in);
        Parse();
        r = PopOperand();
    } catch (const char *e) {
        dbp("exception: parse/lex error: %s", e);
        if(popUpError) {
            Error("Not a valid number or expression: '%s'", in);
        }
        return NULL;
    }
    return r;
}

