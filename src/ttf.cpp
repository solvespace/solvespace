//-----------------------------------------------------------------------------
// Routines to read a TrueType font as vector outlines, and generate them
// as entities, since they're always representable as either lines or
// quadratic Bezier curves.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Get the list of available font filenames, and load the name for each of
// them. Only that, though, not the glyphs too.
//-----------------------------------------------------------------------------
void TtfFontList::LoadAll(void) {
    if(loaded) return;

    // Get the list of font files from the platform-specific code.
    LoadAllFontFiles();

    int i;
    for(i = 0; i < l.n; i++) {
        TtfFont *tf = &(l.elem[i]);
        tf->LoadFontFromFile(true);
    }

    loaded = true;
}

void TtfFontList::PlotString(const std::string &font, char *str, double spacing,
                             SBezierList *sbl,
                             Vector origin, Vector u, Vector v)
{
    LoadAll();

    int i;
    for(i = 0; i < l.n; i++) {
        TtfFont *tf = &(l.elem[i]);
        if(tf->FontFileBaseName() == font) {
            tf->LoadFontFromFile(false);
            tf->PlotString(str, spacing, sbl, origin, u, v);
            return;
        }
    }

    // Couldn't find the font; so draw a big X for an error marker.
    SBezier sb;
    sb = SBezier::From(origin, origin.Plus(u).Plus(v));
    sbl->l.Add(&sb);
    sb = SBezier::From(origin.Plus(v), origin.Plus(u));
    sbl->l.Add(&sb);
}


//=============================================================================

//-----------------------------------------------------------------------------
// Get a single character from the open .ttf file; EOF is an error, since
// we can always see that coming.
//-----------------------------------------------------------------------------
int TtfFont::Getc(void) {
    int c = fgetc(fh);
    if(c == EOF) {
        throw "EOF";
    }
    return c;
}

//-----------------------------------------------------------------------------
// Helpers to get 1, 2, or 4 bytes from the .ttf file. Big endian.
// The BYTE, USHORT and ULONG nomenclature comes from the OpenType spec.
//-----------------------------------------------------------------------------
uint8_t TtfFont::GetBYTE(void) {
    return (uint8_t)Getc();
}
uint16_t TtfFont::GetUSHORT(void) {
    uint8_t b0, b1;
    b1 = (uint8_t)Getc();
    b0 = (uint8_t)Getc();

    return (uint16_t)(b1 << 8) | b0;
}
uint32_t TtfFont::GetULONG(void) {
    uint8_t b0, b1, b2, b3;
    b3 = (uint8_t)Getc();
    b2 = (uint8_t)Getc();
    b1 = (uint8_t)Getc();
    b0 = (uint8_t)Getc();

    return
        (uint32_t)(b3 << 24) |
        (uint32_t)(b2 << 16) |
        (uint32_t)(b1 <<  8) |
        b0;
}

