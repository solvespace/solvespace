//-----------------------------------------------------------------------------
// The file format-specific stuff for all of the 2d vector output formats.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <libdxfrw.h>
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Routines for DXF export
//-----------------------------------------------------------------------------
class DxfWriteInterface : public DRW_Interface {
public:
    DxfFileWriter *writer;
    dxfRW         *dxf;

    std::set<std::string> messages;

    static DRW_Coord toCoord(const Vector &v) {
        return DRW_Coord(v.x, v.y, v.z);
    }

    Vector xfrm(Vector v) {
        return writer->Transform(v);
    }

    void writeTextstyles() override {
        DRW_Textstyle ts;
        ts.name = "unicode";
        ts.font = "unicode";
        dxf->writeTextstyle(&ts);
    }

    void writeLayers() override {
        DRW_Layer layer;

        layer.name = "dimensions";
        dxf->writeLayer(&layer);
        layer.name = "text";
        dxf->writeLayer(&layer);

        std::set<uint32_t> usedStyles;

        for(DxfFileWriter::BezierPath &path : writer->paths) {
            for(SBezier *sb : path.beziers) {
                usedStyles.insert((uint32_t)sb->auxA);
            }
        }

        for(uint32_t v : usedStyles) {
            Style *s = Style::Get(hStyle{v});
            layer.name = s->DescriptionString();
            dxf->writeLayer(&layer);
        }
    }

    void writeLTypes() override {
        for(uint32_t i = 0; i <= (uint32_t)StipplePattern::LAST; i++) {
            StipplePattern st = (StipplePattern)i;
            DRW_LType type;
            // LibreCAD requires the line type to have one of these exact names,
            // or otherwise it overwrites it with its own (continuous) style.
            type.name = DxfFileWriter::lineTypeName(st);
            double sw = 1.0;
            switch(st) {
                case StipplePattern::CONTINUOUS:
                    break;

                case StipplePattern::SHORT_DASH:
                    type.path.push_back(sw);
                    type.path.push_back(-sw * 2.0);
                    break;

                case StipplePattern::DASH:
                    type.path.push_back(sw);
                    type.path.push_back(-sw);
                    break;

                case StipplePattern::LONG_DASH:
                    type.path.push_back(sw * 2.0);
                    type.path.push_back(-sw);
                    break;

                case StipplePattern::DASH_DOT:
                    type.path.push_back(sw);
                    type.path.push_back(-sw);
                    type.path.push_back(0.0);
                    type.path.push_back(-sw);
                    break;

                case StipplePattern::DOT:
                    type.path.push_back(sw);
                    type.path.push_back(0.0);
                    break;

                case StipplePattern::DASH_DOT_DOT:
                    type.path.push_back(sw);
                    type.path.push_back(-sw);
                    type.path.push_back(0.0);
                    type.path.push_back(-sw);
                    type.path.push_back(0.0);
                    type.path.push_back(-sw);
                    break;

                case StipplePattern::FREEHAND:
                case StipplePattern::ZIGZAG:
                    // Not implemented; exported as continuous.
                    break;
            }
            dxf->writeLineType(&type);
        }
    }

