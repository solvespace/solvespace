
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

    int op;
    Expr    *a;
    Expr    *b;
    union {
        hParam  parh;
        double *parp;
        hPoint  point;
        hEntity entity;
        double  v;
    }       x;
};

#endif
