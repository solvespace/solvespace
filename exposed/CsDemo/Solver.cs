namespace SolveSpace;

using System.Runtime.InteropServices;

// This is a thin wrapper around those functions, which provides
// convenience functions similar to those provided in slvs.h for the C API.
public class Solver
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
    public Solver Solver;
    public uint H;

    public Param(Solver s, uint group, double val)
    {
      Solver = s;
      H = (uint)(Solver.Params.Count + 1);
      Solver.AddParam(H, group, val);
    }
  }

  public abstract class Entity
  {
    public Solver Solver;
    public uint H;

    public Entity(Solver s)
    {
      Solver = s;
      H = (uint)(Solver.Entities.Count + 1);
    }
  }

  public abstract class Point : Entity
  {
    public Point(Solver s) : base(s)
    {
    }
  }

  public abstract class Normal : Entity
  {
    public Normal(Solver s) : base(s)
    {
    }
  }

  public class Point2d : Point
  {
    public Param up, vp;

    public Point2d(Solver s, uint group, Workplane wrkpl, double u, double v) : base(s)
    {
      up = new Param(Solver, group, u);
      vp = new Param(Solver, group, v);
      Solver.AddPoint2d(H, group, wrkpl.H, up.H, vp.H);
    }

    public Point2d(Solver s, uint group, Workplane wrkpl, Param u, Param v) : base(s)
    {
      Solver.AddPoint2d(H, group, wrkpl.H, u.H, v.H);
      up = u;
      vp = v;
    }

    public double GetU()
    {
      return Solver.GetParamByHandle(up.H);
    }

    public double GetV()
    {
      return Solver.GetParamByHandle(vp.H);
    }
  }

  public class Point3d : Point
  {
    public Param xp, yp, zp;

    public Point3d(Solver s, uint group, double x, double y, double z) : base(s)
    {
      xp = new Param(Solver, group, x);
      yp = new Param(Solver, group, y);
      zp = new Param(Solver, group, z);
      Solver.AddPoint3d(H, group, xp.H, yp.H, zp.H);
    }

    public Point3d(Solver s, uint group, Param x, Param y, Param z) : base(s)
    {
      Solver.AddPoint3d(H, group, x.H, y.H, z.H);
      xp = x;
      yp = y;
      zp = z;
    }

    public double GetX()
    {
      return Solver.GetParamByHandle(xp.H);
    }

    public double GetY()
    {
      return Solver.GetParamByHandle(yp.H);
    }

    public double GetZ()
    {
      return Solver.GetParamByHandle(zp.H);
    }
  }

  public class Normal3d : Normal
  {
    private Param qwp, qxp, qyp, qzp;

    public Normal3d(Solver s, uint group, double ux, double uy, double uz, double vx, double vy, double vz) : base(s)
    {
      double qw = 0, qx = 0, qy = 0, qz = 0;
      Solver.MakeQuaternion(ux, uy, uz, vx, vy, vz, ref qw, ref qx, ref qy, ref qz);
      qwp = new Param(Solver, group, qw);
      qxp = new Param(Solver, group, qx);
      qyp = new Param(Solver, group, qy);
      qzp = new Param(Solver, group, qz);
      Solver.AddNormal3d(H, group, qwp.H, qxp.H, qyp.H, qzp.H);
    }

    public Normal3d(Solver s, uint group, Param qw, Param qx, Param qy, Param qz) : base(s)
    {
      Solver.AddNormal3d(H, group, qw.H, qx.H, qy.H, qz.H);
      qwp = qw;
      qxp = qx;
      qyp = qy;
      qzp = qz;
    }
  }

  public class Normal2d : Normal
  {
    public Normal2d(Solver s, uint group, Workplane wrkpl) : base(s)
    {
      Solver.AddNormal2d(H, group, wrkpl.H);
    }
  }

  public class Distance : Entity
  {
    public Param dp;

    public Distance(Solver s, uint group, Workplane wrkpl, double d) : base(s)
    {
      dp = new Param(Solver, group, d);
      Solver.AddDistance(H, group, wrkpl.H, dp.H);
    }

    public Distance(Solver s, uint group, Workplane wrkpl, Param d) : base(s)
    {
      Solver.AddDistance(H, group, wrkpl.H, d.H);
      dp = d;
    }

    public double GetDistance()
    {
      return Solver.GetParamByHandle(dp.H);
    }
  }

  public class LineSegment : Entity
  {
    public LineSegment(Solver s, uint group, Workplane wrkpl, Point ptA, Point ptB) : base(s)
    {
      Solver.AddLineSegment(H, group, wrkpl.H, ptA.H, ptB.H);
    }
  }

  public class Cubic : Entity
  {
    public Cubic(Solver s, uint group, Workplane wrkpl, Point pt0, Point pt1, Point pt2, Point pt3) : base(s)
    {
      Solver.AddCubic(H, group, wrkpl.H, pt0.H, pt1.H, pt2.H, pt3.H);
    }
  }

  public class ArcOfCircle : Entity
  {
    public ArcOfCircle(Solver s, uint group, Workplane wrkpl, Normal normal, Point center, Point pstart, Point pend) : base(s)
    {
      Solver.AddArcOfCircle(H, group, wrkpl.H, normal.H, center.H, pstart.H, pend.H);
    }
  }

  public class Circle : Entity
  {
    public Circle(Solver s, uint group, Workplane wrkpl, Point center, Normal normal, Distance radius) : base(s)
    {
      Solver.AddCircle(H, group, wrkpl.H, center.H, normal.H, radius.H);
    }
  }

  public class Workplane : Entity
  {
    public Workplane(Solver s) : base(s)
    {
      H = SLVS_FREE_IN_3D;
    }

    public Workplane(Solver s, uint group, Point origin, Normal normal) : base(s)
    {
      Solver.AddWorkplane(H, group, origin.H, normal.H);
    }
  }
}
