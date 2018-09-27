//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_FILEWRITERS_H
#define SOLVESPACE_FILEWRITERS_H

#include "platform.h"
#include "dsc.h"
#include "list.h"
#include "srf/surface.h"
#include "striangle.h"
#include "smesh.h"

namespace SolveSpace {

class StepFileWriter {
public:
    void ExportSurfacesTo(const Platform::Path &filename);
    void WriteHeader();
    void WriteProductHeader();
    int ExportCurve(SBezier *sb);
    int ExportCurveLoop(SBezierLoop *loop, bool inner);
    void ExportSurface(SSurface *ss, SBezierList *sbl);
    void WriteWireframe();
    void WriteFooter();

    List<int> curves;
    List<int> advancedFaces;
    FILE *f;
    int id;
};

class VectorFileWriter {
protected:
    Vector u, v, n, origin;
    double cameraTan, scale;

public:
    FILE *f;
    Platform::Path filename;
    Vector ptMin, ptMax;

    static double MmToPts(double mm);

    static VectorFileWriter *ForFile(const Platform::Path &filename);

    void SetModelviewProjection(const Vector &u, const Vector &v, const Vector &n,
                                const Vector &origin, double cameraTan, double scale);
    Vector Transform(Vector &pos) const;

    void OutputLinesAndMesh(SBezierLoopSetSet *sblss, SMesh *sm);

    void BezierAsPwl(SBezier *sb);
    void BezierAsNonrationalCubic(SBezier *sb, int depth=0);

    virtual void StartPath(RgbaColor strokeRgb, double lineWidth,
                           bool filled, RgbaColor fillRgb, hStyle hs) = 0;
    virtual void FinishPath(RgbaColor strokeRgb, double lineWidth,
                            bool filled, RgbaColor fillRgb, hStyle hs) = 0;
    virtual void Bezier(SBezier *sb) = 0;
    virtual void Triangle(STriangle *tr) = 0;
    virtual bool OutputConstraints(IdList<Constraint,hConstraint> *) { return false; }
    virtual void StartFile() = 0;
    virtual void FinishAndCloseFile() = 0;
    virtual bool HasCanvasSize() const = 0;
    virtual bool CanOutputMesh() const = 0;
};
class DxfFileWriter : public VectorFileWriter {
public:
    struct BezierPath {
        std::vector<SBezier *> beziers;
    };

    std::vector<BezierPath>         paths;
    IdList<Constraint,hConstraint> *constraint;

    static const char *lineTypeName(StipplePattern stippleType);

    bool OutputConstraints(IdList<Constraint,hConstraint> *constraint) override;

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
    bool NeedToOutput(Constraint *c);
};
class EpsFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class PdfFileWriter : public VectorFileWriter {
public:
    uint32_t xref[10];
    uint32_t bodyStart;
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class SvgFileWriter : public VectorFileWriter {
public:
    Vector prevPt;
    void MaybeMoveTo(Vector s, Vector f);

    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return true; }
    bool CanOutputMesh() const override { return true; }
};
class HpglFileWriter : public VectorFileWriter {
public:
    static double MmToHpglUnits(double mm);
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};
class Step2dFileWriter : public VectorFileWriter {
    StepFileWriter sfw;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};
class GCodeFileWriter : public VectorFileWriter {
public:
    SEdgeList sel;
    void StartPath( RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void FinishPath(RgbaColor strokeRgb, double lineWidth,
                    bool filled, RgbaColor fillRgb, hStyle hs) override;
    void Triangle(STriangle *tr) override;
    void Bezier(SBezier *sb) override;
    void StartFile() override;
    void FinishAndCloseFile() override;
    bool HasCanvasSize() const override { return false; }
    bool CanOutputMesh() const override { return false; }
};


}

#endif //SOLVESPACE_FILEWRITERS_H
