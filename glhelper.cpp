#include "solvespace.h"

// A public-domain Hershey vector font ("Simplex").
#include "font.table"

static bool ColorLocked;

void glxWriteText(char *str)
{
    double scale = 0.5/SS.GW.scale;
    int xo = 5;
    int yo = 5;
    
    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;

        c -= 32;

        glBegin(GL_LINE_STRIP);
        int j;
        for(j = 0; j < Font[c].points; j++) {
            int x = Font[c].coord[j*2];
            int y = Font[c].coord[j*2+1];

            if(x == PEN_UP && y == PEN_UP) {
                glEnd();
                glBegin(GL_LINE_STRIP);
            } else {
                glVertex3d((xo + x)*scale, (yo + y)*scale, 0);
            }
        }
        glEnd();

        xo += Font[c].width;
    }
}

void glxVertex3v(Vector u)
{
    glVertex3f((GLfloat)u.x, (GLfloat)u.y, (GLfloat)u.z);
}

void glxTranslatev(Vector u)
{
    glTranslated((GLdouble)u.x, (GLdouble)u.y, (GLdouble)u.z);
}

void glxOntoCsys(Vector u, Vector v)
{
    double mat[16];
    Vector n = u.Cross(v);
    MakeMatrix(mat,     u.x, v.x, n.x, 0,
                        u.y, v.y, n.y, 0,
                        u.z, v.z, n.z, 0,
                        0,   0,   0,   1);
    glMultMatrixd(mat);
}

void glxLockColorTo(double r, double g, double b)
{
    ColorLocked = false;    
    glxColor(r, g, b);
    ColorLocked = true;
}

void glxUnlockColor(void)
{
    ColorLocked = false;    
}

void glxColor(double r, double g, double b)
{
    if(!ColorLocked) {
        glColor3f((GLfloat)r, (GLfloat)g, (GLfloat)b);
    }
}