    void writePolylines() {
        PolylineBuilder builder;

        for(DxfFileWriter::BezierPath &path : writer->paths) {
            for(SBezier *sb : path.beziers) {
                if(sb->deg != 1) continue;
                builder.AddEdge(sb->ctrl[0], sb->ctrl[1], (uint32_t)sb->auxA);
            }
        }

        DRW_Polyline polyline;

        auto startFunc = [&](PolylineBuilder::Vertex *start,
                             PolylineBuilder::Vertex *next,
                             PolylineBuilder::Edge *e) {
            hStyle hs = { e->kind };
            polyline = {};
            assignEntityDefaults(&polyline, hs);

            if(!(EXACT(start->pos.z == 0.0) && EXACT(next->pos.z == 0.0))) {
                polyline.flags |= 8 /* 3d polyline */;
            }
            polyline.vertlist.push_back(
                new DRW_Vertex(start->pos.x, start->pos.y, start->pos.z, 0.0));
            polyline.vertlist.push_back(
                new DRW_Vertex(next->pos.x, next->pos.y, next->pos.z, 0.0));
        };

        auto nextFunc = [&](PolylineBuilder::Vertex *next, PolylineBuilder::Edge *e) {
            if(!EXACT(next->pos.z == 0.0)) {
                polyline.flags |= 8 /* 3d polyline */;
            }
            polyline.vertlist.push_back(
                new DRW_Vertex(next->pos.x, next->pos.y, next->pos.z, 0.0));
        };

        auto endFunc = [&]() {
            dxf->writePolyline(&polyline);
        };

        auto aloneFunc = [&](PolylineBuilder::Edge *e) {
            hStyle hs = { e->kind };
            writeLine(e->a->pos, e->b->pos, hs);
        };

        builder.Generate(startFunc, nextFunc, aloneFunc, endFunc);
    }

