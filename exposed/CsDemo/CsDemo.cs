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

using System.Runtime.InteropServices;

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
  // as .net objects. So we create an Slvs object, which will contain
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
    var slv = new Slvs();

    // This will contain a single group, which will arbitrarily number 1.
    uint g = 1;

    // A point, initially at (x y z) = (10 10 10)
    var p1 = slv.NewPoint3d(g, 10.0, 10.0, 10.0);
    // and a second point at (20 20 20)
    var p2 = slv.NewPoint3d(g, 20.0, 20.0, 20.0);

    // and a line segment connecting them.
    var ln = slv.NewLineSegment(g, slv.FreeIn3d(), p1, p2);

    // The distance between the points should be 30.0 units.
    slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, slv.FreeIn3d(), 30.0, p1, p2, null, null);

    // Let's tell the solver to keep the second point as close to constant
    // as possible, instead moving the first point.
    slv.Solve(g, p2, true);

    if ((slv.GetResult() == Slvs.SLVS_RESULT_OKAY))
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
    var slv = new Slvs();

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
    slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, wrkpl, 30.0, pl1, pl2, null, null);

    // And the distance from our line segment to the origin is 10.0 units.
    slv.AddConstraint(2, g, Slvs.SLVS_C_PT_LINE_DISTANCE, wrkpl, 10.0, origin, null, ln, null);

    // And the line segment is vertical.
    slv.AddConstraint(3, g, Slvs.SLVS_C_VERTICAL, wrkpl, 0.0, null, null, ln, null);

    // And the distance from one endpoint to the origin is 15.0 units.
    slv.AddConstraint(4, g, Slvs.SLVS_C_PT_PT_DISTANCE, wrkpl, 15.0, pl1, origin, null, null);

    // And same for the other endpoint; so if you add this constraint then
    // the sketch is overconstrained and will signal an error.
    // slv.AddConstraint(5, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
    // wrkpl, 18.0, pl2, origin, Nothing, Nothing)

    // The arc and the circle have equal radius.
    slv.AddConstraint(6, g, Slvs.SLVS_C_EQUAL_RADIUS, wrkpl, 0.0, null, null, arc, circle);

    // The arc has radius 17.0 units.
    slv.AddConstraint(7, g, Slvs.SLVS_C_DIAMETER, wrkpl, 2 * 17.0, null, null, arc, null);

    // If the solver fails, then ask it to report which constraints caused
    // the problem.
    slv.Solve(g, true);

    if (slv.GetResult() == Slvs.SLVS_RESULT_OKAY)
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
      if ((slv.GetResult() == Slvs.SLVS_RESULT_INCONSISTENT))
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
    var slv = new Slvs();

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
    slv.AddLineSegment(200, g, Slvs.SLVS_FREE_IN_3D, 101, 102);

    // The distance between the points should be 30.0 units.
    slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, Slvs.SLVS_FREE_IN_3D, 30.0, 101, 102, 0, 0);

    // Let's tell the solver to keep the second point as close to constant
    // as possible, instead moving the first point. That's parameters
    // 4, 5, and 6.
    slv.Solve(g, 4, 5, 6, 0, true);

    if ((slv.GetResult() == Slvs.SLVS_RESULT_OKAY))
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

    var slv = new Slvs();

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
    slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, 200, 30.0, 301, 302, 0, 0);

    // And the distance from our line segment to the origin is 10.0 units.
    slv.AddConstraint(2, g, Slvs.SLVS_C_PT_LINE_DISTANCE, 200, 10.0, 101, 0, 400, 0);

    // And the line segment is vertical.
    slv.AddConstraint(3, g, Slvs.SLVS_C_VERTICAL, 200, 0.0, 0, 0, 400, 0);

    // And the distance from one endpoint to the origin is 15.0 units.
    slv.AddConstraint(4, g, Slvs.SLVS_C_PT_PT_DISTANCE, 200, 15.0, 301, 101, 0, 0);

    // And same for the other endpoint; so if you add this constraint then
    // the sketch is overconstrained and will signal an error.
    // slv.AddConstraint(5, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
    // 200, 18.0, 302, 101, 0, 0)

    // The arc and the circle have equal radius.
    slv.AddConstraint(6, g, Slvs.SLVS_C_EQUAL_RADIUS, 200, 0.0, 0, 0, 401, 402);

    // The arc has radius 17.0 units.
    slv.AddConstraint(7, g, Slvs.SLVS_C_DIAMETER, 200, 2 * 17.0, 0, 0, 401, 0);

    // If the solver fails, then ask it to report which constraints caused
    // the problem.
    slv.Solve(g, 0, 0, 0, 0, true);

    if ((slv.GetResult() == Slvs.SLVS_RESULT_OKAY))
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
      if ((slv.GetResult() == Slvs.SLVS_RESULT_INCONSISTENT))
      {
        Console.WriteLine("system inconsistent");
      }
      else
      {
        Console.WriteLine("system nonconvergent");
      }
    }
  }

  // This is a thin wrapper around those functions, which provides
  // convenience functions similar to those provided in slvs.h for the C API.
  public class Slvs
  {
    // The interface to the library, and the wrapper functions around
    // that interface, follow.

    // These are the core functions imported from the DLL
    [DllImport("slvs.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Slvs_Solve(IntPtr sys, uint hg);

    [DllImport("slvs.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Slvs_MakeQuaternion(double ux, double uy, double uz, double vx, double vy, double vz, ref double qw, ref double qx, ref double qy, ref double qz);


    [StructLayout(LayoutKind.Sequential)]
    public struct Slvs_Param
    {
      public uint h;
      public uint group;
      public double val;
    }

    public const int SLVS_FREE_IN_3D = 0;

    #region Entities

    public const int SLVS_E_POINT_IN_3D = 50000;
    public const int SLVS_E_POINT_IN_2D = 50001;
    public const int SLVS_E_NORMAL_IN_3D = 60000;
    public const int SLVS_E_NORMAL_IN_2D = 60001;
    public const int SLVS_E_DISTANCE = 70000;
    public const int SLVS_E_WORKPLANE = 80000;
    public const int SLVS_E_LINE_SEGMENT = 80001;
    public const int SLVS_E_CUBIC = 80002;
    public const int SLVS_E_CIRCLE = 80003;
    public const int SLVS_E_ARC_OF_CIRCLE = 80004;

    #endregion

    [StructLayout(LayoutKind.Sequential)]
    public struct Slvs_Entity
    {
      public uint h;
      public uint group;

      public int type;

      public uint wrkpl;
      public uint point0;
      public uint point1;
      public uint point2;
      public uint point3;
      public uint normal;
      public uint distance;

      public uint param0;
      public uint param1;
      public uint param2;
      public uint param3;
    }

    #region Constraints

    public const int SLVS_C_POINTS_COINCIDENT = 100000;
    public const int SLVS_C_PT_PT_DISTANCE = 100001;
    public const int SLVS_C_PT_PLANE_DISTANCE = 100002;
    public const int SLVS_C_PT_LINE_DISTANCE = 100003;
    public const int SLVS_C_PT_FACE_DISTANCE = 100004;
    public const int SLVS_C_PT_IN_PLANE = 100005;
    public const int SLVS_C_PT_ON_LINE = 100006;
    public const int SLVS_C_PT_ON_FACE = 100007;
    public const int SLVS_C_EQUAL_LENGTH_LINES = 100008;
    public const int SLVS_C_LENGTH_RATIO = 100009;
    public const int SLVS_C_EQ_LEN_PT_LINE_D = 100010;
    public const int SLVS_C_EQ_PT_LN_DISTANCES = 100011;
    public const int SLVS_C_EQUAL_ANGLE = 100012;
    public const int SLVS_C_EQUAL_LINE_ARC_LEN = 100013;
    public const int SLVS_C_SYMMETRIC = 100014;
    public const int SLVS_C_SYMMETRIC_HORIZ = 100015;
    public const int SLVS_C_SYMMETRIC_VERT = 100016;
    public const int SLVS_C_SYMMETRIC_LINE = 100017;
    public const int SLVS_C_AT_MIDPOINT = 100018;
    public const int SLVS_C_HORIZONTAL = 100019;
    public const int SLVS_C_VERTICAL = 100020;
    public const int SLVS_C_DIAMETER = 100021;
    public const int SLVS_C_PT_ON_CIRCLE = 100022;
    public const int SLVS_C_SAME_ORIENTATION = 100023;
    public const int SLVS_C_ANGLE = 100024;
    public const int SLVS_C_PARALLEL = 100025;
    public const int SLVS_C_PERPENDICULAR = 100026;
    public const int SLVS_C_ARC_LINE_TANGENT = 100027;
    public const int SLVS_C_CUBIC_LINE_TANGENT = 100028;
    public const int SLVS_C_EQUAL_RADIUS = 100029;
    public const int SLVS_C_PROJ_PT_DISTANCE = 100030;
    public const int SLVS_C_WHERE_DRAGGED = 100031;
    public const int SLVS_C_CURVE_CURVE_TANGENT = 100032;
    public const int SLVS_C_LENGTH_DIFFERENCE = 100033;

    #endregion

    [StructLayout(LayoutKind.Sequential)]
    public struct Slvs_Constraint
    {
      public uint h;
      public uint group;

      public int type;

      public uint wrkpl;

      public double valA;
      public uint ptA;
      public uint ptB;
      public uint entityA;
      public uint entityB;
      public uint entityC;
      public uint entityD;

      public int other;
      public int other2;
    }

    #region Results

    public const int SLVS_RESULT_OKAY = 0;
    public const int SLVS_RESULT_INCONSISTENT = 1;
    public const int SLVS_RESULT_DIDNT_CONVERGE = 2;
    public const int SLVS_RESULT_TOO_MANY_UNKNOWNS = 3;

    #endregion

    [StructLayout(LayoutKind.Sequential)]
    public struct Slvs_System
    {
      public IntPtr param;
      public int @params;
      public IntPtr entity;
      public int entities;
      public IntPtr constraint;
      public int constraints;

      public uint dragged0;
      public uint dragged1;
      public uint dragged2;
      public uint dragged3;

      public int calculatedFaileds;

      public IntPtr failed;
      public int faileds;

      public int dof;

      public int result;
    }

    private readonly List<Slvs_Param> Params = new();
    private readonly List<Slvs_Entity> Entities = new();
    private readonly List<Slvs_Constraint> Constraints = new();

    private readonly List<uint> Faileds = new();

    private int Result;
    private int Dof;

    // Return the value of a parameter, by its handle. This function
    // may be used, for example, to obtain the new values of the
    // parameters after a call to Solve().
    public double GetParamByHandle(uint h)
    {
      foreach (var t in Params)
      {
        if ((t.h == h))
        {
          return t.val;
        }
      }

      throw new Exception("Invalid parameter handle.");
    }

    // Return the value of a parameter, by its index in the list (where
    // that index is determined by the order in which the parameters
    // were inserted with AddParam(), not by its handle).
    public double GetParamByIndex(int i)
    {
      return Params[i].val;
    }

    // Get the result after a call to Solve(). This may be
    // SLVS_RESULT_OKAY              - it worked
    // SLVS_RESULT_INCONSISTENT      - failed, inconsistent
    // SLVS_RESULT_DIDNT_CONVERGE    - consistent, but still failed
    // SLVS_RESULT_TOO_MANY_UNKNOWNS - too many parameters in one group
    public int GetResult()
    {
      return Result;
    }

    // After a call to Solve(), this returns the number of unconstrained 
    // degrees of freedom for the sketch.
    public int GetDof()
    {
      return Dof;
    }

    // After a failing call to Solve(), this returns the list of
    // constraints, identified by their handle, that would fix the
    // system if they were deleted. This list will be populated only
    // if calculateFaileds is True in the Solve() call.
    public List<uint> GetFaileds()
    {
      return Faileds;
    }

    // Clear our lists of entities, constraints, and parameters.
    public void ResetAll()
    {
      Params.Clear();
      Entities.Clear();
      Constraints.Clear();
    }


    // These functions are broadly similar to the Slvs_Make...
    // functions in slvs.h. See the file DOC.txt accompanying the
    // library for details.

    public void AddParam(uint h, uint group, double val)
    {
      Slvs_Param p = new()
      {
        h = h,
        group = group,
        val = val
      };
      Params.Add(p);
    }

    public void AddPoint2d(uint h, uint group, uint wrkpl, uint u, uint v)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_POINT_IN_2D,
        wrkpl = wrkpl,
        param0 = u,
        param1 = v
      };
      Entities.Add(e);
    }

    public void AddPoint3d(uint h, uint group, uint x, uint y, uint z)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_POINT_IN_3D,
        wrkpl = SLVS_FREE_IN_3D,
        param0 = x,
        param1 = y,
        param2 = z
      };
      Entities.Add(e);
    }

    public void AddNormal3d(uint h, uint group, uint qw, uint qx, uint qy, uint qz)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_NORMAL_IN_3D,
        wrkpl = SLVS_FREE_IN_3D,
        param0 = qw,
        param1 = qx,
        param2 = qy,
        param3 = qz
      };
      Entities.Add(e);
    }

    public void AddNormal2d(uint h, uint group, uint wrkpl)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_NORMAL_IN_2D,
        wrkpl = wrkpl
      };
      Entities.Add(e);
    }

    public void AddDistance(uint h, uint group, uint wrkpl, uint d)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_DISTANCE,
        wrkpl = wrkpl,
        param0 = d
      };
      Entities.Add(e);
    }

    public void AddLineSegment(uint h, uint group, uint wrkpl, uint ptA, uint ptB)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_LINE_SEGMENT,
        wrkpl = wrkpl,
        point0 = ptA,
        point1 = ptB
      };
      Entities.Add(e);
    }

    public void AddCubic(uint h, uint group, uint wrkpl, uint pt0, uint pt1, uint pt2, uint pt3)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_CUBIC,
        wrkpl = wrkpl,
        point0 = pt0,
        point1 = pt1,
        point2 = pt2,
        point3 = pt3
      };
      Entities.Add(e);
    }

    public void AddArcOfCircle(uint h, uint group, uint wrkpl, uint normal, uint center, uint pstart, uint pend)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_ARC_OF_CIRCLE,
        wrkpl = wrkpl,
        normal = normal,
        point0 = center,
        point1 = pstart,
        point2 = pend
      };
      Entities.Add(e);
    }

    public void AddCircle(uint h, uint group, uint wrkpl, uint center, uint normal, uint radius)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_CIRCLE,
        wrkpl = wrkpl,
        point0 = center,
        normal = normal,
        distance = radius
      };
      Entities.Add(e);
    }

    public void AddWorkplane(uint h, uint group, uint origin, uint normal)
    {
      Slvs_Entity e = new()
      {
        h = h,
        group = group,
        type = SLVS_E_WORKPLANE,
        wrkpl = SLVS_FREE_IN_3D,
        point0 = origin,
        normal = normal
      };
      Entities.Add(e);
    }

    public void AddConstraint(uint h, uint group, int type, uint wrkpl, double valA, uint ptA, uint ptB, uint entityA, uint entityB)
    {
      Slvs_Constraint c = new()
      {
        h = h,
        group = group,
        type = type,
        wrkpl = wrkpl,
        valA = valA,
        ptA = ptA,
        ptB = ptB,
        entityA = entityA,
        entityB = entityB
      };
      Constraints.Add(c);
    }

    // Solve the system. The geometry of the system must already have
    // been specified through the Add...() calls. The result of the
    // solution process may be obtained by calling GetResult(),
    // GetFaileds(), GetDof(), and GetParamByXXX().
    // 
    // The parameters draggedx (indicated by their handles) will be held
    // as close as possible to their original positions, even if this
    // results in large moves for other parameters. This feature may be
    // useful if, for example, the user is dragging the point whose
    // location is defined by those parameters. Unused draggedx
    // parameters may be specified as zero.
    public void Solve(uint group, uint dragged0, uint dragged1, uint dragged2, uint dragged3, bool calculateFaileds)
    {
      var p = new Slvs_Param[Params.Count + 1];
      var i = 0;
      foreach (var pp in Params)
      {
        p[i] = pp;
        i += 1;
      }

      var e = new Slvs_Entity[Entities.Count + 1];
      i = 0;
      foreach (var ee in Entities)
      {
        e[i] = ee;
        i += 1;
      }

      var c = new Slvs_Constraint[Constraints.Count + 1];
      i = 0;
      foreach (var cc in Constraints)
      {
        c[i] = cc;
        i += 1;
      }

      var f = new uint[Constraints.Count + 1];

      Slvs_System sys = new();

      var pgc = GCHandle.Alloc(p, GCHandleType.Pinned);
      sys.param = pgc.AddrOfPinnedObject();
      sys.@params = Params.Count;
      var egc = GCHandle.Alloc(e, GCHandleType.Pinned);
      sys.entity = egc.AddrOfPinnedObject();
      sys.entities = Entities.Count;
      var cgc = GCHandle.Alloc(c, GCHandleType.Pinned);
      sys.constraint = cgc.AddrOfPinnedObject();
      sys.constraints = Constraints.Count;

      sys.dragged0 = dragged0;
      sys.dragged1 = dragged1;
      sys.dragged2 = dragged2;
      sys.dragged3 = dragged3;

      var fgc = GCHandle.Alloc(f, GCHandleType.Pinned);
      sys.calculatedFaileds = calculateFaileds ? 1 : 0;
      sys.faileds = Constraints.Count;
      sys.failed = fgc.AddrOfPinnedObject();

      var sysgc = GCHandle.Alloc(sys, GCHandleType.Pinned);

      Slvs_Solve(sysgc.AddrOfPinnedObject(), group);

      sys = (Slvs_System)sysgc.Target;

      for (i = 0; i <= Params.Count - 1; i++)
      {
        Params[i] = p[i];
      }

      Faileds.Clear();
      for (i = 0; i <= sys.faileds - 1; i++)
      {
        Faileds.Add(f[i]);
      }

      sysgc.Free();
      fgc.Free();
      pgc.Free();
      egc.Free();
      cgc.Free();

      Result = sys.result;
      Dof = sys.dof;
    }

    // A simpler version of the function, if the parameters being dragged
    // correspond to a single point.
    public void Solve(uint group, Point dragged, bool calculatedFaileds)
    {
      switch (dragged)
      {
        case Point2d point2d:
          Solve(group, point2d.up.H, point2d.vp.H, 0, 0, calculatedFaileds);
          break;
        case Point3d point3d:
          Solve(group, point3d.xp.H, point3d.yp.H, point3d.zp.H, 0, calculatedFaileds);
          break;
        default:
          throw new Exception("Can't get dragged params for point.");
      }
    }

    // or if it's a single distance (e.g., the radius of a circle)
    public void Solve(uint group, Distance dragged, bool calculatedFaileds)
    {
      Solve(group, dragged.dp.H, 0, 0, 0, calculatedFaileds);
    }

    // or if it's nothing.
    public void Solve(uint group, bool calculateFaileds)
    {
      Solve(group, 0, 0, 0, 0, calculateFaileds);
    }

    // Return the quaternion in (qw, qx, qy, qz) that represents a
    // rotation from the base frame to a coordinate system with the
    // specified basis vectors u and v. For example, u = (0, 1, 0)
    // and v = (0, 0, 1) specifies the yz plane, such that a point with
    // (u, v) = (7, 12) has (x, y, z) = (0, 7, 12).
    public void MakeQuaternion(double ux, double uy, double uz, double vx, double vy, double vz, ref double qw, ref double qx, ref double qy, ref double qz)
    {
      Slvs_MakeQuaternion(ux, uy, uz, vx, vy, vz, ref qw, ref qx, ref qy, ref qz);
    }

    public Workplane FreeIn3d()
    {
      return new Workplane(this);
    }

    // Functions to create the object-oriented wrappers defined below.

    public Param NewParam(uint group, double val)
    {
      return new Param(this, group, val);
    }

    public Point2d NewPoint2d(uint group, Workplane wrkpl, double u, double v)
    {
      return new Point2d(this, group, wrkpl, u, v);
    }

    public Point2d NewPoint2d(uint group, Workplane wrkpl, Param u, Param v)
    {
      return new Point2d(this, group, wrkpl, u, v);
    }

    public Point3d NewPoint3d(uint group, double x, double y, double z)
    {
      return new Point3d(this, group, x, y, z);
    }

    public Point3d NewPoint3d(uint group, Param x, Param y, Param z)
    {
      return new Point3d(this, group, x, y, z);
    }

    public Normal3d NewNormal3d(uint group, double ux, double uy, double uz, double vx, double vy, double vz)
    {
      return new Normal3d(this, group, ux, uy, uz, vx, vy, vz);
    }

    public Normal3d NewNormal3d(uint group, Param qw, Param qx, Param qy, Param qz)
    {
      return new Normal3d(this, group, qw, qx, qy, qz);
    }

    public Normal2d NewNormal2d(uint group, Workplane wrkpl)
    {
      return new Normal2d(this, group, wrkpl);
    }

    public Distance NewDistance(uint group, Workplane wrkpl, double d)
    {
      return new Distance(this, group, wrkpl, d);
    }

    public Distance NewDistance(uint group, Workplane wrkpl, Param d)
    {
      return new Distance(this, group, wrkpl, d);
    }

    public LineSegment NewLineSegment(uint group, Workplane wrkpl, Point ptA, Point ptB)
    {
      return new LineSegment(this, group, wrkpl, ptA, ptB);
    }

    public ArcOfCircle NewArcOfCircle(uint group, Workplane wrkpl, Normal normal, Point center, Point pstart, Point pend)
    {
      return new ArcOfCircle(this, group, wrkpl, normal, center, pstart, pend);
    }

    public Circle NewCircle(uint group, Workplane wrkpl, Point center, Normal normal, Distance radius)
    {
      return new Circle(this, group, wrkpl, center, normal, radius);
    }

    public Workplane NewWorkplane(uint group, Point origin, Normal normal)
    {
      return new Workplane(this, group, origin, normal);
    }

    public void AddConstraint(uint h, uint group, int type, Workplane wrkpl, double valA, Point ptA, Point ptB, Entity entityA, Entity entityB)
    {
      AddConstraint(h, group, type, wrkpl is null ? 0 : wrkpl.H, valA, ptA is null ? 0 : ptA.H, ptB is null ? 0 : ptB.H, entityA is null ? 0 : entityA.H, entityB is null ? 0 : entityB.H);
    }


    // The object-oriented wrapper classes themselves, to allow the
    // representation of entities and constraints as .net objects, not
    // integer handles. These don't do any work themselves, beyond
    // allocating and storing a unique integer handle.
    // 
    // These functions will assign parameters and entities with
    // consecutive handles starting from 1. If they are intermixed
    // with parameters and entities with user-specified handles, then
    // those handles must be chosen not to conflict, e.g. starting
    // from 100 000 or another large number.

    public class Param
    {
      public Slvs Slv;
      public uint H;

      public Param(Slvs s, uint group, double val)
      {
        Slv = s;
        H = (uint)(Slv.Params.Count + 1);
        Slv.AddParam(H, group, val);
      }
    }

    public abstract class Entity
    {
      public Slvs Slv;
      public uint H;

      public Entity(Slvs s)
      {
        Slv = s;
        H = (uint)(Slv.Entities.Count + 1);
      }
    }

    public abstract class Point : Entity
    {
      public Point(Slvs s) : base(s)
      {
      }
    }

    public abstract class Normal : Entity
    {
      public Normal(Slvs s) : base(s)
      {
      }
    }

    public class Point2d : Point
    {
      public Param up, vp;

      public Point2d(Slvs s, uint group, Workplane wrkpl, double u, double v) : base(s)
      {
        up = new Param(Slv, group, u);
        vp = new Param(Slv, group, v);
        Slv.AddPoint2d(H, group, wrkpl.H, up.H, vp.H);
      }

      public Point2d(Slvs s, uint group, Workplane wrkpl, Param u, Param v) : base(s)
      {
        Slv.AddPoint2d(H, group, wrkpl.H, u.H, v.H);
        up = u;
        vp = v;
      }

      public double GetU()
      {
        return Slv.GetParamByHandle(up.H);
      }

      public double GetV()
      {
        return Slv.GetParamByHandle(vp.H);
      }
    }

    public class Point3d : Point
    {
      public Param xp, yp, zp;

      public Point3d(Slvs s, uint group, double x, double y, double z) : base(s)
      {
        xp = new Param(Slv, group, x);
        yp = new Param(Slv, group, y);
        zp = new Param(Slv, group, z);
        Slv.AddPoint3d(H, group, xp.H, yp.H, zp.H);
      }

      public Point3d(Slvs s, uint group, Param x, Param y, Param z) : base(s)
      {
        Slv.AddPoint3d(H, group, x.H, y.H, z.H);
        xp = x;
        yp = y;
        zp = z;
      }

      public double GetX()
      {
        return Slv.GetParamByHandle(xp.H);
      }

      public double GetY()
      {
        return Slv.GetParamByHandle(yp.H);
      }

      public double GetZ()
      {
        return Slv.GetParamByHandle(zp.H);
      }
    }

    public class Normal3d : Normal
    {
      private Param qwp, qxp, qyp, qzp;

      public Normal3d(Slvs s, uint group, double ux, double uy, double uz, double vx, double vy, double vz) : base(s)
      {
        double qw = 0, qx = 0, qy = 0, qz = 0;
        Slv.MakeQuaternion(ux, uy, uz, vx, vy, vz, ref qw, ref qx, ref qy, ref qz);
        qwp = new Param(Slv, group, qw);
        qxp = new Param(Slv, group, qx);
        qyp = new Param(Slv, group, qy);
        qzp = new Param(Slv, group, qz);
        Slv.AddNormal3d(H, group, qwp.H, qxp.H, qyp.H, qzp.H);
      }

      public Normal3d(Slvs s, uint group, Param qw, Param qx, Param qy, Param qz) : base(s)
      {
        Slv.AddNormal3d(H, group, qw.H, qx.H, qy.H, qz.H);
        qwp = qw;
        qxp = qx;
        qyp = qy;
        qzp = qz;
      }
    }

    public class Normal2d : Normal
    {
      public Normal2d(Slvs s, uint group, Workplane wrkpl) : base(s)
      {
        Slv.AddNormal2d(H, group, wrkpl.H);
      }
    }

    public class Distance : Entity
    {
      public Param dp;

      public Distance(Slvs s, uint group, Workplane wrkpl, double d) : base(s)
      {
        dp = new Param(Slv, group, d);
        Slv.AddDistance(H, group, wrkpl.H, dp.H);
      }

      public Distance(Slvs s, uint group, Workplane wrkpl, Param d) : base(s)
      {
        Slv.AddDistance(H, group, wrkpl.H, d.H);
        dp = d;
      }

      public double GetDistance()
      {
        return Slv.GetParamByHandle(dp.H);
      }
    }

    public class LineSegment : Entity
    {
      public LineSegment(Slvs s, uint group, Workplane wrkpl, Point ptA, Point ptB) : base(s)
      {
        Slv.AddLineSegment(H, group, wrkpl.H, ptA.H, ptB.H);
      }
    }

    public class Cubic : Entity
    {
      public Cubic(Slvs s, uint group, Workplane wrkpl, Point pt0, Point pt1, Point pt2, Point pt3) : base(s)
      {
        Slv.AddCubic(H, group, wrkpl.H, pt0.H, pt1.H, pt2.H, pt3.H);
      }
    }

    public class ArcOfCircle : Entity
    {
      public ArcOfCircle(Slvs s, uint group, Workplane wrkpl, Normal normal, Point center, Point pstart, Point pend) : base(s)
      {
        Slv.AddArcOfCircle(H, group, wrkpl.H, normal.H, center.H, pstart.H, pend.H);
      }
    }

    public class Circle : Entity
    {
      public Circle(Slvs s, uint group, Workplane wrkpl, Point center, Normal normal, Distance radius) : base(s)
      {
        Slv.AddCircle(H, group, wrkpl.H, center.H, normal.H, radius.H);
      }
    }

    public class Workplane : Entity
    {
      public Workplane(Slvs s) : base(s)
      {
        H = SLVS_FREE_IN_3D;
      }

      public Workplane(Slvs s, uint group, Point origin, Normal normal) : base(s)
      {
        Slv.AddWorkplane(H, group, origin.H, normal.H);
      }
    }
  }
}