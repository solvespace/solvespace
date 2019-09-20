//-----------------------------------------------------------------------------
// Routines to write and read our .slvs file format.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#define VERSION_STRING "\261\262\263" "SolveSpaceREVa"

static int StrStartsWith(const char *str, const char *start) {
    return memcmp(str, start, strlen(start)) == 0;
}

//-----------------------------------------------------------------------------
// Clear and free all the dynamic memory associated with our currently-loaded
// sketch. This does not leave the program in an acceptable state (with the
// references created, and so on), so anyone calling this must fix that later.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ClearExisting() {
    UndoClearStack(&redo);
    UndoClearStack(&undo);

    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        g->Clear();
    }

    SK.constraint.Clear();
    SK.request.Clear();
    SK.group.Clear();
    SK.groupOrder.Clear();
    SK.style.Clear();

    SK.entity.Clear();
    SK.param.Clear();
    images.clear();
}

hGroup SolveSpaceUI::CreateDefaultDrawingGroup() {
    Group g = {};

    // And an empty group, for the first stuff the user draws.
    g.visible = true;
    g.name = C_("group-name", "sketch-in-plane");
    g.type = Group::Type::DRAWING_WORKPLANE;
    g.subtype = Group::Subtype::WORKPLANE_BY_POINT_ORTHO;
    g.order = 1;
    g.predef.q = Quaternion::From(1, 0, 0, 0);
    hRequest hr = Request::HREQUEST_REFERENCE_XY;
    g.predef.origin = hr.entity(1);
    SK.group.AddAndAssignId(&g);
    SK.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    return g.h;
}

void SolveSpaceUI::NewFile() {
    ClearExisting();

    // Our initial group, that contains the references.
    Group g = {};
    g.visible = true;
    g.name = C_("group-name", "#references");
    g.type = Group::Type::DRAWING_3D;
    g.order = 0;
    g.h = Group::HGROUP_REFERENCES;
    SK.group.Add(&g);

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r = {};
    r.type = Request::Type::WORKPLANE;
    r.group = Group::HGROUP_REFERENCES;
    r.workplane = Entity::FREE_IN_3D;

    r.h = Request::HREQUEST_REFERENCE_XY;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_YZ;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_ZX;
    SK.request.Add(&r);

    CreateDefaultDrawingGroup();
}