    void writeEntities() override {
        writePolylines();

        for(DxfFileWriter::BezierPath &path : writer->paths) {
            for(SBezier *sb : path.beziers) {
                if(sb->deg == 1) continue;
                writeBezier(sb);
            }
        }

        if(writer->constraint) {
            Constraint *c;
            for(c = writer->constraint->First(); c; c = writer->constraint->NextAfter(c)) {
                if(!writer->NeedToOutput(c)) continue;
                switch(c->type) {
                    case Constraint::Type::PT_PT_DISTANCE: {
                        Vector ap = SK.GetEntity(c->ptA)->PointGetNum();
                        Vector bp = SK.GetEntity(c->ptB)->PointGetNum();
                        Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(c->disp.offset);
                        writeAlignedDimension(xfrm(ap),  xfrm(bp), xfrm(ref),
                                              xfrm(ref), c->Label(), c->GetStyle(), c->valA);
                        break;
                    }

                    case Constraint::Type::PT_LINE_DISTANCE: {
                        Vector pt = SK.GetEntity(c->ptA)->PointGetNum();
                        Entity *line = SK.GetEntity(c->entityA);
                        Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
                        Vector lB = SK.GetEntity(line->point[1])->PointGetNum();
                        Vector dl = lB.Minus(lA);

                        Vector closest = pt.ClosestPointOnLine(lA, dl);

                        if(pt.Equals(closest)) break;

                        Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(c->disp.offset);
                        Vector refClosest = ref.ClosestPointOnLine(lA, dl);

                        double ddl = dl.Dot(dl);
                        if(fabs(ddl) > LENGTH_EPS * LENGTH_EPS) {
                            double t = refClosest.Minus(lA).Dot(dl) / ddl;
                            if(t < 0.0) {
                                refClosest = lA;
                            } else if(t > 1.0) {
                                refClosest = lB;
                            }
                        }

                        Vector xdl = xfrm(lB).Minus(xfrm(lA));
                        writeLinearDimension(xfrm(pt), xfrm(refClosest), xfrm(ref),
                                             xfrm(ref), c->Label(),
                                             atan2(xdl.y, xdl.x) / PI * 180.0 + 90.0, 0.0,
                                             c->GetStyle(), c->valA);
                        break;
                    }

                    case Constraint::Type::DIAMETER: {
                        Entity *circle = SK.GetEntity(c->entityA);
                        Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
                        Quaternion q = SK.GetEntity(circle->normal)->NormalGetNum();
                        Vector n = q.RotationN().WithMagnitude(1);
                        double r = circle->CircleGetRadiusNum();

                        Vector ref = center.Plus(c->disp.offset);
                        // Force the label into the same plane as the circle.
                        ref = ref.Minus(n.ScaledBy(n.Dot(ref) - n.Dot(center)));

                        Vector rad = ref.Minus(center).WithMagnitude(r);
                        if(/*isRadius*/c->other) {
                            writeRadialDimension(
                                xfrm(center), xfrm(center.Plus(rad)),
                                xfrm(ref), c->Label(), c->GetStyle(), c->valA);
                        } else {
                            writeDiametricDimension(
                                xfrm(center.Minus(rad)), xfrm(center.Plus(rad)),
                                xfrm(ref), c->Label(), c->GetStyle(), c->valA);
                        }
                        break;
                    }

                    case Constraint::Type::ANGLE: {
                        Entity *a = SK.GetEntity(c->entityA);
                        Entity *b = SK.GetEntity(c->entityB);

                        Vector a0 = a->VectorGetStartPoint();
                        Vector b0 = b->VectorGetStartPoint();
                        Vector da = a->VectorGetNum();
                        Vector db = b->VectorGetNum();
                        if(/*otherAngle*/c->other) {
                            a0 = a0.Plus(da);
                            da = da.ScaledBy(-1);
                        }

                        bool skew = false;
                        Vector ref = c->disp.offset;
                        Vector pi = Vector::AtIntersectionOfLines(a0, a0.Plus(da), b0, b0.Plus(db),
                                                                  &skew);
                        if(!skew) ref = pi.Plus(c->disp.offset);

                        Vector norm = da.Cross(db);
                        Vector dna = norm.Cross(da).WithMagnitude(1.0);

                        double thetaf = acos(da.DirectionCosineWith(db));

                        // Calculate median
                        Vector m = da.WithMagnitude(1.0).ScaledBy(cos(thetaf/2)).Plus(
                                   dna.ScaledBy(sin(thetaf/2)));
                        Vector rm = ref.Minus(pi);

                        // Test which side we have to place an arc
                        if(m.Dot(rm) < 0) {
                            da = da.ScaledBy(-1); dna = dna.ScaledBy(-1);
                            db = db.ScaledBy(-1);
                        }

                        Vector bisect = da.WithMagnitude(1.0).ScaledBy(cos(thetaf / 2.0)).Plus(
                                        dna.ScaledBy(sin(thetaf / 2.0)));

                        ref = pi.Plus(bisect.WithMagnitude(c->disp.offset.Magnitude()));

                        // Get lines again to write exact line.
                        a0 = a->VectorGetStartPoint();
                        b0 = b->VectorGetStartPoint();
                        da = a->VectorGetNum();
                        db = b->VectorGetNum();

                        writeAngularDimension(
                            xfrm(a0), xfrm(a0.Plus(da)), xfrm(b0), xfrm(b0.Plus(db)), xfrm(ref),
                            xfrm(ref), c->Label(), c->GetStyle(), c->valA);
                        break;
                    }

                    case Constraint::Type::COMMENT: {
                        Style *st = SK.style.FindById(c->GetStyle());
                        writeText(xfrm(c->disp.offset), c->Label(),
                                  Style::TextHeight(c->GetStyle()) / SS.GW.scale,
                                  st->textAngle, st->textOrigin, c->GetStyle());
                        break;
                    }

                    default:
                        // Other types of constraints do not have a DXF dimension equivalent.
                        break;
                }
            }
        }
    }

    int findDxfColor(const RgbaColor &src) {
        int best = 0;
        double minDist = VERY_POSITIVE;
        Vector srcv = Vector::From(src.redF(), src.greenF(), src.blueF());
        for(int i = 1; i < 256; i++) {
            RgbaColor dst = RGBi(DRW::dxfColors[i][0], DRW::dxfColors[i][1], DRW::dxfColors[i][2]);
            Vector dstv = Vector::From(dst.redF(), dst.greenF(), dst.blueF());
            double dist = srcv.Minus(dstv).Magnitude();
            if(dist < minDist || best == 0) {
                best = i;
                minDist = dist;
            }
        }
        return best;
    }

