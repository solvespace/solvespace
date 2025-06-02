//-----------------------------------------------------------------------------
// Export a GEO file describing our ratpoly shell.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2025 Stefano Guidoni.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const double PRECISION = 2*LENGTH_EPS;

// Add a point to the list.
// input:
//        v -> the cartesian coordinates of the point as a vector
void GeoFileWriter::AddPoint(Vector v) {
    int id = 0;
    for(point_t &p : points) {
        if(p.reference > id) {
            id = p.reference;
        }
        if(p.v.Equals(v, PRECISION)) {
            // Already in the list.
            return;
        }
    }

    // Add a new point.
    point_t newPoint;
    newPoint.v = v;
    newPoint.reference = id + 1;
    points.push_back(newPoint);
}

// Add a curve to the list.
// input:
//        degree -> the degree of the curve
//        pointVectors -> the control points of the curve as a list of point vectors
// return:
//        the id number of the curve
int GeoFileWriter::AddCurve(size_t degree, std::vector<Vector> pointVectors) {
    int id = 0;
    geoEl_t newCurve;

    // If this is a circle arc, we have to find the centre.
    if(degree == 3) {
        Vector v1 = (pointVectors[2]).Minus(pointVectors[1]);
        pointVectors[1] = (pointVectors[0]).Plus(v1);
        AddPoint(pointVectors[1]);
    }

    // Let's find the id numbers of the control points of this curve.
    for(size_t i = 0; i < degree; i++) {
        for(point_t p : points) {
            if(pointVectors[i].Equals(p.v, PRECISION)) {
                newCurve.members.push_back(p.reference);
            }
        }
    }

    for(geoEl_t c : curves) {
        if(c.reference > id) {
            id = c.reference;
        }

        if(exportParts && !(c.color.Equals(currentColor))) {
            continue;
        }

        if(CompareGeoEl(newCurve.members, c.members)) {
            // Same curve.
            return c.reference;
        }
    }

    // Add a new curve.
    newCurve.reference = id + 1;
    newCurve.color = currentColor;
    curves.push_back(newCurve);

    return newCurve.reference;
}

// Add a curve loop to the list.
// input:
//        loopMembers -> the members of the loop (a list of curves)
// return:
//        the id number of the loop
int GeoFileWriter::AddLoop(std::vector<int> loopMembers) {
    int id = 0;

    geoEl_t newLoop;

    for(geoEl_t l : loops) {
        if(l.reference > id) {
            id = l.reference;
        }

        if(exportParts && !(l.color.Equals(currentColor))) {
            continue;
        }

        if(CompareGeoEl(loopMembers, l.members)) {
            // Same loop.
            return l.reference;
        }
    }

    // Set the direction of the edges.
    std::vector<int> previousEdge = GetEdgeMembers(loopMembers[0]);
    std::vector<int> firstEdge = previousEdge;
    for(size_t i = 1; i < loopMembers.size(); i++) {
        std::vector<int> followingEdge = GetEdgeMembers(loopMembers[i]);
        if(previousEdge[0] == followingEdge[0] ||
           previousEdge[0] == followingEdge[followingEdge.size()-1]) {
            // Reverse the first one of the two edges.
            loopMembers[i-1] = -1 * loopMembers[i-1];
        }
        previousEdge = followingEdge;
    }
    // Now previousEdge is the last edge.
    if(previousEdge[0] == firstEdge[0] ||
       previousEdge[0] == firstEdge[firstEdge.size()-1]) {
        // Reverse the last one.
        loopMembers[loopMembers.size()-1] = -1 * loopMembers[loopMembers.size()-1];
    }

    newLoop.members = loopMembers;
    newLoop.reference = id + 1;
    newLoop.color = currentColor;
    loops.push_back(newLoop);

    return newLoop.reference;
}

// Add a surface to the list.
// input:
//        surfaceMembers -> the members of the surface (a list of loops)
void GeoFileWriter::AddSurface(std::vector<int> surfaceMembers) {
    int id = 0;

    geoEl_t newSurface;

    for(geoEl_t s : surfaces) {
        if(s.reference > id) {
            id = s.reference;
        }

        if(exportParts && !(s.color.Equals(currentColor))) {
            continue;
        }

        if(CompareGeoEl(surfaceMembers, s.members)) {
            // Same surface.
            return;
        }
    }

    newSurface.members = surfaceMembers;
    newSurface.reference = id + 1;
    newSurface.color = currentColor;
    surfaces.push_back(newSurface);
}