const SolveSpaceUI::SaveTable SolveSpaceUI::SAVED[] = {
    { 'g',  "Group.h.v",                'x',    &(SS.sv.g.h.v)                },
    { 'g',  "Group.type",               'd',    &(SS.sv.g.type)               },
    { 'g',  "Group.order",              'd',    &(SS.sv.g.order)              },
    { 'g',  "Group.name",               'S',    &(SS.sv.g.name)               },
    { 'g',  "Group.activeWorkplane.v",  'x',    &(SS.sv.g.activeWorkplane.v)  },
    { 'g',  "Group.opA.v",              'x',    &(SS.sv.g.opA.v)              },
    { 'g',  "Group.opB.v",              'x',    &(SS.sv.g.opB.v)              },
    { 'g',  "Group.valA",               'f',    &(SS.sv.g.valA)               },
    { 'g',  "Group.valB",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.valC",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.color",              'c',    &(SS.sv.g.color)              },
    { 'g',  "Group.subtype",            'd',    &(SS.sv.g.subtype)            },
    { 'g',  "Group.skipFirst",          'b',    &(SS.sv.g.skipFirst)          },
    { 'g',  "Group.meshCombine",        'd',    &(SS.sv.g.meshCombine)        },
    { 'g',  "Group.forceToMesh",        'd',    &(SS.sv.g.forceToMesh)        },
    { 'g',  "Group.predef.q.w",         'f',    &(SS.sv.g.predef.q.w)         },
    { 'g',  "Group.predef.q.vx",        'f',    &(SS.sv.g.predef.q.vx)        },
    { 'g',  "Group.predef.q.vy",        'f',    &(SS.sv.g.predef.q.vy)        },
    { 'g',  "Group.predef.q.vz",        'f',    &(SS.sv.g.predef.q.vz)        },
    { 'g',  "Group.predef.origin.v",    'x',    &(SS.sv.g.predef.origin.v)    },
    { 'g',  "Group.predef.entityB.v",   'x',    &(SS.sv.g.predef.entityB.v)   },
    { 'g',  "Group.predef.entityC.v",   'x',    &(SS.sv.g.predef.entityC.v)   },
    { 'g',  "Group.predef.swapUV",      'b',    &(SS.sv.g.predef.swapUV)      },
    { 'g',  "Group.predef.negateU",     'b',    &(SS.sv.g.predef.negateU)     },
    { 'g',  "Group.predef.negateV",     'b',    &(SS.sv.g.predef.negateV)     },
    { 'g',  "Group.visible",            'b',    &(SS.sv.g.visible)            },
    { 'g',  "Group.suppress",           'b',    &(SS.sv.g.suppress)           },
    { 'g',  "Group.relaxConstraints",   'b',    &(SS.sv.g.relaxConstraints)   },
    { 'g',  "Group.allowRedundant",     'b',    &(SS.sv.g.allowRedundant)     },
    { 'g',  "Group.allDimsReference",   'b',    &(SS.sv.g.allDimsReference)   },
    { 'g',  "Group.scale",              'f',    &(SS.sv.g.scale)              },
    { 'g',  "Group.remap",              'M',    &(SS.sv.g.remap)              },
    { 'g',  "Group.impFile",            'i',    NULL                          },
    { 'g',  "Group.impFileRel",         'P',    &(SS.sv.g.linkFile)           },

    { 'p',  "Param.h.v.",               'x',    &(SS.sv.p.h.v)                },
    { 'p',  "Param.val",                'f',    &(SS.sv.p.val)                },

    { 'r',  "Request.h.v",              'x',    &(SS.sv.r.h.v)                },
    { 'r',  "Request.type",             'd',    &(SS.sv.r.type)               },
    { 'r',  "Request.extraPoints",      'd',    &(SS.sv.r.extraPoints)        },
    { 'r',  "Request.workplane.v",      'x',    &(SS.sv.r.workplane.v)        },
    { 'r',  "Request.group.v",          'x',    &(SS.sv.r.group.v)            },
    { 'r',  "Request.construction",     'b',    &(SS.sv.r.construction)       },
    { 'r',  "Request.style",            'x',    &(SS.sv.r.style)              },
    { 'r',  "Request.str",              'S',    &(SS.sv.r.str)                },
    { 'r',  "Request.font",             'S',    &(SS.sv.r.font)               },
    { 'r',  "Request.file",             'P',    &(SS.sv.r.file)               },
    { 'r',  "Request.aspectRatio",      'f',    &(SS.sv.r.aspectRatio)        },

    { 'e',  "Entity.h.v",               'x',    &(SS.sv.e.h.v)                },
    { 'e',  "Entity.type",              'd',    &(SS.sv.e.type)               },
    { 'e',  "Entity.construction",      'b',    &(SS.sv.e.construction)       },
    { 'e',  "Entity.style",             'x',    &(SS.sv.e.style)              },
    { 'e',  "Entity.str",               'S',    &(SS.sv.e.str)                },
    { 'e',  "Entity.font",              'S',    &(SS.sv.e.font)               },
    { 'e',  "Entity.file",              'P',    &(SS.sv.e.file)               },
    { 'e',  "Entity.point[0].v",        'x',    &(SS.sv.e.point[0].v)         },
    { 'e',  "Entity.point[1].v",        'x',    &(SS.sv.e.point[1].v)         },
    { 'e',  "Entity.point[2].v",        'x',    &(SS.sv.e.point[2].v)         },
    { 'e',  "Entity.point[3].v",        'x',    &(SS.sv.e.point[3].v)         },
    { 'e',  "Entity.point[4].v",        'x',    &(SS.sv.e.point[4].v)         },
    { 'e',  "Entity.point[5].v",        'x',    &(SS.sv.e.point[5].v)         },
    { 'e',  "Entity.point[6].v",        'x',    &(SS.sv.e.point[6].v)         },
    { 'e',  "Entity.point[7].v",        'x',    &(SS.sv.e.point[7].v)         },
    { 'e',  "Entity.point[8].v",        'x',    &(SS.sv.e.point[8].v)         },
    { 'e',  "Entity.point[9].v",        'x',    &(SS.sv.e.point[9].v)         },
    { 'e',  "Entity.point[10].v",       'x',    &(SS.sv.e.point[10].v)        },
    { 'e',  "Entity.point[11].v",       'x',    &(SS.sv.e.point[11].v)        },
    { 'e',  "Entity.extraPoints",       'd',    &(SS.sv.e.extraPoints)        },
    { 'e',  "Entity.normal.v",          'x',    &(SS.sv.e.normal.v)           },
    { 'e',  "Entity.distance.v",        'x',    &(SS.sv.e.distance.v)         },
    { 'e',  "Entity.workplane.v",       'x',    &(SS.sv.e.workplane.v)        },
    { 'e',  "Entity.actPoint.x",        'f',    &(SS.sv.e.actPoint.x)         },
    { 'e',  "Entity.actPoint.y",        'f',    &(SS.sv.e.actPoint.y)         },
    { 'e',  "Entity.actPoint.z",        'f',    &(SS.sv.e.actPoint.z)         },
    { 'e',  "Entity.actNormal.w",       'f',    &(SS.sv.e.actNormal.w)        },
    { 'e',  "Entity.actNormal.vx",      'f',    &(SS.sv.e.actNormal.vx)       },
    { 'e',  "Entity.actNormal.vy",      'f',    &(SS.sv.e.actNormal.vy)       },
    { 'e',  "Entity.actNormal.vz",      'f',    &(SS.sv.e.actNormal.vz)       },
    { 'e',  "Entity.actDistance",       'f',    &(SS.sv.e.actDistance)        },
    { 'e',  "Entity.actVisible",        'b',    &(SS.sv.e.actVisible),        },


    { 'c',  "Constraint.h.v",           'x',    &(SS.sv.c.h.v)                },
    { 'c',  "Constraint.type",          'd',    &(SS.sv.c.type)               },
    { 'c',  "Constraint.group.v",       'x',    &(SS.sv.c.group.v)            },
    { 'c',  "Constraint.workplane.v",   'x',    &(SS.sv.c.workplane.v)        },
    { 'c',  "Constraint.valA",          'f',    &(SS.sv.c.valA)               },
    { 'c',  "Constraint.valP.v",        'x',    &(SS.sv.c.valP.v)             },
    { 'c',  "Constraint.ptA.v",         'x',    &(SS.sv.c.ptA.v)              },
    { 'c',  "Constraint.ptB.v",         'x',    &(SS.sv.c.ptB.v)              },
    { 'c',  "Constraint.entityA.v",     'x',    &(SS.sv.c.entityA.v)          },
    { 'c',  "Constraint.entityB.v",     'x',    &(SS.sv.c.entityB.v)          },
    { 'c',  "Constraint.entityC.v",     'x',    &(SS.sv.c.entityC.v)          },
    { 'c',  "Constraint.entityD.v",     'x',    &(SS.sv.c.entityD.v)          },
    { 'c',  "Constraint.other",         'b',    &(SS.sv.c.other)              },
    { 'c',  "Constraint.other2",        'b',    &(SS.sv.c.other2)             },
    { 'c',  "Constraint.reference",     'b',    &(SS.sv.c.reference)          },
    { 'c',  "Constraint.comment",       'S',    &(SS.sv.c.comment)            },
    { 'c',  "Constraint.disp.offset.x", 'f',    &(SS.sv.c.disp.offset.x)      },
    { 'c',  "Constraint.disp.offset.y", 'f',    &(SS.sv.c.disp.offset.y)      },
    { 'c',  "Constraint.disp.offset.z", 'f',    &(SS.sv.c.disp.offset.z)      },
    { 'c',  "Constraint.disp.style",    'x',    &(SS.sv.c.disp.style)         },

    { 's',  "Style.h.v",                'x',    &(SS.sv.s.h.v)                },
    { 's',  "Style.name",               'S',    &(SS.sv.s.name)               },
    { 's',  "Style.width",              'f',    &(SS.sv.s.width)              },
    { 's',  "Style.widthAs",            'd',    &(SS.sv.s.widthAs)            },
    { 's',  "Style.textHeight",         'f',    &(SS.sv.s.textHeight)         },
    { 's',  "Style.textHeightAs",       'd',    &(SS.sv.s.textHeightAs)       },
    { 's',  "Style.textAngle",          'f',    &(SS.sv.s.textAngle)          },
    { 's',  "Style.textOrigin",         'x',    &(SS.sv.s.textOrigin)         },
    { 's',  "Style.color",              'c',    &(SS.sv.s.color)              },
    { 's',  "Style.fillColor",          'c',    &(SS.sv.s.fillColor)          },
    { 's',  "Style.filled",             'b',    &(SS.sv.s.filled)             },
    { 's',  "Style.visible",            'b',    &(SS.sv.s.visible)            },
    { 's',  "Style.exportable",         'b',    &(SS.sv.s.exportable)         },
    { 's',  "Style.stippleType",        'd',    &(SS.sv.s.stippleType)        },
    { 's',  "Style.stippleScale",       'f',    &(SS.sv.s.stippleScale)       },

    { 0, NULL, 0, NULL }
};

