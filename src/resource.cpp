//-----------------------------------------------------------------------------
// Discovery and loading of our resources (icons, fonts, templates, etc).
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <png.h>

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Resource loading functions
//-----------------------------------------------------------------------------

std::string LoadString(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    return std::string(static_cast<const char *>(data), size);
}

Pixmap LoadPNG(const std::string &name) {
    size_t size;
    const void *data = LoadResource(name, &size);
    if(data == NULL) oops();

    Pixmap pixmap = Pixmap::FromPNG(static_cast<const uint8_t *>(data), size);
    if(pixmap.IsEmpty()) oops();

    return pixmap;
}

//-----------------------------------------------------------------------------
// Pixmap manipulation
//-----------------------------------------------------------------------------

void Pixmap::Clear() {
    *this = {};
}

static Pixmap ReadPNGIntoPixmap(png_struct *png_ptr, png_info *info_ptr) {
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_GRAY_TO_RGB, NULL);

    Pixmap pixmap = {};
    pixmap.width    = png_get_image_width(png_ptr, info_ptr);
    pixmap.height   = png_get_image_height(png_ptr, info_ptr);
    pixmap.hasAlpha = png_get_color_type(png_ptr, info_ptr) & PNG_COLOR_MASK_ALPHA;

    size_t stride = pixmap.width * pixmap.GetBytesPerPixel();
    if(stride % 4 != 0) stride += 4 - stride % 4;
    pixmap.stride = stride;

    pixmap.data = std::unique_ptr<uint8_t[]>(new uint8_t[pixmap.stride * pixmap.height]);
    uint8_t **rows = png_get_rows(png_ptr, info_ptr);
    for(size_t y = 0; y < pixmap.height; y++) {
        memcpy(&pixmap.data[pixmap.stride * y], rows[y],
               pixmap.width * pixmap.GetBytesPerPixel());
    }

    return pixmap;
}

Pixmap Pixmap::FromPNG(const uint8_t *data, size_t size) {
    Pixmap pixmap = {};
    struct Slice { const uint8_t *data; size_t size; };
    Slice dataSlice = { data, size };
    png_struct *png_ptr = NULL;
    png_info *info_ptr = NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr) goto exit;
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) goto exit;

    if(setjmp(png_jmpbuf(png_ptr))) goto exit;

    png_set_read_fn(png_ptr, &dataSlice,
        [](png_struct *png_ptr, uint8_t *data, size_t size) {
            Slice *dataSlice = (Slice *)png_get_io_ptr(png_ptr);
            if(size <= dataSlice->size) {
                memcpy(data, dataSlice->data, size);
                dataSlice->data += size;
                dataSlice->size -= size;
            } else {
                png_error(png_ptr, "EOF");
            }
        });

    pixmap = ReadPNGIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return pixmap;
}

Pixmap Pixmap::FromPNG(FILE *f) {
    Pixmap pixmap = {};

    png_struct *png_ptr = NULL;
    png_info *info_ptr = NULL;

    uint8_t header[8];
    if(fread(header, 1, sizeof(header), f) != sizeof(header)) goto exit;
    if(png_sig_cmp(header, 0, sizeof(header))) goto exit;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr) goto exit;
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) goto exit;

    if(setjmp(png_jmpbuf(png_ptr))) goto exit;

    png_init_io(png_ptr, f);
    png_set_sig_bytes(png_ptr, sizeof(header));

    pixmap = ReadPNGIntoPixmap(png_ptr, info_ptr);

exit:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return pixmap;
}

}
