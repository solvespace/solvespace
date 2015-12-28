//-----------------------------------------------------------------------------
// The file format-specific stuff for all of the 2d vector output formats.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <libdxfrw.h>
#include "solvespace.h"

void VectorFileWriter::Dummy(void) {
    // This out-of-line virtual method definition quells the following warning
    // from Clang++: "'VectorFileWriter' has no out-of-line virtual method
    // definitions; its vtable will be emitted in every translation unit
    // [-Wweak-vtables]"
}

//-----------------------------------------------------------------------------
// Routines for DXF export
//-----------------------------------------------------------------------------
class DxfWriteInterface : public DRW_Interface {
    DxfFileWriter *writer;
    dxfRW *dxf;
    int currentColor;
    double currentWidth;

public:
    DxfWriteInterface(DxfFileWriter *w, dxfRW *dxfrw) :
        writer(w), dxf(dxfrw) {}

    virtual void writeEntities() {
        for(DxfFileWriter::BezierPath &path : writer->paths) {
            currentColor = path.color;
            currentWidth = path.width;
            for(SBezier *sb : path.beziers) {
                writeBezier(sb);
            }
        }
    }

    void assignEntityDefaults(DRW_Entity *entity) {
        entity->color24 = currentColor;
        if(currentWidth > 0.0) entity->setWidthMm(currentWidth);
    }

    void writeLine(const Vector &p0, const Vector &p1) {
        DRW_Line line;
        assignEntityDefaults(&line);
        line.basePoint = DRW_Coord(p0.x, p0.y, 0.0);
        line.secPoint = DRW_Coord(p1.x, p1.y, 0.0);
        dxf->writeLine(&line);
    }

    void writeArc(const Vector &c, double r, double sa, double ea) {
        DRW_Arc arc;
        assignEntityDefaults(&arc);
        arc.radious = r;
        arc.basePoint = DRW_Coord(c.x, c.y, 0.0);
        arc.staangle = sa;
        arc.endangle = ea;
        dxf->writeArc(&arc);
    }

    void writeBezierAsPwl(SBezier *sb) {
        List<Vector> lv = {};
        sb->MakePwlInto(&lv, SS.ExportChordTolMm());
        DRW_LWPolyline polyline;
        for(int i = 0; i < lv.n; i++) {
            Vector *v = &lv.elem[i];
            DRW_Vertex2D *vertex = new DRW_Vertex2D();
            vertex->x = v->x;
            vertex->y = v->y;
            polyline.vertlist.push_back(vertex);
        }
        dxf->writeLWPolyline(&polyline);
        lv.Clear();
    }

    void makeKnotsFor(DRW_Spline *spline) {
        // QCad/LibreCAD require this for some reason.
        if(spline->degree == 3) {
            spline->nknots = 8;
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(1.0);
            spline->knotslist.push_back(1.0);
            spline->knotslist.push_back(1.0);
            spline->knotslist.push_back(1.0);
        } else if(spline->degree == 2) {
            spline->nknots = 6;
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(0.0);
            spline->knotslist.push_back(1.0);
            spline->knotslist.push_back(1.0);
            spline->knotslist.push_back(1.0);
        } else {
            oops();
        }
    }

    void writeSpline(SBezier *sb) {
        bool isRational = sb->IsRational();
        DRW_Spline spline;
        assignEntityDefaults(&spline);
        spline.flags = (isRational) ? 0x04 : 0x08;
        spline.degree = sb->deg;
        spline.ncontrol = sb->deg + 1;
        makeKnotsFor(&spline);
        for(int i = 0; i <= sb->deg; i++) {
            spline.controllist.push_back(new DRW_Coord(sb->ctrl[i].x, sb->ctrl[i].y, 0.0));
            if(isRational) spline.weightlist.push_back(sb->weight[i]);
        }
        dxf->writeSpline(&spline);
    }

