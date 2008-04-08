
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

    static const int CONSTANT       = 10;
    static const int PLUS           = 21;
    static const int MINUS          = 22;
    static const int TIMES          = 23;
    static const int DIV            = 24;
    static const int NEGATE         = 25;
    static const int SQRT           = 26;
    static const int SQUARE         = 27;
    static const int SIN            = 28;
    static const int COS            = 29;

    int op;
    Expr    *a;
    Expr    *b;
    union {
        hParam  parh;
        double *parp;
        double  v;
    }       x;
};

#endif
