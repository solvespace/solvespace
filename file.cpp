#include "solvespace.h"

#define VERSION_STRING "±²³SolveSpaceREVa"

hGroup SolveSpace::CreateDefaultDrawingGroup(void) {
    Group g;
    ZERO(&g);

    // And an empty group, for the first stuff the user draws.
    g.visible = true;
    g.type = Group::DRAWING_WORKPLANE;
    g.subtype = Group::WORKPLANE_BY_POINT_ORTHO;
    g.predef.q = Quaternion::From(1, 0, 0, 0);
    hRequest hr = Request::HREQUEST_REFERENCE_XY;
    g.predef.origin = hr.entity(1);
    g.name.strcpy("sketch-in-plane");
    group.AddAndAssignId(&g);
    SS.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    return g.h;
}

void SolveSpace::NewFile(void) {
    UndoClearStack(&redo);
    UndoClearStack(&undo);

    constraint.Clear();
    request.Clear();
    group.Clear();

    entity.Clear();
    param.Clear();

    // Our initial group, that contains the references.
    Group g;
    memset(&g, 0, sizeof(g));
    g.visible = true;
    g.name.strcpy("#references");
    g.type = Group::DRAWING_3D;
    g.h = Group::HGROUP_REFERENCES;
    group.Add(&g);

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r;
    ZERO(&r);
    r.type = Request::WORKPLANE;
    r.group = Group::HGROUP_REFERENCES;
    r.workplane = Entity::FREE_IN_3D;

    r.h = Request::HREQUEST_REFERENCE_XY;
    request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_YZ;
    request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_ZX;
    request.Add(&r);

    CreateDefaultDrawingGroup();
}

const SolveSpace::SaveTable SolveSpace::SAVED[] = {
    { 'g',  "Group.h.v",                'x',    &(SS.sv.g.h.v)                },
    { 'g',  "Group.type",               'd',    &(SS.sv.g.type)               },
    { 'g',  "Group.name",               'N',    &(SS.sv.g.name)               },
    { 'g',  "Group.activeWorkplane.v",  'x',    &(SS.sv.g.activeWorkplane.v)  },
    { 'g',  "Group.opA.v",              'x',    &(SS.sv.g.opA.v)              },
    { 'g',  "Group.opB.v",              'x',    &(SS.sv.g.opB.v)              },
    { 'g',  "Group.valA",               'f',    &(SS.sv.g.valA)               },
    { 'g',  "Group.valB",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.valC",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.color",              'x',    &(SS.sv.g.color)              },
    { 'g',  "Group.subtype",            'd',    &(SS.sv.g.subtype)            },
    { 'g',  "Group.skipFirst",          'b',    &(SS.sv.g.skipFirst)          },
    { 'g',  "Group.meshCombine",        'd',    &(SS.sv.g.meshCombine)        },
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
    { 'g',  "Group.remap",              'M',    &(SS.sv.g.remap)              },
    { 'g',  "Group.impFile",            'P',    &(SS.sv.g.impFile)            },
    { 'g',  "Group.impFileRel",         'P',    &(SS.sv.g.impFileRel)         },

    { 'p',  "Param.h.v.",               'x',    &(SS.sv.p.h.v)                },
    { 'p',  "Param.val",                'f',    &(SS.sv.p.val)                },

    { 'r',  "Request.h.v",              'x',    &(SS.sv.r.h.v)                },
    { 'r',  "Request.type",             'd',    &(SS.sv.r.type)               },
    { 'r',  "Request.workplane.v",      'x',    &(SS.sv.r.workplane.v)        },
    { 'r',  "Request.group.v",          'x',    &(SS.sv.r.group.v)            },
    { 'r',  "Request.construction",     'b',    &(SS.sv.r.construction)       },
    { 'r',  "Request.str",              'N',    &(SS.sv.r.str)                },
    { 'r',  "Request.font",             'N',    &(SS.sv.r.font)               },

    { 'e',  "Entity.h.v",               'x',    &(SS.sv.e.h.v)                },
    { 'e',  "Entity.type",              'd',    &(SS.sv.e.type)               },
    { 'e',  "Entity.construction",      'b',    &(SS.sv.e.construction)       },
    { 'e',  "Entity.str",               'N',    &(SS.sv.e.str)                },
    { 'e',  "Entity.font",              'N',    &(SS.sv.e.font)               },
    { 'e',  "Entity.point[0].v",        'x',    &(SS.sv.e.point[0].v)         },
    { 'e',  "Entity.point[1].v",        'x',    &(SS.sv.e.point[1].v)         },
    { 'e',  "Entity.point[2].v",        'x',    &(SS.sv.e.point[2].v)         },
    { 'e',  "Entity.point[3].v",        'x',    &(SS.sv.e.point[3].v)         },
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
    { 'c',  "Constraint.ptA.v",         'x',    &(SS.sv.c.ptA.v)              },
    { 'c',  "Constraint.ptB.v",         'x',    &(SS.sv.c.ptB.v)              },
    { 'c',  "Constraint.entityA.v",     'x',    &(SS.sv.c.entityA.v)          },
    { 'c',  "Constraint.entityB.v",     'x',    &(SS.sv.c.entityB.v)          },
    { 'c',  "Constraint.entityC.v",     'x',    &(SS.sv.c.entityC.v)          },
    { 'c',  "Constraint.entityD.v",     'x',    &(SS.sv.c.entityD.v)          },
    { 'c',  "Constraint.other",         'b',    &(SS.sv.c.other)              },
    { 'c',  "Constraint.reference",     'b',    &(SS.sv.c.reference)          },
    { 'c',  "Constraint.comment",       'N',    &(SS.sv.c.comment)            },
    { 'c',  "Constraint.disp.offset.x", 'f',    &(SS.sv.c.disp.offset.x)      },
    { 'c',  "Constraint.disp.offset.y", 'f',    &(SS.sv.c.disp.offset.y)      },
    { 'c',  "Constraint.disp.offset.z", 'f',    &(SS.sv.c.disp.offset.z)      },

    { 0, NULL, NULL, NULL     },
};

