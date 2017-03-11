//-----------------------------------------------------------------------------
// Our harness for running test cases, and reusable checks.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

// Hack... we should rename the ones in ui.h instead.
#undef CHECK_TRUE
#undef CHECK_FALSE

namespace SolveSpace {
namespace Test {

class Helper {
public:
    size_t  checkCount;
    size_t  failCount;

    bool RecordCheck(bool success);
    void PrintFailure(const char *file, int line, std::string msg);
    Platform::Path GetAssetPath(std::string testFile, std::string assetName,
                                std::string mangle = "");

    bool CheckBool(const char *file, int line, const char *expr,
                   bool value, bool reference);
    bool CheckEqualString(const char *file, int line, const char *valueExpr,
                          const std::string &value, const std::string &reference);
    bool CheckEqualEpsilon(const char *file, int line, const char *valueExpr,
                           double value, double reference);
    bool CheckLoad(const char *file, int line, const char *fixture);
    bool CheckSave(const char *file, int line, const char *reference);
    bool CheckRender(const char *file, int line, const char *fixture);
    bool CheckRenderXY(const char *file, int line, const char *fixture);
    bool CheckRenderIso(const char *file, int line, const char *fixture);
};

class Case {
public:
    std::string fileName;
    std::string caseName;
    std::function<void(Helper *)> fn;

    static int Register(Case testCase);
};

}
}

using namespace SolveSpace;

#define TEST_CASE(name) \
    static void Test_##name(Test::Helper *); \
    static Test::Case TestCase_##name = { __FILE__, #name, Test_##name }; \
    static int TestReg_##name = Test::Case::Register(TestCase_##name); \
    static void Test_##name(Test::Helper *helper) // { ... }

#define CHECK_TRUE(cond) \
    do { if(!helper->CheckBool(__FILE__, __LINE__, #cond, cond, true)) return; } while(0)
#define CHECK_FALSE(cond) \
    do { if(!helper->CheckBool(__FILE__, __LINE__, #cond, cond, false)) return; } while(0)
#define CHECK_EQ_STR(value, reference) \
    do { if(!helper->CheckEqualString(__FILE__, __LINE__, \
                                      #value, value, reference)) return; } while(0)
#define CHECK_EQ_EPS(value, reference) \
    do { if(!helper->CheckEqualEpsilon(__FILE__, __LINE__, \
                                       #value, value, reference)) return; } while(0)
#define CHECK_LOAD(fixture) \
    do { if(!helper->CheckLoad(__FILE__, __LINE__, fixture)) return; } while(0)
#define CHECK_SAVE(fixture) \
    do { if(!helper->CheckSave(__FILE__, __LINE__, fixture)) return; } while(0)
#define CHECK_RENDER(reference) \
    do { if(!helper->CheckRenderXY(__FILE__, __LINE__, reference)) return; } while(0)
#define CHECK_RENDER_ISO(reference) \
    do { if(!helper->CheckRenderIso(__FILE__, __LINE__, reference)) return; } while(0)
