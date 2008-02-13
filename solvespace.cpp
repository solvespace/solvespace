#include "solvespace.h"

SolveSpace SS;

void SolveSpace::CheckLicenseFromRegistry(void) {
    // First, let's see if we're running licensed or free
    CnfThawString(license.line1, sizeof(license.line1), "LicenseLine1");
    CnfThawString(license.line2, sizeof(license.line2), "LicenseLine2");
    CnfThawString(license.users, sizeof(license.users), "LicenseUsers");
    license.key = CnfThawDWORD(0, "LicenseKey");

    license.licensed =
        LicenseValid(license.line1, license.line2, license.users, license.key);
}

void SolveSpace::Init(char *cmdLine) {
    CheckLicenseFromRegistry();

    // Then, load the registry settings.
    int i;
    // Default list of colors for the model material
    modelColor[0] = CnfThawDWORD(RGB(150, 150, 150), "ModelColor_0");
    modelColor[1] = CnfThawDWORD(RGB(100, 100, 100), "ModelColor_1");
    modelColor[2] = CnfThawDWORD(RGB( 30,  30,  30), "ModelColor_2");
    modelColor[3] = CnfThawDWORD(RGB(150,   0,   0), "ModelColor_3");
    modelColor[4] = CnfThawDWORD(RGB(  0, 100,   0), "ModelColor_4");
    modelColor[5] = CnfThawDWORD(RGB(  0,  80,  80), "ModelColor_5");
    modelColor[6] = CnfThawDWORD(RGB(  0,   0, 130), "ModelColor_6");
    modelColor[7] = CnfThawDWORD(RGB( 80,   0,  80), "ModelColor_7");
    // Light intensities
    lightIntensity[0] = CnfThawFloat(1.0f, "LightIntensity_0");
    lightIntensity[1] = CnfThawFloat(0.5f, "LightIntensity_1");
    // Light positions
    lightDir[0].x = CnfThawFloat(-1.0f, "LightDir_0_Right"     );
    lightDir[0].y = CnfThawFloat( 1.0f, "LightDir_0_Up"        );
    lightDir[0].z = CnfThawFloat( 0.0f, "LightDir_0_Forward"   );
    lightDir[1].x = CnfThawFloat( 1.0f, "LightDir_1_Right"     );
    lightDir[1].y = CnfThawFloat( 0.0f, "LightDir_1_Up"        );
    lightDir[1].z = CnfThawFloat( 0.0f, "LightDir_1_Forward"   );
    // Chord tolerance
    chordTol = CnfThawFloat(2.0f, "ChordTolerance");
    // Max pwl segments to generate
    maxSegments = CnfThawDWORD(40, "MaxSegments");
    // View units
    viewUnits = (Unit)CnfThawDWORD((DWORD)UNIT_MM, "ViewUnits");
    // Camera tangent (determines perspective)
    cameraTangent = CnfThawFloat(0.0f, "CameraTangent");
    // Color for edges (drawn as lines for emphasis)
    edgeColor = CnfThawDWORD(RGB(0, 0, 0), "EdgeColor");
    // Export scale factor
    exportScale = CnfThawFloat(1.0f, "ExportScale");
    // Recent files menus
    for(i = 0; i < MAX_RECENT; i++) {
        char name[100];
        sprintf(name, "RecentFile_%d", i);
        strcpy(RecentFile[i], "");
        CnfThawString(RecentFile[i], MAX_PATH, name);
    }
    RefreshRecentMenus();

    // Start with either an empty file, or the file specified on the
    // command line.
    NewFile();
    AfterNewFile();
    if(strlen(cmdLine) != 0) {
        if(LoadFromFile(cmdLine)) {
            strcpy(saveFile, cmdLine);
        } else {
            NewFile();
        }
    }
    AfterNewFile();
}

