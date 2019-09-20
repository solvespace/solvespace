//-----------------------------------------------------------------------------
// Platform-dependent GUI functionality that has only minor differences.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Keyboard events
//-----------------------------------------------------------------------------

std::string AcceleratorDescription(const KeyboardEvent &accel) {
    std::string label;
    if(accel.controlDown) {
#ifdef __APPLE__
        label += "⌘+";
#else
        label += "Ctrl+";
#endif
    }

    if(accel.shiftDown) {
        label += "Shift+";
    }

    switch(accel.key) {
        case KeyboardEvent::Key::FUNCTION:
            label += ssprintf("F%d", accel.num);
            break;

        case KeyboardEvent::Key::CHARACTER:
            if(accel.chr == '\t') {
                label += "Tab";
            } else if(accel.chr == ' ') {
                label += "Space";
            } else if(accel.chr == '\x1b') {
                label += "Esc";
            } else if(accel.chr == '\x7f') {
                label += "Del";
            } else if(accel.chr != 0) {
                label += toupper((char)(accel.chr & 0xff));
            }
            break;
    }

    return label;
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

void Settings::FreezeBool(const std::string &key, bool value) {
    FreezeInt(key, (int)value);
}

bool Settings::ThawBool(const std::string &key, bool defaultValue) {
    return ThawInt(key, (int)defaultValue) != 0;
}

void Settings::FreezeColor(const std::string &key, RgbaColor value) {
    FreezeInt(key, value.ToPackedInt());
}

RgbaColor Settings::ThawColor(const std::string &key, RgbaColor defaultValue) {
    return RgbaColor::FromPackedInt(ThawInt(key, defaultValue.ToPackedInt()));
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

void FileDialog::AddFilter(const FileFilter &filter) {
    AddFilter(Translate("file-type", filter.name.c_str()), filter.extensions);
}

void FileDialog::AddFilters(const std::vector<FileFilter> &filters) {
    for(auto filter : filters) AddFilter(filter);
}

std::vector<FileFilter> SolveSpaceModelFileFilters = {
    { CN_("file-type", "SolveSpace models"), { "slvs" } },
};

std::vector<FileFilter> RasterFileFilters = {
    { CN_("file-type", "PNG image"), { "png" } },
};

std::vector<FileFilter> MeshFileFilters = {
    { CN_("file-type", "STL mesh"), { "stl" } },
    { CN_("file-type", "Wavefront OBJ mesh"), { "obj" } },
    { CN_("file-type", "Three.js-compatible mesh, with viewer"), { "html" } },
    { CN_("file-type", "Three.js-compatible mesh, mesh only"), { "js" } },
    { CN_("file-type", "Q3D Object file"), { "q3do" } },
    { CN_("file-type", "VRML text file"), { "wrl" } },
};

std::vector<FileFilter> SurfaceFileFilters = {
    { CN_("file-type", "STEP file"), { "step", "stp" } },
};

std::vector<FileFilter> VectorFileFilters = {
    { CN_("file-type", "PDF file"), { "pdf" } },
    { CN_("file-type", "Encapsulated PostScript"), { "eps",  "ps" } },
    { CN_("file-type", "Scalable Vector Graphics"), { "svg" } },
    { CN_("file-type", "STEP file"), { "step", "stp" } },
    { CN_("file-type", "DXF file (AutoCAD 2007)"), { "dxf" } },
    { CN_("file-type", "HPGL file"), { "plt",  "hpgl" } },
    { CN_("file-type", "G Code"), { "ngc",  "txt" } },
};

std::vector<FileFilter> Vector3dFileFilters = {
    { CN_("file-type", "STEP file"), { "step", "stp" } },
    { CN_("file-type", "DXF file (AutoCAD 2007)"), { "dxf" } },
};

std::vector<FileFilter> ImportFileFilters = {
    { CN_("file-type", "AutoCAD DXF and DWG files"), { "dxf", "dwg" } },
};

std::vector<FileFilter> CsvFileFilters = {
    { CN_("file-type", "Comma-separated values"), { "csv" } },
};


}
}
