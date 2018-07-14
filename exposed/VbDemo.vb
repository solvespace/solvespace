'-----------------------------------------------------------------------------
' Some sample code for slvs.dll. We draw some geometric entities, provide
' initial guesses for their positions, and then constrain them. The solver
' calculates their new positions, in order to satisfy the constraints.
'
' The library is distributed as a DLL, but the functions are designed to
' be usable from .net languages through a P/Invoke. This file contains an
' example of that process, and a wrapper class around those P/Invoke'd
' functions that you may wish to use a starting point in your own
' application.
'
' Copyright 2008-2013 Jonathan Westhues.
'-----------------------------------------------------------------------------

Imports System.Runtime.InteropServices

Module VbDemo

    ' Call our example functions, which set up some kind of sketch, solve
    ' it, and then print the result. 
    Sub Main()
        Console.WriteLine("EXAMPLE IN 3d (by objects):")
        Example3dWithObjects()
        Console.WriteLine("")

        Console.WriteLine("EXAMPLE IN 2d (by objects):")
        Example2dWithObjects()
        Console.WriteLine("")

        Console.WriteLine("EXAMPLE IN 3d (by handles):")
        Example3dWithHandles()
        Console.WriteLine("")

        Console.WriteLine("EXAMPLE IN 2d (by handles):")
        Example2dWithHandles()
        Console.WriteLine("")
    End Sub


    '''''''''''''''''''''''''''''''
    ' This is the simplest way to use the library. A set of wrapper
    ' classes allow us to represent entities (e.g., lines and points)
    ' as .net objects. So we create an Slvs object, which will contain
    ' the entire sketch, with all the entities and constraints.
    ' 
    ' We then create entity objects (for example, points and lines)
    ' associated with that sketch, indicating the initial positions of
    ' those entities and any hierarchical relationships among them (e.g.,
    ' defining a line entity in terms of endpoint entities). We also add
    ' constraints associated with those entities.
    ' 
    ' Finally, we solve, and print the new positions of the entities. If the
    ' solution succeeded, then the entities will satisfy the constraints. If
    ' not, then the solver will suggest problematic constraints that, if
    ' removed, would render the sketch solvable.

    Sub Example3dWithObjects()
        Dim g As UInteger
        Dim slv As New Slvs

        ' This will contain a single group, which will arbitrarily number 1.
        g = 1

        Dim p1, p2 As Slvs.Point3d
        ' A point, initially at (x y z) = (10 10 10)
        p1 = slv.NewPoint3d(g, 10.0, 10.0, 10.0)
        ' and a second point at (20 20 20)
        p2 = slv.NewPoint3d(g, 20.0, 20.0, 20.0)

        Dim ln As Slvs.LineSegment
        ' and a line segment connecting them.
        ln = slv.NewLineSegment(g, slv.FreeIn3d(), p1, p2)

        ' The distance between the points should be 30.0 units.
        slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
                              slv.FreeIn3d(), 30.0, p1, p2, Nothing, Nothing)

        ' Let's tell the solver to keep the second point as close to constant
        ' as possible, instead moving the first point.
        slv.Solve(g, p2, True)

        If (slv.GetResult() = Slvs.SLVS_RESULT_OKAY) Then
            ' We call the GetX(), GetY(), and GetZ() functions to see
            ' where the solver moved our points to.
            Console.WriteLine(String.Format( _
                "okay; now at ({0:F3}, {1:F3}, {2:F3})", _
                p1.GetX(), p1.GetY(), p1.GetZ()))
            Console.WriteLine(String.Format( _
                "             ({0:F3}, {1:F3}, {2:F3})", _
                p2.GetX(), p2.GetY(), p2.GetZ()))
            Console.WriteLine(slv.GetDof().ToString() + " DOF")
        Else
            Console.WriteLine("solve failed")
        End If
    End Sub

    Sub Example2dWithObjects()
        Dim g As UInteger
        Dim slv As New Slvs

        g = 1

        ' First, we create our workplane. Its origin corresponds to the origin
        ' of our base frame (x y z) = (0 0 0)
        Dim origin As Slvs.Point3d
        origin = slv.NewPoint3d(g, 0.0, 0.0, 0.0)
        ' and it is parallel to the xy plane, so it has basis vectors (1 0 0)
        ' and (0 1 0).
        Dim normal As Slvs.Normal3d
        normal = slv.NewNormal3d(g, 1.0, 0.0, 0.0, _
                                    0.0, 1.0, 0.0)

        Dim wrkpl As Slvs.Workplane
        wrkpl = slv.NewWorkplane(g, origin, normal)

        ' Now create a second group. We'll solve group 2, while leaving group 1
        ' constant; so the workplane that we've created will be locked down,
        ' and the solver can't move it.
        g = 2
        ' These points are represented by their coordinates (u v) within the
        ' workplane, so they need only two parameters each.
        Dim pl1, pl2 As Slvs.Point2d
        pl1 = slv.NewPoint2d(g, wrkpl, 10.0, 20.0)
        pl2 = slv.NewPoint2d(g, wrkpl, 20.0, 10.0)

        ' And we create a line segment with those endpoints.
        Dim ln As Slvs.LineSegment
        ln = slv.NewLineSegment(g, wrkpl, pl1, pl2)

        ' Now three more points.
        Dim pc, ps, pf As Slvs.Point2d
        pc = slv.NewPoint2d(g, wrkpl, 100.0, 120.0)
        ps = slv.NewPoint2d(g, wrkpl, 120.0, 110.0)
        pf = slv.NewPoint2d(g, wrkpl, 115.0, 115.0)

        ' And arc, centered at point pc, starting at point ps, ending at
        ' point pf.
        Dim arc As Slvs.ArcOfCircle
        arc = slv.NewArcOfCircle(g, wrkpl, normal, pc, ps, pf)

        ' Now one more point, and a distance
        Dim pcc As Slvs.Point2d
        pcc = slv.NewPoint2d(g, wrkpl, 200.0, 200.0)
        Dim r As Slvs.Distance
        r = slv.NewDistance(g, wrkpl, 30.0)

        ' And a complete circle, centered at point pcc with radius equal to
        ' distance r. The normal is the same as for our workplane.
        Dim circle As Slvs.Circle
        circle = slv.NewCircle(g, wrkpl, pcc, normal, r)

        ' The length of our line segment is 30.0 units.
        slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
            wrkpl, 30.0, pl1, pl2, Nothing, Nothing)

        ' And the distance from our line segment to the origin is 10.0 units.
        slv.AddConstraint(2, g, Slvs.SLVS_C_PT_LINE_DISTANCE, _
            wrkpl, 10.0, origin, Nothing, ln, Nothing)

        ' And the line segment is vertical.
        slv.AddConstraint(3, g, Slvs.SLVS_C_VERTICAL, _
            wrkpl, 0.0, Nothing, Nothing, ln, Nothing)

        ' And the distance from one endpoint to the origin is 15.0 units.
        slv.AddConstraint(4, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
            wrkpl, 15.0, pl1, origin, Nothing, Nothing)

        ' And same for the other endpoint; so if you add this constraint then
        ' the sketch is overconstrained and will signal an error.
        'slv.AddConstraint(5, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
        '    wrkpl, 18.0, pl2, origin, Nothing, Nothing)

        ' The arc and the circle have equal radius.
        slv.AddConstraint(6, g, Slvs.SLVS_C_EQUAL_RADIUS, _
            wrkpl, 0.0, Nothing, Nothing, arc, circle)

        ' The arc has radius 17.0 units.
        slv.AddConstraint(7, g, Slvs.SLVS_C_DIAMETER, _
            wrkpl, 2 * 17.0, Nothing, Nothing, arc, Nothing)

        ' If the solver fails, then ask it to report which constraints caused
        ' the problem.
        slv.Solve(g, True)

        If (slv.GetResult() = Slvs.SLVS_RESULT_OKAY) Then
            Console.WriteLine("solved okay")
            ' We call the GetU(), GetV(), and GetDistance() functions to
            ' see where the solver moved our points and distances to.
            Console.WriteLine(String.Format( _
                "line from ({0:F3} {1:F3}) to ({2:F3} {3:F3})", _
                pl1.GetU(), pl1.GetV(), _
                pl2.GetU(), pl2.GetV()))
            Console.WriteLine(String.Format( _
                "arc center ({0:F3} {1:F3}) start ({2:F3} {3:F3}) " + _
                    "finish ({4:F3} {5:F3})", _
                pc.GetU(), pc.GetV(), _
                ps.GetU(), ps.GetV(), _
                pf.GetU(), pf.GetV()))
            Console.WriteLine(String.Format( _
                "circle center ({0:F3} {1:F3}) radius {2:F3}", _
                pcc.GetU(), pcc.GetV(), _
                r.GetDistance()))

            Console.WriteLine(slv.GetDof().ToString() + " DOF")
        Else
            Console.Write("solve failed; problematic constraints are:")
            Dim t As UInteger
            For Each t In slv.GetFaileds()
                Console.Write(" " + t.ToString())
            Next
            Console.WriteLine("")
            If (slv.GetResult() = Slvs.SLVS_RESULT_INCONSISTENT) Then
                Console.WriteLine("system inconsistent")
            Else
                Console.WriteLine("system nonconvergent")
            End If
        End If

    End Sub


    '''''''''''''''''''''''''''''''
    ' This is a lower-level way to use the library. Internally, the library
    ' represents parameters, entities, and constraints by integer handles.
    ' Here, those handles are assigned manually, and not by the wrapper
    ' classes.

    Sub Example3dWithHandles()
        Dim g As UInteger
        Dim slv As New Slvs

        ' This will contain a single group, which will arbitrarily number 1.
        g = 1

        ' A point, initially at (x y z) = (10 10 10)
        slv.AddParam(1, g, 10.0)
        slv.AddParam(2, g, 10.0)
        slv.AddParam(3, g, 10.0)
        slv.AddPoint3d(101, g, 1, 2, 3)

        ' and a second point at (20 20 20)
        slv.AddParam(4, g, 20.0)
        slv.AddParam(5, g, 20.0)
        slv.AddParam(6, g, 20.0)
        slv.AddPoint3d(102, g, 4, 5, 6)

        ' and a line segment connecting them.
        slv.AddLineSegment(200, g, Slvs.SLVS_FREE_IN_3D, 101, 102)

        ' The distance between the points should be 30.0 units.
        slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
            Slvs.SLVS_FREE_IN_3D, 30.0, 101, 102, 0, 0)

        ' Let's tell the solver to keep the second point as close to constant
        ' as possible, instead moving the first point. That's parameters
        ' 4, 5, and 6.
        slv.Solve(g, 4, 5, 6, 0, True)

        If (slv.GetResult() = Slvs.SLVS_RESULT_OKAY) Then
            ' Note that we are referring to the parameters by their handles,
            ' and not by their index in the list. This is a difference from
            ' the C example.
            Console.WriteLine(String.Format( _
                "okay; now at ({0:F3}, {1:F3}, {2:F3})", _
                slv.GetParamByHandle(1), slv.GetParamByHandle(2), _
                slv.GetParamByHandle(3)))
            Console.WriteLine(String.Format( _
                "             ({0:F3}, {1:F3}, {2:F3})", _
                slv.GetParamByHandle(4), slv.GetParamByHandle(5), _
                slv.GetParamByHandle(6)))
            Console.WriteLine(slv.GetDof().ToString() + " DOF")
        Else
            Console.WriteLine("solve failed")
        End If
    End Sub


    Sub Example2dWithHandles()
        Dim g As UInteger
        Dim qw, qx, qy, qz As Double

        Dim slv As New Slvs

        g = 1

        ' First, we create our workplane. Its origin corresponds to the origin
        ' of our base frame (x y z) = (0 0 0)
        slv.AddParam(1, g, 0.0)
        slv.AddParam(2, g, 0.0)
        slv.AddParam(3, g, 0.0)
        slv.AddPoint3d(101, g, 1, 2, 3)
        ' and it is parallel to the xy plane, so it has basis vectors (1 0 0)
        ' and (0 1 0).
        slv.MakeQuaternion(1, 0, 0, _
                           0, 1, 0, qw, qx, qy, qz)
        slv.AddParam(4, g, qw)
        slv.AddParam(5, g, qx)
        slv.AddParam(6, g, qy)
        slv.AddParam(7, g, qz)
        slv.AddNormal3d(102, g, 4, 5, 6, 7)

        slv.AddWorkplane(200, g, 101, 102)

        ' Now create a second group. We'll solve group 2, while leaving group 1
        ' constant; so the workplane that we've created will be locked down,
        ' and the solver can't move it.
        g = 2
        ' These points are represented by their coordinates (u v) within the
        ' workplane, so they need only two parameters each.
        slv.AddParam(11, g, 10.0)
        slv.AddParam(12, g, 20.0)
        slv.AddPoint2d(301, g, 200, 11, 12)

        slv.AddParam(13, g, 20.0)
        slv.AddParam(14, g, 10.0)
        slv.AddPoint2d(302, g, 200, 13, 14)

        ' And we create a line segment with those endpoints.
        slv.AddLineSegment(400, g, 200, 301, 302)

        ' Now three more points.
        slv.AddParam(15, g, 100.0)
        slv.AddParam(16, g, 120.0)
        slv.AddPoint2d(303, g, 200, 15, 16)

        slv.AddParam(17, g, 120.0)
        slv.AddParam(18, g, 110.0)
        slv.AddPoint2d(304, g, 200, 17, 18)

        slv.AddParam(19, g, 115.0)
        slv.AddParam(20, g, 115.0)
        slv.AddPoint2d(305, g, 200, 19, 20)

        ' And arc, centered at point 303, starting at point 304, ending at
        ' point 305.
        slv.AddArcOfCircle(401, g, 200, 102, 303, 304, 305)

        ' Now one more point, and a distance
        slv.AddParam(21, g, 200.0)
        slv.AddParam(22, g, 200.0)
        slv.AddPoint2d(306, g, 200, 21, 22)

        slv.AddParam(23, g, 30.0)
        slv.AddDistance(307, g, 200, 23)

        ' And a complete circle, centered at point 306 with radius equal to
        ' distance 307. The normal is 102, the same as our workplane.
        slv.AddCircle(402, g, 200, 306, 102, 307)

        ' The length of our line segment is 30.0 units.
        slv.AddConstraint(1, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
            200, 30.0, 301, 302, 0, 0)

        ' And the distance from our line segment to the origin is 10.0 units.
        slv.AddConstraint(2, g, Slvs.SLVS_C_PT_LINE_DISTANCE, _
            200, 10.0, 101, 0, 400, 0)

        ' And the line segment is vertical.
        slv.AddConstraint(3, g, Slvs.SLVS_C_VERTICAL, _
            200, 0.0, 0, 0, 400, 0)

        ' And the distance from one endpoint to the origin is 15.0 units.
        slv.AddConstraint(4, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
            200, 15.0, 301, 101, 0, 0)

        ' And same for the other endpoint; so if you add this constraint then
        ' the sketch is overconstrained and will signal an error.
        'slv.AddConstraint(5, g, Slvs.SLVS_C_PT_PT_DISTANCE, _
        '    200, 18.0, 302, 101, 0, 0)

        ' The arc and the circle have equal radius.
        slv.AddConstraint(6, g, Slvs.SLVS_C_EQUAL_RADIUS, _
            200, 0.0, 0, 0, 401, 402)

        ' The arc has radius 17.0 units.
        slv.AddConstraint(7, g, Slvs.SLVS_C_DIAMETER, _
            200, 2 * 17.0, 0, 0, 401, 0)

        ' If the solver fails, then ask it to report which constraints caused
        ' the problem.
        slv.Solve(g, 0, 0, 0, 0, True)

        If (slv.GetResult() = Slvs.SLVS_RESULT_OKAY) Then
            Console.WriteLine("solved okay")
            ' Note that we are referring to the parameters by their handles,
            ' and not by their index in the list. This is a difference from
            ' the C example.
            Console.WriteLine(String.Format( _
                "line from ({0:F3} {1:F3}) to ({2:F3} {3:F3})", _
                slv.GetParamByHandle(11), slv.GetParamByHandle(12), _
                slv.GetParamByHandle(13), slv.GetParamByHandle(14)))
            Console.WriteLine(String.Format( _
                "arc center ({0:F3} {1:F3}) start ({2:F3} {3:F3}) " + _
                    "finish ({4:F3} {5:F3})", _
                slv.GetParamByHandle(15), slv.GetParamByHandle(16), _
                slv.GetParamByHandle(17), slv.GetParamByHandle(18), _
                slv.GetParamByHandle(19), slv.GetParamByHandle(20)))
            Console.WriteLine(String.Format( _
                "circle center ({0:F3} {1:F3}) radius {2:F3}", _
                slv.GetParamByHandle(21), slv.GetParamByHandle(22), _
                slv.GetParamByHandle(23)))

            Console.WriteLine(slv.GetDof().ToString() + " DOF")
        Else
            Console.Write("solve failed; problematic constraints are:")
            Dim t As UInteger
            For Each t In slv.GetFaileds()
                Console.Write(" " + t.ToString())
            Next
            Console.WriteLine("")
            If (slv.GetResult() = Slvs.SLVS_RESULT_INCONSISTENT) Then
                Console.WriteLine("system inconsistent")
            Else
                Console.WriteLine("system nonconvergent")
            End If
        End If

    End Sub


    '''''''''''''''''''''''''''''''
    ' The interface to the library, and the wrapper functions around
    ' that interface, follow.

    ' These are the core functions imported from the DLL
    <DllImport("slvs.dll", CallingConvention:=CallingConvention.Cdecl)> _
    Public Sub Slvs_Solve(ByVal sys As IntPtr, ByVal hg As UInteger)
    End Sub

    <DllImport("slvs.dll", CallingConvention:=CallingConvention.Cdecl)> _
    Public Sub Slvs_MakeQuaternion(
        ByVal ux As Double, ByVal uy As Double, ByVal uz As Double,
        ByVal vx As Double, ByVal vy As Double, ByVal vz As Double,
        ByRef qw As Double, ByRef qx As Double, ByRef qy As Double,
            ByRef qz As Double)
    End Sub

    ' And this is a thin wrapper around those functions, which provides
    ' convenience functions similar to those provided in slvs.h for the C API.
    Public Class Slvs

        <StructLayout(LayoutKind.Sequential)> Public Structure Slvs_Param
            Public h As UInteger
            Public group As UInteger
            Public val As Double
        End Structure

        Public Const SLVS_FREE_IN_3D As Integer = 0

        Public Const SLVS_E_POINT_IN_3D As Integer = 50000
        Public Const SLVS_E_POINT_IN_2D As Integer = 50001
        Public Const SLVS_E_NORMAL_IN_3D As Integer = 60000
        Public Const SLVS_E_NORMAL_IN_2D As Integer = 60001
        Public Const SLVS_E_DISTANCE As Integer = 70000
        Public Const SLVS_E_WORKPLANE As Integer = 80000
        Public Const SLVS_E_LINE_SEGMENT As Integer = 80001
        Public Const SLVS_E_CUBIC As Integer = 80002
        Public Const SLVS_E_CIRCLE As Integer = 80003
        Public Const SLVS_E_ARC_OF_CIRCLE As Integer = 80004

        <StructLayout(LayoutKind.Sequential)> Public Structure Slvs_Entity
            Public h As UInteger
            Public group As UInteger

            Public type As Integer

            Public wrkpl As UInteger
            Public point0 As UInteger
            Public point1 As UInteger
            Public point2 As UInteger
            Public point3 As UInteger
            Public normal As UInteger
            Public distance As UInteger

            Public param0 As UInteger
            Public param1 As UInteger
            Public param2 As UInteger
            Public param3 As UInteger
        End Structure

        Public Const SLVS_C_POINTS_COINCIDENT As Integer = 100000
        Public Const SLVS_C_PT_PT_DISTANCE As Integer = 100001
        Public Const SLVS_C_PT_PLANE_DISTANCE As Integer = 100002
        Public Const SLVS_C_PT_LINE_DISTANCE As Integer = 100003
        Public Const SLVS_C_PT_FACE_DISTANCE As Integer = 100004
        Public Const SLVS_C_PT_IN_PLANE As Integer = 100005
        Public Const SLVS_C_PT_ON_LINE As Integer = 100006
        Public Const SLVS_C_PT_ON_FACE As Integer = 100007
        Public Const SLVS_C_EQUAL_LENGTH_LINES As Integer = 100008
        Public Const SLVS_C_LENGTH_RATIO As Integer = 100009
        Public Const SLVS_C_EQ_LEN_PT_LINE_D As Integer = 100010
        Public Const SLVS_C_EQ_PT_LN_DISTANCES As Integer = 100011
        Public Const SLVS_C_EQUAL_ANGLE As Integer = 100012
        Public Const SLVS_C_EQUAL_LINE_ARC_LEN As Integer = 100013
        Public Const SLVS_C_SYMMETRIC As Integer = 100014
        Public Const SLVS_C_SYMMETRIC_HORIZ As Integer = 100015
        Public Const SLVS_C_SYMMETRIC_VERT As Integer = 100016
        Public Const SLVS_C_SYMMETRIC_LINE As Integer = 100017
        Public Const SLVS_C_AT_MIDPOINT As Integer = 100018
        Public Const SLVS_C_HORIZONTAL As Integer = 100019
        Public Const SLVS_C_VERTICAL As Integer = 100020
        Public Const SLVS_C_DIAMETER As Integer = 100021
        Public Const SLVS_C_PT_ON_CIRCLE As Integer = 100022
        Public Const SLVS_C_SAME_ORIENTATION As Integer = 100023
        Public Const SLVS_C_ANGLE As Integer = 100024
        Public Const SLVS_C_PARALLEL As Integer = 100025
        Public Const SLVS_C_PERPENDICULAR As Integer = 100026
        Public Const SLVS_C_ARC_LINE_TANGENT As Integer = 100027
        Public Const SLVS_C_CUBIC_LINE_TANGENT As Integer = 100028
        Public Const SLVS_C_EQUAL_RADIUS As Integer = 100029
        Public Const SLVS_C_PROJ_PT_DISTANCE As Integer = 100030
        Public Const SLVS_C_WHERE_DRAGGED As Integer = 100031
        Public Const SLVS_C_CURVE_CURVE_TANGENT As Integer = 100032
        Public Const SLVS_C_LENGTH_DIFFERENCE As Integer = 100033

        <StructLayout(LayoutKind.Sequential)> Public Structure Slvs_Constraint
            Public h As UInteger
            Public group As UInteger

            Public type As Integer

            Public wrkpl As UInteger

            Public valA As Double
            Public ptA As UInteger
            Public ptB As UInteger
            Public entityA As UInteger
            Public entityB As UInteger
            Public entityC As UInteger
            Public entityD As UInteger

            Public other As Integer
            Public other2 As Integer
        End Structure

        Public Const SLVS_RESULT_OKAY As Integer = 0
        Public Const SLVS_RESULT_INCONSISTENT As Integer = 1
        Public Const SLVS_RESULT_DIDNT_CONVERGE As Integer = 2
        Public Const SLVS_RESULT_TOO_MANY_UNKNOWNS As Integer = 3

        <StructLayout(LayoutKind.Sequential)> Public Structure Slvs_System
            Public param As IntPtr
            Public params As Integer
            Public entity As IntPtr
            Public entities As Integer
            Public constraint As IntPtr
            Public constraints As Integer

            Public dragged0 As UInteger
            Public dragged1 As UInteger
            Public dragged2 As UInteger
            Public dragged3 As UInteger

            Public calculatedFaileds As Integer

            Public failed As IntPtr
            Public faileds As Integer

            Public dof As Integer

            Public result As Integer
        End Structure

        Dim Params As New List(Of Slvs_Param)
        Dim Entities As New List(Of Slvs_Entity)
        Dim Constraints As New List(Of Slvs_Constraint)

        Dim Faileds As New List(Of UInteger)

        Dim Result As Integer
        Dim Dof As Integer

        ' Return the value of a parameter, by its handle. This function
        ' may be used, for example, to obtain the new values of the
        ' parameters after a call to Solve().
        Public Function GetParamByHandle(ByVal h As UInteger) As Double
            Dim t As Slvs_Param
            For Each t In Params
                If (t.h = h) Then
                    Return t.val
                End If
            Next
            Throw New Exception("Invalid parameter handle.")
        End Function

        ' Return the value of a parameter, by its index in the list (where
        ' that index is determined by the order in which the parameters
        ' were inserted with AddParam(), not by its handle).
        Public Function GetParamByIndex(ByVal i As Integer) As Double
            Return Params(i).val
        End Function

        ' Get the result after a call to Solve(). This may be
        '   SLVS_RESULT_OKAY              - it worked
        '   SLVS_RESULT_INCONSISTENT      - failed, inconsistent
        '   SLVS_RESULT_DIDNT_CONVERGE    - consistent, but still failed
        '   SLVS_RESULT_TOO_MANY_UNKNOWNS - too many parameters in one group
        Public Function GetResult() As Integer
            Return Result
        End Function

        ' After a call to Solve(), this returns the number of unconstrained 
        ' degrees of freedom for the sketch.
        Public Function GetDof() As Integer
            Return Dof
        End Function

        ' After a failing call to Solve(), this returns the list of
        ' constraints, identified by their handle, that would fix the
        ' system if they were deleted. This list will be populated only
        ' if calculateFaileds is True in the Solve() call.
        Public Function GetFaileds() As List(Of UInteger)
            Return Faileds
        End Function

        ' Clear our lists of entities, constraints, and parameters.
        Public Sub ResetAll()
            Params.Clear()
            Entities.Clear()
            Constraints.Clear()
        End Sub


        '''''''''''''''''''''''''''''''
        ' These functions are broadly similar to the Slvs_Make...
        ' functions in slvs.h. See the file DOC.txt accompanying the
        ' library for details.

        Public Sub AddParam(ByVal h As UInteger, ByVal group As UInteger,
                            ByVal val As Double)
            Dim p As Slvs_Param
            p.h = h
            p.group = group
            p.val = val
            Params.Add(p)
        End Sub

        Public Sub AddPoint2d(ByVal h As UInteger, ByVal group As UInteger,
                              ByVal wrkpl As UInteger,
                              ByVal u As UInteger, ByVal v As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_POINT_IN_2D
            e.wrkpl = wrkpl
            e.param0 = u
            e.param1 = v
            Entities.Add(e)
        End Sub

        Public Sub AddPoint3d(ByVal h As UInteger, ByVal group As UInteger,
                 ByVal x As UInteger, ByVal y As UInteger, ByVal z As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_POINT_IN_3D
            e.wrkpl = SLVS_FREE_IN_3D
            e.param0 = x
            e.param1 = y
            e.param2 = z
            Entities.Add(e)
        End Sub

        Public Sub AddNormal3d(ByVal h As UInteger, ByVal group As UInteger,
                                    ByVal qw As UInteger, ByVal qx As UInteger,
                                    ByVal qy As UInteger, ByVal qz As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_NORMAL_IN_3D
            e.wrkpl = SLVS_FREE_IN_3D
            e.param0 = qw
            e.param1 = qx
            e.param2 = qy
            e.param3 = qz
            Entities.Add(e)
        End Sub

        Public Sub AddNormal2d(ByVal h As UInteger, ByVal group As UInteger,
                               ByVal wrkpl As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_NORMAL_IN_2D
            e.wrkpl = wrkpl
            Entities.Add(e)
        End Sub

        Public Sub AddDistance(ByVal h As UInteger, ByVal group As UInteger,
                               ByVal wrkpl As UInteger, ByVal d As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_DISTANCE
            e.wrkpl = wrkpl
            e.param0 = d
            Entities.Add(e)
        End Sub

        Public Sub AddLineSegment(ByVal h As UInteger, ByVal group As UInteger,
                                  ByVal wrkpl As UInteger,
                                  ByVal ptA As UInteger, ByVal ptB As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_LINE_SEGMENT
            e.wrkpl = wrkpl
            e.point0 = ptA
            e.point1 = ptB
            Entities.Add(e)
        End Sub

        Public Sub AddCubic(ByVal h As UInteger, ByVal group As UInteger,
                            ByVal wrkpl As UInteger,
                            ByVal pt0 As UInteger, ByVal pt1 As UInteger,
                            ByVal pt2 As UInteger, ByVal pt3 As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_CUBIC
            e.wrkpl = wrkpl
            e.point0 = pt0
            e.point1 = pt1
            e.point2 = pt2
            e.point3 = pt3
            Entities.Add(e)
        End Sub

        Public Sub AddArcOfCircle(ByVal h As UInteger, ByVal group As UInteger,
                                  ByVal wrkpl As UInteger,
                                  ByVal normal As UInteger,
                                  ByVal center As UInteger,
                                  ByVal pstart As UInteger,
                                  ByVal pend As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_ARC_OF_CIRCLE
            e.wrkpl = wrkpl
            e.normal = normal
            e.point0 = center
            e.point1 = pstart
            e.point2 = pend
            Entities.Add(e)
        End Sub

        Public Sub AddCircle(ByVal h As UInteger, ByVal group As UInteger,
                             ByVal wrkpl As UInteger,
                             ByVal center As UInteger, ByVal normal As UInteger,
                             ByVal radius As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_CIRCLE
            e.wrkpl = wrkpl
            e.point0 = center
            e.normal = normal
            e.distance = radius
            Entities.Add(e)
        End Sub

        Public Sub AddWorkplane(ByVal h As UInteger, ByVal group As UInteger,
                                ByVal origin As UInteger,
                                ByVal normal As UInteger)
            Dim e As Slvs_Entity
            e.h = h
            e.group = group
            e.type = SLVS_E_WORKPLANE
            e.wrkpl = SLVS_FREE_IN_3D
            e.point0 = origin
            e.normal = normal
            Entities.Add(e)
        End Sub

        Public Sub AddConstraint(ByVal h As UInteger,
                                 ByVal group As UInteger,
                                 ByVal type As Integer,
                                 ByVal wrkpl As UInteger,
                                 ByVal valA As Double,
                                 ByVal ptA As UInteger,
                                 ByVal ptB As UInteger,
                                 ByVal entityA As UInteger,
                                 ByVal entityB As UInteger)
            Dim c As Slvs_Constraint
            c.h = h
            c.group = group
            c.type = type
            c.wrkpl = wrkpl
            c.valA = valA
            c.ptA = ptA
            c.ptB = ptB
            c.entityA = entityA
            c.entityB = entityB
            Constraints.Add(c)
        End Sub

        ' Solve the system. The geometry of the system must already have
        ' been specified through the Add...() calls. The result of the
        ' solution process may be obtained by calling GetResult(),
        ' GetFaileds(), GetDof(), and GetParamByXXX().
        '
        ' The parameters draggedx (indicated by their handles) will be held
        ' as close as possible to their original positions, even if this
        ' results in large moves for other parameters. This feature may be
        ' useful if, for example, the user is dragging the point whose
        ' location is defined by those parameters. Unused draggedx
        ' parameters may be specified as zero.
        Public Sub Solve(ByVal group As UInteger,
                         ByVal dragged0 As UInteger, ByVal dragged1 As UInteger,
                         ByVal dragged2 As UInteger, ByVal dragged3 As UInteger,
                         ByVal calculateFaileds As Boolean)
            Dim i As Integer

            Dim pp, p(Params.Count()) As Slvs_Param
            i = 0
            For Each pp In Params
                p(i) = pp
                i += 1
            Next

            Dim ee, e(Entities.Count()) As Slvs_Entity
            i = 0
            For Each ee In Entities
                e(i) = ee
                i += 1
            Next

            Dim cc, c(Constraints.Count()) As Slvs_Constraint
            i = 0
            For Each cc In Constraints
                c(i) = cc
                i += 1
            Next
            Dim f(Constraints.Count()) As UInteger

            Dim sys As Slvs_System

            Dim pgc, egc, cgc As GCHandle
            pgc = GCHandle.Alloc(p, GCHandleType.Pinned)
            sys.param = pgc.AddrOfPinnedObject()
            sys.params = Params.Count()
            egc = GCHandle.Alloc(e, GCHandleType.Pinned)
            sys.entity = egc.AddrOfPinnedObject()
            sys.entities = Entities.Count()
            cgc = GCHandle.Alloc(c, GCHandleType.Pinned)
            sys.constraint = cgc.AddrOfPinnedObject()
            sys.constraints = Constraints.Count()

            sys.dragged0 = dragged0
            sys.dragged1 = dragged1
            sys.dragged2 = dragged2
            sys.dragged3 = dragged3

            Dim fgc As GCHandle
            fgc = GCHandle.Alloc(f, GCHandleType.Pinned)
            If calculateFaileds Then
                sys.calculatedFaileds = 1
            Else
                sys.calculatedFaileds = 0
            End If
            sys.faileds = Constraints.Count()
            sys.failed = fgc.AddrOfPinnedObject()

            Dim sysgc As GCHandle
            sysgc = GCHandle.Alloc(sys, GCHandleType.Pinned)

            Slvs_Solve(sysgc.AddrOfPinnedObject(), group)

            sys = sysgc.Target

            For i = 0 To Params.Count() - 1
                Params(i) = p(i)
            Next

            Faileds.Clear()
            For i = 0 To sys.faileds - 1
                Faileds.Add(f(i))
            Next

            sysgc.Free()
            fgc.Free()
            pgc.Free()
            egc.Free()
            cgc.Free()

            Result = sys.result
            Dof = sys.dof
        End Sub
        ' A simpler version of the function, if the parameters being dragged
        ' correspond to a single point.
        Public Sub Solve(ByVal group As UInteger, ByVal dragged As Point,
                         ByVal calculatedFaileds As Boolean)
            If TypeOf dragged Is Point2d Then
                Dim p As Point2d
                p = dragged
                Solve(group, p.up.H, p.vp.H, 0, 0, calculatedFaileds)
            ElseIf TypeOf dragged Is Point3d Then
                Dim p As Point3d
                p = dragged
                Solve(group, p.xp.H, p.yp.H, p.zp.H, 0, calculatedFaileds)
            Else
                Throw New Exception("Can't get dragged params for point.")
            End If
        End Sub
        ' or if it's a single distance (e.g., the radius of a circle)
        Public Sub Solve(ByVal group As UInteger, ByVal dragged As Distance,
                         ByVal calculatedFaileds As Boolean)
            Solve(group, dragged.dp.H, 0, 0, 0, calculatedFaileds)
        End Sub
        ' or if it's nothing.
        Public Sub Solve(ByVal group As UInteger,
                         ByVal calculateFaileds As Boolean)
            Solve(group, 0, 0, 0, 0, calculateFaileds)
        End Sub

        ' Return the quaternion in (qw, qx, qy, qz) that represents a
        ' rotation from the base frame to a coordinate system with the
        ' specified basis vectors u and v. For example, u = (0, 1, 0)
        ' and v = (0, 0, 1) specifies the yz plane, such that a point with
        ' (u, v) = (7, 12) has (x, y, z) = (0, 7, 12).
        Public Sub MakeQuaternion(
            ByVal ux As Double, ByVal uy As Double, ByVal uz As Double,
            ByVal vx As Double, ByVal vy As Double, ByVal vz As Double,
            ByRef qw As Double, ByRef qx As Double, ByRef qy As Double,
                ByRef qz As Double)
            Slvs_MakeQuaternion(ux, uy, uz, _
                                vx, vy, vz, _
                                qw, qx, qy, qz)
        End Sub

        Public Function FreeIn3d()
            Return New Workplane(Me)
        End Function


        '''''''''''''''''''''''''''''''
        ' Functions to create the object-oriented wrappers defined below.

        Public Function NewParam(ByVal group As UInteger, ByVal val As Double)
            Return New Param(Me, group, val)
        End Function
        Public Function NewPoint2d(ByVal group As UInteger,
                                   ByVal wrkpl As Workplane,
                                   ByVal u As Double, ByVal v As Double)
            Return New Point2d(Me, group, wrkpl, u, v)
        End Function
        Public Function NewPoint2d(ByVal group As UInteger,
                                   ByVal wrkpl As Workplane,
                                   ByVal u As Param, ByVal v As Param)
            Return New Point2d(Me, group, wrkpl, u, v)
        End Function
        Public Function NewPoint3d(ByVal group As UInteger,
                                   ByVal x As Double,
                                   ByVal y As Double,
                                   ByVal z As Double)
            Return New Point3d(Me, group, x, y, z)
        End Function
        Public Function NewPoint3d(ByVal group As UInteger,
                                   ByVal x As Param,
                                   ByVal y As Param,
                                   ByVal z As Param)
            Return New Point3d(Me, group, x, y, z)
        End Function
        Public Function NewNormal3d(ByVal group As UInteger,
                    ByVal ux As Double, ByVal uy As Double, ByVal uz As Double,
                    ByVal vx As Double, ByVal vy As Double, ByVal vz As Double)
            Return New Normal3d(Me, group, ux, uy, uz, vx, vy, vz)
        End Function
        Public Function NewNormal3d(ByVal group As UInteger,
                                    ByVal qw As Param, ByVal qx As Param,
                                    ByVal qy As Param, ByVal qz As Param)
            Return New Normal3d(Me, group, qw, qx, qy, qz)
        End Function
        Public Function NewNormal2d(ByVal group As UInteger,
                                    ByVal wrkpl As Workplane)
            Return New Normal2d(Me, group, wrkpl)
        End Function
        Public Function NewDistance(ByVal group As UInteger,
                                    ByVal wrkpl As Workplane, ByVal d As Double)
            Return New Distance(Me, group, wrkpl, d)
        End Function
        Public Function NewDistance(ByVal group As UInteger,
                                    ByVal wrkpl As Workplane, ByVal d As Param)
            Return New Distance(Me, group, wrkpl, d)
        End Function
        Public Function NewLineSegment(ByVal group As UInteger,
                                       ByVal wrkpl As Workplane,
                                       ByVal ptA As Point, ByVal ptB As Point)
            Return New LineSegment(Me, group, wrkpl, ptA, ptB)
        End Function
        Public Function NewArcOfCircle(ByVal group As UInteger,
                                       ByVal wrkpl As Workplane,
                                       ByVal normal As Normal,
                                       ByVal center As Point,
                                       ByVal pstart As Point,
                                       ByVal pend As Point)
            Return New ArcOfCircle(Me, group, wrkpl, normal, _
                                   center, pstart, pend)
        End Function
        Public Function NewCircle(ByVal group As UInteger,
                                  ByVal wrkpl As Workplane,
                                  ByVal center As Point,
                                  ByVal normal As Normal,
                                  ByVal radius As Distance)
            Return New Circle(Me, group, wrkpl, center, normal, radius)
        End Function
        Public Function NewWorkplane(ByVal group As UInteger,
                                     ByVal origin As Point,
                                     ByVal normal As Normal)
            Return New Workplane(Me, group, origin, normal)
        End Function

        Public Sub AddConstraint(ByVal H As UInteger, ByVal group As UInteger,
                                 ByVal type As Integer,
                                 ByVal wrkpl As Workplane,
                                 ByVal valA As Double,
                                 ByVal ptA As Point, ByVal ptB As Point,
                                 ByVal entityA As Entity,
                                 ByVal entityB As Entity)
            AddConstraint(H, group, type, _
                              If(IsNothing(wrkpl), 0, wrkpl.H), _
                              valA, _
                              If(IsNothing(ptA), 0, ptA.H), _
                              If(IsNothing(ptB), 0, ptB.H), _
                              If(IsNothing(entityA), 0, entityA.H), _
                              If(IsNothing(entityB), 0, entityB.H))
        End Sub


        '''''''''''''''''''''''''''''''
        ' The object-oriented wrapper classes themselves, to allow the
        ' representation of entities and constraints as .net objects, not
        ' integer handles. These don't do any work themselves, beyond
        ' allocating and storing a unique integer handle.
        '
        ' These functions will assign parameters and entities with
        ' consecutive handles starting from 1. If they are intermixed
        ' with parameters and entities with user-specified handles, then
        ' those handles must be chosen not to conflict, e.g. starting
        ' from 100 000 or another large number.

        Public Class Param
            Public Slv As Slvs
            Public H As UInteger

            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal val As Double)
                Slv = s
                H = Slv.Params.Count() + 1
                Slv.AddParam(H, group, val)
            End Sub
        End Class

        Public MustInherit Class Entity
            Public Slv As Slvs
            Public H As UInteger

            Public Sub New(ByVal s As Slvs)
                Slv = s
                H = Slv.Entities.Count() + 1
            End Sub
        End Class

        Public MustInherit Class Point
            Inherits Entity
            Public Sub New(ByVal s As Slvs)
                MyBase.New(s)
            End Sub
        End Class

        Public MustInherit Class Normal
            Inherits Entity
            Public Sub New(ByVal s As Slvs)
                MyBase.New(s)
            End Sub
        End Class

        Public Class Point2d
            Inherits Point
            Public up, vp As Param
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane,
                           ByVal u As Double, ByVal v As Double)
                MyBase.New(s)
                up = New Param(Slv, group, u)
                vp = New Param(Slv, group, v)
                Slv.AddPoint2d(H, group, wrkpl.H, up.H, vp.H)
            End Sub
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane,
                           ByVal u As Param, ByVal v As Param)
                MyBase.New(s)
                Slv.AddPoint2d(H, group, wrkpl.H, u.H, v.H)
                up = u
                vp = v
            End Sub
            Function GetU()
                Return Slv.GetParamByHandle(up.H)
            End Function
            Function GetV()
                Return Slv.GetParamByHandle(vp.H)
            End Function
        End Class

        Public Class Point3d
            Inherits Point
            Public xp, yp, zp As Param
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal x As Double, ByVal y As Double,
                           ByVal z As Double)
                MyBase.New(s)
                xp = New Param(Slv, group, x)
                yp = New Param(Slv, group, y)
                zp = New Param(Slv, group, z)
                Slv.AddPoint3d(H, group, xp.H, yp.H, zp.H)
            End Sub
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal x As Param, ByVal y As Param, ByVal z As Param)
                MyBase.New(s)
                Slv.AddPoint3d(H, group, x.H, y.H, z.H)
                xp = x
                yp = y
                zp = z
            End Sub
            Function GetX()
                Return Slv.GetParamByHandle(xp.H)
            End Function
            Function GetY()
                Return Slv.GetParamByHandle(yp.H)
            End Function
            Function GetZ()
                Return Slv.GetParamByHandle(zp.H)
            End Function
        End Class

        Public Class Normal3d
            Inherits Normal
            Dim qwp, qxp, qyp, qzp As Param
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                   ByVal ux As Double, ByVal uy As Double, ByVal uz As Double,
                   ByVal vx As Double, ByVal vy As Double, ByVal vz As Double)
                MyBase.New(s)
                Dim qw, qx, qy, qz As Double
                Slv.MakeQuaternion(ux, uy, uz, vx, vy, vz, qw, qx, qy, qz)
                qwp = New Param(Slv, group, qw)
                qxp = New Param(Slv, group, qx)
                qyp = New Param(Slv, group, qy)
                qzp = New Param(Slv, group, qz)
                Slv.AddNormal3d(H, group, qwp.H, qxp.H, qyp.H, qzp.H)
            End Sub
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal qw As Param, ByVal qx As Param,
                           ByVal qy As Param, ByVal qz As Param)
                MyBase.New(s)
                Slv.AddNormal3d(H, group, qw.H, qx.H, qy.H, qz.H)
                qwp = qw
                qxp = qx
                qyp = qy
                qzp = qz
            End Sub
        End Class

        Public Class Normal2d
            Inherits Normal
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane)
                MyBase.New(s)
                Slv.AddNormal2d(H, group, wrkpl.H)
            End Sub
        End Class

        Public Class Distance
            Inherits Entity
            Public dp As Param

            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane, ByVal d As Double)
                MyBase.New(s)
                dp = New Param(Slv, group, d)
                Slv.AddDistance(H, group, wrkpl.H, dp.H)
            End Sub
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane, ByVal d As Param)
                MyBase.New(s)
                Slv.AddDistance(H, group, wrkpl.H, d.H)
                dp = d
            End Sub
            Function GetDistance() As Double
                Return Slv.GetParamByHandle(dp.H)
            End Function
        End Class

        Public Class LineSegment
            Inherits Entity
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane,
                           ByVal ptA As Point, ByVal ptB As Point)
                MyBase.New(s)
                Slv.AddLineSegment(H, group, wrkpl.H, ptA.H, ptB.H)
            End Sub
        End Class

        Public Class Cubic
            Inherits Entity
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane,
                           ByVal pt0 As Point, ByVal pt1 As Point,
                           ByVal pt2 As Point, ByVal pt3 As Point)
                MyBase.New(s)
                Slv.AddCubic(H, group, wrkpl.H, pt0.H, pt1.H, pt2.H, pt3.H)
            End Sub
        End Class

        Public Class ArcOfCircle
            Inherits Entity
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane, ByVal normal As Normal,
                           ByVal center As Point, ByVal pstart As Point,
                           ByVal pend As Point)
                MyBase.New(s)
                Slv.AddArcOfCircle(H, group, wrkpl.H, normal.H, _
                                   center.H, pstart.H, pend.H)
            End Sub
        End Class

        Public Class Circle
            Inherits Entity
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal wrkpl As Workplane, ByVal center As Point,
                           ByVal normal As Normal, ByVal radius As Distance)
                MyBase.New(s)
                Slv.AddCircle(H, group, wrkpl.H, center.H, normal.H, radius.H)
            End Sub
        End Class

        Public Class Workplane
            Inherits Entity
            Public Sub New(ByVal s As Slvs)
                MyBase.New(s)
                H = SLVS_FREE_IN_3D
            End Sub
            Public Sub New(ByVal s As Slvs, ByVal group As UInteger,
                           ByVal origin As Point, ByVal normal As Normal)
                MyBase.New(s)
                Slv.AddWorkplane(H, group, origin.H, normal.H)
            End Sub
        End Class

    End Class

End Module