    void assignEntityDefaults(DRW_Entity *entity, hStyle hs) {
        Style *s = Style::Get(hs);
        RgbaColor color = Style::Color(hs, /*forExport=*/true);
        entity->color24 = color.ToPackedIntBGRA();
        entity->color = findDxfColor(color);
        entity->layer = s->DescriptionString();
        entity->lineType = DxfFileWriter::lineTypeName(s->stippleType);
        entity->ltypeScale = Style::StippleScaleMm(s->h);
        entity->setWidthMm(Style::WidthMm(hs.v));

        if(s->stippleType == StipplePattern::FREEHAND) {
            messages.insert(_("freehand lines were replaced with continuous lines"));
        } else if(s->stippleType == StipplePattern::ZIGZAG) {
            messages.insert(_("zigzag lines were replaced with continuous lines"));
        }
    }

    void assignDimensionDefaults(DRW_Dimension *dimension, hStyle hs) {
        assignEntityDefaults(dimension, hs);
        dimension->layer = "dimensions";
    }

    void writeLine(const Vector &p0, const Vector &p1, hStyle hs) {
        DRW_Line line;
        assignEntityDefaults(&line, hs);
        line.basePoint = toCoord(p0);
        line.secPoint = toCoord(p1);
        dxf->writeLine(&line);
    }

    void writeArc(const Vector &c, double r, double sa, double ea, hStyle hs) {
        DRW_Arc arc;
        assignEntityDefaults(&arc, hs);
        arc.radious = r;
        arc.basePoint = toCoord(c);
        arc.staangle = sa;
        arc.endangle = ea;
        dxf->writeArc(&arc);
    }