//-----------------------------------------------------------------------------
// Load a glyph from the .ttf file into memory. Assumes that the .ttf file
// is already seeked to the correct location, and writes the result to
// glyphs[index]
//-----------------------------------------------------------------------------
void TtfFont::LoadGlyph(int index) {
    if(index < 0 || index >= glyphs) return;

    int i;

    int16_t contours        = (int16_t)GetUSHORT();
    int16_t xMin            = (int16_t)GetUSHORT();
    int16_t yMin            = (int16_t)GetUSHORT();
    int16_t xMax            = (int16_t)GetUSHORT();
    int16_t yMax            = (int16_t)GetUSHORT();

    if(useGlyph[(int)'A'] == index) {
        scale = (1024*1024) / yMax;
    }

    if(contours > 0) {
        uint16_t *endPointsOfContours =
            (uint16_t *)AllocTemporary(contours*sizeof(uint16_t));

        for(i = 0; i < contours; i++) {
            endPointsOfContours[i] = GetUSHORT();
        }
        uint16_t totalPts = endPointsOfContours[i-1] + 1;

        uint16_t instructionLength = GetUSHORT();
        for(i = 0; i < instructionLength; i++) {
            // We can ignore the instructions, since we're doing vector
            // output.
            (void)GetBYTE();
        }

        uint8_t *flags = (uint8_t *)AllocTemporary(totalPts*sizeof(uint8_t));
        int16_t *x     = (int16_t *)AllocTemporary(totalPts*sizeof(int16_t));
        int16_t *y     = (int16_t *)AllocTemporary(totalPts*sizeof(int16_t));

        // Flags, that indicate format of the coordinates
#define FLAG_ON_CURVE           (1 << 0)
#define FLAG_DX_IS_BYTE         (1 << 1)
#define FLAG_DY_IS_BYTE         (1 << 2)
#define FLAG_REPEAT             (1 << 3)
#define FLAG_X_IS_SAME          (1 << 4)
#define FLAG_X_IS_POSITIVE      (1 << 4)
#define FLAG_Y_IS_SAME          (1 << 5)
#define FLAG_Y_IS_POSITIVE      (1 << 5)
        for(i = 0; i < totalPts; i++) {
            flags[i] = GetBYTE();
            if(flags[i] & FLAG_REPEAT) {
                int n = GetBYTE();
                uint8_t f = flags[i];
                int j;
                for(j = 0; j < n; j++) {
                    i++;
                    if(i >= totalPts) {
                        throw "too many points in glyph";
                    }
                    flags[i] = f;
                }
            }
        }

        // x coordinates
        int16_t xa = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DX_IS_BYTE) {
                uint8_t v = GetBYTE();
                if(flags[i] & FLAG_X_IS_POSITIVE) {
                    xa += v;
                } else {
                    xa -= v;
                }
            } else {
                if(flags[i] & FLAG_X_IS_SAME) {
                    // no change
                } else {
                    int16_t d = (int16_t)GetUSHORT();
                    xa += d;
                }
            }
            x[i] = xa;
        }

        // y coordinates
        int16_t ya = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DY_IS_BYTE) {
                uint8_t v = GetBYTE();
                if(flags[i] & FLAG_Y_IS_POSITIVE) {
                    ya += v;
                } else {
                    ya -= v;
                }
            } else {
                if(flags[i] & FLAG_Y_IS_SAME) {
                    // no change
                } else {
                    int16_t d = (int16_t)GetUSHORT();
                    ya += d;
                }
            }
            y[i] = ya;
        }

        Glyph *g = &(glyph[index]);
        g->pt = (FontPoint *)MemAlloc(totalPts*sizeof(FontPoint));
        int contour = 0;
        for(i = 0; i < totalPts; i++) {
            g->pt[i].x = x[i];
            g->pt[i].y = y[i];
            g->pt[i].onCurve = (uint8_t)(flags[i] & FLAG_ON_CURVE);

            if(i == endPointsOfContours[contour]) {
                g->pt[i].lastInContour = true;
                contour++;
            } else {
                g->pt[i].lastInContour = false;
            }
        }
        g->pts = totalPts;
        g->xMax = xMax;
        g->xMin = xMin;

    } else {
        // This is a composite glyph, TODO.
    }
}

