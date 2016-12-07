//-----------------------------------------------------------------------------
// Utility functions used by the Unix port. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2013 Daniel Richard G. <skunk@iSKUNK.ORG>
//-----------------------------------------------------------------------------
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#ifdef __APPLE__
#   include <strings.h> // for strcasecmp
#   include <CoreFoundation/CFString.h>
#   include <CoreFoundation/CFURL.h>
#   include <CoreFoundation/CFBundle.h>
#endif

#include "solvespace.h"
#include "config.h"

namespace SolveSpace {

void dbp(const char *str, ...)
{
    va_list f;
    static char buf[1024*50];
    va_start(f, str);
    vsnprintf(buf, sizeof(buf), str, f);
    va_end(f);

    fputs(buf, stderr);
    fputc('\n', stderr);
}

void assert_failure(const char *file, unsigned line, const char *function,
                    const char *condition, const char *message) {
    fprintf(stderr, "File %s, line %u, function %s:\n", file, line, function);
    fprintf(stderr, "Assertion '%s' failed: ((%s) == false).\n", message, condition);

#ifndef LIBRARY
    static void *ptrs[1024] = {};
    size_t nptrs = backtrace(ptrs, sizeof(ptrs) / sizeof(ptrs[0]));
    char **syms = backtrace_symbols(ptrs, nptrs);

    fprintf(stderr, "Backtrace:\n");
    if(syms != NULL) {
        for(size_t i = 0; i < nptrs; i++) {
            fprintf(stderr, "%2zu: %s\n", i, syms[i]);
        }
    } else {
        for(size_t i = 0; i < nptrs; i++) {
            fprintf(stderr, "%2zu: %p\n", i, ptrs[i]);
        }
    }
#endif

    abort();
}

bool PathEqual(const std::string &a, const std::string &b)
{
#if defined(__APPLE__)
    // Case-sensitivity is actually per-volume on OS X,
    // but it is tedious to implement and test for little benefit.
    return !strcasecmp(a.c_str(), b.c_str());
#else
    return a == b;
#endif
}

std::string PathSepPlatformToUnix(const std::string &filename)
{
    return filename;
}

std::string PathSepUnixToPlatform(const std::string &filename)
{
    return filename;
}

std::string PathFromCurrentDirectory(const std::string &relFilename)
{
    // On Unix we can just pass this to ssfopen directly.
    return relFilename;
}

FILE *ssfopen(const std::string &filename, const char *mode)
{
    ssassert(filename.length() == strlen(filename.c_str()),
             "Unexpected null byte in middle of a path");
    return fopen(filename.c_str(), mode);
}

void ssremove(const std::string &filename)
{
    ssassert(filename.length() == strlen(filename.c_str()),
             "Unexpected null byte in middle of a path");
    remove(filename.c_str());
}

static std::string ExpandPath(std::string path) {
    char *expanded_c_path = realpath(path.c_str(), NULL);
    if(expanded_c_path == NULL) return "";

    std::string expanded_path = expanded_c_path;
    free(expanded_c_path);
    return expanded_path;
}

static const std::string &FindLocalResourceDir() {
    static std::string resourceDir;
    static bool checked;

    if(checked) return resourceDir;
    checked = true;

    // Getting path to your own executable is a total portability disaster.
    // Good job *nix OSes; you're basically all awful here.
    std::string selfPath;
#if defined(__linux__)
    selfPath = "/proc/self/exe";
#elif defined(__NetBSD__)
    selfPath = "/proc/curproc/exe"
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
    selfPath = "/proc/curproc/file";
#elif defined(__APPLE__)
    CFURLRef cfUrl =
        CFBundleCopyExecutableURL(CFBundleGetMainBundle());
    CFStringRef cfPath = CFURLCopyFileSystemPath(cfUrl, kCFURLPOSIXPathStyle);
    selfPath.resize(CFStringGetLength(cfPath) + 1); // reserve space for NUL
    ssassert(CFStringGetCString(cfPath, &selfPath[0], selfPath.size(), kCFStringEncodingUTF8),
             "Cannot convert CFString to C string");
    selfPath.resize(selfPath.size() - 1);
    CFRelease(cfUrl);
    CFRelease(cfPath);
#else
    // We don't know how to find the local resource directory on this platform,
    // so use the global one (by returning an empty string).
    return resourceDir;
#endif

    resourceDir = ExpandPath(selfPath);
    if(!resourceDir.empty()) {
        resourceDir.erase(resourceDir.rfind('/'));
        resourceDir += "/../res";
        resourceDir = ExpandPath(resourceDir);
    }
    if(!resourceDir.empty()) {
        struct stat st;
        if(stat(resourceDir.c_str(), &st)) {
            // We looked at the path where the local resource directory ought to be,
            // but there isn't one, so use the global one.
            resourceDir = "";
        }
    }
    return resourceDir;
}

const void *LoadResource(const std::string &name, size_t *size) {
    static std::map<std::string, std::string> cache;

    auto it = cache.find(name);
    if(it == cache.end()) {
        const std::string &resourceDir = FindLocalResourceDir();

        std::string path;
        if(resourceDir.empty()) {
#if defined(__APPLE__)
            CFStringRef cfName =
                CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(),
                                          kCFStringEncodingUTF8);
            CFURLRef cfUrl =
                CFBundleCopyResourceURL(CFBundleGetMainBundle(), cfName, NULL, NULL);
            CFStringRef cfPath = CFURLCopyFileSystemPath(cfUrl, kCFURLPOSIXPathStyle);
            path.resize(CFStringGetLength(cfPath) + 1); // reserve space for NUL
            ssassert(CFStringGetCString(cfPath, &path[0], path.size(), kCFStringEncodingUTF8),
                     "Cannot convert CFString to C string");
            path.resize(path.size() - 1);
            CFRelease(cfName);
            CFRelease(cfUrl);
            CFRelease(cfPath);
#else
            path = (UNIX_DATADIR "/") + name;
#endif
        } else {
            path = resourceDir + "/" + name;
        }

        ssassert(ReadFile(path, &cache[name]), "Cannot read resource");
        it = cache.find(name);
    }