struct SAVEDptr {
    EntityMap      &M() { return *((EntityMap *)this); }
    std::string    &S() { return *((std::string *)this); }
    Platform::Path &P() { return *((Platform::Path *)this); }
    bool      &b() { return *((bool *)this); }
    RgbaColor &c() { return *((RgbaColor *)this); }
    int       &d() { return *((int *)this); }
    double    &f() { return *((double *)this); }
    uint32_t  &x() { return *((uint32_t *)this); }
};

void SolveSpaceUI::SaveUsingTable(const Platform::Path &filename, int type) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(SAVED[i].type != type) continue;

        int fmt = SAVED[i].fmt;
        SAVEDptr *p = (SAVEDptr *)SAVED[i].ptr;
        // Any items that aren't specified are assumed to be zero
        if(fmt == 'S' && p->S().empty())          continue;
        if(fmt == 'P' && p->P().IsEmpty())        continue;
        if(fmt == 'd' && p->d() == 0)             continue;
        if(fmt == 'f' && EXACT(p->f() == 0.0))    continue;
        if(fmt == 'x' && p->x() == 0)             continue;
        if(fmt == 'i')                            continue;

        fprintf(fh, "%s=", SAVED[i].desc);
        switch(fmt) {
            case 'S': fprintf(fh, "%s",    p->S().c_str());       break;
            case 'b': fprintf(fh, "%d",    p->b() ? 1 : 0);       break;
            case 'c': fprintf(fh, "%08x",  p->c().ToPackedInt()); break;
            case 'd': fprintf(fh, "%d",    p->d());               break;
            case 'f': fprintf(fh, "%.20f", p->f());               break;
            case 'x': fprintf(fh, "%08x",  p->x());               break;

            case 'P': {
                if(!p->P().IsEmpty()) {
                    Platform::Path relativePath = p->P().RelativeTo(filename.Parent());
                    ssassert(!relativePath.IsEmpty(), "Cannot relativize path");
                    fprintf(fh, "%s", relativePath.ToPortable().c_str());
                }
                break;
            }

            case 'M': {
                fprintf(fh, "{\n");
                // Sort the mapping, since EntityMap is not deterministic.
                std::vector<std::pair<EntityKey, EntityId>> sorted(p->M().begin(), p->M().end());
                std::sort(sorted.begin(), sorted.end(),
                    [](std::pair<EntityKey, EntityId> &a, std::pair<EntityKey, EntityId> &b) {
                        return a.second.v < b.second.v;
                    });
                for(auto it : sorted) {
                    fprintf(fh, "    %d %08x %d\n",
                            it.second.v, it.first.input.v, it.first.copyNumber);
                }
                fprintf(fh, "}");
                break;
            }

            case 'i': break;

            default: ssassert(false, "Unexpected value format");
        }
        fprintf(fh, "\n");
    }
}

