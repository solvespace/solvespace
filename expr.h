
#ifndef __EXPR_H
#define __EXPR_H

class Expr;

class Expr {
public:
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
        hPoint  point;
        hEntity entity;

        // For use while parsing
        char    c;
    }       x;

    static Expr *FromParam(hParam p);
    static Expr *FromConstant(double v);

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

    Expr *PartialWrt(hParam p);
    double Eval(void);

    void ParamsToPointers(void);

    void App(char *str, ...);
    char *Print(void);
    void PrintW(void); // worker

    // Make a copy of an expression that won't get blown away when we
    // do a FreeAllExprs()
    Expr *Keep(void);

    static Expr *FromString(char *in);
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

#endif
