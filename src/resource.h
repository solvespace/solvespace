//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------

#ifndef __RESOURCE_H
#define __RESOURCE_H

class Pixmap;

// Only the following function is platform-specific.
// It returns a pointer to resource contents that is aligned to at least
// sizeof(void*) and has a global lifetime, or NULL if a resource with
// the specified name does not exist.
const void *LoadResource(const std::string &name, size_t *size);

std::string LoadString(const std::string &name);
Pixmap LoadPNG(const std::string &name);

class Pixmap {
public:
    size_t                     width;
    size_t                     height;
    size_t                     stride;
    bool                       hasAlpha;
    std::unique_ptr<uint8_t[]> data;

    static Pixmap FromPNG(const uint8_t *data, size_t size);
    static Pixmap FromPNG(FILE *f);

    bool IsEmpty() const { return width == 0 && height == 0; }
    size_t GetBytesPerPixel() const { return hasAlpha ? 4 : 3; }

    void Clear();
};

#endif