bool SolveSpaceUI::SaveToFile(const Platform::Path &filename) {
    // Make sure all the entities are regenerated up to date, since they will be exported.
    SS.ScheduleShowTW();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    for(Group &g : SK.group) {
        if(g.type != Group::Type::LINKED) continue;

        if(g.linkFile.RelativeTo(filename).IsEmpty()) {
            Error("This sketch links the sketch '%s'; it can only be saved "
                  "on the same volume.", g.linkFile.raw.c_str());
            return false;
        }
    }

    fh = OpenFile(filename, "wb");
    if(!fh) {
        Error("Couldn't write to file '%s'", filename.raw.c_str());
        return false;
    }

    fprintf(fh, "%s\n\n\n", VERSION_STRING);

    int i, j;
    for(auto &g : SK.group) {
        sv.g = g;
        SaveUsingTable(filename, 'g');
        fprintf(fh, "AddGroup\n\n");
    }

    for(auto &p : SK.param) {
        sv.p = p;
        SaveUsingTable(filename, 'p');
        fprintf(fh, "AddParam\n\n");
    }

    for(auto &r : SK.request) {
        sv.r = r;
        SaveUsingTable(filename, 'r');
        fprintf(fh, "AddRequest\n\n");
    }

    for(auto &e : SK.entity) {
        e.CalculateNumerical(/*forExport=*/true);
        sv.e = e;
        SaveUsingTable(filename, 'e');
        fprintf(fh, "AddEntity\n\n");
    }

    for(auto &c : SK.constraint) {
        sv.c = c;
        SaveUsingTable(filename, 'c');
        fprintf(fh, "AddConstraint\n\n");
    }

    for(auto &s : SK.style) {
        sv.s = s;
        if(sv.s.h.v >= Style::FIRST_CUSTOM) {
            SaveUsingTable(filename, 's');
            fprintf(fh, "AddStyle\n\n");
        }
    }

    // A group will have either a mesh or a shell, but not both; but the code
    // to print either of those just does nothing if the mesh/shell is empty.

    Group *g = SK.GetGroup(*SK.groupOrder.Last());
    SMesh *m = &g->runningMesh;
    for(i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l[i]);
        fprintf(fh, "Triangle %08x %08x "
                "%.20f %.20f %.20f  %.20f %.20f %.20f  %.20f %.20f %.20f\n",
            tr->meta.face, tr->meta.color.ToPackedInt(),
            CO(tr->a), CO(tr->b), CO(tr->c));
    }

    SShell *s = &g->runningShell;
    SSurface *srf;
    for(srf = s->surface.First(); srf; srf = s->surface.NextAfter(srf)) {
        fprintf(fh, "Surface %08x %08x %08x %d %d\n",
            srf->h.v, srf->color.ToPackedInt(), srf->face, srf->degm, srf->degn);
        for(i = 0; i <= srf->degm; i++) {
            for(j = 0; j <= srf->degn; j++) {
                fprintf(fh, "SCtrl %d %d %.20f %.20f %.20f Weight %20.20f\n",
                    i, j, CO(srf->ctrl[i][j]), srf->weight[i][j]);
            }
        }

        STrimBy *stb;
        for(stb = srf->trim.First(); stb; stb = srf->trim.NextAfter(stb)) {
            fprintf(fh, "TrimBy %08x %d %.20f %.20f %.20f  %.20f %.20f %.20f\n",
                stb->curve.v, stb->backwards ? 1 : 0,
                CO(stb->start), CO(stb->finish));
        }

        fprintf(fh, "AddSurface\n");
    }
    SCurve *sc;
    for(sc = s->curve.First(); sc; sc = s->curve.NextAfter(sc)) {
        fprintf(fh, "Curve %08x %d %d %08x %08x\n",
            sc->h.v,
            sc->isExact ? 1 : 0, sc->exact.deg,
            sc->surfA.v, sc->surfB.v);

        if(sc->isExact) {
            for(i = 0; i <= sc->exact.deg; i++) {
                fprintf(fh, "CCtrl %d %.20f %.20f %.20f Weight %.20f\n",
                    i, CO(sc->exact.ctrl[i]), sc->exact.weight[i]);
            }
        }
        SCurvePt *scpt;
        for(scpt = sc->pts.First(); scpt; scpt = sc->pts.NextAfter(scpt)) {
            fprintf(fh, "CurvePt %d %.20f %.20f %.20f\n",
                scpt->vertex ? 1 : 0, CO(scpt->p));
        }

        fprintf(fh, "AddCurve\n");
    }

    fclose(fh);

    return true;
}