    void writeBezier(SBezier *sb) {
        Vector c;
        Vector n = Vector::From(0.0, 0.0, 1.0);
        double r;

        if(sb->deg == 1) {
            // Line
            writeLine(sb->ctrl[0], sb->ctrl[1]);
        } else if(sb->IsInPlane(n, 0) && sb->IsCircle(n, &c, &r)) {
            // Circle perpendicular to camera
            double theta0 = atan2(sb->ctrl[0].y - c.y, sb->ctrl[0].x - c.x);
            double theta1 = atan2(sb->ctrl[2].y - c.y, sb->ctrl[2].x - c.x);
            double dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2.0 * PI);
            if(dtheta < 0.0) swap(theta0, theta1);

            writeArc(c, r, theta0, theta1);
        } else if(sb->IsRational()) {
            // Rational bezier
            // We'd like to export rational beziers exactly, but the resulting DXF
            // files can only be read by AutoCAD; LibreCAD/QCad simply do not
            // implement the feature. So, export as piecewise linear for compatiblity.
            writeBezierAsPwl(sb);
        } else {
            // Any other curve
            writeSpline(sb);
        }
    }

    void writeBezierAsPwl(SBezier &sb) {
        List<Vector> lv = {};
        sb.MakePwlInto(&lv, SS.ChordTolMm() / SS.exportScale);
        DRW_LWPolyline polyline;
        assignEntityDefaults(&polyline);
        for(int i = 0; i < lv.n; i++) {
            DRW_Vertex2D *vertex = new DRW_Vertex2D();
            vertex->x = lv.elem[i].x;
            vertex->y = lv.elem[i].y;
            polyline.vertlist.push_back(vertex);
        }
    }
};

void DxfFileWriter::StartFile(void) {
    paths.clear();
}

void DxfFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb)
{
    BezierPath path = {};
    path.color = strokeRgb.ToPackedIntBGRA();
    path.width = lineWidth;
    paths.push_back(path);
}
void DxfFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb)
{
}

void DxfFileWriter::Triangle(STriangle *tr) {
}

void DxfFileWriter::Bezier(SBezier *sb) {
    paths.back().beziers.push_back(sb);
}

void DxfFileWriter::FinishAndCloseFile(void) {
    dxfRW dxf(filename.c_str());
    DxfWriteInterface interface(this, &dxf);
    dxf.write(&interface, DRW::AC1018, false);
    paths.clear();
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

void EpsFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb)
{
    fprintf(f, "newpath\r\n");
    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void EpsFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb)
{
    fprintf(f, "    %.3f setlinewidth\r\n"
               "    %.3f %.3f %.3f setrgbcolor\r\n"
               "    1 setlinejoin\r\n"  // rounded
               "    1 setlinecap\r\n"   // rounded
               "    gsave stroke grestore\r\n",
        MmToPts(lineWidth),
        strokeRgb.redF(), strokeRgb.greenF(), strokeRgb.blueF());
    if(filled) {
        fprintf(f, "    %.3f %.3f %.3f setrgbcolor\r\n"
                   "    gsave fill grestore\r\n",
            fillRgb.redF(), fillRgb.greenF(), fillRgb.blueF());
    }
}

void EpsFileWriter::MaybeMoveTo(Vector st, Vector fi) {
    if(!prevPt.Equals(st)) {
        fprintf(f, "    %.3f %.3f moveto\r\n",
            MmToPts(st.x - ptMin.x), MmToPts(st.y - ptMin.y));
    }
    prevPt = fi;
}

void EpsFileWriter::Triangle(STriangle *tr) {
    fprintf(f,
"%.3f %.3f %.3f setrgbcolor\r\n"
"newpath\r\n"
"    %.3f %.3f moveto\r\n"
"    %.3f %.3f lineto\r\n"
"    %.3f %.3f lineto\r\n"
"    closepath\r\n"
"gsave fill grestore\r\n",
            tr->meta.color.redF(), tr->meta.color.greenF(), tr->meta.color.blueF(),
            MmToPts(tr->a.x - ptMin.x), MmToPts(tr->a.y - ptMin.y),
            MmToPts(tr->b.x - ptMin.x), MmToPts(tr->b.y - ptMin.y),
            MmToPts(tr->c.x - ptMin.x), MmToPts(tr->c.y - ptMin.y));

    // same issue with cracks, stroke it to avoid them
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;
    fprintf(f,
"1 setlinejoin\r\n"
"1 setlinecap\r\n"
"%.3f setlinewidth\r\n"
"gsave stroke grestore\r\n",
            MmToPts(sw));
}

