//-----------------------------------------------------------------------------
// Intermediate Data Format (IDF) file reader. Reads an IDF file for PCB outlines and creates
// an equivalent SovleSpace sketch/extrusion. Supports only Linking, not import.
// Part placement is not currently supported.
//
// Copyright 2020 Paul Kahler.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "sketch.h"

// Split a string into substrings separated by spaces.
// Allow quotes to enclose spaces within a string
static std::vector <std::string> splitString(const std::string line) {
    std::vector <std::string> v = {};

    if(line.length() == 0) return v;

    std::string s = "";
    bool inString = false;
    bool inQuotes = false;
    
    for (size_t i=0; i<line.length(); i++) {
        char c = line.at(i);
        if (inQuotes) {
            if (c != '"') {
                s.push_back(c);
            } else {
                v.push_back(s);
                inQuotes = false;
                inString = false;
                s = "";
            }
        } else if (inString) {
            if (c != ' ') {
                s.push_back(c);
            } else {
                v.push_back(s);
                inString = false;
                s = "";
            }
        } else if(c == '"') {
            inString = true;
            inQuotes = true;
        } else if(c != ' ') {
            s = "";
            s.push_back(c);
            inString = true;
        }
    }
    if(s.length() > 0)
        v.push_back(s);

    return v;
}

static bool isHoleDuplicate(EntityList *el, double x, double y, double r) {
    bool duplicate = false;
    for(int i = 0; i < el->n && !duplicate; i++) {
        Entity &en = el->Get(i);
        if(en.type != Entity::Type::CIRCLE)
            continue;
        Entity *distance = el->FindById(en.distance);
        Entity *center   = el->FindById(en.point[0]);
        duplicate =
            center->actPoint.x == x && center->actPoint.y == y && distance->actDistance == r;
    }
    return duplicate;
}

//////////////////////////////////////////////////////////////////////////////
// Functions for linking an IDF file - we need to create entities that
// get remapped into a linked group similar to linking .slvs files
//////////////////////////////////////////////////////////////////////////////

// Make a new point - type doesn't matter since we will make a copy later
static hEntity newPoint(EntityList *el, int *id, Vector p, bool visible = true) {
    Entity en = {};
    en.type = Entity::Type::POINT_N_COPY;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 462;
    en.actPoint = p;
    en.construction = false;
    en.style.v = Style::DATUM;
    en.actVisible = visible;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;    
    el->Add(&en);
    return en.h;
}

static hEntity newLine(EntityList *el, int *id, hEntity p0, hEntity p1, bool keepout) {
    Entity en = {};
    en.type = Entity::Type::LINE_SEGMENT;
    en.point[0] = p0;
    en.point[1] = p1;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 493;
    en.construction = keepout;
    en.style.v = keepout? Style::CONSTRUCTION : Style::ACTIVE_GRP;
    en.actVisible = true;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;    
    el->Add(&en);
    return en.h;
}

static hEntity newNormal(EntityList *el, int *id, Quaternion normal) {
    // normals have parameters, but we don't need them to make a NORMAL_N_COPY from this
    Entity en = {};
    en.type = Entity::Type::NORMAL_N_COPY;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 472;
    en.actNormal = normal;
    en.construction = false;
    en.style.v = Style::ACTIVE_GRP;
    // to be visible we need to add a point.
    en.point[0] = newPoint(el, id, Vector::From(0,0,3), /*visible=*/ true);
    en.actVisible = true;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;    
    el->Add(&en);
    return en.h;
}

static hEntity newArc(EntityList *el, int *id, hEntity p0, hEntity p1, hEntity pc, hEntity hnorm, bool keepout) {
    Entity en = {};
    en.type = Entity::Type::ARC_OF_CIRCLE;
    en.point[0] = pc;
    en.point[1] = p0;
    en.point[2] = p1;
    en.normal = hnorm;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 403;
    en.construction = keepout;
    en.style.v = keepout? Style::CONSTRUCTION : Style::ACTIVE_GRP;
    en.actVisible = true;
    en.forceHidden = false;    *id = *id+1;

    *id = *id + 1;
    en.h.v = *id + en.group.v*65536;
    el->Add(&en);
    return en.h;
}

