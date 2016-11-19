/*-----------------------------------------------------------------------------
 * Some sample code for slvs.dll. We draw some geometric entities, provide
 * initial guesses for their positions, and then constrain them. The solver
 * calculates their new positions, in order to satisfy the constraints.
 *
 * Copyright 2008-2013 Jonathan Westhues.
 *---------------------------------------------------------------------------*/
#ifdef WIN32
#   include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <slvs.h>

static Slvs_System sys;

static void *CheckMalloc(size_t n)
{
    void *r = malloc(n);
    if(!r) {
        printf("out of memory!\n");
        exit(-1);
    }
    return r;
}

/*-----------------------------------------------------------------------------
 * An example of a constraint in 3d. We create a single group, with some
 * entities and constraints.
 *---------------------------------------------------------------------------*/
void Example3d()
{
    /* This will contain a single group, which will arbitrarily number 1. */
    Slvs_hGroup g = 1;

    /* A point, initially at (x y z) = (10 10 10) */
    sys.param[sys.params++] = Slvs_MakeParam(1, g, 10.0);
    sys.param[sys.params++] = Slvs_MakeParam(2, g, 10.0);
    sys.param[sys.params++] = Slvs_MakeParam(3, g, 10.0);
    sys.entity[sys.entities++] = Slvs_MakePoint3d(101, g, 1, 2, 3);
    /* and a second point at (20 20 20) */
    sys.param[sys.params++] = Slvs_MakeParam(4, g, 20.0);
    sys.param[sys.params++] = Slvs_MakeParam(5, g, 20.0);
    sys.param[sys.params++] = Slvs_MakeParam(6, g, 20.0);
    sys.entity[sys.entities++] = Slvs_MakePoint3d(102, g, 4, 5, 6);
    /* and a line segment connecting them. */
    sys.entity[sys.entities++] = Slvs_MakeLineSegment(200, g,
                                        SLVS_FREE_IN_3D, 101, 102);

    /* The distance between the points should be 30.0 units. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            1, g,
                                            SLVS_C_PT_PT_DISTANCE,
                                            SLVS_FREE_IN_3D,
                                            30.0,
                                            101, 102, 0, 0);

    /* Let's tell the solver to keep the second point as close to constant
     * as possible, instead moving the first point. */
    sys.dragged[0] = 4;
    sys.dragged[1] = 5;
    sys.dragged[2] = 6;

    /* Now that we have written our system, we solve. */
    Slvs_Solve(&sys, g);

    if(sys.result == SLVS_RESULT_OKAY) {
        printf("okay; now at (%.3f %.3f %.3f)\n"
               "             (%.3f %.3f %.3f)\n",
                sys.param[0].val, sys.param[1].val, sys.param[2].val,
                sys.param[3].val, sys.param[4].val, sys.param[5].val);
        printf("%d DOF\n", sys.dof);
    } else {
        printf("solve failed");
    }
}

/*-----------------------------------------------------------------------------
 * An example of a constraint in 2d. In our first group, we create a workplane
 * along the reference frame's xy plane. In a second group, we create some
 * entities in that group and dimension them.
 *---------------------------------------------------------------------------*/
