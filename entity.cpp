#include "solvespace.h"

void Entity::Draw(void) {
    int i;
    for(i = 0; i < 3; i++) {
        Vector p, u, v;

        if(i == 0) {
            p.x = 0; p.y = 0; p.z = 1;
        } else if(i == 1) {
            p.x = 0; p.y = 1; p.z = 0;
        } else {
            p.x = 1; p.y = 0; p.z = 0;
        }

        u = p.Normal(0);
        v = p.Normal(1);

        double s = (min(SS.GW.width, SS.GW.height))*0.4;

        u = u.ScaledBy(s);
        v = v.ScaledBy(s);

        Vector r;
        glBegin(GL_LINE_LOOP);
            r = p; r = r.Minus(v); r = r.Minus(u); glVertex3v(r);
            r = p; r = r.Plus(v);  r = r.Minus(u); glVertex3v(r);
            r = p; r = r.Plus(v);  r = r.Plus(u);  glVertex3v(r);
            r = p; r = r.Minus(v); r = r.Plus(u);  glVertex3v(r);
        glEnd();
    }
}
