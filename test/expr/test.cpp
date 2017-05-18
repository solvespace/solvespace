#include "harness.h"
#include <iostream>

// uncomment this to debug the test cases in this file
#define TEST_EXPR_DEBUG(expression, expected, parsed, result) \
    // std::cout << "input string: '" << expression << "', expected value: " << expected << ", parsed expression: '" << e->Print() << "', result: " << result << std::endl;


// The crazy fabs() code in this test is because there are many numbers
// that floats can't represent exactly, so we have to check for proximity
// rather than equality.

TEST_CASE(addition) {
    std::string expression = "1.2 + 2.3";
    double expected = 3.5;
    Expr *e = Expr::From(expression.c_str(), false);
    double result = e->Eval();
    TEST_EXPR_DEBUG(expression, expected, e->Print(), result);
    CHECK_TRUE(fabs(result - expected) < 0.000001);
}

TEST_CASE(subtraction) {
    std::string expression = "99.9-88.8";
    double expected = 11.1;
    Expr *e = Expr::From(expression.c_str(), false);
    double result = e->Eval();
    TEST_EXPR_DEBUG(expression, expected, e->Print(), result);
    CHECK_TRUE(fabs(result - expected) < 0.000001);
}

TEST_CASE(multiplication) {
    std::string expression = "6 * 9";
    double expected = 54;
    Expr *e = Expr::From(expression.c_str(), false);
    double result = e->Eval();
    TEST_EXPR_DEBUG(expression, expected, e->Print(), result);
    CHECK_TRUE(fabs(result - expected) < 0.000001);
}

TEST_CASE(division) {
    std::string expression = "1 / 8";
    double expected = 0.125;
    Expr *e = Expr::From(expression.c_str(), false);
    double result = e->Eval();
    TEST_EXPR_DEBUG(expression, expected, e->Print(), result);
    CHECK_TRUE(fabs(result - expected) < 0.000001);
}
