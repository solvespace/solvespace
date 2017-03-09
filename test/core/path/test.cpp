#include "harness.h"

using Platform::Path;

#if defined(WIN32)
#define S "\\"
#define R "C:"
#define U "\\\\?\\C:"
#else
#define S "/"
#define R ""
#define U ""
#endif

TEST_CASE(from_raw) {
    Path path = Path::From("/foo");
    CHECK_EQ_STR(path.raw, "/foo");
}

#if defined(WIN32) || defined(__APPLE__)
TEST_CASE(equals_win32_apple) {
    CHECK_TRUE(Path::From(R S "foo").Equals(Path::From(R S "foo")));
    CHECK_TRUE(Path::From(R S "foo").Equals(Path::From(R S "FOO")));
    CHECK_FALSE(Path::From(R S "foo").Equals(Path::From(R S "bar")));
}
#else
TEST_CASE(equals_unix) {
    CHECK_TRUE(Path::From(R S "foo").Equals(Path::From(R S "foo")));
    CHECK_FALSE(Path::From(R S "foo").Equals(Path::From(R S "FOO")));
    CHECK_FALSE(Path::From(R S "foo").Equals(Path::From(R S "bar")));
}
#endif

#if defined(WIN32)
TEST_CASE(is_absolute_win32) {
    CHECK_TRUE(Path::From("c:\\foo").IsAbsolute());
    CHECK_TRUE(Path::From("\\\\?\\c:\\").IsAbsolute());
    CHECK_TRUE(Path::From("\\\\server\\share\\").IsAbsolute());
    CHECK_FALSE(Path::From("c:/foo").IsAbsolute());
    CHECK_FALSE(Path::From("c:foo").IsAbsolute());
    CHECK_FALSE(Path::From("\\\\?").IsAbsolute());
    CHECK_FALSE(Path::From("\\\\server\\").IsAbsolute());
    CHECK_FALSE(Path::From("\\\\?\\c:").IsAbsolute());
    CHECK_FALSE(Path::From("\\\\server\\share").IsAbsolute());
    CHECK_FALSE(Path::From("foo").IsAbsolute());
    CHECK_FALSE(Path::From("/foo").IsAbsolute());
}
#else
TEST_CASE(is_absolute_unix) {
    CHECK_TRUE(Path::From("/foo").IsAbsolute());
    CHECK_FALSE(Path::From("c:/foo").IsAbsolute());
    CHECK_FALSE(Path::From("c:\\foo").IsAbsolute());
    CHECK_FALSE(Path::From("\\\\?\\foo").IsAbsolute());
    CHECK_FALSE(Path::From("c:foo").IsAbsolute());
    CHECK_FALSE(Path::From("foo").IsAbsolute());
}
#endif

TEST_CASE(has_extension) {
    CHECK_TRUE(Path::From("foo.bar").HasExtension("bar"));
    CHECK_TRUE(Path::From("foo.bar").HasExtension("BAR"));
    CHECK_TRUE(Path::From("foo.bAr").HasExtension("BaR"));
    CHECK_TRUE(Path::From("foo.bar").HasExtension("bar"));
    CHECK_FALSE(Path::From("foo.bar").HasExtension("baz"));
}

TEST_CASE(file_name) {
    CHECK_EQ_STR(Path::From("foo").FileName(), "foo");
    CHECK_EQ_STR(Path::From("foo" S "bar").FileName(), "bar");
}

TEST_CASE(file_stem) {
    CHECK_EQ_STR(Path::From("foo").FileStem(), "foo");
    CHECK_EQ_STR(Path::From("foo" S "bar").FileStem(), "bar");
    CHECK_EQ_STR(Path::From("foo.ext").FileStem(), "foo");
    CHECK_EQ_STR(Path::From("foo" S "bar.ext").FileStem(), "bar");
}

TEST_CASE(extension) {
    CHECK_EQ_STR(Path::From("foo").Extension(), "");
    CHECK_EQ_STR(Path::From("foo.bar").Extension(), "bar");
    CHECK_EQ_STR(Path::From("foo.bar.baz").Extension(), "baz");
}

TEST_CASE(with_extension) {
    CHECK_EQ_STR(Path::From("foo.bar").WithExtension("baz").raw, "foo.baz");
    CHECK_EQ_STR(Path::From("foo").WithExtension("baz").raw, "foo.baz");
}

TEST_CASE(parent) {
    Path path;
    path = Path::From("foo" S "bar");
    CHECK_EQ_STR(path.Parent().raw, "foo" S);
    path = Path::From("foo" S "bar" S);
    CHECK_EQ_STR(path.Parent().raw, "foo" S);
    path = Path::From(R S "foo" S "bar");
    CHECK_EQ_STR(path.Parent().raw, R S "foo" S);
    path = Path::From(R S "foo");
    CHECK_EQ_STR(path.Parent().raw, R S);

    path = Path::From("");
    CHECK_TRUE(path.Parent().IsEmpty());
    path = Path::From("foo");
    CHECK_TRUE(path.Parent().IsEmpty());
    path = Path::From("foo" S);
    CHECK_TRUE(path.Parent().IsEmpty());
    path = Path::From(R S);
    CHECK_TRUE(path.Parent().IsEmpty());
}

#if defined(WIN32)
TEST_CASE(parent_win32) {
    Path path;
    path = Path::From(U S);
    CHECK_TRUE(path.Parent().IsEmpty());
}
#endif

