//-----------------------------------------------------------------------------
// Common platform-dependent functionality.
//
// Copyright 2017 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_PLATFORM_H
#define SOLVESPACE_PLATFORM_H

namespace Platform {

// A filesystem path, respecting the conventions of the current platform.
// Transformation functions return an empty path on error.
class Path {
public:
    std::string raw;

    static Path From(std::string raw);
    static Path CurrentDirectory();

    void Clear() { raw.clear(); }

    bool Equals(const Path &other) const;
    bool IsEmpty() const { return raw.empty(); }
    bool IsAbsolute() const;
    bool HasExtension(std::string ext) const;

    std::string FileName() const;
    std::string FileStem() const;
    std::string Extension() const;

    Path WithExtension(std::string ext) const;
    Path Parent() const;
    Path Join(const std::string &component) const;
    Path Join(const Path &other) const;
    Path Expand(bool fromCurrentDirectory = false) const;
    Path RelativeTo(const Path &base) const;

    // Converting to and from a platform-independent representation
    // (conventionally, the Unix one).
    static Path FromPortable(const std::string &repr);
    std::string ToPortable() const;
};

struct PathLess {
    bool operator()(const Path &a, const Path &b) const { return a.raw < b.raw; }
};

}

#endif
