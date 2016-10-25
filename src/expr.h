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
    uint32_t marker;

    // A parameter, by the hParam handle
    static const int PARAM          =  0;
    // A parameter, by a pointer straight in to the param table (faster,
    // if we know that the param table won't move around)
    static const int PARAM_PTR      =  1;

    // These are used only for user-entered expressions.
    static const int POINT          = 10;
    static const int ENTITY         = 11;

    static const int CONSTANT       = 20;

    static const int PLUS           = 100;
    static const int MINUS          = 101;
    static const int TIMES          = 102;
    static const int DIV            = 103;
    static const int NEGATE         = 104;
    static const int SQRT           = 105;
    static const int SQUARE         = 106;
    static const int SIN            = 107;
    static const int COS            = 108;
    static const int ASIN           = 109;
    static const int ACOS           = 110;

    // Special helpers for when we're parsing an expression from text.
    // Initially, literals (like a constant number) appear in the same
    // format as they will in the finished expression, but the operators
    // are different until the parser fixes things up (and builds the
    // tree from the flat list that the lexer outputs).
    static const int ALL_RESOLVED   = 1000;
    static const int PAREN          = 1001;
    static const int BINARY_OP      = 1002;
    static const int UNARY_OP       = 1003;

    int op;
    Expr    *a;
    Expr    *b;
    union {
        double  v;
        hParam  parh;
        Param  *parp;
        hEntity entity;

        // For use while parsing
        char    c;
    }       x;

    static inline Expr *AllocExpr(void)
        { return (Expr *)AllocTemporary(sizeof(Expr)); }

    static Expr *From(hParam p);
    static Expr *From(double v);

    Expr *AnyOp(int op, Expr *b);
    inline Expr *Plus (Expr *b) { return AnyOp(PLUS,  b); }
    inline Expr *Minus(Expr *b) { return AnyOp(MINUS, b); }
    inline Expr *Times(Expr *b) { return AnyOp(TIMES, b); }
    inline Expr *Div  (Expr *b) { return AnyOp(DIV,   b); }

    inline Expr *Negate(void) { return AnyOp(NEGATE, NULL); }
    inline Expr *Sqrt  (void) { return AnyOp(SQRT,   NULL); }
    inline Expr *Square(void) { return AnyOp(SQUARE, NULL); }
    inline Expr *Sin   (void) { return AnyOp(SIN,    NULL); }
    inline Expr *Cos   (void) { return AnyOp(COS,    NULL); }
    inline Expr *ASin  (void) { return AnyOp(ASIN,   NULL); }
    inline Expr *ACos  (void) { return AnyOp(ACOS,   NULL); }

    Expr *PartialWrt(hParam p);
    double Eval(void);
    uint64_t ParamsUsed(void);
    bool DependsOn(hParam p);
    static bool Tol(double a, double b);
    Expr *FoldConstants(void);
    void Substitute(hParam oldh, hParam newh);

    static const hParam NO_PARAMS, MULTIPLE_PARAMS;
    hParam ReferencedParams(ParamList *pl);

    void ParamsToPointers(void);

    void App(const char *str, ...);
    const char *Print(void);
    void PrintW(void); // worker

    // number of child nodes: 0 (e.g. constant), 1 (sqrt), or 2 (+)
    int Children(void);
    // total number of nodes in the tree
    int Nodes(void);

    // Make a simple copy
    Expr *DeepCopy(void);
    // Make a copy, with the parameters (usually referenced by hParam)
    // resolved to pointers to the actual value. This speeds things up
    // considerably.
    Expr *DeepCopyWithParamsAsPointers(IdList<Param,hParam> *firstTry,
        IdList<Param,hParam> *thenTry);

    static Expr *From(char *in, bool popUpError);
    static void  Lex(char *in);
    static Expr *Next(void);
    static void  Consume(void);

    static void PushOperator(Expr *e);
    static Expr *PopOperator(void);
    static Expr *TopOperator(void);
    static void PushOperand(Expr *e);
    static Expr *PopOperand(void);

    static void Reduce(void);
    static void ReduceAndPush(Expr *e);
    static int Precedence(Expr *e);

    static int Precedence(int op);
    static void Parse(void);
};

class ExprVector {
public:
    Expr *x, *y, *z;

    static ExprVector From(Expr *x, Expr *y, Expr *z);
    static ExprVector From(Vector vn);
    static ExprVector From(hParam x, hParam y, hParam z);
    static ExprVector From(double x, double y, double z);

    ExprVector Plus(ExprVector b);
    ExprVector Minus(ExprVector b);
    Expr *Dot(ExprVector b);
    ExprVector Cross(ExprVector b);
    ExprVector ScaledBy(Expr *s);
    ExprVector WithMagnitude(Expr *s);
    Expr *Magnitude(void);

    Vector Eval(void);
};

class ExprQuaternion {
public:
    Expr *w, *vx, *vy, *vz;

    static ExprQuaternion From(Expr *w, Expr *vx, Expr *vy, Expr *vz);
    static ExprQuaternion From(Quaternion qn);
    static ExprQuaternion From(hParam w, hParam vx, hParam vy, hParam vz);

    ExprVector RotationU(void);
    ExprVector RotationV(void);
    ExprVector RotationN(void);

    ExprVector Rotate(ExprVector p);
    ExprQuaternion Times(ExprQuaternion b);

    Expr *Magnitude(void);
};

#endif