//-----------------------------------------------------------------------------
// Return the basename of our font filename; that's how the requests and
// entities that reference us will store it.
//-----------------------------------------------------------------------------
std::string TtfFont::FontFileBaseName(void) {
    std::string baseName = fontFile;
    size_t pos = baseName.rfind(PATH_SEP);
    if(pos != std::string::npos)
        return baseName.erase(0, pos + 1);
    return "";
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory. We care about the curves that define
// the letter shapes, and about the mappings that determine which glyph goes
// with which character.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFontFromFile(bool nameOnly) {
    if(loaded) return true;

    int i;

    fh = ssfopen(fontFile, "rb");
    if(!fh) {
        return false;
    }

    try {
        // First, load the Offset Table
        uint32_t   version         = GetULONG();
        uint16_t   numTables       = GetUSHORT();
        uint16_t   searchRange     = GetUSHORT();
        uint16_t   entrySelector   = GetUSHORT();
        uint16_t   rangeShift      = GetUSHORT();

        // Now load the Table Directory; our goal in doing this will be to
        // find the addresses of the tables that we will need.
        uint32_t   glyfAddr = (uint32_t)-1, glyfLen;
        uint32_t   cmapAddr = (uint32_t)-1, cmapLen;
        uint32_t   headAddr = (uint32_t)-1, headLen;
        uint32_t   locaAddr = (uint32_t)-1, locaLen;
        uint32_t   maxpAddr = (uint32_t)-1, maxpLen;
        uint32_t   nameAddr = (uint32_t)-1, nameLen;
        uint32_t   hmtxAddr = (uint32_t)-1, hmtxLen;
        uint32_t   hheaAddr = (uint32_t)-1, hheaLen;

        for(i = 0; i < numTables; i++) {
            char tag[5] = "xxxx";
            tag[0]              = (char)GetBYTE();
            tag[1]              = (char)GetBYTE();
            tag[2]              = (char)GetBYTE();
            tag[3]              = (char)GetBYTE();
            uint32_t  checksum  = GetULONG();
            uint32_t  offset    = GetULONG();
            uint32_t  length    = GetULONG();

            if(strcmp(tag, "glyf")==0) {
                glyfAddr = offset;
                glyfLen = length;
            } else if(strcmp(tag, "cmap")==0) {
                cmapAddr = offset;
                cmapLen = length;
            } else if(strcmp(tag, "head")==0) {
                headAddr = offset;
                headLen = length;
            } else if(strcmp(tag, "loca")==0) {
                locaAddr = offset;
                locaLen = length;
            } else if(strcmp(tag, "maxp")==0) {
                maxpAddr = offset;
                maxpLen = length;
            } else if(strcmp(tag, "name")==0) {
                nameAddr = offset;
                nameLen = length;
            } else if(strcmp(tag, "hhea")==0) {
                hheaAddr = offset;
                hheaLen = length;
            } else if(strcmp(tag, "hmtx")==0) {
                hmtxAddr = offset;
                hmtxLen = length;
            }
        }

        if(glyfAddr == (uint32_t)-1 ||
           cmapAddr == (uint32_t)-1 ||
           headAddr == (uint32_t)-1 ||
           locaAddr == (uint32_t)-1 ||
           maxpAddr == (uint32_t)-1 ||
           hmtxAddr == (uint32_t)-1 ||
           nameAddr == (uint32_t)-1 ||
           hheaAddr == (uint32_t)-1)
        {
            throw "missing table addr";
        }

        // Load the name table. This gives us display names for the font, which
        // we need when we're giving the user a list to choose from.
        fseek(fh, nameAddr, SEEK_SET);

        uint16_t  nameFormat        = GetUSHORT();
        uint16_t  nameCount         = GetUSHORT();
        uint16_t  nameStringOffset  = GetUSHORT();
        // And now we're at the name records. Go through those till we find
        // one that we want.
        int displayNameOffset = 0, displayNameLength = 0;
        for(i = 0; i < nameCount; i++) {
            uint16_t  platformID    = GetUSHORT();
            uint16_t  encodingID    = GetUSHORT();
            uint16_t  languageID    = GetUSHORT();
            uint16_t  nameId        = GetUSHORT();
            uint16_t  length        = GetUSHORT();
            uint16_t  offset        = GetUSHORT();

            if(nameId == 4) {
                displayNameOffset = offset;
                displayNameLength = length;
                break;
            }
        }
        if(nameOnly && i >= nameCount) {
            throw "no name";
        }

        if(nameOnly) {
            // Find the display name, and store it in the provided buffer.
            fseek(fh, nameAddr+nameStringOffset+displayNameOffset, SEEK_SET);
            int c = 0;
            for(i = 0; i < displayNameLength; i++) {
                uint8_t b = GetBYTE();
                if(b && c < ((int)sizeof(name.str) - 2)) {
                    name.str[c++] = b;
                }
            }
            name.str[c++] = '\0';

            fclose(fh);
            return true;
        }


        // Load the head table; we need this to determine the format of the
        // loca table, 16- or 32-bit entries
        fseek(fh, headAddr, SEEK_SET);

        uint32_t headVersion           = GetULONG();
        uint32_t headFontRevision      = GetULONG();
        uint32_t headCheckSumAdj       = GetULONG();
        uint32_t headMagicNumber       = GetULONG();
        uint16_t headFlags             = GetUSHORT();
        uint16_t headUnitsPerEm        = GetUSHORT();
        (void)GetULONG(); // created time
        (void)GetULONG();
        (void)GetULONG(); // modified time
        (void)GetULONG();
        uint16_t headXmin              = GetUSHORT();
        uint16_t headYmin              = GetUSHORT();
        uint16_t headXmax              = GetUSHORT();
        uint16_t headYmax              = GetUSHORT();
        uint16_t headMacStyle          = GetUSHORT();
        uint16_t headLowestRecPPEM     = GetUSHORT();
        uint16_t headFontDirectionHint = GetUSHORT();
        uint16_t headIndexToLocFormat  = GetUSHORT();
        uint16_t headGlyphDataFormat   = GetUSHORT();

        if(headMagicNumber != 0x5F0F3CF5) {
            throw "bad magic number";
        }

        // Load the hhea table, which contains the number of entries in the
        // horizontal metrics (hmtx) table.
        fseek(fh, hheaAddr, SEEK_SET);
        uint32_t hheaVersion           = GetULONG();
        uint16_t hheaAscender          = GetUSHORT();
        uint16_t hheaDescender         = GetUSHORT();
        uint16_t hheaLineGap           = GetUSHORT();
        uint16_t hheaAdvanceWidthMax   = GetUSHORT();
        uint16_t hheaMinLsb            = GetUSHORT();
        uint16_t hheaMinRsb            = GetUSHORT();
        uint16_t hheaXMaxExtent        = GetUSHORT();
        uint16_t hheaCaretSlopeRise    = GetUSHORT();
        uint16_t hheaCaretSlopeRun     = GetUSHORT();
        uint16_t hheaCaretOffset       = GetUSHORT();
        (void)GetUSHORT();
        (void)GetUSHORT();
        (void)GetUSHORT();
        (void)GetUSHORT();
        uint16_t hheaMetricDataFormat  = GetUSHORT();
        uint16_t hheaNumberOfMetrics   = GetUSHORT();

        // Load the maxp table, which determines (among other things) the number
        // of glyphs in the font
        fseek(fh, maxpAddr, SEEK_SET);

        uint32_t maxpVersion               = GetULONG();
        uint16_t maxpNumGlyphs             = GetUSHORT();
        uint16_t maxpMaxPoints             = GetUSHORT();
        uint16_t maxpMaxContours           = GetUSHORT();
        uint16_t maxpMaxComponentPoints    = GetUSHORT();
        uint16_t maxpMaxComponentContours  = GetUSHORT();
        uint16_t maxpMaxZones              = GetUSHORT();
        uint16_t maxpMaxTwilightPoints     = GetUSHORT();
        uint16_t maxpMaxStorage            = GetUSHORT();
        uint16_t maxpMaxFunctionDefs       = GetUSHORT();
        uint16_t maxpMaxInstructionDefs    = GetUSHORT();
        uint16_t maxpMaxStackElements      = GetUSHORT();
        uint16_t maxpMaxSizeOfInstructions = GetUSHORT();
        uint16_t maxpMaxComponentElements  = GetUSHORT();
        uint16_t maxpMaxComponentDepth     = GetUSHORT();

        glyphs = maxpNumGlyphs;
        glyph = (Glyph *)MemAlloc(glyphs*sizeof(glyph[0]));

        // Load the hmtx table, which gives the horizontal metrics (spacing
        // and advance width) of the font.
        fseek(fh, hmtxAddr, SEEK_SET);

        uint16_t hmtxAdvanceWidth = 0;
        int16_t  hmtxLsb = 0;
        for(i = 0; i < min(glyphs, (int)hheaNumberOfMetrics); i++) {
            hmtxAdvanceWidth = GetUSHORT();
            hmtxLsb          = (int16_t)GetUSHORT();

            glyph[i].leftSideBearing = hmtxLsb;
            glyph[i].advanceWidth = hmtxAdvanceWidth;
        }
        // The last entry in the table applies to all subsequent glyphs also.
        for(; i < glyphs; i++) {
            glyph[i].leftSideBearing = hmtxLsb;
            glyph[i].advanceWidth = hmtxAdvanceWidth;
        }

        // Load the cmap table, which determines the mapping of characters to
        // glyphs.
        fseek(fh, cmapAddr, SEEK_SET);

        uint32_t usedTableAddr = (uint32_t)-1;

        uint16_t cmapVersion    = GetUSHORT();
        uint16_t cmapTableCount = GetUSHORT();
        for(i = 0; i < cmapTableCount; i++) {
            uint16_t platformId = GetUSHORT();
            uint16_t encodingId = GetUSHORT();
            uint32_t offset     = GetULONG();

            if(platformId == 3 && encodingId == 1) {
                // The Windows Unicode mapping is our preference
                usedTableAddr = cmapAddr + offset;
            }
        }

        if(usedTableAddr == (uint32_t)-1) {
            throw "no used table addr";
        }

        // So we can load the desired subtable; in this case, Windows Unicode,
        // which is us.
        fseek(fh, usedTableAddr, SEEK_SET);

        uint16_t mapFormat          = GetUSHORT();
        uint16_t mapLength          = GetUSHORT();
        uint16_t mapVersion         = GetUSHORT();
        uint16_t mapSegCountX2      = GetUSHORT();
        uint16_t mapSearchRange     = GetUSHORT();
        uint16_t mapEntrySelector   = GetUSHORT();
        uint16_t mapRangeShift      = GetUSHORT();

        if(mapFormat != 4) {
            // Required to use format 4 per spec
            throw "not format 4";
        }

        int segCount = mapSegCountX2 / 2;
        uint16_t *endChar       = (uint16_t *)AllocTemporary(segCount*sizeof(uint16_t));
        uint16_t *startChar     = (uint16_t *)AllocTemporary(segCount*sizeof(uint16_t));
        uint16_t *idDelta       = (uint16_t *)AllocTemporary(segCount*sizeof(uint16_t));
        uint16_t *idRangeOffset = (uint16_t *)AllocTemporary(segCount*sizeof(uint16_t));

        uint32_t *filePos = (uint32_t *)AllocTemporary(segCount*sizeof(uint32_t));

        for(i = 0; i < segCount; i++) {
            endChar[i] = GetUSHORT();
        }
        uint16_t mapReservedPad = GetUSHORT();
        for(i = 0; i < segCount; i++) {
            startChar[i] = GetUSHORT();
        }
        for(i = 0; i < segCount; i++) {
            idDelta[i] = GetUSHORT();
        }
        for(i = 0; i < segCount; i++) {
            filePos[i] = (uint32_t)ftell(fh);
            idRangeOffset[i] = GetUSHORT();
        }

        // So first, null out the glyph table in our in-memory representation
        // of the font; any character for which cmap does not provide a glyph
        // corresponds to -1
        for(i = 0; i < (int)arraylen(useGlyph); i++) {
            useGlyph[i] = 0;
        }

        for(i = 0; i < segCount; i++) {
            uint16_t v = idDelta[i];
            if(idRangeOffset[i] == 0) {
                int j;
                for(j = startChar[i]; j <= endChar[i]; j++) {
                    if(j > 0 && j < (int)arraylen(useGlyph)) {
                        // Don't create a reference to a glyph that we won't
                        // store because it's bigger than the table.
                        if((uint16_t)(j + v) < glyphs) {
                            // Arithmetic is modulo 2^16
                            useGlyph[j] = (uint16_t)(j + v);
                        }
                    }
                }
            } else {
                int j;
                for(j = startChar[i]; j <= endChar[i]; j++) {
                    if(j > 0 && j < (int)arraylen(useGlyph)) {
                        int fp = filePos[i];
                        fp += (j - startChar[i])*sizeof(uint16_t);
                        fp += idRangeOffset[i];
                        fseek(fh, fp, SEEK_SET);

                        useGlyph[j] = GetUSHORT();
                    }
                }
            }
        }

        // Load the loca table. This contains the offsets of each glyph,
        // relative to the beginning of the glyf table.
        fseek(fh, locaAddr, SEEK_SET);

        uint32_t *glyphOffsets = (uint32_t *)AllocTemporary(glyphs*sizeof(uint32_t));

        for(i = 0; i < glyphs; i++) {
            if(headIndexToLocFormat == 1) {
                // long offsets, 32 bits
                glyphOffsets[i] = GetULONG();
            } else if(headIndexToLocFormat == 0) {
                // short offsets, 16 bits but divided by 2
                glyphOffsets[i] = GetUSHORT()*2;
            } else {
                throw "bad headIndexToLocFormat";
            }
        }

        scale = 1024;
        // Load the glyf table. This contains the actual representations of the
        // letter forms, as piecewise linear or quadratic outlines.
        for(i = 0; i < glyphs; i++) {
            fseek(fh, glyfAddr + glyphOffsets[i], SEEK_SET);
            LoadGlyph(i);
        }
    } catch (const char *s) {
        dbp("ttf: file %s failed: '%s'", fontFile.c_str(), s);
        fclose(fh);
        return false;
    }

    fclose(fh);
    loaded = true;
    return true;
}

