//-----------------------------------------------------------------------------
// Our harness for running test cases, and reusable checks.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

// Hack... we should rename the one in ui.h instead.
#undef CHECK_TRUE

namespace SolveSpace {
namespace Test {

class Helper {
public:
    size_t  checkCount;
    size_t  failCount;

    bool RecordCheck(bool success);
    void PrintFailure(const char *file, int line, std::string msg);
    std::string GetAssetPath(std::string testFile, std::string assetName,
                             std::string mangle = "");

    bool CheckTrue(const char *file, int line, const char *expr, bool result);
    bool CheckLoad(const char *file, int line, const char *fixture);
    bool CheckSave(const char *file, int line, const char *reference);
    bool CheckRender(const char *file, int line, const char *fixture);
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
    do { if(!helper->CheckTrue(__FILE__, __LINE__, #cond, cond)) return; } while(0)
#define CHECK_LOAD(fixture) \
    do { if(!helper->CheckLoad(__FILE__, __LINE__, fixture)) return; } while(0)
#define CHECK_SAVE(fixture) \
    do { if(!helper->CheckSave(__FILE__, __LINE__, fixture)) return; } while(0)
#define CHECK_RENDER(reference) \
    do { if(!helper->CheckRender(__FILE__, __LINE__, reference)) return; } while(0)