// Compare two elements.
// input:
//        membersA -> members of element A
//        membersB -> members of element B
// return:
//        true, if the two elements are the same
bool GeoFileWriter::CompareGeoEl(std::vector<int> membersA, std::vector<int> membersB) {
    size_t matches = 0; // is this the same element?
    size_t size = membersA.size();

    if(size != membersB.size()) {
        return false;
    }
    for(size_t i = 0; i < size; i++) {
        for(size_t j = 0; j < size; j++) {
            if(abs(membersA[i]) == abs(membersB[j])) {
                matches++;
            }
        }
    }

    if(matches == size) {
        return true;
    }

    return false;
}

// Return the list of members of a given curve.
// input:
//        number -> the id number of the curve
// return:
//        a list of member points
std::vector<int> GeoFileWriter::GetEdgeMembers(int number) {
    for(geoEl_t c : curves) {
        if(number == c.reference) {
            return c.members;
        }
    }

    std::vector<int> nullElement = {};
    return nullElement;
}

// Process a single curve: find the cartesian points that define the curve.
// A curve defined by two points is a line, by three points a circle arc,
// by four points a Bezier.
// input:
//        sb -> a bezier curve
// return:
//        the id number of the curve
int GeoFileWriter::ExportCurve(SBezier *sb) {
    int i;
    std::vector<Vector> pointVectors = {};

    for(i = 0; i <= sb->deg; i++) {
        AddPoint(sb->ctrl[i]);
        pointVectors.push_back(sb->ctrl[i]);
    }

    int retNumber = AddCurve((size_t)sb->deg + 1, pointVectors);
    pointVectors.clear();
    return retNumber;
}

// Process a curve loop of a surface: find all the curves that define the loop.
// input:
//        loop -> a list of loops for a given surface
// return:
//        the id number of the loop
int  GeoFileWriter::ExportCurveLoop(SBezierLoop *loop) {
    ssassert(loop->l.n >= 1, "Expected at least one loop");
    std::vector<int> loopMembers = {};

    SBezier *sb = loop->l.Last();
    AddPoint(sb->Finish());

    for(sb = loop->l.First(); sb; sb = loop->l.NextAfter(sb)) {
        int curveNumber = ExportCurve(sb);
        loopMembers.push_back(curveNumber);

        if(loop->l.NextAfter(sb) != NULL) {
            AddPoint(sb->Finish());
        }
    }

    int retNumber = AddLoop(loopMembers);
    loopMembers.clear();
    return retNumber;
}

// Process each surface of the model: find all the curve loops that define the
// surface.
// input:
//        ss -> the surface
//        sbl -> the list of curves that trim the surface
void GeoFileWriter::ExportSurface(SSurface *ss, SBezierList *sbl) {
    int i, j;
    std::vector<int> surfaceMembers;

    // Read the colour of the surface: use it to tell apart surfaces
    // from different parts.
    currentColor = ss->color;

    for(i = 0; i <= ss->degm; i++) {
        for(j = 0; j <= ss->degn; j++) {
            AddPoint(ss->ctrl[i][j]);
        }
    }

    SBezierLoopSetSet sblss = {};
    SPolygon spxyz = {};
    bool allClosed;
    SEdge notClosedAt;
    // We specify a surface, so it doesn't check for coplanarity; and we
    // don't want it to give us any open contours. The polygon and chord
    // tolerance are required, because they are used to calculate the
    // contour directions and determine inner vs. outer contours.
    sblss.FindOuterFacesFrom(sbl, &spxyz, ss,
                             SS.ExportChordTolMm(),
                             &allClosed, &notClosedAt,
                             NULL, NULL,
                             NULL);

    // So in our list of SBezierLoopSet, each set contains at least one loop
    // (the outer boundary), plus any inner loops associated with that outer
    // loop.
    SBezierLoopSet *sbls;
    for(sbls = sblss.l.First(); sbls; sbls = sblss.l.NextAfter(sbls)) {
        SBezierLoop *loop = sbls->l.First();

        // Create the face outer boundary from the outer loop.
        surfaceMembers.push_back(ExportCurveLoop(loop));

        // And create the face inner boundaries from any inner loops that
        // lie within this contour.
        loop = sbls->l.NextAfter(loop);
        for(; loop; loop = sbls->l.NextAfter(loop)) {
            surfaceMembers.push_back(ExportCurveLoop(loop));
        }

    }
    sblss.Clear();
    spxyz.Clear();

    AddSurface(surfaceMembers);
    surfaceMembers.clear();
}