static hEntity newDistance(EntityList *el, int *id, double distance) {
    // normals have parameters, but we don't need them to make a NORMAL_N_COPY from this
    Entity en = {};
    en.type = Entity::Type::DISTANCE;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 472;
    en.actDistance = distance;
    en.construction = false;
    en.style.v = Style::ACTIVE_GRP;
    // to be visible we'll need to add a point?
    en.actVisible = false;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;    
    el->Add(&en);
    return en.h;
}

static hEntity newCircle(EntityList *el, int *id, hEntity p0, hEntity hdist, hEntity hnorm, bool keepout) {
    Entity en = {};
    en.type = Entity::Type::CIRCLE;
    en.point[0] = p0;
    en.normal = hnorm;
    en.distance = hdist;
    en.extraPoints = 0;
    en.timesApplied = 0;
    en.group.v = 399;
    en.construction = keepout;
    en.style.v = keepout? Style::CONSTRUCTION : Style::ACTIVE_GRP;
    en.actVisible = true;
    en.forceHidden = false;

    *id = *id+1;
    en.h.v = *id + en.group.v*65536;
    el->Add(&en);
    return en.h;
}

static Vector ArcCenter(Vector p0, Vector p1, double angle) {
    // locate the center of an arc
    Vector m = p0.Plus(p1).ScaledBy(0.5);
    Vector perp = Vector::From(p1.y-p0.y, p0.x-p1.x, 0.0).WithMagnitude(1.0);
    double dist = 0;
    if (angle != 180) {
        dist = (p1.Minus(m).Magnitude())/tan(0.5*angle*3.141592653589793/180.0);
    } else {
        dist = 0.0;
    }
    Vector c = m.Minus(perp.ScaledBy(dist));
    return c;
}

// Add an IDF line or arc to the entity list. According to spec, zero angle indicates a line.
// Positive angles are counter clockwise, negative are clockwise. An angle of 360
// indicates a circle centered at x1,y1 passing through x2,y2 and is a complete loop.
static void CreateEntity(EntityList *el, int *id, hEntity h0, hEntity h1, hEntity hnorm,
                    Vector p0, Vector p1, double angle, bool keepout) {
    if (fabs(angle) < 0.1) {
        //line
        if(p0.Equals(p1)) return;

        newLine(el, id, h0, h1, keepout);

    } else if(angle == 360.0) {
        // circle
        double d = p1.Minus(p0).Magnitude();
        hEntity hd = newDistance(el, id, d);
        newCircle(el, id, h1, hd, hnorm, keepout);
        
    } else {
        // arc
        if(angle < 0.0) {
            swap(p0,p1);
            swap(h0,h1);
            angle = fabs(angle);
        }
        // locate the center of the arc
        Vector m = p0.Plus(p1).ScaledBy(0.5);
        Vector perp = Vector::From(p1.y-p0.y, p0.x-p1.x, 0.0).WithMagnitude(1.0);
        // half angle in radians
        double theta = 0.5*angle*PI/180.0;
        double dist = (p1.Minus(m).Magnitude())*cos(theta)/sin(theta);
        Vector c = m.Minus(perp.ScaledBy(dist));
        hEntity hc = newPoint(el, id, c, /*visible=*/false);
        newArc(el, id, h0, h1, hc, hnorm, keepout);
    }
}

// borrowed from Entity::GenerateBezierCurves because we don't have parameters.
static void MakeBeziersForArcs(SBezierList *sbl, Vector center, Vector pa, Vector pb,
                               Quaternion q, double angle) {

    Vector u = q.RotationU(), v = q.RotationV();
    double r = pa.Minus(center).Magnitude();
    double theta, dtheta;
    
    if(angle == 360.0) {
        theta = 0;
    } else {
        Point2d c2  = center.Project2d(u, v);
        Point2d pa2 = (pa.Project2d(u, v)).Minus(c2);

        theta = atan2(pa2.y, pa2.x);
    }
    dtheta = angle * PI/180;
    
    int i, n;
    if(dtheta > (3*PI/2 + 0.01)) {
        n = 4;
    } else if(dtheta > (PI + 0.01)) {
        n = 3;
    } else if(dtheta > (PI/2 + 0.01)) {
        n = 2;
    } else {
        n = 1;
    }
    dtheta /= n;

    for(i = 0; i < n; i++) {
        double s, c;

        c = cos(theta);
        s = sin(theta);
        // The start point of the curve, and the tangent vector at
        // that start point.
        Vector p0 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
               t0 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

        theta += dtheta;

        c = cos(theta);
        s = sin(theta);
        Vector p2 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
               t2 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

        // The control point must lie on both tangents.
        Vector p1 = Vector::AtIntersectionOfLines(p0, p0.Plus(t0),
                                                  p2, p2.Plus(t2),
                                                  NULL);

        SBezier sb = SBezier::From(p0, p1, p2);
        sb.weight[1] = cos(dtheta/2);
        sbl->l.Add(&sb);
    }
}

