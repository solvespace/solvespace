// -----------------------------------------------------------------------------
// Some sample code for slvs.dll. We draw some geometric entities, provide
// initial guesses for their positions, and then constrain them. The solver
// calculates their new positions, in order to satisfy the constraints.
// 
// The library is distributed as a DLL, but the functions are designed to
// be usable from .net languages through a P/Invoke. This file contains an
// example of that process, and a wrapper class around those P/Invoke'd
// functions that you may wish to use a starting point in your own
// application.
// 
// Copyright 2008-2013 Jonathan Westhues.
// -----------------------------------------------------------------------------

namespace CsDemo;

using SolveSpace;

public static class CsDemo
{
  // Call our example functions, which set up some kind of sketch, solve
  // it, and then print the result. 
  public static void Main()
  {
    Console.WriteLine("EXAMPLE IN 3d (by objects):");
    Example3dWithObjects();
    Console.WriteLine("");

    Console.WriteLine("EXAMPLE IN 2d (by objects):");
    Example2dWithObjects();
    Console.WriteLine("");

    Console.WriteLine("EXAMPLE IN 3d (by handles):");
    Example3dWithHandles();
    Console.WriteLine("");

    Console.WriteLine("EXAMPLE IN 2d (by handles):");
    Example2dWithHandles();
    Console.WriteLine("");
  }

  // This is the simplest way to use the library. A set of wrapper
  // classes allow us to represent entities (e.g., lines and points)
  // as .net objects. So we create an Solver object, which will contain
  // the entire sketch, with all the entities and constraints.
  // 
  // We then create entity objects (for example, points and lines)
  // associated with that sketch, indicating the initial positions of
  // those entities and any hierarchical relationships among them (e.g.,
  // defining a line entity in terms of endpoint entities). We also add
  // constraints associated with those entities.
  // 
  // Finally, we solve, and print the new positions of the entities. If the
  // solution succeeded, then the entities will satisfy the constraints. If
  // not, then the solver will suggest problematic constraints that, if
  // removed, would render the sketch solvable.

  private static void Example3dWithObjects()
  {
    var slv = new Solver();

    // This will contain a single group, which will arbitrarily number 1.
    uint g = 1;

    // A point, initially at (x y z) = (10 10 10)
    var p1 = slv.NewPoint3d(g, 10.0, 10.0, 10.0);
    // and a second point at (20 20 20)
    var p2 = slv.NewPoint3d(g, 20.0, 20.0, 20.0);

    // and a line segment connecting them.
    var ln = slv.NewLineSegment(g, slv.FreeIn3d(), p1, p2);

    // The distance between the points should be 30.0 units.
    slv.AddConstraint(1, g, Solver.SLVS_C_PT_PT_DISTANCE, slv.FreeIn3d(), 30.0, p1, p2, null, null);

    // Let's tell the solver to keep the second point as close to constant
    // as possible, instead moving the first point.
    slv.Solve(g, p2, true);

    if ((slv.GetResult() == Solver.SLVS_RESULT_OKAY))
    {
      // We call the GetX(), GetY(), and GetZ() functions to see
      // where the solver moved our points to.
      Console.WriteLine($"okay; now at ({p1.GetX():F3}, {p1.GetY():F3}, {p1.GetZ():F3})");
      Console.WriteLine($"             ({p2.GetX():F3}, {p2.GetY():F3}, {p2.GetZ():F3})");
      Console.WriteLine(slv.GetDof() + " DOF");
    }
    else
      Console.WriteLine("solve failed");
  }