// Main procedure: call this to export a GEO file.
// input:
//        filename -> full path of the exported file
void GeoFileWriter::ExportSurfacesTo(const Platform::Path &filename) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    SShell *shell = &(g->runningShell);

    if(shell->surface.IsEmpty()) {
        Error("The model does not contain any surfaces to export.%s",
              !g->runningMesh.l.IsEmpty()
                  ? "\n\nThe model does contain triangles from a mesh, but "
                    "Solvespace cannot export a triangle mesh as a Geo file. "
                    "Try File -> Export Mesh... instead."
                  : "");
        return;
    }

    f = OpenFile(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.raw.c_str());
        return;
    }

    // Initialization of lists.
    points = {};
    curves = {};
    loops = {};
    surfaces = {};
    std::vector<int> planeSurfaces = {};

    for(SSurface &ss : shell->surface) {
        if(ss.trim.IsEmpty())
            continue;

        // Get all of the loops of Beziers that trim our surface (with each
        // Bezier split so that we use the section as t goes from 0 to 1), and
        // the piecewise linearization of those loops in xyz space.
        SBezierList sbl = {};
        ss.MakeSectionEdgesInto(shell, NULL, &sbl);

        // Apply the export scale factor.
        ss.ScaleSelfBy(1.0/SS.exportScale);
        sbl.ScaleSelfBy(1.0/SS.exportScale);

        ExportSurface(&ss, &sbl);

        sbl.Clear();
    }

    // Print variable declaration.
    fprintf(f, "// Mesh size.\n");
    fprintf(f, "ms = %f;\n", meshSize);

    // Print points.
    for(point_t p : points) {
        fprintf(f, "//+\n");
        fprintf(f, "Point(%d) = {%f, %f, %f, ms};\n", p.reference, CO(p.v));
    }

    // Print curves.
    for(geoEl_t c : curves) {
        fprintf(f, "//+\n");
        if(c.members.size() == 2) {
            // A straight line.
            fprintf(f, "Line(%d) = {%d, %d};\n", c.reference, c.members[0], c.members[1]);
        } else if(c.members.size() == 3) {
            // A circle arc.
            fprintf(f, "Circle(%d) = {%d, %d, %d};\n", c.reference, c.members[0],
                    c.members[1], c.members[2]);
        } else if(c.members.size() == 4) {
            // A Bezier curve.
            fprintf(f, "Bezier(%d) = {%d, %d, %d, %d};\n", c.reference, c.members[0],
                    c.members[1], c.members[2], c.members[3]);
        }
    }

    // Print loops.
    for(geoEl_t l : loops) {
        fprintf(f, "//+\n");
        fprintf(f, "Curve Loop(%d) = {%d", l.reference, l.members[0]);
        for(size_t i = 1; i < l.members.size(); i++) {
            fprintf(f, ", %d", l.members[i]);
        }
        fprintf(f, "};\n");

        // If this loop has over 4 members, it is a plane surface.
        if(l.members.size() > 4) {
            planeSurfaces.push_back(l.reference);
        }
    }

    // Print surfaces.
    for(geoEl_t s : surfaces) {
        fprintf(f, "//+\n");
        if(std::find(planeSurfaces.begin(), planeSurfaces.end(), s.members[0]) !=
           planeSurfaces.end()) {
            // It must be a plane surface.
            fprintf(f, "Plane Surface(%d) = {%d", s.reference, s.members[0]);
        } else {
            // It could be either a plane surface or not.
            fprintf(f, "Surface(%d) = {%d", s.reference, s.members[0]);
        }
        for(size_t i = 1; i < s.members.size(); i++) {
            fprintf(f, ", %d", s.members[i]);
        }
        fprintf(f, "};\n");
    }

    fclose(f);
    points.clear();
    curves.clear();
    loops.clear();
    surfaces.clear();
    planeSurfaces.clear();
}