void TtfFont::Flush(void) {
    lastWas = NOTHING;
}

void TtfFont::Handle(int *dx, int x, int y, bool onCurve) {
    x = ((x + *dx)*scale + 512) >> 10;
    y = (y*scale + 512) >> 10;

    if(lastWas == ON_CURVE && onCurve) {
        // This is a line segment.
        LineSegment(lastOnCurve.x, lastOnCurve.y, x, y);
    } else if(lastWas == ON_CURVE && !onCurve) {
        // We can't do the Bezier until we get the next on-curve point,
        // but we must store the off-curve point.
    } else if(lastWas == OFF_CURVE && onCurve) {
        // We are ready to do a Bezier.
        Bezier(lastOnCurve.x, lastOnCurve.y,
               lastOffCurve.x, lastOffCurve.y,
               x, y);
    } else if(lastWas == OFF_CURVE && !onCurve) {
        // Two consecutive off-curve points implicitly have an on-point
        // curve between them, and that should trigger us to generate a
        // Bezier.
        IntPoint fake;
        fake.x = (x + lastOffCurve.x) / 2;
        fake.y = (y + lastOffCurve.y) / 2;
        Bezier(lastOnCurve.x, lastOnCurve.y,
               lastOffCurve.x, lastOffCurve.y,
               fake.x, fake.y);

        lastOnCurve.x = fake.x;
        lastOnCurve.y = fake.y;
    }

    if(onCurve) {
        lastOnCurve.x = x;
        lastOnCurve.y = y;
        lastWas = ON_CURVE;
    } else {
        lastOffCurve.x = x;
        lastOffCurve.y = y;
        lastWas = OFF_CURVE;
    }
}