void SolveSpaceUI::LoadUsingTable(const Platform::Path &filename, char *key, char *val) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(strcmp(SAVED[i].desc, key)==0) {
            SAVEDptr *p = (SAVEDptr *)SAVED[i].ptr;
            unsigned int u = 0;
            switch(SAVED[i].fmt) {
                case 'S': p->S() = val;                     break;
                case 'b': p->b() = (atoi(val) != 0);        break;
                case 'd': p->d() = atoi(val);               break;
                case 'f': p->f() = atof(val);               break;
                case 'x': sscanf(val, "%x", &u); p->x()= u; break;

                case 'P': {
                    Platform::Path path = Platform::Path::FromPortable(val);
                    if(!path.IsEmpty()) {
                        p->P() = filename.Parent().Join(path).Expand();
                    }
                    break;
                }

                case 'c':
                    sscanf(val, "%x", &u);
                    p->c() = RgbaColor::FromPackedInt(u);
                    break;

                case 'M': {
                    p->M().clear();
                    for(;;) {
                        EntityKey ek;
                        EntityId ei;
                        char line2[1024];
                        if (fgets(line2, (int)sizeof(line2), fh) == NULL)
                            break;
                        if(sscanf(line2, "%d %x %d", &(ei.v), &(ek.input.v),
                                                     &(ek.copyNumber)) == 3) {
                            if(ei.v == Entity::NO_ENTITY.v) {
                                // Commit bd84bc1a mistakenly introduced code that would remap
                                // some entities to NO_ENTITY. This was fixed in commit bd84bc1a,
                                // but files created meanwhile are corrupt, and can cause crashes.
                                //
                                // To fix this, we skip any such remaps when loading; they will be
                                // recreated on the next regeneration. Any resulting orphans will
                                // be pruned in the usual way, recovering to a well-defined state.
                                continue;
                            }
                            p->M().insert({ ek, ei });
                        } else {
                            break;
                        }
                    }
                    break;
                }

                case 'i': break;

                default: ssassert(false, "Unexpected value format");
            }
            break;
        }
    }
    if(SAVED[i].type == 0) {
        fileLoadError = true;
    }
}