void SolveSpace::Exit(void) {
    int i;
    char name[100];
    // Recent files
    for(i = 0; i < MAX_RECENT; i++) {
        sprintf(name, "RecentFile_%d", i);
        CnfFreezeString(RecentFile[i], name);
    }
    // Model colors
    for(i = 0; i < MODEL_COLORS; i++) {
        sprintf(name, "ModelColor_%d", i);
        CnfFreezeDWORD(modelColor[i], name);
    }
    // Light intensities
    CnfFreezeFloat((float)lightIntensity[0], "LightIntensity_0");
    CnfFreezeFloat((float)lightIntensity[1], "LightIntensity_1");
    // Light directions
    CnfFreezeFloat((float)lightDir[0].x, "LightDir_0_Right");
    CnfFreezeFloat((float)lightDir[0].y, "LightDir_0_Up");
    CnfFreezeFloat((float)lightDir[0].z, "LightDir_0_Forward");
    CnfFreezeFloat((float)lightDir[1].x, "LightDir_1_Right");
    CnfFreezeFloat((float)lightDir[1].y, "LightDir_1_Up");
    CnfFreezeFloat((float)lightDir[1].z, "LightDir_1_Forward");
    // Chord tolerance
    CnfFreezeFloat((float)chordTol, "ChordTolerance");
    // Max pwl segments to generate
    CnfFreezeDWORD((DWORD)maxSegments, "MaxSegments");
    // Display/entry units
    CnfFreezeDWORD((DWORD)viewUnits, "ViewUnits");
    // Camera tangent (determines perspective)
    CnfFreezeFloat((float)cameraTangent, "CameraTangent");
    // Color for edges (drawn as lines for emphasis)
    CnfFreezeDWORD(edgeColor, "EdgeColor");
    // Export scale (a float, stored as a DWORD)
    CnfFreezeFloat(exportScale, "ExportScale");

    ExitNow();
}

void SolveSpace::DoLater(void) {
    if(later.generateAll) GenerateAll();
    if(later.showTW) TW.Show();
    ZERO(&later);
}

int SolveSpace::CircleSides(double r) {
    // Let the pwl segment be symmetric about the x axis; then the curve
    // goes out to r, and if there's n segments, then the endpoints are
    // at +/- (2pi/n)/2 = +/- pi/n. So the chord goes to x = r cos pi/n,
    // from x = r, so it's
    //   tol = r - r cos pi/n
    //   tol = r(1 - cos pi/n)
    //   tol ~ r(1 - (1 - (pi/n)^2/2))  (Taylor expansion)
    //   tol = r((pi/n)^2/2)
    //   2*tol/r = (pi/n)^2
    //   sqrt(2*tol/r) = pi/n
    //   n = pi/sqrt(2*tol/r);
    
    double tol = chordTol/GW.scale;
    int n = 3 + (int)(PI/sqrt(2*tol/r));

    return max(7, min(n, maxSegments));
}

char *SolveSpace::MmToString(double v) {
    static int WhichBuf;
    static char Bufs[8][128];

    WhichBuf++;
    if(WhichBuf >= 8 || WhichBuf < 0) WhichBuf = 0;

    char *s = Bufs[WhichBuf];
    if(viewUnits == UNIT_INCHES) {
        sprintf(s, "%.3f", v/25.4);
    } else {
        sprintf(s, "%.2f", v);
    }
    return s;
}
double SolveSpace::ExprToMm(Expr *e) {
    if(viewUnits == UNIT_INCHES) {
        return (e->Eval())*25.4;
    } else {
        return e->Eval();
    }
}
double SolveSpace::StringToMm(char *str) {
    if(viewUnits == UNIT_INCHES) {
        return atof(str)*25.4;
    } else {
        return atof(str);
    }
}


void SolveSpace::AfterNewFile(void) {
    // Clear out the traced point, which is no longer valid
    traced.point = Entity::NO_ENTITY;
    traced.path.l.Clear();

    ReloadAllImported();
    GenerateAll(-1, -1);

    TW.Init();
    GW.Init();

    unsaved = false;

    int w, h;
    GetGraphicsWindowSize(&w, &h);
    GW.width = w;
    GW.height = h;

    // The triangles haven't been generated yet, but zoom to fit the entities
    // roughly in the window, since that sets the mesh tolerance.
    GW.ZoomToFit();

    GenerateAll(0, INT_MAX);
    later.showTW = true;
    // Then zoom to fit again, to fit the triangles
    GW.ZoomToFit();

    UpdateWindowTitle();
}

