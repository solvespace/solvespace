//-----------------------------------------------------------------------------
// The file format-specific stuff for all of the 2d vector output formats.
//-----------------------------------------------------------------------------
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Routines for DXF export
//-----------------------------------------------------------------------------
void DxfFileWriter::StartFile(void) {
    // Some software, like Adobe Illustrator, insists on a header.
    fprintf(f,
"  999\r\n"
"file created by SolveSpace\r\n"
"  0\r\n"
"SECTION\r\n"
"  2\r\n"
"HEADER\r\n"
"  9\r\n"
"$ACADVER\r\n"
"  1\r\n"
"AC1006\r\n"
"  9\r\n"
"$ANGDIR\r\n"
"  70\r\n"
"0\r\n"
"  9\r\n"
"$AUNITS\r\n"
"  70\r\n"
"0\r\n"
"  9\r\n"
"$AUPREC\r\n"
"  70\r\n"
"0\r\n"
"  9\r\n"
"$INSBASE\r\n"
"  10\r\n"
"0.0\r\n"
"  20\r\n"
"0.0\r\n"
"  30\r\n"
"0.0\r\n"
"  9\r\n"
"$EXTMIN\r\n"
"  10\r\n"
"0.0\r\n"
"  20\r\n"
"0.0\r\n"
"  9\r\n"
"$EXTMAX\r\n"
"  10\r\n"
"10000.0\r\n"
"  20\r\n"
"10000.0\r\n"
"  0\r\n"
"ENDSEC\r\n");

    // Then start the entities.
    fprintf(f,
"  0\r\n"
"SECTION\r\n"
"  2\r\n"
"ENTITIES\r\n");
}

void DxfFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    fprintf(f,
"  0\r\n"
"LINE\r\n"
"  8\r\n"     // Layer code
"%d\r\n"
"  10\r\n"    // xA
"%.6f\r\n"
"  20\r\n"    // yA
"%.6f\r\n"
"  30\r\n"    // zA
"%.6f\r\n"
"  11\r\n"    // xB
"%.6f\r\n"
"  21\r\n"    // yB
"%.6f\r\n"
"  31\r\n"    // zB
"%.6f\r\n",
                    0,
                    x0, y0, 0.0,
                    x1, y1, 0.0);
}

void DxfFileWriter::Triangle(STriangle *tr) {
}

