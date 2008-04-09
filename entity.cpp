#include "solvespace.h"

void Entity::Draw(void) {
    switch(type) {
        case CSYS_2D: {
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

            u = u.ScaledBy(s);
            v = v.ScaledBy(s);

            Vector r;
            glBegin(GL_LINE_LOOP);
                r = p; r = r.Minus(v); r = r.Minus(u); glVertex3v(r);
                r = p; r = r.Plus(v);  r = r.Minus(u); glVertex3v(r);
                r = p; r = r.Plus(v);  r = r.Plus(u);  glVertex3v(r);
                r = p; r = r.Minus(v); r = r.Plus(u);  glVertex3v(r);
            glEnd();
            break;
        }
        default:
            oops();
    }
}