TEST_CASE(join) {
    Path path;
    path = Path::From("foo");
    CHECK_EQ_STR(path.Join(Path::From("bar")).raw, "foo" S "bar");
    path = Path::From("foo" S);
    CHECK_EQ_STR(path.Join(Path::From("bar")).raw, "foo" S "bar");

    path = Path::From("");
    CHECK_TRUE(path.Join(Path::From("bar")).IsEmpty());
    path = Path::From("foo");
    CHECK_TRUE(path.Join(Path::From("")).IsEmpty());
    path = Path::From("foo");
    CHECK_TRUE(path.Join(Path::From(R S "bar")).IsEmpty());
}

TEST_CASE(expand) {
    Path path;
    path = Path::From("foo");
    CHECK_EQ_STR(path.Expand().raw, "foo");
    path = Path::From("foo" S "bar");
    CHECK_EQ_STR(path.Expand().raw, "foo" S "bar");
    path = Path::From("foo" S);
    CHECK_EQ_STR(path.Expand().raw, "foo");
    path = Path::From("foo" S ".");
    CHECK_EQ_STR(path.Expand().raw, "foo");
    path = Path::From("foo" S "." S "bar");
    CHECK_EQ_STR(path.Expand().raw, "foo" S "bar");
    path = Path::From("foo" S ".." S "bar");
    CHECK_EQ_STR(path.Expand().raw, "bar");
    path = Path::From("foo" S "..");
    CHECK_EQ_STR(path.Expand().raw, ".");
    path = Path::From(R S "foo" S "..");
    CHECK_EQ_STR(path.Expand().raw, U S);
    path = Path::From(R S);
    CHECK_EQ_STR(path.Expand().raw, U S);

    path = Path::From(R S "..");
    CHECK_TRUE(path.Expand().IsEmpty());
    path = Path::From(R S ".." S "foo");
    CHECK_TRUE(path.Expand().IsEmpty());
    path = Path::From("..");
    CHECK_TRUE(path.Expand().IsEmpty());
}

#if defined(WIN32)
TEST_CASE(expand_win32) {
    Path path;
    path = Path::From(R S "foo");
    CHECK_EQ_STR(path.Expand().raw, U S "foo");
    path = Path::From(U S "foo");
    CHECK_EQ_STR(path.Expand().raw, U S "foo");
}
#endif

TEST_CASE(expand_from_cwd) {
    Path cwd = Path::CurrentDirectory().Expand();

    Path path;
    path = Path::From(R S "foo");
    CHECK_EQ_STR(path.Expand(/*fromCurrentDirectory=*/true).raw,
                 U S "foo");
    path = Path::From("foo" S "bar");
    CHECK_EQ_STR(path.Expand(/*fromCurrentDirectory=*/true).raw,
                 cwd.raw + S "foo" S "bar");
    path = Path::From(".." S "bar");
    CHECK_EQ_STR(path.Expand(/*fromCurrentDirectory=*/true).raw,
                 cwd.Parent().raw + "bar");
}

TEST_CASE(relative_to) {
    Path base;
    base = Path::From(R S "foo" S "bar");
    CHECK_EQ_STR(Path::From(R S "foo").RelativeTo(base).raw,
                 "..");
    base = Path::From(R S "foo" S "bar");
    CHECK_EQ_STR(Path::From(R S "foo" S "baz").RelativeTo(base).raw,
                 ".." S "baz");
    base = Path::From(R S "foo" S "bar");
    CHECK_EQ_STR(Path::From(R S "foo" S "bar" S "quux").RelativeTo(base).raw,
                 "quux");
    base = Path::From(R S "foo" S "bar");
    CHECK_EQ_STR(Path::From(R S "foo" S "bar").RelativeTo(base).raw,
                 ".");

    base = Path::From("foo");
    CHECK_TRUE(Path::From(R S "foo" S "bar").RelativeTo(base).IsEmpty());
    base = Path::From(R S "foo" S "bar");
    CHECK_TRUE(Path::From("foo").RelativeTo(base).IsEmpty());
}

#if defined(WIN32)
TEST_CASE(relative_to_win32) {
    Path base;
    base = Path::From("C:\\foo");
    CHECK_EQ_STR(Path::From("\\\\?\\C:\\bar").RelativeTo(base).raw,
                 "..\\bar");
    base = Path::From("C:\\foo");
    CHECK_EQ_STR(Path::From("c:\\FOO").RelativeTo(base).raw,
                 ".");

    base = Path::From("C:\\foo");
    CHECK_TRUE(Path::From("D:\\bar").RelativeTo(base).IsEmpty());
    CHECK_TRUE(Path::From("\\\\server\\share\\bar").RelativeTo(base).IsEmpty());
}
#elif defined(__APPLE__)
TEST_CASE(relative_to_apple) {
    Path base;
    base = Path::From("/users/foo");
    CHECK_EQ_STR(Path::From("/Users/FOO").RelativeTo(base).raw,
                 ".");
}
#else
TEST_CASE(relative_to_unix) {
    Path base;
    base = Path::From("/users/foo");
    CHECK_EQ_STR(Path::From("/Users/FOO").RelativeTo(base).raw,
                 "../../Users/FOO");
}
#endif

TEST_CASE(from_portable) {
    CHECK_EQ_STR(Path::FromPortable("foo/bar").raw, "foo" S "bar");
}

TEST_CASE(to_portable) {
    CHECK_EQ_STR(Path::From("foo" S "bar").ToPortable(), "foo/bar");
}
