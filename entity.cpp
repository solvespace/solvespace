#include "solvespace.h"

void Entity::LineDrawHitTest(Vector a, Vector b) {
    glBegin(GL_LINE_STRIP);
        glxVertex3v(a);
        glxVertex3v(b);
    glEnd();
}

void Entity::Draw(void) {
    switch(type) {
        case CSYS_2D: {
            if(!SS.GW.show2dCsyss) break;

            Vector p;
            double a, b, c, d;

            SS.point.FindById(point(16))->GetInto(&p);
            a = SS.param.FindById(param(0))->val;
            b = SS.param.FindById(param(1))->val;
            c = SS.param.FindById(param(2))->val;
            d = SS.param.FindById(param(3))->val;

            Vector u = Vector::RotationU(a, b, c, d);
            Vector v = Vector::RotationV(a, b, c, d);

            double s = (min(SS.GW.width, SS.GW.height))*0.4;

            Vector pp = (p.Plus(u)).Plus(v);
            Vector pm = (p.Plus(u)).Minus(v);
            Vector mm = (p.Minus(u)).Minus(v);
            Vector mp = (p.Minus(u)).Plus(v);
            pp = pp.ScaledBy(s);
            pm = pm.ScaledBy(s);
            mm = mm.ScaledBy(s);
            mp = mp.ScaledBy(s);

            LineDrawHitTest(pp, pm);
            LineDrawHitTest(pm, mm);
            LineDrawHitTest(mm, mp);
            LineDrawHitTest(mp, pp);

            Request *r = SS.request.FindById(this->request());
            glPushMatrix();
                glxTranslatev(mm);
                glxOntoCsys(u, v);
                glxWriteText(r->name.str);
            glPopMatrix();

            glEnd();
            break;
        }
        default:
            oops();
    }
}
