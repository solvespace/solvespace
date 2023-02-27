# slvs

SolveSpace's geometric constraint solver for the browser and node.js

## example

```html
<script src="slvs.js"></script>
<script type="text/javascript">
solvespace().then(function (slvs) {
  slvs.clearSketch()
  var g = 1
  var wp = slvs.addBase2D(g)
  var p0 = slvs.addPoint2D(g, 0, 0, wp)
  slvs.dragged(g, p0, wp)
  var p1 = slvs.addPoint2D(g, 90, 0, wp)
  slvs.dragged(g, p1, wp)
  var line0 = slvs.addLine2D(g, p0, p1, wp)
  var p2 = slvs.addPoint2D(g, 20, 20, wp)
  var p3 = slvs.addPoint2D(g, 0, 10, wp)
  var p4 = slvs.addPoint2D(g, 30, 20, wp)
  slvs.distance(g, p2, p3, 40, wp)
  slvs.distance(g, p2, p4, 40, wp)
  slvs.distance(g, p3, p4, 70, wp)
  slvs.distance(g, p0, p3, 35, wp)
  slvs.distance(g, p1, p4, 70, wp)
  var line1 = slvs.addLine2D(g, p0, p3, wp)
  // slvs.angle(g, line0, line1, 45, wp, false)
  slvs.addConstraint(
    g,
    slvs.C_ANGLE,
    wp,
    45.0,
    slvs.E_NONE,
    slvs.E_NONE,
    line0,
    line1,
    slvs.E_NONE,
    slvs.E_NONE,
    false,
    false,
  )
  var result = slvs.solveSketch(g, false)
  console.log(result['result'], '==', 0)
  var x = slvs.getParamValue(p2.param[0])
  var y = slvs.getParamValue(p2.param[1])
  console.log(39.54852, x)
  console.log(61.91009, y)


  var r = 10
  angle = 45
  slvs.clearSketch()
  var g = 1
  var wp = slvs.addBase2D(g)
  var p0 = slvs.addPoint2D(g, 0, 0, wp)
  slvs.dragged(g, p0, wp)
  var p1 = slvs.addPoint2D(g, 0, 10, wp)
  slvs.distance(g, p0, p1, r, wp)
  var line0 = slvs.addLine2D(g, p0, p1, wp)

  var p2 = slvs.addPoint2D(g, 10, 10, wp)
  var line1 = slvs.addLine2D(g, p1, p2, wp)
  slvs.distance(g, p1, p2, r * (angle * (Math.PI / 180)), wp)
  slvs.perpendicular(g, line0, line1, wp, false)

  var p3 = slvs.addPoint2D(g, 10, 0, wp)
  slvs.dragged(g, p3, wp)
  var lineBase = slvs.addLine2D(g, p0, p3, wp)
  slvs.angle(g, line0, lineBase, angle, wp, false)

  var result = slvs.solveSketch(g, false)
  console.log(result['result'], '==', 0)
  var x = slvs.getParamValue(p2.param[0])
  var y = slvs.getParamValue(p2.param[1])
  console.log(12.62467, '==', x)
  console.log(1.51746, '==', y)

  q1 = slvs.Quaternion.from(1, 2, 3, 4)
  q2 = slvs.Quaternion.from(1, 2, 3, 4)
  console.log(q1.times(q2).rotationU().x)
})
</script>
```