void SolveSpace::RemoveFromRecentList(char *file) {
    int src, dest;
    dest = 0;
    for(src = 0; src < MAX_RECENT; src++) {
        if(strcmp(file, RecentFile[src]) != 0) {
            if(src != dest) strcpy(RecentFile[dest], RecentFile[src]);
            dest++;
        }
    }
    while(dest < MAX_RECENT) strcpy(RecentFile[dest++], "");
    RefreshRecentMenus();
}
void SolveSpace::AddToRecentList(char *file) {
    RemoveFromRecentList(file);

    int src;
    for(src = MAX_RECENT - 2; src >= 0; src--) {
        strcpy(RecentFile[src+1], RecentFile[src]);
    }
    strcpy(RecentFile[0], file);
    RefreshRecentMenus();
}

bool SolveSpace::GetFilenameAndSave(bool saveAs) {
    char newFile[MAX_PATH];
    strcpy(newFile, saveFile);
    if(saveAs || strlen(newFile)==0) {
        if(!GetSaveFile(newFile, SLVS_EXT, SLVS_PATTERN)) return false;
    }

    if(SaveToFile(newFile)) {
        AddToRecentList(newFile);
        strcpy(saveFile, newFile);
        unsaved = false;
        return true;
    } else {
        return false;
    }
}

bool SolveSpace::OkayToStartNewFile(void) {
    if(!unsaved) return true;

    switch(SaveFileYesNoCancel()) {
        case IDYES:
            return GetFilenameAndSave(false);

        case IDNO:
            return true;

        case IDCANCEL:
            return false;
        
        default: oops();
    }
}

void SolveSpace::UpdateWindowTitle(void) {
    if(strlen(saveFile) == 0) {
        SetWindowTitle("SolveSpace - (not yet saved)");
    } else {
        char buf[MAX_PATH+100];
        sprintf(buf, "SolveSpace - %s", saveFile);
        SetWindowTitle(buf);
    }
}

void SolveSpace::MenuFile(int id) {
    if(id >= RECENT_OPEN && id < (RECENT_OPEN+MAX_RECENT)) {
        if(!SS.OkayToStartNewFile()) return;

        char newFile[MAX_PATH];
        strcpy(newFile, RecentFile[id-RECENT_OPEN]);
        RemoveFromRecentList(newFile);
        if(SS.LoadFromFile(newFile)) {
            strcpy(SS.saveFile, newFile);
            AddToRecentList(newFile);
        } else {
            strcpy(SS.saveFile, "");
            SS.NewFile();
        }
        SS.AfterNewFile();
        return;
    }

    switch(id) {
        case GraphicsWindow::MNU_NEW:
            if(!SS.OkayToStartNewFile()) break;

            strcpy(SS.saveFile, "");
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case GraphicsWindow::MNU_OPEN: {
            if(!SS.OkayToStartNewFile()) break;

            char newFile[MAX_PATH] = "";
            if(GetOpenFile(newFile, SLVS_EXT, SLVS_PATTERN)) {
                if(SS.LoadFromFile(newFile)) {
                    strcpy(SS.saveFile, newFile);
                    AddToRecentList(newFile);
                } else {
                    strcpy(SS.saveFile, "");
                    SS.NewFile();
                }
                SS.AfterNewFile();
            }
            break;
        }

        case GraphicsWindow::MNU_SAVE:
            SS.GetFilenameAndSave(false);
            break;

        case GraphicsWindow::MNU_SAVE_AS:
            SS.GetFilenameAndSave(true);
            break;

        case GraphicsWindow::MNU_EXPORT_PNG: {
            char exportFile[MAX_PATH] = "";
            if(!GetSaveFile(exportFile, PNG_EXT, PNG_PATTERN)) break;
            SS.ExportAsPngTo(exportFile); 
            break;
        }

        case GraphicsWindow::MNU_EXPORT_DXF: {
            char exportFile[MAX_PATH] = "";
            if(!GetSaveFile(exportFile, DXF_EXT, DXF_PATTERN)) break;
            SS.ExportDxfTo(exportFile); 
            break;
        }

        case GraphicsWindow::MNU_EXPORT_MESH: {
            char exportFile[MAX_PATH] = "";
            if(!GetSaveFile(exportFile, STL_EXT, STL_PATTERN)) break;
            SS.ExportMeshTo(exportFile); 
            break;
        }

        case GraphicsWindow::MNU_EXIT:
            if(!SS.OkayToStartNewFile()) break;
            SS.Exit();
            break;

        default: oops();
    }

    SS.UpdateWindowTitle();
}