void EpsFileWriter::Bezier(SBezier *sb) {
    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(sb->deg == 1) {
        MaybeMoveTo(sb->ctrl[0], sb->ctrl[1]);
        fprintf(f,     "    %.3f %.3f lineto\r\n",
                MmToPts(sb->ctrl[1].x - ptMin.x),
                MmToPts(sb->ctrl[1].y - ptMin.y));
    } else if(sb->IsCircle(n, &c, &r)) {
        Vector p0 = sb->ctrl[0], p1 = sb->ctrl[2];
        double theta0 = atan2(p0.y - c.y, p0.x - c.x),
               theta1 = atan2(p1.y - c.y, p1.x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        MaybeMoveTo(p0, p1);
        fprintf(f,
"    %.3f %.3f %.3f %.3f %.3f %s\r\n",
            MmToPts(c.x - ptMin.x),  MmToPts(c.y - ptMin.y),
            MmToPts(r),
            theta0*180/PI, theta1*180/PI,
            dtheta < 0 ? "arcn" : "arc");
    } else if(sb->deg == 3 && !sb->IsRational()) {
        MaybeMoveTo(sb->ctrl[0], sb->ctrl[3]);
        fprintf(f,
"    %.3f %.3f %.3f %.3f %.3f %.3f curveto\r\n",
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
    if((ptMax.x - ptMin.x) > 200*25.4 ||
       (ptMax.y - ptMin.y) > 200*25.4)
    {
        Message("PDF page size exceeds 200 by 200 inches; many viewers may "
                "reject this file.");
    }

    fprintf(f,
"%%PDF-1.1\r\n"
"%%%c%c%c%c\r\n",
        0xe2, 0xe3, 0xcf, 0xd3);

    xref[1] = (uint32_t)ftell(f);
    fprintf(f,
"1 0 obj\r\n"
"  << /Type /Catalog\r\n"
"     /Outlines 2 0 R\r\n"
"     /Pages 3 0 R\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[2] = (uint32_t)ftell(f);
    fprintf(f,
"2 0 obj\r\n"
"  << /Type /Outlines\r\n"
"     /Count 0\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[3] = (uint32_t)ftell(f);
    fprintf(f,
"3 0 obj\r\n"
"  << /Type /Pages\r\n"
"     /Kids [4 0 R]\r\n"
"     /Count 1\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[4] = (uint32_t)ftell(f);
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

    xref[5] = (uint32_t)ftell(f);
    fprintf(f,
"5 0 obj\r\n"
"  << /Length 6 0 R >>\r\n"
"stream\r\n");
    bodyStart = (uint32_t)ftell(f);
}

void PdfFileWriter::FinishAndCloseFile(void) {
    uint32_t bodyEnd = (uint32_t)ftell(f);

    fprintf(f,
"endstream\r\n"
"endobj\r\n");

    xref[6] = (uint32_t)ftell(f);
    fprintf(f,
"6 0 obj\r\n"
"  %d\r\n"
"endobj\r\n",
        bodyEnd - bodyStart);

    xref[7] = (uint32_t)ftell(f);
    fprintf(f,
"7 0 obj\r\n"
"  [/PDF /Text]\r\n"
"endobj\r\n");

    xref[8] = (uint32_t)ftell(f);
    fprintf(f,
"8 0 obj\r\n"
"  << /Type /Font\r\n"
"     /Subtype /Type1\r\n"
"     /Name /F1\r\n"
"     /BaseFont /Helvetica\r\n"
"     /Encoding /WinAnsiEncoding\r\n"
"  >>\r\n"
"endobj\r\n");

    xref[9] = (uint32_t)ftell(f);
    fprintf(f,
"9 0 obj\r\n"
"  << /Creator (SolveSpace)\r\n"
"  >>\r\n");

    uint32_t xrefStart = (uint32_t)ftell(f);
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

void PdfFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb)
{
    fprintf(f, "1 J 1 j " // round endcaps and joins
               "%.3f w "
               "%.3f %.3f %.3f RG\r\n",
        MmToPts(lineWidth),
        strokeRgb.redF(), strokeRgb.greenF(), strokeRgb.blueF());
    if(filled) {
        fprintf(f, "%.3f %.3f %.3f rg\r\n",
            fillRgb.redF(), fillRgb.greenF(), fillRgb.blueF());
    }

    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void PdfFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb)
{
    if(filled) {
        fprintf(f, "b\r\n");
    } else {
        fprintf(f, "S\r\n");
    }
}

void PdfFileWriter::MaybeMoveTo(Vector st, Vector fi) {
    if(!prevPt.Equals(st)) {
        fprintf(f, "%.3f %.3f m\r\n",
            MmToPts(st.x - ptMin.x), MmToPts(st.y - ptMin.y));
    }
    prevPt = fi;
}

void PdfFileWriter::Triangle(STriangle *tr) {
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;

    fprintf(f,
"1 J 1 j\r\n"
"%.3f %.3f %.3f RG\r\n"
"%.3f %.3f %.3f rg\r\n"
"%.3f w\r\n"
"%.3f %.3f m\r\n"
"%.3f %.3f l\r\n"
"%.3f %.3f l\r\n"
"b\r\n",
            tr->meta.color.redF(), tr->meta.color.greenF(), tr->meta.color.blueF(),
            tr->meta.color.redF(), tr->meta.color.greenF(), tr->meta.color.blueF(),
            MmToPts(sw),
            MmToPts(tr->a.x - ptMin.x), MmToPts(tr->a.y - ptMin.y),
            MmToPts(tr->b.x - ptMin.x), MmToPts(tr->b.y - ptMin.y),
            MmToPts(tr->c.x - ptMin.x), MmToPts(tr->c.y - ptMin.y));
}

void PdfFileWriter::Bezier(SBezier *sb) {
    if(sb->deg == 1) {
        MaybeMoveTo(sb->ctrl[0], sb->ctrl[1]);
        fprintf(f,
"%.3f %.3f l\r\n",
            MmToPts(sb->ctrl[1].x - ptMin.x), MmToPts(sb->ctrl[1].y - ptMin.y));
    } else if(sb->deg == 3 && !sb->IsRational()) {
        MaybeMoveTo(sb->ctrl[0], sb->ctrl[3]);
        fprintf(f,
"%.3f %.3f %.3f %.3f %.3f %.3f c\r\n",
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
        (ptMax.x - ptMin.x), (ptMax.y - ptMin.y),
        (ptMax.x - ptMin.x), (ptMax.y - ptMin.y));
}

void SvgFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb)
{
    fprintf(f, "<path d='");
    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void SvgFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb)
{
    std::string fill;
    if(filled) {
        fill = ssprintf("#%02x%02x%02x",
            fillRgb.red, fillRgb.green, fillRgb.blue);
    } else {
        fill = "none";
    }
    fprintf(f, "' stroke-width='%.3f' stroke='#%02x%02x%02x' "
                 "stroke-linecap='round' stroke-linejoin='round' "
                 "fill='%s' />\r\n",
        lineWidth, strokeRgb.red, strokeRgb.green, strokeRgb.blue,
        fill.c_str());
}

void SvgFileWriter::MaybeMoveTo(Vector st, Vector fi) {
    // SVG uses a coordinate system with the origin at top left, +y down
    if(!prevPt.Equals(st)) {
        fprintf(f, "M%.3f %.3f ", (st.x - ptMin.x), (ptMax.y - st.y));
    }
    prevPt = fi;
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
            tr->meta.color.red, tr->meta.color.green, tr->meta.color.blue,
            sw,
            tr->meta.color.red, tr->meta.color.green, tr->meta.color.blue);
}

void SvgFileWriter::Bezier(SBezier *sb) {
    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(sb->deg == 1) {
        MaybeMoveTo(sb->ctrl[0], sb->ctrl[1]);
        fprintf(f, "L%.3f,%.3f ",
            (sb->ctrl[1].x - ptMin.x), (ptMax.y - sb->ctrl[1].y));
    } else if(sb->IsCircle(n, &c, &r)) {
        Vector p0 = sb->ctrl[0], p1 = sb->ctrl[2];
        double theta0 = atan2(p0.y - c.y, p0.x - c.x),
               theta1 = atan2(p1.y - c.y, p1.x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        // The arc must be less than 180 degrees, or else it couldn't have
        // been represented as a single rational Bezier. So large-arc-flag
        // must be false. sweep-flag is determined by the sign of dtheta.
        // Note that clockwise and counter-clockwise are backwards in SVG's
        // mirrored csys.
        MaybeMoveTo(p0, p1);
        fprintf(f, "A%.3f,%.3f 0 0,%d %.3f,%.3f ",
                        r, r,
                        (dtheta < 0) ? 1 : 0,
                        p1.x - ptMin.x, ptMax.y - p1.y);
    } else if(!sb->IsRational()) {
        if(sb->deg == 2) {
            MaybeMoveTo(sb->ctrl[0], sb->ctrl[2]);
            fprintf(f, "Q%.3f,%.3f %.3f,%.3f ",
                sb->ctrl[1].x - ptMin.x, ptMax.y - sb->ctrl[1].y,
                sb->ctrl[2].x - ptMin.x, ptMax.y - sb->ctrl[2].y);
        } else if(sb->deg == 3) {
            MaybeMoveTo(sb->ctrl[0], sb->ctrl[3]);
            fprintf(f, "C%.3f,%.3f %.3f,%.3f %.3f,%.3f ",
                sb->ctrl[1].x - ptMin.x, ptMax.y - sb->ctrl[1].y,
                sb->ctrl[2].x - ptMin.x, ptMax.y - sb->ctrl[2].y,
                sb->ctrl[3].x - ptMin.x, ptMax.y - sb->ctrl[3].y);
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

void HpglFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb)
{
}
void HpglFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                bool filled, RgbaColor fillRgb)
{
}

void HpglFileWriter::Triangle(STriangle *tr) {
}

void HpglFileWriter::Bezier(SBezier *sb) {
    if(sb->deg == 1) {
        fprintf(f, "PU%d,%d;\r\n",
            (int)MmToHpglUnits(sb->ctrl[0].x),
            (int)MmToHpglUnits(sb->ctrl[0].y));
        fprintf(f, "PD%d,%d;\r\n",
            (int)MmToHpglUnits(sb->ctrl[1].x),
            (int)MmToHpglUnits(sb->ctrl[1].y));
    } else {
        BezierAsPwl(sb);
    }
}

void HpglFileWriter::FinishAndCloseFile(void) {
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for G Code output. Slightly complicated by our ability to generate
// multiple passes, and to specify the feeds and depth; those parameters get
// set in the configuration screen.
//-----------------------------------------------------------------------------
void GCodeFileWriter::StartFile(void) {
    sel = {};
}
void GCodeFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                                bool filled, RgbaColor fillRgb)
{
}
void GCodeFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                 bool filled, RgbaColor fillRgb)
{
}
void GCodeFileWriter::Triangle(STriangle *tr) {
}

void GCodeFileWriter::Bezier(SBezier *sb) {
    if(sb->deg == 1) {
        sel.AddEdge(sb->ctrl[0], sb->ctrl[1]);
    } else {
        BezierAsPwl(sb);
    }
}

void GCodeFileWriter::FinishAndCloseFile(void) {
    SPolygon sp = {};
    sel.AssemblePolygon(&sp, NULL);

    int i;
    for(i = 0; i < SS.gCode.passes; i++) {
        double depth = (SS.gCode.depth / SS.gCode.passes)*(i+1);

        SContour *sc;
        for(sc = sp.l.First(); sc; sc = sp.l.NextAfter(sc)) {
            if(sc->l.n < 2) continue;

            SPoint *pt = sc->l.First();
            fprintf(f, "G00 X%s Y%s\r\n",
                    SS.MmToString(pt->p.x).c_str(), SS.MmToString(pt->p.y).c_str());
            fprintf(f, "G01 Z%s F%s\r\n",
                    SS.MmToString(depth).c_str(), SS.MmToString(SS.gCode.plungeFeed).c_str());

            pt = sc->l.NextAfter(pt);
            for(; pt; pt = sc->l.NextAfter(pt)) {
                fprintf(f, "G01 X%s Y%s F%s\r\n",
                        SS.MmToString(pt->p.x).c_str(), SS.MmToString(pt->p.y).c_str(),
                        SS.MmToString(SS.gCode.feed).c_str());
            }
            // Move up to a clearance plane 5mm above the work.
            fprintf(f, "G00 Z%s\r\n",
                    SS.MmToString(SS.gCode.depth < 0 ? +5 : -5).c_str());
        }
    }

    sp.Clear();
    sel.Clear();
    fclose(f);
}


//-----------------------------------------------------------------------------
// Routine for STEP output; just a wrapper around the general STEP stuff that
// can also be used for surfaces or 3d curves.
//-----------------------------------------------------------------------------
void Step2dFileWriter::StartFile(void) {
    sfw = {};
    sfw.f = f;
    sfw.WriteHeader();
}

void Step2dFileWriter::Triangle(STriangle *tr) {
}

void Step2dFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                                 bool filled, RgbaColor fillRgb)
{
}
void Step2dFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                  bool filled, RgbaColor fillRgb)
{
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