void TtfFont::PlotCharacter(int *dx, int c, double spacing) {
    int gli = useGlyph[c];

    if(gli < 0 || gli >= glyphs) return;
    Glyph *g = &(glyph[gli]);
    if(!g->pt) return;

    if(c == ' ') {
        *dx += g->advanceWidth;
        return;
    }

    int dx0 = *dx;

    // A point that has x = xMin should be plotted at (dx0 + lsb); fix up
    // our x-position so that the curve-generating code will put stuff
    // at the right place.
    *dx = dx0 - g->xMin;
    *dx += g->leftSideBearing;

    int i;
    int firstInContour = 0;
    for(i = 0; i < g->pts; i++) {
        Handle(dx, g->pt[i].x, g->pt[i].y, g->pt[i].onCurve);

        if(g->pt[i].lastInContour) {
            int f = firstInContour;
            Handle(dx, g->pt[f].x, g->pt[f].y, g->pt[f].onCurve);
            firstInContour = i + 1;
            Flush();
        }
    }

    // And we're done, so advance our position by the requested advance
    // width, plus the user-requested extra advance.
    *dx = dx0 + g->advanceWidth + (int)(spacing + 0.5);
}

void TtfFont::PlotString(char *str, double spacing,
                         SBezierList *sbl,
                         Vector porigin, Vector pu, Vector pv)
{
    beziers = sbl;
    u = pu;
    v = pv;
    origin = porigin;

    if(!loaded || !str || *str == '\0') {
        LineSegment(0, 0, 1024, 0);
        LineSegment(1024, 0, 1024, 1024);
        LineSegment(1024, 1024, 0, 1024);
        LineSegment(0, 1024, 0, 0);
        return;
    }

    int dx = 0;

    while(*str) {
        PlotCharacter(&dx, *str, spacing);
        str++;
    }
}

Vector TtfFont::TransformIntPoint(int x, int y) {
    Vector r = origin;
    r = r.Plus(u.ScaledBy(x / 1024.0));
    r = r.Plus(v.ScaledBy(y / 1024.0));
    return r;
}

void TtfFont::LineSegment(int x0, int y0, int x1, int y1) {
    SBezier sb = SBezier::From(TransformIntPoint(x0, y0),
                               TransformIntPoint(x1, y1));
    beziers->l.Add(&sb);
}

void TtfFont::Bezier(int x0, int y0, int x1, int y1, int x2, int y2) {
    SBezier sb = SBezier::From(TransformIntPoint(x0, y0),
                               TransformIntPoint(x1, y1),
                               TransformIntPoint(x2, y2));
    beziers->l.Add(&sb);
}