void SolveSpace::MenuAnalyze(int id) {
    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_STEP_DIM:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SS.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && !c->reference) {
                    SS.TW.shown.dimFinish = c->valA;
                    SS.TW.shown.dimSteps = 10;
                    SS.TW.shown.dimIsDistance =
                        (c->type != Constraint::ANGLE) &&
                        (c->type != Constraint::LENGTH_RATIO);
                    SS.TW.shown.constraint = c->h;
                    SS.TW.shown.screen = TextWindow::SCREEN_STEP_DIMENSION;

                    SS.later.showTW = true;
                    SS.GW.ClearSelection();
                } else {
                    Error("Constraint must have a label, and must not be "
                          "a reference dimension.");
                }
            } else {
                Error("Bad selection for step dimension; select a constraint.");
            }
            break;

        case GraphicsWindow::MNU_VOLUME: {
            SMesh *m = &(SS.GetGroup(SS.GW.activeGroup)->runningMesh);
           
            double vol = 0;
            int i;
            for(i = 0; i < m->l.n; i++) {
                STriangle tr = m->l.elem[i];

                // Translate to place vertex A at (x, y, 0)
                Vector trans = Vector::From(tr.a.x, tr.a.y, 0);
                tr.a = (tr.a).Minus(trans);
                tr.b = (tr.b).Minus(trans);
                tr.c = (tr.c).Minus(trans);

                // Rotate to place vertex B on the y-axis. Depending on
                // whether the triangle is CW or CCW, C is either to the
                // right or to the left of the y-axis. This handles the
                // sign of our normal.
                Vector u = Vector::From(-tr.b.y, tr.b.x, 0);
                u = u.WithMagnitude(1);
                Vector v = Vector::From(tr.b.x, tr.b.y, 0);
                v = v.WithMagnitude(1);
                Vector n = Vector::From(0, 0, 1);

                tr.a = (tr.a).DotInToCsys(u, v, n);
                tr.b = (tr.b).DotInToCsys(u, v, n);
                tr.c = (tr.c).DotInToCsys(u, v, n);

                n = tr.Normal().WithMagnitude(1);

                // Triangles on edge don't contribute
                if(fabs(n.z) < LENGTH_EPS) continue;
               
                // The plane has equation p dot n = a dot n
                double d = (tr.a).Dot(n);
                // nx*x + ny*y + nz*z = d
                // nz*z = d - nx*x - ny*y
                double A = -n.x/n.z, B = -n.y/n.z, C = d/n.z;

                double mac = tr.c.y/tr.c.x, mbc = (tr.c.y - tr.b.y)/tr.c.x;
                double xc = tr.c.x, yb = tr.b.y;
               
                // I asked Maple for
                //    int(int(A*x + B*y +C, y=mac*x..(mbc*x + yb)), x=0..xc);
                double integral = 
                    (1.0/3)*(
                        A*(mbc-mac)+
                        (1.0/2)*B*(mbc*mbc-mac*mac)
                    )*(xc*xc*xc)+
                    (1.0/2)*(A*yb+B*yb*mbc+C*(mbc-mac))*xc*xc+
                    C*yb*xc+
                    (1.0/2)*B*yb*yb*xc;

                vol += integral;
            }
            SS.TW.shown.volume = vol;
            SS.TW.GoToScreen(TextWindow::SCREEN_MESH_VOLUME);
            SS.later.showTW = true;
            break;
        }

        case GraphicsWindow::MNU_TRACE_PT:
            if(gs.points == 1 && gs.n == 1) {
                SS.traced.point = gs.point[0];
                SS.GW.ClearSelection();
            } else {
                Error("Bad selection for trace; select a single point.");
            }
            break;
            
        case GraphicsWindow::MNU_STOP_TRACING: {
            char exportFile[MAX_PATH] = "";
            if(GetSaveFile(exportFile, CSV_EXT, CSV_PATTERN)) {
                FILE *f = fopen(exportFile, "wb");
                if(f) {
                    int i;
                    SContour *sc = &(SS.traced.path);
                    for(i = 0; i < sc->l.n; i++) {
                        Vector p = sc->l.elem[i].p;
                        double s = SS.exportScale;
                        fprintf(f, "%.10f, %.10f, %.10f\r\n",
                            p.x/s, p.y/s, p.z/s);
                    }
                    fclose(f);
                } else {
                    Error("Couldn't write to '%s'", exportFile);
                }
            }
            // Clear the trace, and stop tracing
            SS.traced.point = Entity::NO_ENTITY;
            SS.traced.path.l.Clear();
            InvalidateGraphics();
            break;
        }

        default: oops();
    }
}