namespace SolveSpace {

// Here we read the important section of an IDF file. SolveSpace Entities are directly created by
// the functions above, which is only OK because of the way linking works. For example points do
// not have handles for solver parameters (coordinates), they only have their actPoint values
// set (or actNormal or actDistance). These are incomplete entities and would be a problem if
// they were part of the sketch, but they are not. After making a list of them here, a new group
// gets created from copies of these. Those copies are complete and part of the sketch group.
bool LinkIDF(const Platform::Path &filename, EntityList *el, SMesh *m, SShell *sh) {
    dbp("\nLink IDF board outline.");
    el->Clear();
    std::string data;
    if(!ReadFile(filename, &data)) {
        Error("Couldn't read from '%s'", filename.raw.c_str());
        return false;
    }
    
    enum IDF_SECTION {
        none,
        header,
        board_outline,
        other_outline,
        routing_outline,
        placement_outline,
        routing_keepout,
        via_keepout,
        placement_group,
        drilled_holes,
        notes,
        component_placement
    } section;

    section = IDF_SECTION::none;
    int record_number = 0;
    int curve = -1;
    int entityCount = 0;
    
    hEntity hprev;
    hEntity hprevTop;
    Vector pprev = Vector::From(0,0,0);
    Vector pprevTop = Vector::From(0,0,0);
    
    double board_thickness = 10.0;
    double scale = 1.0; //mm
    bool topEntities       = false;
    bool bottomEntities    = false;

    Quaternion normal = Quaternion::From(Vector::From(1,0,0), Vector::From(0,1,0));
    hEntity hnorm = newNormal(el, &entityCount, normal);

    // to create the extursion we will need to collect a set of bezier curves defined
    // by the perimeter, cutouts, and holes.
    SBezierList sbl = {};
    
    std::stringstream stream(data);
    for(std::string line; getline( stream, line ); ) {
        if (line.find(".END_") == 0) {
            section = none;
            curve = -1;
        }
        switch (section) {
            case none:
                if(line.find(".HEADER") == 0) {
                    section = header;
                    record_number = 1;
                } else if (line.find(".BOARD_OUTLINE") == 0) {
                    section = board_outline;
                    record_number = 1;
                } else if (line.find(".ROUTE_KEEPOUT") == 0) {
                    section = routing_keepout;
                    record_number = 1;
                } else if(line.find(".DRILLED_HOLES") == 0) {
                    section = drilled_holes;
                    record_number = 1;
                }
                break;
                
            case header:
                if(record_number == 3) {
                    if(line.find("MM") != std::string::npos) {
                        dbp("IDF units are MM");
                        scale = 1.0;
                    } else if(line.find("THOU") != std::string::npos) {
                        dbp("IDF units are thousandths of an inch");
                        scale = 0.0254;
                    } else {
                        dbp("IDF import, no units found in file.");
                    }
                }
                break;
            
            case routing_keepout:   
            case board_outline:
                if (record_number == 2) {
                    if(section == board_outline) {
                        topEntities = true;
                        bottomEntities = true;
                        board_thickness = std::stod(line) * scale;
                        dbp("IDF board thickness: %lf", board_thickness);
                    } else if (section == routing_keepout) {
                        topEntities = false;
                        bottomEntities = false;
                        if(line.find("TOP") == 0 || line.find("BOTH") == 0)
                            topEntities = true;
                        if(line.find("BOTTOM") == 0 || line.find("BOTH") == 0)
                            bottomEntities = true;
                    }
                } else { // records 3+ are lines, arcs, and circles
                    std::vector <std::string> values = splitString(line);
                    if(values.size() != 4) continue;
                    int c = stoi(values[0]);
                    double x = stof(values[1]);
                    double y = stof(values[2]);
                    double ang = stof(values[3]);
                    Vector point = Vector::From(x,y,0.0);
                    Vector pTop = Vector::From(x,y,board_thickness);
                    if(c != curve) { // start a new curve
                        curve = c;
                        if (bottomEntities)
                            hprev = newPoint(el, &entityCount, point, /*visible=*/false);
                        if (topEntities)
                            hprevTop = newPoint(el, &entityCount, pTop, /*visible=*/false);
                        pprev = point;
                        pprevTop = pTop;
                    } else {
                        if(section == board_outline) {
                            // create a bezier for the extrusion
                            if (ang == 0) {
                                // straight lines
                                SBezier sb = SBezier::From(pprev, point);
                                sbl.l.Add(&sb);
                            } else if (ang != 360.0) {
                                // Arcs
                                Vector c = ArcCenter(pprev, point, ang);
                                MakeBeziersForArcs(&sbl, c, pprev, point, normal, ang);
                            } else {
                                // circles
                                MakeBeziersForArcs(&sbl, point, pprev, pprev, normal, ang);
                            }
                        }
                        // next create the entities
                        // only curves and points at circle centers will be visible
                        bool vis = (ang == 360.0);
                        if (bottomEntities) {
                            hEntity hp = newPoint(el, &entityCount, point, /*visible=*/vis);
                            CreateEntity(el, &entityCount, hprev, hp, hnorm, pprev, point, ang,
                                (section == routing_keepout) );
                            pprev = point;
                            hprev = hp;
                        }
                        if (topEntities) {
                            hEntity hp = newPoint(el, &entityCount, pTop, /*visible=*/vis);
                            CreateEntity(el, &entityCount, hprevTop, hp, hnorm, pprevTop, pTop,
                                 ang, (section == routing_keepout) );
                            pprevTop = pTop;
                            hprevTop = hp;
                        }
                    }
                }
                break;

            case other_outline:
            case routing_outline:
            case placement_outline:
            case via_keepout:
            case placement_group:
                break;

            case drilled_holes: {
                    std::vector <std::string> values = splitString(line);
                    if(values.size() < 6) continue;
                    double d = stof(values[0]);
                    double x = stof(values[1]);
                    double y = stof(values[2]);
                    bool duplicate = isHoleDuplicate(el, x, y, d / 2);
                    // Only show holes likely to be useful in MCAD to reduce complexity.
                    if(((d > 1.7) || (values[5].compare(0,3,"PIN") == 0)
                         || (values[5].compare(0,3,"MTG") == 0)) && !duplicate) {
                        // create the entity
                        Vector cent = Vector::From(x,y,0.0);
                        hEntity hcent = newPoint(el, &entityCount, cent);
                        hEntity hdist = newDistance(el, &entityCount, d/2);
                        newCircle(el, &entityCount, hcent, hdist, hnorm, false);
                        // and again for the top
                        Vector cTop = Vector::From(x,y,board_thickness);
                        hcent = newPoint(el, &entityCount, cTop);
                        hdist = newDistance(el, &entityCount, d/2);
                        newCircle(el, &entityCount, hcent, hdist, hnorm, false);
                        // create the curves for the extrusion
                        Vector pt = Vector::From(x+d/2, y, 0.0);
                        MakeBeziersForArcs(&sbl, cent, pt, pt, normal, 360.0);
                    }

                    break;
                }            
            case notes:
            case component_placement:
                break;
                
            default:
                section = none;
                break;
        }
        record_number++;
    }
    // now we can create an extrusion from all the Bezier curves. We can skip things
    // like checking for a coplanar sketch because everything is at z=0.
    SPolygon polyLoops = {};
    bool allClosed;
    bool allCoplanar;
    Vector errorPointAt = Vector::From(0,0,0);
    SEdge errorAt = {};
    
    SBezierLoopSetSet sblss = {};
    sblss.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                             100.0, &allClosed, &errorAt,
                             &allCoplanar, &errorPointAt, NULL);

    //hack for when there is no sketch yet and the first group is a linked IDF
    double ctc = SS.chordTolCalculated;
    if(ctc == 0.0) SS.chordTolCalculated = 0.1; //mm
    // there should only by one sbls in the sblss unless a board has disjointed parts...
    sh->MakeFromExtrusionOf(sblss.l.First(), Vector::From(0.0, 0.0, 0.0),
                                   Vector::From(0.0, 0.0, board_thickness),
                                   RgbaColor::From(0, 180, 0) );
    SS.chordTolCalculated = ctc;
    sblss.Clear();
    sbl.Clear();
    sh->booleanFailed = false;
    
    return true;
}

}