void SolveSpace::SaveUsingTable(int type) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(SAVED[i].type != type) continue;

        int fmt = SAVED[i].fmt;
        void *p = SAVED[i].ptr;
        // Any items that aren't specified are assumed to be zero
        if(fmt == 'd' && *((int *)p)    == 0)   continue;
        if(fmt == 'x' && *((DWORD *)p)  == 0)   continue;
        if(fmt == 'f' && *((double *)p) == 0.0) continue;
        if(fmt == 'N' && strlen(((NameStr *)p)->str) == 0) continue;

        fprintf(fh, "%s=", SAVED[i].desc);
        switch(fmt) {
            case 'd': fprintf(fh, "%d", *((int *)p)); break;
            case 'b': fprintf(fh, "%d", *((bool *)p) ? 1 : 0); break;
            case 'x': fprintf(fh, "%08x", *((DWORD *)p)); break;
            case 'f': fprintf(fh, "%.20f", *((double *)p)); break;
            case 'N': fprintf(fh, "%s", ((NameStr *)p)->str); break;
            case 'P': fprintf(fh, "%s", (char *)p); break;

            case 'M': {
                int j;
                fprintf(fh, "{\n");
                IdList<EntityMap,EntityId> *m = (IdList<EntityMap,EntityId> *)p;
                for(j = 0; j < m->n; j++) {
                    EntityMap *em = &(m->elem[j]);
                    fprintf(fh, "    %d %08x %d\n", 
                            em->h.v, em->input.v, em->copyNumber);
                }
                fprintf(fh, "}");
                break;
            }

            default: oops();
        }
        fprintf(fh, "\n");
    }
}