  private static void Example2dWithObjects()
  {
    var slv = new Solver();

    uint g = 1;

    // First, we create our workplane. Its origin corresponds to the origin
    // of our base frame (x y z) = (0 0 0)
    var origin = slv.NewPoint3d(g, 0.0, 0.0, 0.0);
    // and it is parallel to the xy plane, so it has basis vectors (1 0 0)
    // and (0 1 0).
    var normal = slv.NewNormal3d(g, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0);

    var wrkpl = slv.NewWorkplane(g, origin, normal);

    // Now create a second group. We'll solve group 2, while leaving group 1
    // constant; so the workplane that we've created will be locked down,
    // and the solver can't move it.
    g = 2;
    // These points are represented by their coordinates (u v) within the
    // workplane, so they need only two parameters each.
    var pl1 = slv.NewPoint2d(g, wrkpl, 10.0, 20.0);
    var pl2 = slv.NewPoint2d(g, wrkpl, 20.0, 10.0);

    // And we create a line segment with those endpoints.
    var ln = slv.NewLineSegment(g, wrkpl, pl1, pl2);

    // Now three more points.
    var pc = slv.NewPoint2d(g, wrkpl, 100.0, 120.0);
    var ps = slv.NewPoint2d(g, wrkpl, 120.0, 110.0);
    var pf = slv.NewPoint2d(g, wrkpl, 115.0, 115.0);

    // And arc, centered at point pc, starting at point ps, ending at
    // point pf.
    var arc = slv.NewArcOfCircle(g, wrkpl, normal, pc, ps, pf);

    // Now one more point, and a distance
    var pcc = slv.NewPoint2d(g, wrkpl, 200.0, 200.0);
    var r = slv.NewDistance(g, wrkpl, 30.0);

    // And a complete circle, centered at point pcc with radius equal to
    // distance r. The normal is the same as for our workplane.
    var circle = slv.NewCircle(g, wrkpl, pcc, normal, r);

    // The length of our line segment is 30.0 units.
    slv.AddConstraint(1, g, Solver.SLVS_C_PT_PT_DISTANCE, wrkpl, 30.0, pl1, pl2, null, null);

    // And the distance from our line segment to the origin is 10.0 units.
    slv.AddConstraint(2, g, Solver.SLVS_C_PT_LINE_DISTANCE, wrkpl, 10.0, origin, null, ln, null);

    // And the line segment is vertical.
    slv.AddConstraint(3, g, Solver.SLVS_C_VERTICAL, wrkpl, 0.0, null, null, ln, null);

    // And the distance from one endpoint to the origin is 15.0 units.
    slv.AddConstraint(4, g, Solver.SLVS_C_PT_PT_DISTANCE, wrkpl, 15.0, pl1, origin, null, null);

    // And same for the other endpoint; so if you add this constraint then
    // the sketch is overconstrained and will signal an error.
    // slv.AddConstraint(5, g, Solver.SLVS_C_PT_PT_DISTANCE, _
    // wrkpl, 18.0, pl2, origin, Nothing, Nothing)

    // The arc and the circle have equal radius.
    slv.AddConstraint(6, g, Solver.SLVS_C_EQUAL_RADIUS, wrkpl, 0.0, null, null, arc, circle);

    // The arc has radius 17.0 units.
    slv.AddConstraint(7, g, Solver.SLVS_C_DIAMETER, wrkpl, 2 * 17.0, null, null, arc, null);

    // If the solver fails, then ask it to report which constraints caused
    // the problem.
    slv.Solve(g, true);

    if (slv.GetResult() == Solver.SLVS_RESULT_OKAY)
    {
      Console.WriteLine("solved okay");
      // We call the GetU(), GetV(), and GetDistance() functions to
      // see where the solver moved our points and distances to.
      Console.WriteLine($"line from ({pl1.GetU():F3} {pl1.GetV():F3}) to ({pl2.GetU():F3} {pl2.GetV():F3})");
      Console.WriteLine("arc center ({0:F3} {1:F3}) start ({2:F3} {3:F3}) " + "finish ({4:F3} {5:F3})", pc.GetU(), pc.GetV(), ps.GetU(), ps.GetV(), pf.GetU(), pf.GetV());
      Console.WriteLine($"circle center ({pcc.GetU():F3} {pcc.GetV():F3}) radius {r.GetDistance():F3}");

      Console.WriteLine(slv.GetDof() + " DOF");
    }
    else
    {
      Console.Write("solve failed; problematic constraints are:");
      foreach (var t in slv.GetFaileds())
      {
        Console.Write(" " + t);
      }

      Console.WriteLine("");
      if ((slv.GetResult() == Solver.SLVS_RESULT_INCONSISTENT))
      {
        Console.WriteLine("system inconsistent");
      }
      else
      {
        Console.WriteLine("system nonconvergent");
      }
    }
  }