    *size = (*it).second.size();
    return static_cast<const void *>(&(*it).second[0]);
}

//-----------------------------------------------------------------------------
// A separate heap, on which we allocate expressions. Maybe a bit faster,
// since fragmentation is less of a concern, and it also makes it possible
// to be sloppy with our memory management, and just free everything at once
// at the end.
//-----------------------------------------------------------------------------

typedef struct _AllocTempHeader AllocTempHeader;

typedef struct _AllocTempHeader {
    AllocTempHeader *prev;
    AllocTempHeader *next;
} AllocTempHeader;

static AllocTempHeader *Head = NULL;

void *AllocTemporary(size_t n)
{
    AllocTempHeader *h =
        (AllocTempHeader *)malloc(n + sizeof(AllocTempHeader));
    h->prev = NULL;
    h->next = Head;
    if(Head) Head->prev = h;
    Head = h;
    memset(&h[1], 0, n);
    return (void *)&h[1];
}

void FreeTemporary(void *p)
{
    AllocTempHeader *h = (AllocTempHeader *)p - 1;
    if(h->prev) {
        h->prev->next = h->next;
    } else {
        Head = h->next;
    }
    if(h->next) h->next->prev = h->prev;
    free(h);
}

void FreeAllTemporary(void)
{
    AllocTempHeader *h = Head;
    while(h) {
        AllocTempHeader *f = h;
        h = h->next;
        free(f);
    }
    Head = NULL;
}

void *MemAlloc(size_t n) {
    void *p = malloc(n);
    ssassert(p != NULL, "Cannot allocate memory");
    return p;
}

void MemFree(void *p) {
    free(p);
}

std::vector<std::string> InitPlatform(int argc, char **argv) {
    std::vector<std::string> args;
    for(int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }
    return args;
}

};