bool SolveSpace::SaveToFile(char *filename) {
    // Make sure all the entities are regenerated up to date, since they
    // will be exported.
    SS.GenerateAll(0, INT_MAX);

    fh = fopen(filename, "wb");
    if(!fh) {   
        Error("Couldn't write to file '%s'", filename);
        return false;
    }

    fprintf(fh, "%s\n\n\n", VERSION_STRING);

    int i;
    for(i = 0; i < group.n; i++) {
        sv.g = group.elem[i];
        SaveUsingTable('g');
        fprintf(fh, "AddGroup\n\n");
    }

    for(i = 0; i < param.n; i++) {
        sv.p = param.elem[i];
        SaveUsingTable('p');
        fprintf(fh, "AddParam\n\n");
    }

    for(i = 0; i < request.n; i++) {
        sv.r = request.elem[i];
        SaveUsingTable('r');
        fprintf(fh, "AddRequest\n\n");
    }

    for(i = 0; i < entity.n; i++) {
        (entity.elem[i]).CalculateNumerical(true);
        sv.e = entity.elem[i];
        SaveUsingTable('e');
        fprintf(fh, "AddEntity\n\n");
    }

    for(i = 0; i < constraint.n; i++) {
        sv.c = constraint.elem[i];
        SaveUsingTable('c');
        fprintf(fh, "AddConstraint\n\n");
    }

    SMesh *m = &(group.elem[group.n-1].runningMesh);
    for(i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);
        fprintf(fh, "Triangle %08x %08x  "
                "%.20f %.20f %.20f  %.20f %.20f %.20f  %.20f %.20f %.20f\n",
            tr->meta.face, tr->meta.color,
            CO(tr->a), CO(tr->b), CO(tr->c));
    }

    fclose(fh);

    return true;
}

void SolveSpace::LoadUsingTable(char *key, char *val) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(strcmp(SAVED[i].desc, key)==0) {
            void *p = SAVED[i].ptr;
            switch(SAVED[i].fmt) {
                case 'd': *((int *)p) = atoi(val); break;
                case 'b': *((bool *)p) = (atoi(val) != 0); break;
                case 'x': sscanf(val, "%x", (DWORD *)p); break;
                case 'f': *((double *)p) = atof(val); break;
                case 'N': ((NameStr *)p)->strcpy(val); break;

                case 'P':
                    if(strlen(val)+1 < MAX_PATH) strcpy((char *)p, val);
                    break;

                case 'M': {
                    IdList<EntityMap,EntityId> *m =
                                (IdList<EntityMap,EntityId> *)p;
                    // Don't clear this list! When the group gets added, it
                    // makes a shallow copy, so that would result in us
                    // freeing memory that we want to keep around. Just
                    // zero it out so that new memory is allocated.
                    memset(m, 0, sizeof(*m));
                    for(;;) {
                        EntityMap em;
                        char line2[1024];
                        fgets(line2, sizeof(line2), fh);
                        if(sscanf(line2, "%d %x %d", &(em.h.v), &(em.input.v),
                                                     &(em.copyNumber)) == 3)
                        {
                            m->Add(&em);
                        } else {
                            break;
                        }
                    }
                    break;
                }

                default: oops();
            }
            break;
        }
    }
    if(SAVED[i].type == 0) oops();
}

