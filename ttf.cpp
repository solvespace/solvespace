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

void TtfFontList::PlotString(char *font, char *str, double spacing,
                             SBezierList *sbl,
                             Vector origin, Vector u, Vector v)
{
    LoadAll();

    int i;
    for(i = 0; i < l.n; i++) {
        TtfFont *tf = &(l.elem[i]);
        if(strcmp(tf->FontFileBaseName(), font)==0) {
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
    if(c < 0) {
        throw "EOF";
    }
    return c;
}

//-----------------------------------------------------------------------------
// Helpers to get 1, 2, or 4 bytes from the .ttf file. Big endian.
//-----------------------------------------------------------------------------
int TtfFont::GetBYTE(void) {
    return Getc();
}
int TtfFont::GetWORD(void) {
    BYTE b0, b1;
    b1 = Getc();
    b0 = Getc();

    return (b1 << 8) | b0;
}
int TtfFont::GetDWORD(void) {
    BYTE b0, b1, b2, b3;
    b3 = Getc();
    b2 = Getc();
    b1 = Getc();
    b0 = Getc();

    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

//-----------------------------------------------------------------------------
// Load a glyph from the .ttf file into memory. Assumes that the .ttf file
// is already seeked to the correct location, and writes the result to
// glyphs[index]
//-----------------------------------------------------------------------------
void TtfFont::LoadGlyph(int index) {
    if(index < 0 || index >= glyphs) return;

    int i;

    SWORD contours          = GetWORD();
    SWORD xMin              = GetWORD();
    SWORD yMin              = GetWORD();
    SWORD xMax              = GetWORD();
    SWORD yMax              = GetWORD();

    if(useGlyph['A'] == index) {
        scale = (1024*1024) / yMax;
    }

    if(contours > 0) {
        WORD *endPointsOfContours =
            (WORD *)AllocTemporary(contours*sizeof(WORD));

        for(i = 0; i < contours; i++) {
            endPointsOfContours[i] = GetWORD();
        }
        WORD totalPts = endPointsOfContours[i-1] + 1;

        WORD instructionLength = GetWORD();
        for(i = 0; i < instructionLength; i++) {
            // We can ignore the instructions, since we're doing vector
            // output.
            (void)GetBYTE();
        }

        BYTE  *flags = (BYTE *)AllocTemporary(totalPts*sizeof(BYTE));
        SWORD *x     = (SWORD *)AllocTemporary(totalPts*sizeof(SWORD));
        SWORD *y     = (SWORD *)AllocTemporary(totalPts*sizeof(SWORD));

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
                BYTE f = flags[i];
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
        SWORD xa = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DX_IS_BYTE) {
                BYTE v = GetBYTE();
                if(flags[i] & FLAG_X_IS_POSITIVE) {
                    xa += v;
                } else {
                    xa -= v;
                }
            } else {
                if(flags[i] & FLAG_X_IS_SAME) {
                    // no change
                } else {
                    SWORD d = GetWORD();
                    xa += d;
                }
            }
            x[i] = xa;
        }

        // y coordinates
        SWORD ya = 0;
        for(i = 0; i < totalPts; i++) {
            if(flags[i] & FLAG_DY_IS_BYTE) {
                BYTE v = GetBYTE();
                if(flags[i] & FLAG_Y_IS_POSITIVE) {
                    ya += v;
                } else {
                    ya -= v;
                }
            } else {
                if(flags[i] & FLAG_Y_IS_SAME) {
                    // no change
                } else {
                    SWORD d = GetWORD();
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
            g->pt[i].onCurve = (BYTE)(flags[i] & FLAG_ON_CURVE);

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
char *TtfFont::FontFileBaseName(void) {
    char *sb = strrchr(fontFile, '\\');
    char *sf = strrchr(fontFile, '/');
    char *s = sf ? sf : sb;
    if(!s) return "";
    return s + 1;
}

//-----------------------------------------------------------------------------
// Load a TrueType font into memory. We care about the curves that define
// the letter shapes, and about the mappings that determine which glyph goes
// with which character.
//-----------------------------------------------------------------------------
bool TtfFont::LoadFontFromFile(bool nameOnly) {
    if(loaded) return true;

    int i;
    
    fh = fopen(fontFile, "rb");
    if(!fh) {
        return false;
    }

    try {
        // First, load the Offset Table
        DWORD   version         = GetDWORD();
        WORD    numTables       = GetWORD();
        WORD    searchRange     = GetWORD();
        WORD    entrySelector   = GetWORD();
        WORD    rangeShift      = GetWORD();

        // Now load the Table Directory; our goal in doing this will be to
        // find the addresses of the tables that we will need.
        DWORD   glyfAddr = -1, glyfLen;
        DWORD   cmapAddr = -1, cmapLen;
        DWORD   headAddr = -1, headLen;
        DWORD   locaAddr = -1, locaLen;
        DWORD   maxpAddr = -1, maxpLen;
        DWORD   nameAddr = -1, nameLen;
        DWORD   hmtxAddr = -1, hmtxLen;
        DWORD   hheaAddr = -1, hheaLen;

        for(i = 0; i < numTables; i++) {
            char tag[5] = "xxxx";
            tag[0]              = GetBYTE();
            tag[1]              = GetBYTE();
            tag[2]              = GetBYTE();
            tag[3]              = GetBYTE();
            DWORD   checksum    = GetDWORD();
            DWORD   offset      = GetDWORD();
            DWORD   length      = GetDWORD();

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

        if(glyfAddr == -1 || cmapAddr == -1 || headAddr == -1 ||
           locaAddr == -1 || maxpAddr == -1 || hmtxAddr == -1 ||
           nameAddr == -1 || hheaAddr == -1)
        {
            throw "missing table addr";
        }

        // Load the name table. This gives us display names for the font, which
        // we need when we're giving the user a list to choose from.
        fseek(fh, nameAddr, SEEK_SET);

        WORD  nameFormat            = GetWORD();
        WORD  nameCount             = GetWORD();
        WORD  nameStringOffset      = GetWORD();
        // And now we're at the name records. Go through those till we find
        // one that we want.
        int displayNameOffset, displayNameLength;
        for(i = 0; i < nameCount; i++) {
            WORD    platformID      = GetWORD();
            WORD    encodingID      = GetWORD();
            WORD    languageID      = GetWORD();
            WORD    nameId          = GetWORD();
            WORD    length          = GetWORD();
            WORD    offset          = GetWORD();

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
                BYTE b = GetBYTE();
                if(b && c < (sizeof(name.str) - 2)) {
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

        DWORD headVersion           = GetDWORD();
        DWORD headFontRevision      = GetDWORD();
        DWORD headCheckSumAdj       = GetDWORD();
        DWORD headMagicNumber       = GetDWORD();
        WORD  headFlags             = GetWORD();
        WORD  headUnitsPerEm        = GetWORD();
        (void)GetDWORD(); // created time
        (void)GetDWORD();
        (void)GetDWORD(); // modified time
        (void)GetDWORD();
        WORD  headXmin              = GetWORD();
        WORD  headYmin              = GetWORD();
        WORD  headXmax              = GetWORD();
        WORD  headYmax              = GetWORD();
        WORD  headMacStyle          = GetWORD();
        WORD  headLowestRecPPEM     = GetWORD();
        WORD  headFontDirectionHint = GetWORD();
        WORD  headIndexToLocFormat  = GetWORD();
        WORD  headGlyphDataFormat   = GetWORD();
        
        if(headMagicNumber != 0x5F0F3CF5) {
            throw "bad magic number";
        }

        // Load the hhea table, which contains the number of entries in the
        // horizontal metrics (hmtx) table.
        fseek(fh, hheaAddr, SEEK_SET);
        DWORD hheaVersion           = GetDWORD();
        WORD  hheaAscender          = GetWORD();
        WORD  hheaDescender         = GetWORD();
        WORD  hheaLineGap           = GetWORD();
        WORD  hheaAdvanceWidthMax   = GetWORD();
        WORD  hheaMinLsb            = GetWORD();
        WORD  hheaMinRsb            = GetWORD();
        WORD  hheaXMaxExtent        = GetWORD();
        WORD  hheaCaretSlopeRise    = GetWORD();
        WORD  hheaCaretSlopeRun     = GetWORD();
        WORD  hheaCaretOffset       = GetWORD();
        (void)GetWORD();
        (void)GetWORD();
        (void)GetWORD();
        (void)GetWORD();
        WORD  hheaMetricDataFormat  = GetWORD();
        WORD  hheaNumberOfMetrics   = GetWORD();

        // Load the maxp table, which determines (among other things) the number
        // of glyphs in the font
        fseek(fh, maxpAddr, SEEK_SET);

        DWORD maxpVersion               = GetDWORD();
        WORD  maxpNumGlyphs             = GetWORD();
        WORD  maxpMaxPoints             = GetWORD();
        WORD  maxpMaxContours           = GetWORD();
        WORD  maxpMaxComponentPoints    = GetWORD();
        WORD  maxpMaxComponentContours  = GetWORD();
        WORD  maxpMaxZones              = GetWORD();
        WORD  maxpMaxTwilightPoints     = GetWORD();
        WORD  maxpMaxStorage            = GetWORD();
        WORD  maxpMaxFunctionDefs       = GetWORD();
        WORD  maxpMaxInstructionDefs    = GetWORD();
        WORD  maxpMaxStackElements      = GetWORD();
        WORD  maxpMaxSizeOfInstructions = GetWORD();
        WORD  maxpMaxComponentElements  = GetWORD();
        WORD  maxpMaxComponentDepth     = GetWORD();

        glyphs = maxpNumGlyphs;
        glyph = (Glyph *)MemAlloc(glyphs*sizeof(glyph[0]));

        // Load the hmtx table, which gives the horizontal metrics (spacing
        // and advance width) of the font.
        fseek(fh, hmtxAddr, SEEK_SET);

        WORD  hmtxAdvanceWidth;
        SWORD hmtxLsb;
        for(i = 0; i < min(glyphs, hheaNumberOfMetrics); i++) {
            hmtxAdvanceWidth = GetWORD();
            hmtxLsb          = (SWORD)GetWORD();

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

        DWORD usedTableAddr = -1;

        WORD  cmapVersion        = GetWORD();
        WORD  cmapTableCount     = GetWORD();
        for(i = 0; i < cmapTableCount; i++) {
            WORD  platformId = GetWORD();
            WORD  encodingId = GetWORD();
            DWORD offset     = GetDWORD();

            if(platformId == 3 && encodingId == 1) {
                // The Windows Unicode mapping is our preference
                usedTableAddr = cmapAddr + offset;
            }
        }

        if(usedTableAddr == -1) {
            throw "no used table addr";
        }

        // So we can load the desired subtable; in this case, Windows Unicode,
        // which is us.
        fseek(fh, usedTableAddr, SEEK_SET);

        WORD  mapFormat             = GetWORD();
        WORD  mapLength             = GetWORD();
        WORD  mapVersion            = GetWORD();
        WORD  mapSegCountX2         = GetWORD();
        WORD  mapSearchRange        = GetWORD();
        WORD  mapEntrySelector      = GetWORD();
        WORD  mapRangeShift         = GetWORD();
        
        if(mapFormat != 4) {
            // Required to use format 4 per spec
            throw "not format 4";
        }

        int segCount = mapSegCountX2 / 2;
        WORD *endChar       = (WORD *)AllocTemporary(segCount*sizeof(WORD));
        WORD *startChar     = (WORD *)AllocTemporary(segCount*sizeof(WORD));
        WORD *idDelta       = (WORD *)AllocTemporary(segCount*sizeof(WORD));
        WORD *idRangeOffset = (WORD *)AllocTemporary(segCount*sizeof(WORD));

        DWORD *filePos = (DWORD *)AllocTemporary(segCount*sizeof(DWORD));

        for(i = 0; i < segCount; i++) {
            endChar[i] = GetWORD();
        }
        WORD  mapReservedPad        = GetWORD();
        for(i = 0; i < segCount; i++) {
            startChar[i] = GetWORD();
        }
        for(i = 0; i < segCount; i++) {
            idDelta[i] = GetWORD();
        }
        for(i = 0; i < segCount; i++) {
            filePos[i] = ftell(fh);
            idRangeOffset[i] = GetWORD();
        }

        // So first, null out the glyph table in our in-memory representation
        // of the font; any character for which cmap does not provide a glyph
        // corresponds to -1
        for(i = 0; i < arraylen(useGlyph); i++) {
            useGlyph[i] = 0;
        }

        for(i = 0; i < segCount; i++) {
            WORD v = idDelta[i];
            if(idRangeOffset[i] == 0) {
                int j;
                for(j = startChar[i]; j <= endChar[i]; j++) {
                    if(j > 0 && j < arraylen(useGlyph)) {
                        // Don't create a reference to a glyph that we won't
                        // store because it's bigger than the table.
                        if((WORD)(j + v) < glyphs) {
                            // Arithmetic is modulo 2^16
                            useGlyph[j] = (WORD)(j + v);
                        }
                    }
                }
            } else {
                int j;
                for(j = startChar[i]; j <= endChar[i]; j++) {
                    if(j > 0 && j < arraylen(useGlyph)) {
                        int fp = filePos[i];
                        fp += (j - startChar[i])*sizeof(WORD);
                        fp += idRangeOffset[i];
                        fseek(fh, fp, SEEK_SET);

                        useGlyph[j] = GetWORD();
                    }
                }
            }
        }

        // Load the loca table. This contains the offsets of each glyph,
        // relative to the beginning of the glyf table.
        fseek(fh, locaAddr, SEEK_SET);

        DWORD *glyphOffsets = (DWORD *)AllocTemporary(glyphs*sizeof(DWORD));

        for(i = 0; i < glyphs; i++) {
            if(headIndexToLocFormat == 1) {
                // long offsets, 32 bits
                glyphOffsets[i] = GetDWORD();
            } else if(headIndexToLocFormat == 0) {
                // short offsets, 16 bits but divided by 2
                glyphOffsets[i] = GetWORD()*2;
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
    } catch (char *s) {
        dbp("failed: '%s'", s);
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