void DxfFileWriter::Bezier(SBezier *sb) {
    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(sb->IsCircle(n, &c, &r)) {
        double theta0 = atan2(sb->ctrl[0].y - c.y, sb->ctrl[0].x - c.x),
               theta1 = atan2(sb->ctrl[2].y - c.y, sb->ctrl[2].x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        if(dtheta < 0) {
            SWAP(double, theta0, theta1);
        }

        fprintf(f,
"  0\r\n"
"ARC\r\n"
"  8\r\n"     // Layer code
"%d\r\n"
"  10\r\n"    // x
"%.6f\r\n"
"  20\r\n"    // y
"%.6f\r\n"
"  30\r\n"    // z
"%.6f\r\n"
"  40\r\n"    // radius
"%.6f\r\n"
"  50\r\n"    // start angle
"%.6f\r\n"
"  51\r\n"    // end angle
"%.6f\r\n",
                        0,
                        c.x, c.y, 0.0,
                        r,
                        theta0*180/PI, theta1*180/PI);
    } else {
        BezierAsPwl(sb);
    }
}

void DxfFileWriter::FinishAndCloseFile(void) {
    fprintf(f,
"  0\r\n"
"ENDSEC\r\n"
"  0\r\n"
"EOF\r\n" );
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for EPS output
//-----------------------------------------------------------------------------
void EpsFileWriter::StartFile(void) {
    fprintf(f,
"%%!PS-Adobe-2.0\r\n"
"%%%%Creator: SolveSpace\r\n"
"%%%%Title: title\r\n"
"%%%%Pages: 0\r\n"
"%%%%PageOrder: Ascend\r\n"
"%%%%BoundingBox: 0 0 %d %d\r\n"
"%%%%HiResBoundingBox: 0 0 %.3f %.3f\r\n"
"%%%%EndComments\r\n"
"\r\n"
"gsave\r\n"
"\r\n",
            (int)ceil(MmToPts(ptMax.x - ptMin.x)),
            (int)ceil(MmToPts(ptMax.y - ptMin.y)),
            MmToPts(ptMax.x - ptMin.x),
            MmToPts(ptMax.y - ptMin.y));
}

void EpsFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    fprintf(f,
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f lineto\r\n"
"    1 setlinewidth\r\n"
"    0 setgray\r\n"
"stroke\r\n",
            MmToPts(x0 - ptMin.x), MmToPts(y0 - ptMin.y),
            MmToPts(x1 - ptMin.x), MmToPts(y1 - ptMin.y));
}

void EpsFileWriter::Triangle(STriangle *tr) {
    fprintf(f,
"%.3f %.3f %.3f setrgbcolor\r\n"
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f lineto\r\n"
"    %.3f %.3f lineto\r\n"
"    closepath\r\n"
"fill\r\n",
            REDf(tr->meta.color), GREENf(tr->meta.color), BLUEf(tr->meta.color),
            MmToPts(tr->a.x - ptMin.x), MmToPts(tr->a.y - ptMin.y),
            MmToPts(tr->b.x - ptMin.x), MmToPts(tr->b.y - ptMin.y),
            MmToPts(tr->c.x - ptMin.x), MmToPts(tr->c.y - ptMin.y));

    // same issue with cracks, stroke it to avoid them
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;
    fprintf(f,
"%.3f %.3f %.3f setrgbcolor\r\n"
"%.3f setlinewidth\r\n"
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f lineto\r\n"
"    %.3f %.3f lineto\r\n"
"    closepath\r\n"
"stroke\r\n",
            REDf(tr->meta.color), GREENf(tr->meta.color), BLUEf(tr->meta.color),
            MmToPts(sw),
            MmToPts(tr->a.x - ptMin.x), MmToPts(tr->a.y - ptMin.y),
            MmToPts(tr->b.x - ptMin.x), MmToPts(tr->b.y - ptMin.y),
            MmToPts(tr->c.x - ptMin.x), MmToPts(tr->c.y - ptMin.y));
}

void EpsFileWriter::Bezier(SBezier *sb) {
    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(sb->IsCircle(n, &c, &r)) {
        Vector p0 = sb->ctrl[0], p1 = sb->ctrl[2];
        double theta0 = atan2(p0.y - c.y, p0.x - c.x),
               theta1 = atan2(p1.y - c.y, p1.x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        if(dtheta < 0) {
            SWAP(double, theta0, theta1);
            SWAP(Vector, p0, p1);
        }
        fprintf(f,
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f %.3f %.3f %.3f arc\r\n"
"    1 setlinewidth\r\n"
"    0 setgray\r\n"
"stroke\r\n",
            MmToPts(p0.x - ptMin.x), MmToPts(p0.y - ptMin.y),
            MmToPts(c.x - ptMin.x),  MmToPts(c.y - ptMin.y),
            MmToPts(r),
            theta0*180/PI, theta1*180/PI);
    } else if(sb->deg == 3 && !sb->IsRational()) {
        fprintf(f,
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f %.3f %.3f %.3f %.3f curveto\r\n"
"    1 setlinewidth\r\n"
"    0 setgray\r\n"
"stroke\r\n",
            MmToPts(sb->ctrl[0].x - ptMin.x), MmToPts(sb->ctrl[0].y - ptMin.y),
            MmToPts(sb->ctrl[1].x - ptMin.x), MmToPts(sb->ctrl[1].y - ptMin.y),
            MmToPts(sb->ctrl[2].x - ptMin.x), MmToPts(sb->ctrl[2].y - ptMin.y),
            MmToPts(sb->ctrl[3].x - ptMin.x), MmToPts(sb->ctrl[3].y - ptMin.y));
    } else {
        BezierAsNonrationalCubic(sb);
    }
}

void EpsFileWriter::FinishAndCloseFile(void) {
    fprintf(f,
"\r\n"
"grestore\r\n"
"\r\n");
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for PDF output, some extra complexity because we have to generate
// a correct xref table.
//-----------------------------------------------------------------------------
void PdfFileWriter::StartFile(void) {
    fprintf(f,
"%%PDF-1.1\r\n"
"%%%c%c%c%c\r\n",
        0xe2, 0xe3, 0xcf, 0xd3);
    
    xref[1] = ftell(f);
    fprintf(f,
"1 0 obj\r\n"
"  << /Type /Catalog\r\n"
"     /Outlines 2 0 R\r\n"
"     /Pages 3 0 R\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[2] = ftell(f);
    fprintf(f,
"2 0 obj\r\n"
"  << /Type /Outlines\r\n"
"     /Count 0\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[3] = ftell(f);
    fprintf(f,
"3 0 obj\r\n"
"  << /Type /Pages\r\n"
"     /Kids [4 0 R]\r\n"
"     /Count 1\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[4] = ftell(f);
    fprintf(f,
"4 0 obj\r\n"
"  << /Type /Page\r\n"
"     /Parent 3 0 R\r\n"
"     /MediaBox [0 0 %.3f %.3f]\r\n"
"     /Contents 5 0 R\r\n"
"     /Resources << /ProcSet 7 0 R\r\n"
"                   /Font << /F1 8 0 R >>\r\n"
"                >>\r\n"
"  >>\r\n"
"endobj\r\n",
            MmToPts(ptMax.x - ptMin.x),
            MmToPts(ptMax.y - ptMin.y));

    xref[5] = ftell(f);
    fprintf(f,
"5 0 obj\r\n"
"  << /Length 6 0 R >>\r\n"
"stream\r\n");
    bodyStart = ftell(f);
}

void PdfFileWriter::FinishAndCloseFile(void) {
    DWORD bodyEnd = ftell(f);

    fprintf(f,
"endstream\r\n"
"endobj\r\n");

    xref[6] = ftell(f);
    fprintf(f,
"6 0 obj\r\n"
"  %d\r\n"
"endobj\r\n",
        bodyEnd - bodyStart);

    xref[7] = ftell(f);
    fprintf(f,
"7 0 obj\r\n"
"  [/PDF /Text]\r\n"
"endobj\r\n");

    xref[8] = ftell(f);
    fprintf(f,
"8 0 obj\r\n"
"  << /Type /Font\r\n"
"     /Subtype /Type1\r\n"
"     /Name /F1\r\n"
"     /BaseFont /Helvetica\r\n"
"     /Encoding /WinAnsiEncoding\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[9] = ftell(f);
    fprintf(f,
"9 0 obj\r\n"
"  << /Creator (SolveSpace)\r\n"
"  >>\r\n");
    
    DWORD xrefStart = ftell(f);
    fprintf(f,
"xref\r\n"
"0 10\r\n"
"0000000000 65535 f\r\n");
   
    int i;
    for(i = 1; i <= 9; i++) {
        fprintf(f, "%010d %05d n\r\n", xref[i], 0);
    }

    fprintf(f,
"\r\n"
"trailer\r\n"
"  << /Size 10\r\n"
"     /Root 1 0 R\r\n"
"     /Info 9 0 R\r\n"
"  >>\r\n"
"startxref\r\n"
"%d\r\n"
"%%%%EOF\r\n",
        xrefStart);

    fclose(f);

}

void PdfFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    fprintf(f,
"1 w 0 0 0 RG\r\n"
"%.3f %.3f m\r\n"
"%.3f %.3f l\r\n"
"S\r\n",
            MmToPts(x0 - ptMin.x), MmToPts(y0 - ptMin.y),
            MmToPts(x1 - ptMin.x), MmToPts(y1 - ptMin.y));
}

void PdfFileWriter::Triangle(STriangle *tr) {
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;

    fprintf(f,
"%.3f %.3f %.3f RG\r\n"
"%.3f %.3f %.3f rg\r\n"
"%.3f w\r\n"
"%.3f %.3f m\r\n"
"%.3f %.3f l\r\n"
"%.3f %.3f l\r\n"
"b\r\n",
            REDf(tr->meta.color), GREENf(tr->meta.color), BLUEf(tr->meta.color),
            REDf(tr->meta.color), GREENf(tr->meta.color), BLUEf(tr->meta.color),
            MmToPts(sw),
            MmToPts(tr->a.x - ptMin.x), MmToPts(tr->a.y - ptMin.y),
            MmToPts(tr->b.x - ptMin.x), MmToPts(tr->b.y - ptMin.y),
            MmToPts(tr->c.x - ptMin.x), MmToPts(tr->c.y - ptMin.y));
}

void PdfFileWriter::Bezier(SBezier *sb) {
    if(sb->deg == 3 && !sb->IsRational()) {
        fprintf(f,
"1 w 0 0 0 RG\r\n"
"%.3f %.3f m\r\n"
"%.3f %.3f %.3f %.3f %.3f %.3f c\r\n"
"S\r\n",
            MmToPts(sb->ctrl[0].x - ptMin.x), MmToPts(sb->ctrl[0].y - ptMin.y),
            MmToPts(sb->ctrl[1].x - ptMin.x), MmToPts(sb->ctrl[1].y - ptMin.y),
            MmToPts(sb->ctrl[2].x - ptMin.x), MmToPts(sb->ctrl[2].y - ptMin.y),
            MmToPts(sb->ctrl[3].x - ptMin.x), MmToPts(sb->ctrl[3].y - ptMin.y));
    } else {
        BezierAsNonrationalCubic(sb);
    }
}


//-----------------------------------------------------------------------------
// Routines for SVG output
//-----------------------------------------------------------------------------

const char *SvgFileWriter::SVG_STYLE =
    "stroke-width='0.1' stroke='black' style='fill: none;'";

void SvgFileWriter::StartFile(void) {
    fprintf(f,
"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" "
    "\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\r\n"
"<svg xmlns=\"http://www.w3.org/2000/svg\"  "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
    "width='%.3fmm' height='%.3fmm' "
    "viewBox=\"0 0 %.3f %.3f\">\r\n"
"\r\n"
"<title>Exported SVG</title>\r\n"
"\r\n",
        (ptMax.x - ptMin.x) + 1, (ptMax.y - ptMin.y) + 1,
        (ptMax.x - ptMin.x) + 1, (ptMax.y - ptMin.y) + 1);
    // A little bit of extra space for the stroke width.
}

void SvgFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    // SVG uses a coordinate system with the origin at top left, +y down
    fprintf(f,
"<polyline points='%.3f,%.3f %.3f,%.3f' %s />\r\n",
            (x0 - ptMin.x), (ptMax.y - y0),
            (x1 - ptMin.x), (ptMax.y - y1),
            SVG_STYLE);
}

void SvgFileWriter::Triangle(STriangle *tr) {
    // crispEdges turns of anti-aliasing, which tends to cause hairline
    // cracks between triangles; but there still is some cracking, so
    // specify a stroke width too, hope for around a pixel
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;
    fprintf(f,
"<polygon points='%.3f,%.3f %.3f,%.3f %.3f,%.3f' "
    "stroke='#%02x%02x%02x' stroke-width='%.3f' "
    "style='fill:#%02x%02x%02x' shape-rendering='crispEdges'/>\r\n",
            (tr->a.x - ptMin.x), (ptMax.y - tr->a.y),
            (tr->b.x - ptMin.x), (ptMax.y - tr->b.y),
            (tr->c.x - ptMin.x), (ptMax.y - tr->c.y),
            RED(tr->meta.color), GREEN(tr->meta.color), BLUE(tr->meta.color),
            sw,
            RED(tr->meta.color), GREEN(tr->meta.color), BLUE(tr->meta.color));
}

void SvgFileWriter::Bezier(SBezier *sb) {
    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(sb->IsCircle(n, &c, &r)) {
        Vector p0 = sb->ctrl[0], p1 = sb->ctrl[2];
        double theta0 = atan2(p0.y - c.y, p0.x - c.x),
               theta1 = atan2(p1.y - c.y, p1.x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        // The arc must be less than 180 degrees, or else it couldn't have
        // been represented as a single rational Bezier. And arrange it
        // to run counter-clockwise, which corresponds to clockwise in
        // SVG's mirrored coordinate system.
        if(dtheta < 0) {
            SWAP(Vector, p0, p1);
        }
        fprintf(f, 
"<path d='M%.3f,%.3f "
         "A%.3f,%.3f 0 0,0 %.3f,%.3f' %s />\r\n",
                p0.x - ptMin.x, ptMax.y - p0.y,
                r, r,
                p1.x - ptMin.x, ptMax.y - p1.y,
                SVG_STYLE);
    } else if(!sb->IsRational()) {
        if(sb->deg == 1) {
            LineSegment(sb->ctrl[0].x, sb->ctrl[0].y,
                        sb->ctrl[1].x, sb->ctrl[1].y);
        } else if(sb->deg == 2) {
            fprintf(f,
"<path d='M%.3f,%.3f "
         "Q%.3f,%.3f %.3f,%.3f' %s />\r\n",
                sb->ctrl[0].x - ptMin.x, ptMax.y - sb->ctrl[0].y,
                sb->ctrl[1].x - ptMin.x, ptMax.y - sb->ctrl[1].y,
                sb->ctrl[2].x - ptMin.x, ptMax.y - sb->ctrl[2].y,
                SVG_STYLE);
        } else if(sb->deg == 3) {
            fprintf(f,
"<path d='M%.3f,%.3f "
         "C%.3f,%.3f %.3f,%.3f %.3f,%.3f' %s />\r\n",
                sb->ctrl[0].x - ptMin.x, ptMax.y - sb->ctrl[0].y,
                sb->ctrl[1].x - ptMin.x, ptMax.y - sb->ctrl[1].y,
                sb->ctrl[2].x - ptMin.x, ptMax.y - sb->ctrl[2].y,
                sb->ctrl[3].x - ptMin.x, ptMax.y - sb->ctrl[3].y,
                SVG_STYLE);
        }
    } else {
        BezierAsNonrationalCubic(sb);

    }
}

void SvgFileWriter::FinishAndCloseFile(void) {
    fprintf(f, "\r\n</svg>\r\n");
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for HPGL output
//-----------------------------------------------------------------------------
double HpglFileWriter::MmToHpglUnits(double mm) {
    return mm*40;
}

void HpglFileWriter::StartFile(void) {
    fprintf(f, "IN;\r\n");
    fprintf(f, "SP1;\r\n");
}

void HpglFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    fprintf(f, "PU%d,%d;\r\n", (int)MmToHpglUnits(x0), (int)MmToHpglUnits(y0));
    fprintf(f, "PD%d,%d;\r\n", (int)MmToHpglUnits(x1), (int)MmToHpglUnits(y1));
}

void HpglFileWriter::Triangle(STriangle *tr) {
}

void HpglFileWriter::Bezier(SBezier *sb) {
    BezierAsPwl(sb);
}

void HpglFileWriter::FinishAndCloseFile(void) {
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routine for STEP output; just a wrapper around the general STEP stuff that
// can also be used for surfaces or 3d curves.
//-----------------------------------------------------------------------------
void Step2dFileWriter::StartFile(void) {
    ZERO(&sfw);
    sfw.f = f;
    sfw.WriteHeader();
}

void Step2dFileWriter::Triangle(STriangle *tr) {
}

void Step2dFileWriter::LineSegment(double x0, double y0, double x1, double y1) {
    SBezier sb = SBezier::From(Vector::From(x0, y0, 0),
                               Vector::From(x1, y1, 0));
    Bezier(&sb);
}

void Step2dFileWriter::Bezier(SBezier *sb) {
    int c = sfw.ExportCurve(sb);
    sfw.curves.Add(&c);
}

void Step2dFileWriter::FinishAndCloseFile(void) {
    sfw.WriteWireframe();
    sfw.WriteFooter();
    fclose(f);
}

