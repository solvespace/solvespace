//-----------------------------------------------------------------------------
// Triangle mesh file reader. Reads an STL file triangle mesh and creates
// a SovleSpace SMesh from it. Supports only Linking, not import.
//
// Copyright 2020 Paul Kahler.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "sketch.h"
#include <vector>

#define MIN_POINT_DISTANCE 0.001

// we will check for duplicate vertices and keep all their normals
class vertex {
public:
    Vector p;
    std::vector<Vector> normal;
};

static bool isEdgeVertex(vertex &v) {
    unsigned int i,j;
    bool result = false;
    for(i=0;i<v.normal.size();i++) {
        for(j=i;j<v.normal.size();j++) {
            if(v.normal[i].Dot(v.normal[j]) < 0.9) {
                result = true;
            }
        }
    }
    return result;
}
// This function has poor performance, used inside a loop it is O(n**2)
static void addUnique(std::vector<vertex> &lv, Vector &p, Vector &n) {
    unsigned int i;
    for(i=0; i<lv.size(); i++) {
        if(lv[i].p.Equals(p, MIN_POINT_DISTANCE)) {
            break;
        }
    }
    if(i==lv.size()) {
        vertex v;
        v.p = p;
        lv.push_back(v);
    }
    // we could improve a little by only storing unique normals
    lv[i].normal.push_back(n);
};

// Make a new point - type doesn't matter since we will make a copy later
static hEntity newPoint(EntityList *el, int *id, Vector p) {
    Entity en = {};
    en.type = Entity::Type::POINT_N_COPY;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 462;
    en.actPoint = p;
    en.construction = false;
    en.style.v = Style::DATUM;
    en.actVisible = true;
    en.forceHidden = false;

    en.h.v = *id + en.group.v*65536; 
    *id = *id+1;   
    el->Add(&en);
    return en.h;
}

// check if a vertex is unique and add it via newPoint if it is.
static void addVertex(EntityList *el, Vector v) {
    if(el->n < 15000) {
        int id = el->n;
        newPoint(el, &id, v);
    }
}

static hEntity newNormal(EntityList *el, int *id, Quaternion normal, hEntity p) {
    // normals have parameters, but we don't need them to make a NORMAL_N_COPY from this
    Entity en = {};
    en.type = Entity::Type::NORMAL_N_COPY;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 472;
    en.actNormal = normal;
    en.construction = false;
    en.style.v = Style::NORMALS;
    // to be visible we need to add a point.
//    en.point[0] = newPoint(el, id, Vector::From(0,0,0));
    en.point[0] = p;
    en.actVisible = true;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;    
    el->Add(&en);
    return en.h;
}

static hEntity newLine(EntityList *el, int *id, hEntity p0, hEntity p1) {
    Entity en = {};
    en.type = Entity::Type::LINE_SEGMENT;
    en.point[0] = p0;
    en.point[1] = p1;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 493;
    en.construction = true;
    en.style.v = Style::CONSTRUCTION;
    en.actVisible = true;
    en.forceHidden = false;

    en.h.v = *id + en.group.v*65536;
    *id = *id + 1;
    el->Add(&en);
    return en.h;
}

