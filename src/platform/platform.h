//-----------------------------------------------------------------------------
// Platform-dependent functionality.
//
// Copyright 2017 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_PLATFORM_H
#define SOLVESPACE_PLATFORM_H
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <jni.h>
#endif

#include <string>
#include <vector>

namespace SolveSpace {
namespace Platform {

// UTF-8 ⟷ UTF-16 conversion, for Windows.
#if defined(WIN32)
std::string Narrow(const wchar_t *s);
std::wstring Widen(const char *s);
std::string Narrow(const std::wstring &s);
std::wstring Widen(const std::string &s);
#endif

#if defined(_WIN32)
    const std::string embeddedFont = "res://fonts/BitstreamVeraSans-Roman-builtin.ttf";
#else   // Linux and macOS
    const std::string embeddedFont = "BitstreamVeraSans-Roman-builtin.ttf";
#endif

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

// File manipulation functions.
bool FileExists(const Platform::Path &filename);
FILE *OpenFile(const Platform::Path &filename, const char *mode);
bool ReadFile(const Platform::Path &filename, std::string *data);
bool WriteFile(const Platform::Path &filename, const std::string &data);
void RemoveFile(const Platform::Path &filename);

// Resource loading function.
const void *LoadResource(const std::string &name, size_t *size);

// Startup and command-line argument handling.
std::vector<std::string> InitCli(int argc, char **argv);

// Temporary arena functions.
void *AllocTemporary(size_t size);
void FreeAllTemporary();

// Android Env
#ifdef __ANDROID__
extern AAssetManager *amgr;
extern JNIEnv* GetJNIEnv();
// Open a Storage Access Framework content URI through the Java activity's
// ContentResolver. Returns an owned file descriptor (>= 0) on success, or -1
// on failure. The descriptor must be closed by the caller (e.g. via fclose).
int AndroidOpenContentFile(const char *uri, const char *mode);
// Delete a Storage Access Framework content URI through the Java activity.
void AndroidDeleteContentFile(const char *uri);
// Absolute path of the app's private files directory (getFilesDir()).
std::string AndroidInternalStoragePath();
// Maps a content URI to a filesystem-safe name (used as a key for
// autosave/backup companions that must live in private storage).
std::string AndroidContentKey(const char *uri);
#endif

} // namespace Platform
} // namespace SolveSpace

#endif