bool SolveSpace::LoadFromFile(char *filename) {
    allConsistent = false;

    fh = fopen(filename, "rb");
    if(!fh) {   
        Error("Couldn't read from file '%s'", filename);
        return false;
    }

    UndoClearStack(&redo);
    UndoClearStack(&undo);

    constraint.Clear();
    request.Clear();
    group.Clear();
    entity.Clear();
    param.Clear();
    memset(&sv, 0, sizeof(sv));

    char line[1024];
    while(fgets(line, sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';

        if(*line == '\0') continue;
       
        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            SS.group.Add(&(sv.g));
            memset(&(sv.g), 0, sizeof(sv.g));
        } else if(strcmp(line, "AddParam")==0) {
            // params are regenerated, but we want to preload the values
            // for initial guesses
            SS.param.Add(&(sv.p));
            memset(&(sv.p), 0, sizeof(sv.p));
        } else if(strcmp(line, "AddEntity")==0) {
            // entities are regenerated
        } else if(strcmp(line, "AddRequest")==0) {
            SS.request.Add(&(sv.r));
            memset(&(sv.r), 0, sizeof(sv.r));
        } else if(strcmp(line, "AddConstraint")==0) {
            SS.constraint.Add(&(sv.c));
            memset(&(sv.c), 0, sizeof(sv.c));
        } else if(strcmp(line, VERSION_STRING)==0) {
            // do nothing, version string
        } else if(memcmp(line, "Triangle", 8)==0) {
            // likewise ignore the triangles; we generate those
        } else {
            oops();
        }
    }

    fclose(fh);

    return true;
}

bool SolveSpace::LoadEntitiesFromFile(char *file, EntityList *le, SMesh *m) {
    fh = fopen(file, "rb");
    if(!fh) return false;

    le->Clear();
    memset(&sv, 0, sizeof(sv));

    char line[1024];
    while(fgets(line, sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';

        if(*line == '\0') continue;
       
        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            // Don't leak memory; these get allocated whether we want them
            // or not.
            sv.g.remap.Clear();
        } else if(strcmp(line, "AddParam")==0) {

        } else if(strcmp(line, "AddEntity")==0) {
            le->Add(&(sv.e));
            memset(&(sv.e), 0, sizeof(sv.e));
        } else if(strcmp(line, "AddRequest")==0) {

        } else if(strcmp(line, "AddConstraint")==0) {

        } else if(strcmp(line, VERSION_STRING)==0) {

        } else if(memcmp(line, "Triangle", 8)==0) {
            STriangle tr; ZERO(&tr);
            if(sscanf(line, "Triangle %x %x  "
                             "%lf %lf %lf  %lf %lf %lf  %lf %lf %lf",
                &(tr.meta.face), &(tr.meta.color),
                &(tr.a.x), &(tr.a.y), &(tr.a.z), 
                &(tr.b.x), &(tr.b.y), &(tr.b.z), 
                &(tr.c.x), &(tr.c.y), &(tr.c.z)) != 11)
            {
                oops();
            }
            m->AddTriangle(&tr);
        } else {
            oops();
        }
    }

    fclose(fh);
    return true;
}

void SolveSpace::ReloadAllImported(void) {
    allConsistent = false;

    int i;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->type != Group::IMPORTED) continue;

        g->impEntity.Clear();
        g->impMesh.Clear();

        FILE *test = fopen(g->impFile, "rb");
        if(test) {
            fclose(test); // okay, exists
        } else {
            // It doesn't exist. Perhaps the entire tree has moved, and we
            // can use the relative filename to get us back.
            if(SS.saveFile[0]) {
                char fromRel[MAX_PATH];
                strcpy(fromRel, g->impFileRel);
                MakePathAbsolute(SS.saveFile, fromRel);
                test = fopen(fromRel, "rb");
                if(test) {
                    fclose(test);
                    // It worked, this is our new absolute path
                    strcpy(g->impFile, fromRel);
                }
            }
        }

        if(LoadEntitiesFromFile(g->impFile, &(g->impEntity), &(g->impMesh))) {
            if(SS.saveFile[0]) {
                // Record the imported file's name relative to our filename;
                // if the entire tree moves, then everything will still work
                strcpy(g->impFileRel, g->impFile);
                MakePathRelative(SS.saveFile, g->impFileRel);
            } else {
                // We're not yet saved, so can't make it absolute
                strcpy(g->impFileRel, g->impFile);
            }
        } else {
            Error("Failed to load imported file '%s'", g->impFile);
        }
    }
}

