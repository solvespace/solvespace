#include "solvespace.h"
#include "harness.h"

using Platform::Path;

#if defined(WIN32)
#define S "\\"
#else
#define S "/"
#endif

TEST_CASE(ensure_sketch_extension) {
    Path path = Path::From("part");
    bool shouldConfirmOverwrite = true;
    CHECK_TRUE(EnsureSketchExtension(&path, &shouldConfirmOverwrite));
    CHECK_EQ_STR(path.raw, "part.slvs");
    CHECK_FALSE(shouldConfirmOverwrite);

    path = Path::From("part.slvs");
    shouldConfirmOverwrite = true;
    CHECK_FALSE(EnsureSketchExtension(&path, &shouldConfirmOverwrite));
    CHECK_EQ_STR(path.raw, "part.slvs");
    CHECK_FALSE(shouldConfirmOverwrite);

    path = Path::From("part.foo");
    shouldConfirmOverwrite = true;
    CHECK_FALSE(EnsureSketchExtension(&path, &shouldConfirmOverwrite));
    CHECK_EQ_STR(path.raw, "part.foo");
    CHECK_FALSE(shouldConfirmOverwrite);
}

TEST_CASE(ensure_sketch_extension_ignores_parent_dots) {
    Path path = Path::From("dir.with.dots" S "part");
    CHECK_TRUE(EnsureSketchExtension(&path));
    CHECK_EQ_STR(path.raw, "dir.with.dots" S "part.slvs");
}

TEST_CASE(confirm_overwrite_after_appending_sketch_extension) {
    Path path = helper->GetAssetPath(__FILE__, "existing");
    bool shouldConfirmOverwrite = false;
    CHECK_TRUE(EnsureSketchExtension(&path, &shouldConfirmOverwrite));
    CHECK_TRUE(shouldConfirmOverwrite);

    path = helper->GetAssetPath(__FILE__, "absent.slvs");
    shouldConfirmOverwrite = true;
    CHECK_FALSE(EnsureSketchExtension(&path, &shouldConfirmOverwrite));
    CHECK_FALSE(shouldConfirmOverwrite);
}
