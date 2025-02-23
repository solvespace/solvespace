from unittest import TestCase
import slvs
# from solvespace import ConstraintType, E_NONE
from math import radians

class CoreTest(TestCase):
  def test_crank_rocker(self):
    """Crank rocker example."""
    print("Crank rocker")
    slvs.clear_sketch()
    g = 1
    wp = slvs.add_base_2d(g)
    p0 = slvs.add_point_2d(g, 0, 0, wp)
    slvs.dragged(g, p0, wp)
    p1 = slvs.add_point_2d(g, 90, 0, wp)
    slvs.dragged(g, p1, wp)
    line0 = slvs.add_line_2d(g, p0, p1, wp)
    p2 = slvs.add_point_2d(g, 20, 20, wp)
    p3 = slvs.add_point_2d(g, 0, 10, wp)
    p4 = slvs.add_point_2d(g, 30, 20, wp)
    slvs.distance(g, p2, p3, 40, wp)
    slvs.distance(g, p2, p4, 40, wp)
    slvs.distance(g, p3, p4, 70, wp)
    slvs.distance(g, p0, p3, 35, wp)
    slvs.distance(g, p1, p4, 70, wp)
    line1 = slvs.add_line_2d(g, p0, p3, wp)
    slvs.angle(g, line0, line1, 45, wp, False)
    # slvs.add_constraint(g, ConstraintType.ANGLE, wp, 45.0, E_NONE, E_NONE, line0, line1)

    result = slvs.solve_sketch(g, False)
    self.assertEqual(result['result'], slvs.ResultFlag.OKAY)
    x = slvs.get_param_value(p2['param'][0])
    y = slvs.get_param_value(p2['param'][1])
    self.assertAlmostEqual(39.54852, x, 4)
    self.assertAlmostEqual(61.91009, y, 4)

  def test_involute(self):
    """Involute example."""
    print("Involute")
    r = 10
    angle = 45
    slvs.clear_sketch()
    g = 1
    wp = slvs.add_base_2d(g)
    p0 = slvs.add_point_2d(g, 0, 0, wp)
    slvs.dragged(g, p0, wp)
    p1 = slvs.add_point_2d(g, 0, 10, wp)
    slvs.distance(g, p0, p1, r, wp)
    line0 = slvs.add_line_2d(g, p0, p1, wp)

    p2 = slvs.add_point_2d(g, 10, 10, wp)
    line1 = slvs.add_line_2d(g, p1, p2, wp)
    slvs.distance(g, p1, p2, r * radians(angle), wp)
    slvs.perpendicular(g, line0, line1, wp, False)

    p3 = slvs.add_point_2d(g, 10, 0, wp)
    slvs.dragged(g, p3, wp)
    line_base = slvs.add_line_2d(g, p0, p3, wp)
    slvs.angle(g, line0, line_base, angle, wp, False)

    result = slvs.solve_sketch(g, False)
    print("result", result)
    self.assertEqual(result['result'], slvs.ResultFlag.OKAY)
    x = slvs.get_param_value(p2['param'][0])
    y = slvs.get_param_value(p2['param'][1])
    self.assertAlmostEqual(12.62467, x, 4)
    self.assertAlmostEqual(1.51746, y, 4)

  def test_jansen_linkage(self):
    """Jansen's linkage example."""

    print("Jansen's linkage")
    slvs.clear_sketch()
    g = 1
    wp = slvs.add_base_2d(g)
    p0 = slvs.add_point_2d(g, 0, 0, wp)
    slvs.dragged(g, p0, wp)

    p1 = slvs.add_point_2d(g, 0, 20, wp)
    slvs.distance(g, p0, p1, 15, wp)
    line0 = slvs.add_line_2d(g, p0, p1, wp)

    p2 = slvs.add_point_2d(g, -38, -7.8, wp)
    slvs.dragged(g, p2, wp)
    p3 = slvs.add_point_2d(g, -50, 30, wp)
    p4 = slvs.add_point_2d(g, -70, -15, wp)
    slvs.distance(g, p2, p3, 41.5, wp)
    slvs.distance(g, p3, p4, 55.8, wp)
    slvs.distance(g, p2, p4, 40.1, wp)

    p5 = slvs.add_point_2d(g, -50, -50, wp)
    p6 = slvs.add_point_2d(g, -10, -90, wp)
    p7 = slvs.add_point_2d(g, -20, -40, wp)
    slvs.distance(g, p5, p6, 65.7, wp)
    slvs.distance(g, p6, p7, 49.0, wp)
    slvs.distance(g, p5, p7, 36.7, wp)

    slvs.distance(g, p1, p3, 50, wp)
    slvs.distance(g, p1, p7, 61.9, wp)

    p8 = slvs.add_point_2d(g, 20, 0, wp)
    line_base = slvs.add_line_2d(g, p0, p8, wp)
    slvs.angle(g, line0, line_base, 45, wp, False)

    result = slvs.solve_sketch(g, False)
    print("result", result)
    self.assertEqual(result['result'], slvs.ResultFlag.OKAY)
    x = slvs.get_param_value(p8['param'][0])
    y = slvs.get_param_value(p8['param'][1])
    self.assertAlmostEqual(18.93036, x, 4)
    self.assertAlmostEqual(13.63778, y, 4)

  def test_nut_cracker(self):
    print("Nut cracker")

    h0 = 0.5
    b0 = 0.75
    r0 = 0.25
    n1 = 1.5
    n2 = 2.3
    l0 = 3.25

    slvs.clear_sketch()
    g = 1
    wp = slvs.add_base_2d(g)
    p0 = slvs.add_point_2d(g, 0, 0, wp)
    slvs.dragged(g, p0, wp)

    p1 = slvs.add_point_2d(g, 2, 2, wp)
    p2 = slvs.add_point_2d(g, 2, 0, wp)
    line0 = slvs.add_line_2d(g, p0, p2, wp)
    slvs.horizontal(g, line0, wp)
    line1 = slvs.add_line_2d(g, p1, p2, wp)
    p3 = slvs.add_point_2d(g, b0 / 2, h0, wp)
    slvs.dragged(g, p3, wp)
    slvs.distance(g, p3, line1, r0, wp)
    slvs.distance(g, p0, p1, n1, wp)
    slvs.distance(g, p1, p2, n2, wp)

    result = slvs.solve_sketch(g, False)
    print("result", result)
    self.assertEqual(result['result'], slvs.ResultFlag.OKAY)
    x = slvs.get_param_value(p2['param'][0])
    print("x", x)
    ans_min = x - b0 / 2
    ans_max = l0 - r0 - b0 / 2
    self.assertAlmostEqual(1.01576, ans_min, 4)
    self.assertAlmostEqual(2.625, ans_max, 4)

  def test_pydemo(self):
    """Some sample code for slvs.dll.
    We draw some geometric entities, provide initial guesses for their positions,
    and then constrain them. The solver calculates their new positions,
    in order to satisfy the constraints.

    Copyright 2008-2013 Jonathan Westhues.
    Copyright 2016-2017 Yuan Chang [pyslvs@gmail.com] Solvespace bundled.

    An example of a constraint in 2d. In our first group, we create a workplane
    along the reference frame's xy plane. In a second group, we create some
    entities in that group and dimension them.
    """

    slvs.clear_sketch()
    g1 = 1
    g2 = 2
    # First, we create our workplane. Its origin corresponds to the origin
    # of our base frame (x y z) = (0 0 0)
    p101 = slvs.add_point_3d(g1, 0, 0, 0)
    # and it is parallel to the xy plane, so it has basis vectors (1 0 0)
    # and (0 1 0).
    (w, vx, vy, vz) = slvs.make_quaternion(1, 0, 0, 0, 1, 0)
    n102 = slvs.add_normal_3d(g1, w, vx, vy, vz)
    wp200 = slvs.add_workplane(g1, p101, n102)

    # Now create a second group. We'll solve group 2, while leaving group 1
    # constant; so the workplane that we've created will be locked down,
    # and the solver can't move it.
    p301 = slvs.add_point_2d(g2, 10, 20, wp200)
    p302 = slvs.add_point_2d(g2, 20, 10, wp200)

    # And we create a line segment with those endpoints.
    l400 = slvs.add_line_2d(g2, p301, p302, wp200)

    # Now three more points.
    p303 = slvs.add_point_2d(g2, 100, 120, wp200)
    p304 = slvs.add_point_2d(g2, 120, 110, wp200)
    p305 = slvs.add_point_2d(g2, 115, 115, wp200)

    # And arc, centered at point 303, starting at point 304, ending at
    # point 305.
    a401 = slvs.add_arc(g2, n102, p303, p304, p305, wp200)

    # Now one more point, and a distance
    p306 = slvs.add_point_2d(g2, 200, 200, wp200)
    d307 = slvs.add_distance(g2, 30, wp200)

    # And a complete circle, centered at point 306 with radius equal to
    # distance 307. The normal is 102, the same as our workplane.
    c402 = slvs.add_circle(g2, n102, p306, d307, wp200)

    # The length of our line segment is 30.0 units.
    slvs.distance(g2, p301, p302, 30, wp200)

    # And the distance from our line segment to the origin is 10.0 units.
    slvs.distance(g2, p101, l400, 10, wp200)

    # And the line segment is vertical.
    slvs.vertical(g2, l400, wp200)

    # And the distance from one endpoint to the origin is 15.0 units.
    slvs.distance(g2, p301, p101, 15, wp200)

    # The arc and the circle have equal radius.
    slvs.equal(g2, a401, c402, wp200)

    # The arc has radius 17.0 units.
    slvs.diameter(g2, a401, 17 * 2)

    # If the solver fails, then aslvs it to report which constraints caused
    # the problem.

    # And solve.
    g1result = slvs.solve_sketch(g1, False)
    g2result = slvs.solve_sketch(g2, False)
    self.assertEqual(g2result['result'], slvs.ResultFlag.OKAY)
    x = slvs.get_param_value(p301['param'][0])
    y = slvs.get_param_value(p301['param'][1])
    self.assertAlmostEqual(10, x, 4)
    self.assertAlmostEqual(11.18030, y, 4)
    x = slvs.get_param_value(p302['param'][0])
    y = slvs.get_param_value(p302['param'][1])
    self.assertAlmostEqual(10, x, 4)
    self.assertAlmostEqual(-18.81966, y, 4)
    x = slvs.get_param_value(p303['param'][0])
    y = slvs.get_param_value(p303['param'][1])
    self.assertAlmostEqual(101.11418, x, 4)
    self.assertAlmostEqual(119.04153, y, 4)
    x = slvs.get_param_value(p304['param'][0])
    y = slvs.get_param_value(p304['param'][1])
    self.assertAlmostEqual(116.47661, x, 4)
    self.assertAlmostEqual(111.76171, y, 4)
    x = slvs.get_param_value(p305['param'][0])
    y = slvs.get_param_value(p305['param'][1])
    self.assertAlmostEqual(117.40922, x, 4)
    self.assertAlmostEqual(114.19676, y, 4)
    x = slvs.get_param_value(p306['param'][0])
    y = slvs.get_param_value(p306['param'][1])
    self.assertAlmostEqual(200, x, 4)
    self.assertAlmostEqual(200, y, 4)
    x = slvs.get_param_value(d307['param'][0])
    self.assertAlmostEqual(17, x, 4)
    self.assertEqual(6, g2result['dof'])
