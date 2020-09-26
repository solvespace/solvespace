#include "solvespace.h"

namespace SolveSpace {

class CsvImport {
public:
    std::unordered_map<Vector, hEntity, VectorHash, VectorPred> points;

    hEntity findPoint(const Vector &p) {
        auto it = points.find(p);
        if(it == points.end()) return Entity::NO_ENTITY;
        return it->second;
    }

    void processPoint(hEntity he, bool constrain = true) {
        Entity *e = SK.GetEntity(he);
        Vector pos = e->PointGetNum();
        hEntity p = findPoint(pos);
        if(p == he) return;
        if(p != Entity::NO_ENTITY) {
            if(constrain) {
                Constraint::ConstrainCoincident(he, p);
            }
            // We don't add point because we already
            // have point in this position
            return;
        }
        points.emplace(pos, he);
    }
};

void ImportCsv(const Platform::Path &filename) {
    ::FILE *fh = OpenFile(filename, "r");
    if(!fh) {
        Error("Couldn't read from file '%s'", filename.raw.c_str());
        return;
    }
    SS.UndoRemember();
    CsvImport importer = {};
    double lastX, lastY, lastZ;
    bool gotLastPoint = false;
    char line[1024];
    while(fgets(line, (int)sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';
        if(*line == '\0') continue;
        double x, y, z;
        sscanf(line, "%lf, %lf, %lf", &x, &y, &z);
        if (gotLastPoint == true) {
            Vector p0 = Vector::From(lastX, lastY, lastZ);
            Vector p1 = Vector::From(x, y, z);
            if(p0.Equals(p1)) {
                continue;
            }
            hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
            SK.GetEntity(hr.entity(1))->PointForceTo(p0);
            SK.GetEntity(hr.entity(2))->PointForceTo(p1);
            importer.processPoint(hr.entity(1));
            importer.processPoint(hr.entity(2));
        }
        lastX = x;
        lastY = y;
        lastZ = z;
        gotLastPoint = true;
    }
    fclose(fh);
    return;
}

}