    void writeBezierAsPwl(SBezier *sb) {
        List<Vector> lv = {};
        sb->MakePwlInto(&lv, SS.ExportChordTolMm());
        hStyle hs = { (uint32_t)sb->auxA };
        DRW_Polyline polyline;
        assignEntityDefaults(&polyline, hs);
        for(int i = 0; i < lv.n; i++) {
            Vector *v = &lv[i];
            DRW_Vertex *vertex = new DRW_Vertex(v->x, v->y, v->z, 0.0);
            polyline.vertlist.push_back(vertex);
        }
        dxf->writePolyline(&polyline);
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
        } else ssassert(false, "Unexpected degree of spline");
    }

    void writeSpline(SBezier *sb) {
        bool isRational = sb->IsRational();
        hStyle hs = { (uint32_t)sb->auxA };
        DRW_Spline spline;
        assignEntityDefaults(&spline, hs);
        spline.flags = (isRational) ? 0x04 : 0x08;
        spline.degree = sb->deg;
        spline.ncontrol = sb->deg + 1;
        makeKnotsFor(&spline);
        for(int i = 0; i <= sb->deg; i++) {
            spline.controllist.push_back(
                new DRW_Coord(sb->ctrl[i].x, sb->ctrl[i].y, sb->ctrl[i].z));
            if(isRational) spline.weightlist.push_back(sb->weight[i]);
        }
        dxf->writeSpline(&spline);
    }

    void writeBezier(SBezier *sb) {
        hStyle hs = { (uint32_t)sb->auxA };
        Vector c;
        Vector n = Vector::From(0.0, 0.0, 1.0);
        double r;

        if(sb->deg == 1) {
            // Line
            writeLine(sb->ctrl[0], sb->ctrl[1], hs);
        } else if(sb->IsInPlane(n, 0) && sb->IsCircle(n, &c, &r)) {
            // Circle perpendicular to camera
            double theta0 = atan2(sb->ctrl[0].y - c.y, sb->ctrl[0].x - c.x);
            double theta1 = atan2(sb->ctrl[2].y - c.y, sb->ctrl[2].x - c.x);
            double dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2.0 * PI);
            if(dtheta < 0.0) swap(theta0, theta1);

            writeArc(c, r, theta0, theta1, hs);
        } else if(sb->IsRational()) {
            // Rational bezier
            // We'd like to export rational beziers exactly, but the resulting DXF
            // files can only be read by AutoCAD; LibreCAD/QCad simply do not
            // implement the feature. So, export as piecewise linear for compatibility.
            writeBezierAsPwl(sb);
        } else {
            // Any other curve
            writeSpline(sb);
        }
    }

    void writeAlignedDimension(Vector def1, Vector def2, Vector dimp,
                               Vector textp, const std::string &text, hStyle hs, double actual) {
        DRW_DimAligned dim;
        assignDimensionDefaults(&dim, hs);
        dim.setDef1Point(toCoord(def1));
        dim.setDef2Point(toCoord(def2));
        dim.setDimPoint(toCoord(dimp));
        dim.setTextPoint(toCoord(textp));
        dim.setText(text);
        dim.setActualMeasurement(actual);
        dxf->writeDimension(&dim);
    }

    void writeLinearDimension(Vector def1, Vector def2, Vector dimp,
                              Vector textp, const std::string &text,
                              double angle, double oblique, hStyle hs, double actual) {
        DRW_DimLinear dim;
        assignDimensionDefaults(&dim, hs);
        dim.setDef1Point(toCoord(def1));
        dim.setDef2Point(toCoord(def2));
        dim.setDimPoint(toCoord(dimp));
        dim.setTextPoint(toCoord(textp));
        dim.setText(text);
        dim.setAngle(angle);
        dim.setOblique(oblique);
        dim.setActualMeasurement(actual);
        dxf->writeDimension(&dim);
    }

    void writeRadialDimension(Vector center, Vector radius,
                              Vector textp, const std::string &text, hStyle hs, double actual) {
        DRW_DimRadial dim;
        assignDimensionDefaults(&dim, hs);
        dim.setCenterPoint(toCoord(center));
        dim.setDiameterPoint(toCoord(radius));
        dim.setTextPoint(toCoord(textp));
        dim.setText(text);
        dim.setActualMeasurement(actual);
        dxf->writeDimension(&dim);
    }

    void writeDiametricDimension(Vector def1, Vector def2,
                                 Vector textp, const std::string &text, hStyle hs, double actual) {
        DRW_DimDiametric dim;
        assignDimensionDefaults(&dim, hs);
        dim.setDiameter1Point(toCoord(def1));
        dim.setDiameter2Point(toCoord(def2));
        dim.setTextPoint(toCoord(textp));
        dim.setText(text);
        dim.setActualMeasurement(actual);
        dxf->writeDimension(&dim);
    }

    void writeAngularDimension(Vector fl1, Vector fl2, Vector sl1, Vector sl2, Vector dimp,
                               Vector textp, const std::string &text, hStyle hs, double actual) {
        DRW_DimAngular dim;
        assignDimensionDefaults(&dim, hs);
        dim.setFirstLine1(toCoord(fl1));
        dim.setFirstLine2(toCoord(fl2));
        dim.setSecondLine1(toCoord(sl1));
        dim.setSecondLine2(toCoord(sl2));
        dim.setDimPoint(toCoord(dimp));
        dim.setTextPoint(toCoord(textp));
        dim.setText(text);
        dim.setActualMeasurement(actual * PI / 180.0);
        dxf->writeDimension(&dim);
    }

    void writeText(Vector textp, const std::string &text,
                   double height, double angle, Style::TextOrigin origin, hStyle hs) {
        DRW_Text txt;
        assignEntityDefaults(&txt, hs);
        txt.layer = "text";
        txt.style = "unicode";
        txt.basePoint = toCoord(textp);
        txt.secPoint = txt.basePoint;
        txt.text = text;
        txt.height = height;
        txt.angle = angle;
        txt.alignH = DRW_Text::HCenter;
        if((uint32_t)origin & (uint32_t)Style::TextOrigin::LEFT) {
            txt.alignH = DRW_Text::HLeft;
        } else if((uint32_t)origin & (uint32_t)Style::TextOrigin::RIGHT) {
            txt.alignH = DRW_Text::HRight;
        }
        txt.alignV = DRW_Text::VMiddle;
        if((uint32_t)origin & (uint32_t)Style::TextOrigin::TOP) {
            txt.alignV = DRW_Text::VTop;
        } else if((uint32_t)origin & (uint32_t)Style::TextOrigin::BOT) {
            txt.alignV = DRW_Text::VBaseLine;
        }
        dxf->writeText(&txt);
    }
};

