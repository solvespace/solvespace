#include "solvespace.h"

SolveSpace SS;

void SolveSpace::Init(char *cmdLine) {
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
    // Mesh tolerance
    meshTol = CnfThawFloat(1.0f, "MeshTolerance");
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
    // Mesh tolerance
    CnfFreezeFloat((float)meshTol, "MeshTolerance");
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
    int s = 7 + (int)(sqrt(r*SS.GW.scale/meshTol));
    return min(s, 40);
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


void SolveSpace::AfterNewFile(void) {
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