void Example2d()
{
    Slvs_hGroup g;
    double qw, qx, qy, qz;

    g = 1;
    /* First, we create our workplane. Its origin corresponds to the origin
     * of our base frame (x y z) = (0 0 0) */
    sys.param[sys.params++] = Slvs_MakeParam(1, g, 0.0);
    sys.param[sys.params++] = Slvs_MakeParam(2, g, 0.0);
    sys.param[sys.params++] = Slvs_MakeParam(3, g, 0.0);
    sys.entity[sys.entities++] = Slvs_MakePoint3d(101, g, 1, 2, 3);
    /* and it is parallel to the xy plane, so it has basis vectors (1 0 0)
     * and (0 1 0). */
    Slvs_MakeQuaternion(1, 0, 0,
                        0, 1, 0, &qw, &qx, &qy, &qz);
    sys.param[sys.params++] = Slvs_MakeParam(4, g, qw);
    sys.param[sys.params++] = Slvs_MakeParam(5, g, qx);
    sys.param[sys.params++] = Slvs_MakeParam(6, g, qy);
    sys.param[sys.params++] = Slvs_MakeParam(7, g, qz);
    sys.entity[sys.entities++] = Slvs_MakeNormal3d(102, g, 4, 5, 6, 7);

    sys.entity[sys.entities++] = Slvs_MakeWorkplane(200, g, 101, 102);

    /* Now create a second group. We'll solve group 2, while leaving group 1
     * constant; so the workplane that we've created will be locked down,
     * and the solver can't move it. */
    g = 2;
    /* These points are represented by their coordinates (u v) within the
     * workplane, so they need only two parameters each. */
    sys.param[sys.params++] = Slvs_MakeParam(11, g, 10.0);
    sys.param[sys.params++] = Slvs_MakeParam(12, g, 20.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(301, g, 200, 11, 12);

    sys.param[sys.params++] = Slvs_MakeParam(13, g, 20.0);
    sys.param[sys.params++] = Slvs_MakeParam(14, g, 10.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(302, g, 200, 13, 14);

    /* And we create a line segment with those endpoints. */
    sys.entity[sys.entities++] = Slvs_MakeLineSegment(400, g,
                                        200, 301, 302);

    /* Now three more points. */
    sys.param[sys.params++] = Slvs_MakeParam(15, g, 100.0);
    sys.param[sys.params++] = Slvs_MakeParam(16, g, 120.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(303, g, 200, 15, 16);

    sys.param[sys.params++] = Slvs_MakeParam(17, g, 120.0);
    sys.param[sys.params++] = Slvs_MakeParam(18, g, 110.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(304, g, 200, 17, 18);

    sys.param[sys.params++] = Slvs_MakeParam(19, g, 115.0);
    sys.param[sys.params++] = Slvs_MakeParam(20, g, 115.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(305, g, 200, 19, 20);

    /* And arc, centered at point 303, starting at point 304, ending at
     * point 305. */
    sys.entity[sys.entities++] = Slvs_MakeArcOfCircle(401, g, 200, 102,
                                    303, 304, 305);

    /* Now one more point, and a distance */
    sys.param[sys.params++] = Slvs_MakeParam(21, g, 200.0);
    sys.param[sys.params++] = Slvs_MakeParam(22, g, 200.0);
    sys.entity[sys.entities++] = Slvs_MakePoint2d(306, g, 200, 21, 22);

    sys.param[sys.params++] = Slvs_MakeParam(23, g, 30.0);
    sys.entity[sys.entities++] = Slvs_MakeDistance(307, g, 200, 23);

    /* And a complete circle, centered at point 306 with radius equal to
     * distance 307. The normal is 102, the same as our workplane. */
    sys.entity[sys.entities++] = Slvs_MakeCircle(402, g, 200,
                                    306, 102, 307);


    /* The length of our line segment is 30.0 units. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            1, g,
                                            SLVS_C_PT_PT_DISTANCE,
                                            200,
                                            30.0,
                                            301, 302, 0, 0);

    /* And the distance from our line segment to the origin is 10.0 units. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            2, g,
                                            SLVS_C_PT_LINE_DISTANCE,
                                            200,
                                            10.0,
                                            101, 0, 400, 0);
    /* And the line segment is vertical. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            3, g,
                                            SLVS_C_VERTICAL,
                                            200,
                                            0.0,
                                            0, 0, 400, 0);
    /* And the distance from one endpoint to the origin is 15.0 units. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            4, g,
                                            SLVS_C_PT_PT_DISTANCE,
                                            200,
                                            15.0,
                                            301, 101, 0, 0);
#if 0
    /* And same for the other endpoint; so if you add this constraint then
     * the sketch is overconstrained and will signal an error. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            5, g,
                                            SLVS_C_PT_PT_DISTANCE,
                                            200,
                                            18.0,
                                            302, 101, 0, 0);
#endif /* 0 */

    /* The arc and the circle have equal radius. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            6, g,
                                            SLVS_C_EQUAL_RADIUS,
                                            200,
                                            0.0,
                                            0, 0, 401, 402);
    /* The arc has radius 17.0 units. */
    sys.constraint[sys.constraints++] = Slvs_MakeConstraint(
                                            7, g,
                                            SLVS_C_DIAMETER,
                                            200,
                                            17.0*2,
                                            0, 0, 401, 0);

    /* If the solver fails, then ask it to report which constraints caused
     * the problem. */
    sys.calculateFaileds = 1;

    /* And solve. */
    Slvs_Solve(&sys, g);

    if(sys.result == SLVS_RESULT_OKAY) {
        printf("solved okay\n");
        printf("line from (%.3f %.3f) to (%.3f %.3f)\n",
                sys.param[7].val, sys.param[8].val,
                sys.param[9].val, sys.param[10].val);

        printf("arc center (%.3f %.3f) start (%.3f %.3f) finish (%.3f %.3f)\n",
                sys.param[11].val, sys.param[12].val,
                sys.param[13].val, sys.param[14].val,
                sys.param[15].val, sys.param[16].val);

        printf("circle center (%.3f %.3f) radius %.3f\n",
                sys.param[17].val, sys.param[18].val,
                sys.param[19].val);
        printf("%d DOF\n", sys.dof);
    } else {
        int i;
        printf("solve failed: problematic constraints are:");
        for(i = 0; i < sys.faileds; i++) {
            printf(" %d", sys.failed[i]);
        }
        printf("\n");
        if(sys.result == SLVS_RESULT_INCONSISTENT) {
            printf("system inconsistent\n");
        } else {
            printf("system nonconvergent\n");
        }
    }
}

int main()
{
    sys.param      = CheckMalloc(50*sizeof(sys.param[0]));
    sys.entity     = CheckMalloc(50*sizeof(sys.entity[0]));
    sys.constraint = CheckMalloc(50*sizeof(sys.constraint[0]));

    sys.failed  = CheckMalloc(50*sizeof(sys.failed[0]));
    sys.faileds = 50;

    /*Example3d();*/
    for(;;) {
        Example2d();
        sys.params = sys.constraints = sys.entities = 0;
        break;
    }
    return 0;
}