bool DxfFileWriter::OutputConstraints(IdList<Constraint,hConstraint> *constraint) {
    this->constraint = constraint;
    return true;
}

void DxfFileWriter::StartFile() {
    paths.clear();
}

void DxfFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb, hStyle hs)
{
    BezierPath path = {};
    paths.push_back(path);
}
void DxfFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb, hStyle hs)
{
}

void DxfFileWriter::Triangle(STriangle *tr) {
}

void DxfFileWriter::Bezier(SBezier *sb) {
    paths.back().beziers.push_back(sb);
}

void DxfFileWriter::FinishAndCloseFile() {
    dxfRW dxf;

    DxfWriteInterface interface = {};
    interface.writer = this;
    interface.dxf    = &dxf;

    std::stringstream stream;
    dxf.write(stream, &interface, DRW::AC1021, /*bin=*/false);
    paths.clear();
    constraint = NULL;

    if(!WriteFile(filename, stream.str())) {
        Error("Couldn't write to '%s'", filename.raw.c_str());
        return;
    }

    if(!interface.messages.empty()) {
        std::string text = _("Some aspects of the drawing have no DXF equivalent and "
                             "were not exported:\n");
        for(const std::string &message : interface.messages) {
            text += " * " + message + "\n";
        }
        Message(text.c_str());
    }
}

bool DxfFileWriter::NeedToOutput(Constraint *c) {
    switch(c->type) {
        case Constraint::Type::PT_PT_DISTANCE:
        case Constraint::Type::PT_LINE_DISTANCE:
        case Constraint::Type::DIAMETER:
        case Constraint::Type::ANGLE:
        case Constraint::Type::COMMENT:
            return c->IsVisible();

        default: // See writeEntities().
            break;
    }
    return false;
}

const char *DxfFileWriter::lineTypeName(StipplePattern stippleType) {
    switch(stippleType) {
        case StipplePattern::CONTINUOUS:   return "CONTINUOUS";
        case StipplePattern::SHORT_DASH:   return "DASHED";
        case StipplePattern::DASH:         return "DASHED";
        case StipplePattern::LONG_DASH:    return "DASHEDX2";
        case StipplePattern::DASH_DOT:     return "DASHDOT";
        case StipplePattern::DASH_DOT_DOT: return "DIVIDE";
        case StipplePattern::DOT:          return "DOT";

        case StipplePattern::FREEHAND:
        case StipplePattern::ZIGZAG:
            /* no corresponding DXF line type */
           break;
    }

    return "CONTINUOUS";
}

//-----------------------------------------------------------------------------
// Routines for EPS output
//-----------------------------------------------------------------------------

static std::string MakeStipplePattern(StipplePattern pattern, double scale, char delimiter,
                                      bool inkscapeWorkaround = false) {
    scale /= 2.0;

    // Inkscape ignores all elements that are exactly zero instead of drawing
    // them as dots.
    double zero = inkscapeWorkaround ? 1e-6 : 0;

    std::string result;
    switch(pattern) {
        case StipplePattern::CONTINUOUS:
            return "";

        case StipplePattern::SHORT_DASH:
            result = ssprintf("%.3f_%.3f", scale, scale * 2.0);
            break;
        case StipplePattern::DASH:
            result = ssprintf("%.3f_%.3f", scale, scale);
            break;
        case StipplePattern::DASH_DOT:
            result = ssprintf("%.3f_%.3f_%.6f_%.3f",
                              scale, scale * 0.5, zero, scale * 0.5);
            break;
        case StipplePattern::DASH_DOT_DOT:
            result = ssprintf("%.3f_%.3f_%.6f_%.3f_%.6f_%.3f",
                              scale, scale * 0.5, zero,
                              scale * 0.5, scale * 0.5, zero);
            break;
        case StipplePattern::DOT:
            result = ssprintf("%.6f_%.3f", zero, scale * 0.5);
            break;
        case StipplePattern::LONG_DASH:
            result = ssprintf("%.3f_%.3f", scale * 2.0, scale * 0.5);
            break;

        case StipplePattern::FREEHAND:
        case StipplePattern::ZIGZAG:
            ssassert(false, "Freehand and zigzag export not implemented");
    }
    std::replace(result.begin(), result.end(), '_', delimiter);
    return result;
}