bool SolveSpaceUI::LoadFromFile(const Platform::Path &filename, bool canCancel) {
    allConsistent = false;
    fileLoadError = false;

    fh = OpenFile(filename, "rb");
    if(!fh) {
        Error("Couldn't read from file '%s'", filename.raw.c_str());
        return false;
    }

    ClearExisting();

    sv = {};
    sv.g.scale = 1; // default is 1, not 0; so legacy files need this
    Style::FillDefaultStyle(&sv.s);

    char line[1024];
    while(fgets(line, (int)sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;

        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(filename, key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            // legacy files have a spurious dependency between linked groups
            // and their parent groups, remove
            if(sv.g.type == Group::Type::LINKED)
                sv.g.opA.v = 0;

            SK.group.Add(&(sv.g));
            sv.g = {};
            sv.g.scale = 1; // default is 1, not 0; so legacy files need this
        } else if(strcmp(line, "AddParam")==0) {
            // params are regenerated, but we want to preload the values
            // for initial guesses
            SK.param.Add(&(sv.p));
            sv.p = {};
        } else if(strcmp(line, "AddEntity")==0) {
            // entities are regenerated
        } else if(strcmp(line, "AddRequest")==0) {
            SK.request.Add(&(sv.r));
            sv.r = {};
        } else if(strcmp(line, "AddConstraint")==0) {
            SK.constraint.Add(&(sv.c));
            sv.c = {};
        } else if(strcmp(line, "AddStyle")==0) {
            SK.style.Add(&(sv.s));
            sv.s = {};
            Style::FillDefaultStyle(&sv.s);
        } else if(strcmp(line, VERSION_STRING)==0) {
            // do nothing, version string
        } else if(StrStartsWith(line, "Triangle ")      ||
                  StrStartsWith(line, "Surface ")       ||
                  StrStartsWith(line, "SCtrl ")         ||
                  StrStartsWith(line, "TrimBy ")        ||
                  StrStartsWith(line, "Curve ")         ||
                  StrStartsWith(line, "CCtrl ")         ||
                  StrStartsWith(line, "CurvePt ")       ||
                  strcmp(line, "AddSurface")==0         ||
                  strcmp(line, "AddCurve")==0)
        {
            // ignore the mesh or shell, since we regenerate that
        } else {
            fileLoadError = true;
        }
    }

    fclose(fh);

    if(fileLoadError) {
        Error(_("Unrecognized data in file. This file may be corrupt, or "
                "from a newer version of the program."));
        // At least leave the program in a non-crashing state.
        if(SK.group.IsEmpty()) {
            NewFile();
        }
    }
    if(!ReloadAllLinked(filename, canCancel)) {
        return false;
    }
    UpgradeLegacyData();

    return true;
}

void SolveSpaceUI::UpgradeLegacyData() {
    for(Request &r : SK.request) {
        switch(r.type) {
            // TTF text requests saved in versions prior to 3.0 only have two
            // reference points (origin and origin plus v); version 3.0 adds two
            // more points, and if we don't do anything, then they will appear
            // at workplane origin, and the solver will mess up the sketch if
            // it is not fully constrained.
            case Request::Type::TTF_TEXT: {
                IdList<Entity,hEntity> entity = {};
                IdList<Param,hParam>   param = {};
                r.Generate(&entity, &param);

                // If we didn't load all of the entities and params that this
                // request would generate, then add them now, so that we can
                // force them to their appropriate positions.
                for(Param &p : param) {
                    if(SK.param.FindByIdNoOops(p.h) != NULL) continue;
                    SK.param.Add(&p);
                }
                bool allPointsExist = true;
                for(Entity &e : entity) {
                    if(SK.entity.FindByIdNoOops(e.h) != NULL) continue;
                    SK.entity.Add(&e);
                    allPointsExist = false;
                }

                if(!allPointsExist) {
                    Entity *text = entity.FindById(r.h.entity(0));
                    Entity *b = entity.FindById(text->point[2]);
                    Entity *c = entity.FindById(text->point[3]);
                    ExprVector bex, cex;
                    text->RectGetPointsExprs(&bex, &cex);
                    b->PointForceParamTo(bex.Eval());
                    c->PointForceParamTo(cex.Eval());
                }
                entity.Clear();
                param.Clear();
                break;
            }

            default:
                break;
        }
    }

    // Constraints saved in versions prior to 3.0 never had any params;
    // version 3.0 introduced params to constraints to avoid the hairy ball problem,
    // so force them where they belong.
    IdList<Param,hParam> oldParam = {};
    SK.param.DeepCopyInto(&oldParam);
    SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

    auto AllParamsExistFor = [&](Constraint &c) {
        IdList<Param,hParam> param = {};
        c.Generate(&param);
        bool allParamsExist = true;
        for(Param &p : param) {
            if(oldParam.FindByIdNoOops(p.h) != NULL) continue;
            allParamsExist = false;
            break;
        }
        param.Clear();
        return allParamsExist;
    };

    for(Constraint &c : SK.constraint) {
        switch(c.type) {
            case Constraint::Type::PT_ON_LINE: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *eln = SK.GetEntity(c.entityA);
                EntityBase *ea = SK.GetEntity(eln->point[0]);
                EntityBase *eb = SK.GetEntity(eln->point[1]);
                EntityBase *ep = SK.GetEntity(c.ptA);

                ExprVector exp = ep->PointGetExprsInWorkplane(c.workplane);
                ExprVector exa = ea->PointGetExprsInWorkplane(c.workplane);
                ExprVector exb = eb->PointGetExprsInWorkplane(c.workplane);
                ExprVector exba = exb.Minus(exa);
                Param *p = SK.GetParam(c.h.param(0));
                p->val = exba.Dot(exp.Minus(exa))->Eval() / exba.Dot(exba)->Eval();
                break;
            }

            case Constraint::Type::CUBIC_LINE_TANGENT: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *cubic = SK.GetEntity(c.entityA);
                EntityBase *line  = SK.GetEntity(c.entityB);

                ExprVector a;
                if(c.other) {
                    a = cubic->CubicGetFinishTangentExprs();
                } else {
                    a = cubic->CubicGetStartTangentExprs();
                }

                ExprVector b = line->VectorGetExprs();

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            case Constraint::Type::SAME_ORIENTATION: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *an = SK.GetEntity(c.entityA);
                EntityBase *bn = SK.GetEntity(c.entityB);

                ExprVector a = an->NormalExprsN();
                ExprVector b = bn->NormalExprsN();

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            case Constraint::Type::PARALLEL: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *ea = SK.GetEntity(c.entityA),
                           *eb = SK.GetEntity(c.entityB);
                ExprVector a = ea->VectorGetExprsInWorkplane(c.workplane);
                ExprVector b = eb->VectorGetExprsInWorkplane(c.workplane);

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            default:
                break;
        }
    }
    oldParam.Clear();
}