namespace SolveSpace {

bool LinkStl(const Platform::Path &filename, EntityList *el, SMesh *m, SShell *sh) {
    dbp("\nLink STL triangle mesh.");
    el->Clear();
    std::string data;
    if(!ReadFile(filename, &data)) {
        Error("Couldn't read from '%s'", filename.raw.c_str());
        return false;
    }
    
    std::stringstream f(data);

    char str[80] = {};
    f.read(str, 80);
    
    if(0==memcmp("solid", str, 5)) {
    // just returning false will trigger the warning that linked file is not present
    // best solution is to add an importer for text STL.
        Message(_("Text-formated STL files are not currently supported"));
        return false;
    }
    
    uint32_t n;
    uint32_t color;
    
    f.read((char*)&n, 4);
    dbp("%d triangles", n);
    
    float x,y,z;
    float xn,yn,zn;
    
    std::vector<vertex> verts = {};
    
    for(uint32_t i = 0; i<n; i++) {
        STriangle tr = {};

        // read the triangle normal
        f.read((char*)&xn, 4);
        f.read((char*)&yn, 4);
        f.read((char*)&zn, 4);
        tr.an = Vector::From(xn,yn,zn);
        tr.bn = tr.an;
        tr.cn = tr.an;

        f.read((char*)&x, 4);
        f.read((char*)&y, 4);
        f.read((char*)&z, 4);
        tr.a.x = x;
        tr.a.y = y;
        tr.a.z = z;

        f.read((char*)&x, 4);
        f.read((char*)&y, 4);
        f.read((char*)&z, 4);
        tr.b.x = x;
        tr.b.y = y;
        tr.b.z = z;

        f.read((char*)&x, 4);
        f.read((char*)&y, 4);
        f.read((char*)&z, 4);
        tr.c.x = x;
        tr.c.y = y;
        tr.c.z = z;

        f.read((char*)&color,2);
        if(color & 0x8000) {
            tr.meta.color.red = (color >> 7) & 0xf8;
            tr.meta.color.green = (color >> 2) & 0xf8;
            tr.meta.color.blue = (color << 3);
            tr.meta.color.alpha = 255;
        } else {
            tr.meta.color.red = 90;
            tr.meta.color.green = 120;
            tr.meta.color.blue = 140;
            tr.meta.color.alpha = 255;        
        }

        m->AddTriangle(&tr);
        Vector normal = tr.Normal().WithMagnitude(1.0);
        addUnique(verts, tr.a, normal);
        addUnique(verts, tr.b, normal);
        addUnique(verts, tr.c, normal);
    }
    dbp("%d vertices", verts.size());

    int id = 1;

    //add the STL origin and normals
    hEntity origin = newPoint(el, &id, Vector::From(0.0, 0.0, 0.0));    
    newNormal(el, &id, Quaternion::From(Vector::From(1,0,0),Vector::From(0,1,0)), origin);
    newNormal(el, &id, Quaternion::From(Vector::From(0,1,0),Vector::From(0,0,1)), origin);
    newNormal(el, &id, Quaternion::From(Vector::From(0,0,1),Vector::From(1,0,0)), origin);

    BBox box = {};
    box.minp = verts[0].p;
    box.maxp = verts[0].p;

    // determine the bounding box for all vertexes
    for(unsigned int i=1; i<verts.size(); i++) {
        box.Include(verts[i].p);
    }

    hEntity p[8];
    p[0] = newPoint(el, &id, Vector::From(box.minp.x, box.minp.y, box.minp.z));
    p[1] = newPoint(el, &id, Vector::From(box.maxp.x, box.minp.y, box.minp.z));
    p[2] = newPoint(el, &id, Vector::From(box.minp.x, box.maxp.y, box.minp.z));
    p[3] = newPoint(el, &id, Vector::From(box.maxp.x, box.maxp.y, box.minp.z));
    p[4] = newPoint(el, &id, Vector::From(box.minp.x, box.minp.y, box.maxp.z));
    p[5] = newPoint(el, &id, Vector::From(box.maxp.x, box.minp.y, box.maxp.z));
    p[6] = newPoint(el, &id, Vector::From(box.minp.x, box.maxp.y, box.maxp.z));
    p[7] = newPoint(el, &id, Vector::From(box.maxp.x, box.maxp.y, box.maxp.z));

    newLine(el, &id, p[0], p[1]);
    newLine(el, &id, p[0], p[2]);
    newLine(el, &id, p[3], p[1]);
    newLine(el, &id, p[3], p[2]);

    newLine(el, &id, p[4], p[5]);
    newLine(el, &id, p[4], p[6]);
    newLine(el, &id, p[7], p[5]);
    newLine(el, &id, p[7], p[6]);

    newLine(el, &id, p[0], p[4]);
    newLine(el, &id, p[1], p[5]);
    newLine(el, &id, p[2], p[6]);
    newLine(el, &id, p[3], p[7]);
    
    for(unsigned int i=0; i<verts.size(); i++) {
        // create point entities for edge vertexes
        if(isEdgeVertex(verts[i])) {
           addVertex(el, verts[i].p);
        }
    }

    return true;
}

}