  // This is a lower-level way to use the library. Internally, the library
  // represents parameters, entities, and constraints by integer handles.
  // Here, those handles are assigned manually, and not by the wrapper
  // classes.

  private static void Example3dWithHandles()
  {
    var slv = new Solver();

    // This will contain a single group, which will arbitrarily number 1.
    uint g = 1;

    // A point, initially at (x y z) = (10 10 10)
    slv.AddParam(1, g, 10.0);
    slv.AddParam(2, g, 10.0);
    slv.AddParam(3, g, 10.0);
    slv.AddPoint3d(101, g, 1, 2, 3);

    // and a second point at (20 20 20)
    slv.AddParam(4, g, 20.0);
    slv.AddParam(5, g, 20.0);
    slv.AddParam(6, g, 20.0);
    slv.AddPoint3d(102, g, 4, 5, 6);

    // and a line segment connecting them.
    slv.AddLineSegment(200, g, Solver.SLVS_FREE_IN_3D, 101, 102);

    // The distance between the points should be 30.0 units.
    slv.AddConstraint(1, g, Solver.SLVS_C_PT_PT_DISTANCE, Solver.SLVS_FREE_IN_3D, 30.0, 101, 102, 0, 0);

    // Let's tell the solver to keep the second point as close to constant
    // as possible, instead moving the first point. That's parameters
    // 4, 5, and 6.
    slv.Solve(g, 4, 5, 6, 0, true);

    if ((slv.GetResult() == Solver.SLVS_RESULT_OKAY))
    {
      // Note that we are referring to the parameters by their handles,
      // and not by their index in the list. This is a difference from
      // the C example.
      Console.WriteLine($"okay; now at ({slv.GetParamByHandle(1):F3}, {slv.GetParamByHandle(2):F3}, {slv.GetParamByHandle(3):F3})");
      Console.WriteLine($"             ({slv.GetParamByHandle(4):F3}, {slv.GetParamByHandle(5):F3}, {slv.GetParamByHandle(6):F3})");
      Console.WriteLine(slv.GetDof() + " DOF");
    }
    else
    {
      Console.WriteLine("solve failed");
    }
  }