bool SolveSpaceUI::LoadEntitiesFromFile(const Platform::Path &filename, EntityList *le,
                                        SMesh *m, SShell *sh)
{
    SSurface srf = {};
    SCurve crv = {};

    fh = OpenFile(filename, "rb");
    if(!fh) return false;

    le->Clear();
    sv = {};

    char line[1024];
    while(fgets(line, (int)sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;

        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(filename, key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            // These get allocated whether we want them or not.
            sv.g.remap.clear();
        } else if(strcmp(line, "AddParam")==0) {

        } else if(strcmp(line, "AddEntity")==0) {
            le->Add(&(sv.e));
            sv.e = {};
        } else if(strcmp(line, "AddRequest")==0) {

        } else if(strcmp(line, "AddConstraint")==0) {

        } else if(strcmp(line, "AddStyle")==0) {
            // Linked file contains a style that we don't have yet,
            // so import it.
            if (SK.style.FindByIdNoOops(sv.s.h) == nullptr) {
                SK.style.Add(&(sv.s));
            }
            sv.s = {};
            Style::FillDefaultStyle(&sv.s);
        } else if(strcmp(line, VERSION_STRING)==0) {

        } else if(StrStartsWith(line, "Triangle ")) {
            STriangle tr = {};
            unsigned int rgba = 0;
            if(sscanf(line, "Triangle %x %x  "
                             "%lf %lf %lf  %lf %lf %lf  %lf %lf %lf",
                &(tr.meta.face), &rgba,
                &(tr.a.x), &(tr.a.y), &(tr.a.z),
                &(tr.b.x), &(tr.b.y), &(tr.b.z),
                &(tr.c.x), &(tr.c.y), &(tr.c.z)) != 11) {
                ssassert(false, "Unexpected Triangle format");
            }
            tr.meta.color = RgbaColor::FromPackedInt((uint32_t)rgba);
            m->AddTriangle(&tr);
        } else if(StrStartsWith(line, "Surface ")) {
            unsigned int rgba = 0;
            if(sscanf(line, "Surface %x %x %x %d %d",
                &(srf.h.v), &rgba, &(srf.face),
                &(srf.degm), &(srf.degn)) != 5) {
                ssassert(false, "Unexpected Surface format");
            }
            srf.color = RgbaColor::FromPackedInt((uint32_t)rgba);
        } else if(StrStartsWith(line, "SCtrl ")) {
            int i, j;
            Vector c;
            double w;
            if(sscanf(line, "SCtrl %d %d %lf %lf %lf Weight %lf",
                                &i, &j, &(c.x), &(c.y), &(c.z), &w) != 6)
            {
                ssassert(false, "Unexpected SCtrl format");
            }
            srf.ctrl[i][j] = c;
            srf.weight[i][j] = w;
        } else if(StrStartsWith(line, "TrimBy ")) {
            STrimBy stb = {};
            int backwards;
            if(sscanf(line, "TrimBy %x %d  %lf %lf %lf  %lf %lf %lf",
                &(stb.curve.v), &backwards,
                &(stb.start.x), &(stb.start.y), &(stb.start.z),
                &(stb.finish.x), &(stb.finish.y), &(stb.finish.z)) != 8)
            {
                ssassert(false, "Unexpected TrimBy format");
            }
            stb.backwards = (backwards != 0);
            srf.trim.Add(&stb);
        } else if(strcmp(line, "AddSurface")==0) {
            sh->surface.Add(&srf);
            srf = {};
        } else if(StrStartsWith(line, "Curve ")) {
            int isExact;
            if(sscanf(line, "Curve %x %d %d %x %x",
                &(crv.h.v),
                &(isExact),
                &(crv.exact.deg),
                &(crv.surfA.v), &(crv.surfB.v)) != 5)
            {
                ssassert(false, "Unexpected Curve format");
            }
            crv.isExact = (isExact != 0);
        } else if(StrStartsWith(line, "CCtrl ")) {
            int i;
            Vector c;
            double w;
            if(sscanf(line, "CCtrl %d %lf %lf %lf Weight %lf",
                                &i, &(c.x), &(c.y), &(c.z), &w) != 5)
            {
                ssassert(false, "Unexpected CCtrl format");
            }
            crv.exact.ctrl[i] = c;
            crv.exact.weight[i] = w;
        } else if(StrStartsWith(line, "CurvePt ")) {
            SCurvePt scpt;
            int vertex;
            if(sscanf(line, "CurvePt %d %lf %lf %lf",
                &vertex,
                &(scpt.p.x), &(scpt.p.y), &(scpt.p.z)) != 4)
            {
                ssassert(false, "Unexpected CurvePt format");
            }
            scpt.vertex = (vertex != 0);
            crv.pts.Add(&scpt);
        } else if(strcmp(line, "AddCurve")==0) {
            sh->curve.Add(&crv);
            crv = {};
        } else ssassert(false, "Unexpected operation");
    }

    fclose(fh);
    return true;
}

static Platform::MessageDialog::Response LocateImportedFile(const Platform::Path &filename,
                                                            bool canCancel) {
    Platform::MessageDialogRef dialog = CreateMessageDialog(SS.GW.window);

    using Platform::MessageDialog;
    dialog->SetType(MessageDialog::Type::QUESTION);
    dialog->SetTitle(C_("title", "Missing File"));
    dialog->SetMessage(ssprintf(C_("dialog", "The linked file “%s” is not present."),
                                filename.raw.c_str()));
    dialog->SetDescription(C_("dialog", "Do you want to locate it manually?\n\n"
                                        "If you decline, any geometry that depends on "
                                        "the missing file will be permanently removed."));
    dialog->AddButton(C_("button", "&Yes"), MessageDialog::Response::YES,
                      /*isDefault=*/true);
    dialog->AddButton(C_("button", "&No"), MessageDialog::Response::NO);
    if(canCancel) {
        dialog->AddButton(C_("button", "&Cancel"), MessageDialog::Response::CANCEL);
    }

    // FIXME(async): asyncify this call
    return dialog->RunModal();
}

bool SolveSpaceUI::ReloadAllLinked(const Platform::Path &saveFile, bool canCancel) {
    Platform::SettingsRef settings = Platform::GetSettings();

    std::map<Platform::Path, Platform::Path, Platform::PathLess> linkMap;

    allConsistent = false;

    for(Group &g : SK.group) {
        if(g.type != Group::Type::LINKED) continue;

        g.impEntity.Clear();
        g.impMesh.Clear();
        g.impShell.Clear();

        // If we prompted for this specific file before, don't ask again.
        if(linkMap.count(g.linkFile)) {
            g.linkFile = linkMap[g.linkFile];
        }

try_again:
        if(LoadEntitiesFromFile(g.linkFile, &g.impEntity, &g.impMesh, &g.impShell)) {
            // We loaded the data, good. Now import its dependencies as well.
            for(Entity &e : g.impEntity) {
                if(e.type != Entity::Type::IMAGE) continue;
                if(!ReloadLinkedImage(g.linkFile, &e.file, canCancel)) {
                    return false;
                }
            }
        } else if(linkMap.count(g.linkFile) == 0) {
            // The file was moved; prompt the user for its new location.
            switch(LocateImportedFile(g.linkFile.RelativeTo(saveFile), canCancel)) {
                case Platform::MessageDialog::Response::YES: {
                    Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
                    dialog->AddFilters(Platform::SolveSpaceModelFileFilters);
                    dialog->ThawChoices(settings, "LinkSketch");
                    if(dialog->RunModal()) {
                        dialog->FreezeChoices(settings, "LinkSketch");
                        linkMap[g.linkFile] = dialog->GetFilename();
                        g.linkFile = dialog->GetFilename();
                        goto try_again;
                    } else {
                        if(canCancel) return false;
                        break;
                    }
                }

                case Platform::MessageDialog::Response::NO:
                    linkMap[g.linkFile].Clear();
                    // Geometry will be pruned by GenerateAll().
                    break;

                case Platform::MessageDialog::Response::CANCEL:
                    return false;

                default:
                    ssassert(false, "Unexpected dialog response");
            }
        } else {
            // User was already asked to and refused to locate a missing linked file.
        }
    }

    for(Request &r : SK.request) {
        if(r.type != Request::Type::IMAGE) continue;

        if(!ReloadLinkedImage(saveFile, &r.file, canCancel)) {
            return false;
        }
    }

    return true;
}

bool SolveSpaceUI::ReloadLinkedImage(const Platform::Path &saveFile,
                                     Platform::Path *filename, bool canCancel) {
    Platform::SettingsRef settings = Platform::GetSettings();

    std::shared_ptr<Pixmap> pixmap;
    bool promptOpenFile = false;
    if(filename->IsEmpty()) {
        // We're prompting the user for a new image.
        promptOpenFile = true;
    } else {
        auto image = SS.images.find(*filename);
        if(image != SS.images.end()) return true;

        pixmap = Pixmap::ReadPng(*filename);
        if(pixmap == NULL) {
            // The file was moved; prompt the user for its new location.
            switch(LocateImportedFile(filename->RelativeTo(saveFile), canCancel)) {
                case Platform::MessageDialog::Response::YES:
                    promptOpenFile = true;
                    break;

                case Platform::MessageDialog::Response::NO:
                    // We don't know where the file is, record it as absent.
                    break;

                case Platform::MessageDialog::Response::CANCEL:
                    return false;

                default:
                    ssassert(false, "Unexpected dialog response");
            }
        }
    }

    if(promptOpenFile) {
        Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
        dialog->AddFilters(Platform::RasterFileFilters);
        dialog->ThawChoices(settings, "LinkImage");
        if(dialog->RunModal()) {
            dialog->FreezeChoices(settings, "LinkImage");
            *filename = dialog->GetFilename();
            pixmap = Pixmap::ReadPng(*filename);
            if(pixmap == NULL) {
                Error("The image '%s' is corrupted.", filename->raw.c_str());
            }
            // We know where the file is now, good.
        } else if(canCancel) {
            return false;
        }
    }

    // We loaded the data, good.
    SS.images[*filename] = pixmap;
    return true;
}