void SolveSpace::Crc::ProcessBit(int bit) {
    bool topWasSet = ((shiftReg & (1 << 31)) != 0);

    shiftReg <<= 1;
    if(bit) {
        shiftReg |= 1;
    }
    
    if(topWasSet) {
        shiftReg ^= POLY;
    }
}

void SolveSpace::Crc::ProcessByte(BYTE b) {
    int i;
    for(i = 0; i < 8; i++) {
        ProcessBit(b & (1 << i));
    }
}

void SolveSpace::Crc::ProcessString(char *s) {
    for(; *s; s++) {
        if(*s != '\n' && *s != '\r') {
            ProcessByte((BYTE)*s);
        }
    }
}

bool SolveSpace::LicenseValid(char *line1, char *line2, char *users, DWORD key)
{
    BYTE magic[17] = {
        203, 244, 134, 225,  45, 250,  70,  65, 
        224, 189,  35,   3, 228,  51,  77, 169,
        0
    };
    
    crc.shiftReg = 0;
    crc.ProcessString(line1);
    crc.ProcessString(line2);
    crc.ProcessString(users);
    crc.ProcessString((char *)magic);

    return (key == crc.shiftReg);
}

void SolveSpace::CleanEol(char *in) {
    char *s;
    s = strchr(in, '\r');
    if(s) *s = '\0';
    s = strchr(in, '\n');
    if(s) *s = '\0';
}

void SolveSpace::LoadLicenseFile(char *filename) {
    FILE *f = fopen(filename, "rb");
    if(!f) {
        Error("Couldn't open file '%s'", filename);
        return;
    }

    char buf[100];
    fgets(buf, sizeof(buf), f);
    char *str = "±²³SolveSpaceLicense";
    if(memcmp(buf, str, strlen(str)) != 0) {
        fclose(f);
        Error("This is not a license file,");
        return;
    }

    char line1[512], line2[512], users[512];
    fgets(line1, sizeof(line1), f);
    CleanEol(line1);
    fgets(line2, sizeof(line2), f);
    CleanEol(line2);
    fgets(users, sizeof(users), f);
    CleanEol(users);

    fgets(buf, sizeof(buf), f);
    DWORD key = 0;
    sscanf(buf, "%x", &key);

    if(LicenseValid(line1, line2, users, key)) {
        // Install the new key
        CnfFreezeString(line1, "LicenseLine1");
        CnfFreezeString(line2, "LicenseLine2");
        CnfFreezeString(users, "LicenseUsers");
        CnfFreezeDWORD(key, "LicenseKey");
        Message("License key successfully installed.");

        // This updates our display in the text window to show that we're
        // licensed now.
        CheckLicenseFromRegistry();
        SS.later.showTW = true;
    } else {
        Error("License key invalid.");
    }
    fclose(f);
}

void SolveSpace::MenuHelp(int id) {
    switch(id) {
        case GraphicsWindow::MNU_WEBSITE:
            OpenWebsite("http://www.solvespace.com/helpmenu");
            break;
        
        case GraphicsWindow::MNU_ABOUT:
            Message("This is SolveSpace version 0.1.\r\n\r\n"
                  "For more information, see http://www.solvespace.com/\r\n\r\n"
                  "Built " __TIME__ " " __DATE__ ".\r\n\r\n"
                  "Copyright 2008 Jonathan Westhues, All Rights Reserved.");
            break;

        case GraphicsWindow::MNU_LICENSE: {
            char licenseFile[MAX_PATH] = "";
            if(GetOpenFile(licenseFile, LICENSE_EXT, LICENSE_PATTERN)) {
                SS.LoadLicenseFile(licenseFile);
            }
            break;
        }
                
        default: oops();
    }
}