  private static void Example2dWithHandles()
  {
    double qw = 0, qx = 0, qy = 0, qz = 0;

    var slv = new Solver();

    uint g = 1;

    // First, we create our workplane. Its origin corresponds to the origin
    // of our base frame (x y z) = (0 0 0)
    slv.AddParam(1, g, 0.0);
    slv.AddParam(2, g, 0.0);
    slv.AddParam(3, g, 0.0);
    slv.AddPoint3d(101, g, 1, 2, 3);
    // and it is parallel to the xy plane, so it has basis vectors (1 0 0)
    // and (0 1 0).
    slv.MakeQuaternion(1, 0, 0, 0, 1, 0, ref qw, ref qx, ref qy, ref qz);
    slv.AddParam(4, g, qw);
    slv.AddParam(5, g, qx);
    slv.AddParam(6, g, qy);
    slv.AddParam(7, g, qz);
    slv.AddNormal3d(102, g, 4, 5, 6, 7);

    slv.AddWorkplane(200, g, 101, 102);

    // Now create a second group. We'll solve group 2, while leaving group 1
    // constant; so the workplane that we've created will be locked down,
    // and the solver can't move it.
    g = 2;
    // These points are represented by their coordinates (u v) within the
    // workplane, so they need only two parameters each.
    slv.AddParam(11, g, 10.0);
    slv.AddParam(12, g, 20.0);
    slv.AddPoint2d(301, g, 200, 11, 12);

    slv.AddParam(13, g, 20.0);
    slv.AddParam(14, g, 10.0);
    slv.AddPoint2d(302, g, 200, 13, 14);

    // And we create a line segment with those endpoints.
    slv.AddLineSegment(400, g, 200, 301, 302);

    // Now three more points.
    slv.AddParam(15, g, 100.0);
    slv.AddParam(16, g, 120.0);
    slv.AddPoint2d(303, g, 200, 15, 16);

    slv.AddParam(17, g, 120.0);
    slv.AddParam(18, g, 110.0);
    slv.AddPoint2d(304, g, 200, 17, 18);

    slv.AddParam(19, g, 115.0);
    slv.AddParam(20, g, 115.0);
    slv.AddPoint2d(305, g, 200, 19, 20);

    // And arc, centered at point 303, starting at point 304, ending at
    // point 305.
    slv.AddArcOfCircle(401, g, 200, 102, 303, 304, 305);

    // Now one more point, and a distance
    slv.AddParam(21, g, 200.0);
    slv.AddParam(22, g, 200.0);
    slv.AddPoint2d(306, g, 200, 21, 22);

    slv.AddParam(23, g, 30.0);
    slv.AddDistance(307, g, 200, 23);

    // And a complete circle, centered at point 306 with radius equal to
    // distance 307. The normal is 102, the same as our workplane.
    slv.AddCircle(402, g, 200, 306, 102, 307);

    // The length of our line segment is 30.0 units.
    slv.AddConstraint(1, g, Solver.SLVS_C_PT_PT_DISTANCE, 200, 30.0, 301, 302, 0, 0);

    // And the distance from our line segment to the origin is 10.0 units.
    slv.AddConstraint(2, g, Solver.SLVS_C_PT_LINE_DISTANCE, 200, 10.0, 101, 0, 400, 0);

    // And the line segment is vertical.
    slv.AddConstraint(3, g, Solver.SLVS_C_VERTICAL, 200, 0.0, 0, 0, 400, 0);

    // And the distance from one endpoint to the origin is 15.0 units.
    slv.AddConstraint(4, g, Solver.SLVS_C_PT_PT_DISTANCE, 200, 15.0, 301, 101, 0, 0);

    // And same for the other endpoint; so if you add this constraint then
    // the sketch is overconstrained and will signal an error.
    // slv.AddConstraint(5, g, Solver.SLVS_C_PT_PT_DISTANCE, _
    // 200, 18.0, 302, 101, 0, 0)

    // The arc and the circle have equal radius.
    slv.AddConstraint(6, g, Solver.SLVS_C_EQUAL_RADIUS, 200, 0.0, 0, 0, 401, 402);

    // The arc has radius 17.0 units.
    slv.AddConstraint(7, g, Solver.SLVS_C_DIAMETER, 200, 2 * 17.0, 0, 0, 401, 0);

    // If the solver fails, then ask it to report which constraints caused
    // the problem.
    slv.Solve(g, 0, 0, 0, 0, true);

    if ((slv.GetResult() == Solver.SLVS_RESULT_OKAY))
    {
      Console.WriteLine("solved okay");
      // Note that we are referring to the parameters by their handles,
      // and not by their index in the list. This is a difference from
      // the C example.
      Console.WriteLine($"line from ({slv.GetParamByHandle(11):F3} {slv.GetParamByHandle(12):F3}) to ({slv.GetParamByHandle(13):F3} {slv.GetParamByHandle(14):F3})");
      Console.WriteLine("arc center ({0:F3} {1:F3}) start ({2:F3} {3:F3}) " + "finish ({4:F3} {5:F3})", slv.GetParamByHandle(15), slv.GetParamByHandle(16), slv.GetParamByHandle(17), slv.GetParamByHandle(18), slv.GetParamByHandle(19), slv.GetParamByHandle(20));
      Console.WriteLine($"circle center ({slv.GetParamByHandle(21):F3} {slv.GetParamByHandle(22):F3}) radius {slv.GetParamByHandle(23):F3}");

      Console.WriteLine(slv.GetDof() + " DOF");
    }
    else
    {
      Console.Write("solve failed; problematic constraints are:");
      foreach (var t in slv.GetFaileds())
      {
        Console.Write(" " + t);
      }

      Console.WriteLine("");
      if ((slv.GetResult() == Solver.SLVS_RESULT_INCONSISTENT))
      {
        Console.WriteLine("system inconsistent");
      }
      else
      {
        Console.WriteLine("system nonconvergent");
      }
    }
  }
}
