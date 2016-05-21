//-----------------------------------------------------------------------------
// An expression in our symbolic algebra system, used to write, linearize,
// and solve our constraint equations.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __EXPR_H
#define __EXPR_H

class Expr;

class Expr {
public:

    enum {
        // A parameter, by the hParam handle
        PARAM          =  0,
        // A parameter, by a pointer straight in to the param table (faster,
        // if we know that the param table won't move around)
        PARAM_PTR      =  1,

        CONSTANT       = 20,

        PLUS           = 100,
        MINUS          = 101,
        TIMES          = 102,
        DIV            = 103,
        NEGATE         = 104,
        SQRT           = 105,
        SQUARE         = 106,
        SIN            = 107,
        COS            = 108,
        ASIN           = 109,
        ACOS           = 110,

        // Special helpers for when we're parsing an expression from text.
        // Initially, literals (like a constant number) appear in the same
        // format as they will in the finished expression, but the operators
        // are different until the parser fixes things up (and builds the
        // tree from the flat list that the lexer outputs).
        ALL_RESOLVED   = 1000,
        PAREN          = 1001,
        BINARY_OP      = 1002,
        UNARY_OP       = 1003
    };

    int op;
    Expr    *a;
    union {
        double  v;
        hParam  parh;
        Param  *parp;
        Expr    *b;

        // For use while parsing
        char    c;
    };

    Expr() { }
    Expr(double val) : op(CONSTANT) { v = val; }

    static inline Expr *AllocExpr()
        { return (Expr *)AllocTemporary(sizeof(Expr)); }

    static Expr *From(hParam p);
    static Expr *From(double v);

    Expr *AnyOp(int op, Expr *b);
    inline Expr *Plus (Expr *b_) { return AnyOp(PLUS,  b_); }
    inline Expr *Minus(Expr *b_) { return AnyOp(MINUS, b_); }
    inline Expr *Times(Expr *b_) { return AnyOp(TIMES, b_); }
    inline Expr *Div  (Expr *b_) { return AnyOp(DIV,   b_); }

    inline Expr *Negate() { return AnyOp(NEGATE, NULL); }
    inline Expr *Sqrt  () { return AnyOp(SQRT,   NULL); }
    inline Expr *Square() { return AnyOp(SQUARE, NULL); }
    inline Expr *Sin   () { return AnyOp(SIN,    NULL); }
    inline Expr *Cos   () { return AnyOp(COS,    NULL); }
    inline Expr *ASin  () { return AnyOp(ASIN,   NULL); }
    inline Expr *ACos  () { return AnyOp(ACOS,   NULL); }

    Expr *PartialWrt(hParam p) const;
    double Eval() const;
    uint64_t ParamsUsed() const;
    bool DependsOn(hParam p) const;
    static bool Tol(double a, double b);
    Expr *FoldConstants();
    void Substitute(hParam oldh, hParam newh);

    static const hParam NO_PARAMS, MULTIPLE_PARAMS;
    hParam ReferencedParams(ParamList *pl) const;

    void ParamsToPointers();

    std::string Print() const;

    // number of child nodes: 0 (e.g. constant), 1 (sqrt), or 2 (+)
    int Children() const;
    // total number of nodes in the tree
    int Nodes() const;

    // Make a simple copy
    Expr *DeepCopy() const;
    // Make a copy, with the parameters (usually referenced by hParam)
    // resolved to pointers to the actual value. This speeds things up
    // considerably.
    Expr *DeepCopyWithParamsAsPointers(IdList<Param,hParam> *firstTry,
        IdList<Param,hParam> *thenTry) const;

    static Expr *From(const char *in, bool popUpError);
    static void  Lex(const char *in);
    static Expr *Next();
    static void  Consume();

    static void PushOperator(Expr *e);
    static Expr *PopOperator();
    static Expr *TopOperator();
    static void PushOperand(Expr *e);
    static Expr *PopOperand();

    static void Reduce();
    static void ReduceAndPush(Expr *e);
    static int Precedence(Expr *e);

    static int Precedence(int op);
    static void Parse();
};

class ExprVector {
public:
    Expr *x, *y, *z;

    static ExprVector From(Expr *x, Expr *y, Expr *z);
    static ExprVector From(Vector vn);
    static ExprVector From(hParam x, hParam y, hParam z);
    static ExprVector From(double x, double y, double z);

    ExprVector Plus(ExprVector b) const;
    ExprVector Minus(ExprVector b) const;
    Expr *Dot(ExprVector b) const;
    ExprVector Cross(ExprVector b) const;
    ExprVector ScaledBy(Expr *s) const;
    ExprVector WithMagnitude(Expr *s) const;
    Expr *Magnitude() const;

    Vector Eval() const;
};

class ExprQuaternion {
public:
    Expr *w, *vx, *vy, *vz;

    static ExprQuaternion From(Expr *w, Expr *vx, Expr *vy, Expr *vz);
    static ExprQuaternion From(Quaternion qn);
    static ExprQuaternion From(hParam w, hParam vx, hParam vy, hParam vz);

    ExprVector RotationU() const;
    ExprVector RotationV() const;
    ExprVector RotationN() const;

    ExprVector Rotate(ExprVector p) const;
    ExprQuaternion Times(ExprQuaternion b) const;

    Expr *Magnitude() const;
};

#endif