void EpsFileWriter::StartFile() {
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
                              bool filled, RgbaColor fillRgb, hStyle hs)
{
    fprintf(f, "newpath\r\n");
    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void EpsFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb, hStyle hs)
{
    StipplePattern pattern = Style::PatternType(hs);
    double stippleScale = MmToPts(Style::StippleScaleMm(hs));

    fprintf(f, "    %.3f setlinewidth\r\n"
               "    %.3f %.3f %.3f setrgbcolor\r\n"
               "    1 setlinejoin\r\n"  // rounded
               "    1 setlinecap\r\n"   // rounded
               "    [%s] 0 setdash\r\n"
               "    gsave stroke grestore\r\n",
        MmToPts(lineWidth),
        strokeRgb.redF(), strokeRgb.greenF(), strokeRgb.blueF(),
        MakeStipplePattern(pattern, stippleScale, ' ').c_str());
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

void EpsFileWriter::FinishAndCloseFile() {
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
void PdfFileWriter::StartFile() {
    if((ptMax.x - ptMin.x) > 200*25.4 ||
       (ptMax.y - ptMin.y) > 200*25.4)
    {
        Message(_("PDF page size exceeds 200 by 200 inches; many viewers may "
                  "reject this file."));
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

void PdfFileWriter::FinishAndCloseFile() {
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
                              bool filled, RgbaColor fillRgb, hStyle hs)
{
    StipplePattern pattern = Style::PatternType(hs);
    double stippleScale = MmToPts(Style::StippleScaleMm(hs));

    fprintf(f, "1 J 1 j " // round endcaps and joins
               "%.3f w [%s] 0 d "
               "%.3f %.3f %.3f RG\r\n",
        MmToPts(lineWidth),
        MakeStipplePattern(pattern, stippleScale, ' ').c_str(),
        strokeRgb.redF(), strokeRgb.greenF(), strokeRgb.blueF());
    if(filled) {
        fprintf(f, "%.3f %.3f %.3f rg\r\n",
            fillRgb.redF(), fillRgb.greenF(), fillRgb.blueF());
    }

    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void PdfFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb, hStyle hs)
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
void SvgFileWriter::StartFile() {
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

    fprintf(f, "<style><![CDATA[\r\n");
    fprintf(f, "polygon {\r\n");
    fprintf(f, "shape-rendering:crispEdges;\r\n");
    // crispEdges turns of anti-aliasing, which tends to cause hairline
    // cracks between triangles; but there still is some cracking, so
    // specify a stroke width too, hope for around a pixel
    double sw = max(ptMax.x - ptMin.x, ptMax.y - ptMin.y) / 1000;
    fprintf(f, "stroke-width:%f;\r\n", sw);
    fprintf(f, "}\r\n");
    for(auto &style : SK.style) {
        Style *s = &style;

        RgbaColor strokeRgb = Style::Color(s->h, /*forExport=*/true);
        StipplePattern pattern = Style::PatternType(s->h);
        double stippleScale = Style::StippleScaleMm(s->h);

        fprintf(f, ".s%x {\r\n", s->h.v);
        fprintf(f, "stroke:#%02x%02x%02x;\r\n", strokeRgb.red, strokeRgb.green, strokeRgb.blue);
        // don't know why we have to take a half of the width
        fprintf(f, "stroke-width:%f;\r\n", Style::WidthMm(s->h.v) / 2.0);
        fprintf(f, "stroke-linecap:round;\r\n");
        fprintf(f, "stroke-linejoin:round;\r\n");
        std::string patternStr = MakeStipplePattern(pattern, stippleScale, ',',
                                                    /*inkscapeWorkaround=*/true);
        if(!patternStr.empty()) {
            fprintf(f, "stroke-dasharray:%s;\r\n", patternStr.c_str());
        }
        fprintf(f, "fill:none;\r\n");
        fprintf(f, "}\r\n");
    }
    fprintf(f, "]]></style>\r\n");
}

void SvgFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                              bool filled, RgbaColor fillRgb, hStyle hs)
{
    fprintf(f, "<path d='");
    prevPt = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
}
void SvgFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb, hStyle hs)
{
    std::string fill;
    if(filled) {
        fill = ssprintf("fill='#%02x%02x%02x'",
            fillRgb.red, fillRgb.green, fillRgb.blue);
    }
    std::string cls = ssprintf("s%x", hs.v);
    fprintf(f, "' class='%s' %s/>\r\n", cls.c_str(), fill.c_str());
}

void SvgFileWriter::MaybeMoveTo(Vector st, Vector fi) {
    // SVG uses a coordinate system with the origin at top left, +y down
    if(!prevPt.Equals(st)) {
        fprintf(f, "M%.3f %.3f ", (st.x - ptMin.x), (ptMax.y - st.y));
    }
    prevPt = fi;
}

void SvgFileWriter::Triangle(STriangle *tr) {
    fprintf(f,
"<polygon points='%.3f,%.3f %.3f,%.3f %.3f,%.3f' "
    "stroke='#%02x%02x%02x' "
    "fill='#%02x%02x%02x'/>\r\n",
            (tr->a.x - ptMin.x), (ptMax.y - tr->a.y),
            (tr->b.x - ptMin.x), (ptMax.y - tr->b.y),
            (tr->c.x - ptMin.x), (ptMax.y - tr->c.y),
            tr->meta.color.red, tr->meta.color.green, tr->meta.color.blue,
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

void SvgFileWriter::FinishAndCloseFile() {
    fprintf(f, "\r\n</svg>\r\n");
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for HPGL output
//-----------------------------------------------------------------------------
double HpglFileWriter::MmToHpglUnits(double mm) {
    return mm*40;
}

void HpglFileWriter::StartFile() {
    fprintf(f, "IN;\r\n");
    fprintf(f, "SP1;\r\n");
}

void HpglFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                               bool filled, RgbaColor fillRgb, hStyle hs)
{
}
void HpglFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                bool filled, RgbaColor fillRgb, hStyle hs)
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

void HpglFileWriter::FinishAndCloseFile() {
    fclose(f);
}

//-----------------------------------------------------------------------------
// Routines for G Code output. Slightly complicated by our ability to generate
// multiple passes, and to specify the feeds and depth; those parameters get
// set in the configuration screen.
//-----------------------------------------------------------------------------
void GCodeFileWriter::StartFile() {
    sel = {};
}
void GCodeFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                                bool filled, RgbaColor fillRgb, hStyle hs)
{
}
void GCodeFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                 bool filled, RgbaColor fillRgb, hStyle hs)
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

void GCodeFileWriter::FinishAndCloseFile() {
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
void Step2dFileWriter::StartFile() {
    sfw = {};
    sfw.f = f;
    sfw.WriteHeader();
}

void Step2dFileWriter::Triangle(STriangle *tr) {
}

void Step2dFileWriter::StartPath(RgbaColor strokeRgb, double lineWidth,
                                 bool filled, RgbaColor fillRgb, hStyle hs)
{
}
void Step2dFileWriter::FinishPath(RgbaColor strokeRgb, double lineWidth,
                                  bool filled, RgbaColor fillRgb, hStyle hs)
{
}

void Step2dFileWriter::Bezier(SBezier *sb) {
    int c = sfw.ExportCurve(sb);
    sfw.curves.Add(&c);
}

void Step2dFileWriter::FinishAndCloseFile() {
    sfw.WriteWireframe();
    sfw.WriteFooter();
    fclose(f);
}

