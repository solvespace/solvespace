export interface Entity {
  h: number;
  group: number;
  type: number;
  wrkpl: number;
  normal: number;
  distance: number;
  param: [number, number, number, number];
}

export interface Constraint {
  h: number;
  group: number;
  type: number;
  wrkpl: Entity;
  valA: number;
  ptA: Entity;
  ptB: Entity;
  entityA: Entity;
  entityB: Entity;
  entityC: Entity;
  entityD: Entity;
  other: boolean;
  other2: boolean;
}

export interface SolveResult {
  result: number;
  dof: number;
  bad: number;
}

export interface Vector {
  x: number
  y: number
  z: number
}

export interface Quaternion {
  w: number
  vx: number
  vy: number
  vz: number
  plus(b: Quaternion): Quaternion;
  minus(b: Quaternion): Quaternion;
  scaledBy(s: number): Quaternion;
  magnitude(): number;
  withMagnitude(s: number): Quaternion;
  rotationU(): Vector;
  rotationV(): Vector;
  rotationN(): Vector;
  rotate(p: Vector): Vector;
  toThe(p: number): Quaternion;
  inverse(): Quaternion;
  times(b: Quaternion): Quaternion;
  mirror(): Quaternion;
}

export interface QuaternionConstructor {
  new(): Quaternion;
  prototype: Quaternion;
  from(w: number, vx: number, vy: number, vz: number): Quaternion;
}

export interface SlvsModule {
  C_POINTS_COINCIDENT: 100000;
  C_PT_PT_DISTANCE: 100001;
  C_PT_PLANE_DISTANCE: 100002;
  C_PT_LINE_DISTANCE: 100003;
  C_PT_FACE_DISTANCE: 100004;
  C_PT_IN_PLANE: 100005;
  C_PT_ON_LINE: 100006;
  C_PT_ON_FACE: 100007;
  C_EQUAL_LENGTH_LINES: 100008;
  C_LENGTH_RATIO: 100009;
  C_EQ_LEN_PT_LINE_D: 100010;
  C_EQ_PT_LN_DISTANCES: 100011;
  C_EQUAL_ANGLE: 100012;
  C_EQUAL_LINE_ARC_LEN: 100013;
  C_SYMMETRIC: 100014;
  C_SYMMETRIC_HORIZ: 100015;
  C_SYMMETRIC_VERT: 100016;
  C_SYMMETRIC_LINE: 100017;
  C_AT_MIDPOINT: 100018;
  C_HORIZONTAL: 100019;
  C_VERTICAL: 100020;
  C_DIAMETER: 100021;
  C_PT_ON_CIRCLE: 100022;
  C_SAME_ORIENTATION: 100023;
  C_ANGLE: 100024;
  C_PARALLEL: 100025;
  C_PERPENDICULAR: 100026;
  C_ARC_LINE_TANGENT: 100027;
  C_CUBIC_LINE_TANGENT: 100028;
  C_EQUAL_RADIUS: 100029;
  C_PROJ_PT_DISTANCE: 100030;
  C_WHERE_DRAGGED: 100031;
  C_CURVE_CURVE_TANGENT: 100032;
  C_LENGTH_DIFFERENCE: 100033;
  C_ARC_ARC_LEN_RATIO: 100034;
  C_ARC_LINE_LEN_RATIO: 100035;
  C_ARC_ARC_DIFFERENCE: 100036;
  C_ARC_LINE_DIFFERENCE: 100037;

  E_POINT_IN_3D: 50000;
  E_POINT_IN_2D: 50001;
  E_NORMAL_IN_3D: 60000;
  E_NORMAL_IN_2D: 60001;
  E_DISTANCE: 70000;
  E_WORKPLANE: 80000;
  E_LINE_SEGMENT: 80001;
  E_CUBIC: 80002;
  E_CIRCLE: 80003;
  E_ARC_OF_CIRCLE: 80004;

  E_NONE: Entity;
  E_FREE_IN_3D: Entity;

  isFreeIn3D(entity: Entity): boolean;
  is3D(entity: Entity): boolean;
  isNone(entity: Entity): boolean;
  isPoint2D(entity: Entity): boolean;
  isPoint3D(entity: Entity): boolean;
  isNormal2D(entity: Entity): boolean;
  isNormal3D(entity: Entity): boolean;
  isLine(entity: Entity): boolean;
  isLine2D(entity: Entity): boolean;
  isLine3D(entity: Entity): boolean;
  isCubic(entity: Entity): boolean;
  isArc(entity: Entity): boolean;
  isWorkplane(entity: Entity): boolean;
  isDistance(entity: Entity): boolean;
  isPoint(entity: Entity): boolean;

  addPoint2D(grouph: number, u: number, v: number, workplane: Entity): Entity;
  addPoint3D(grouph: number, x: number, y: number, z: number): Entity;
  addNormal2D(grouph: number, workplane: Entity): Entity;
  addNormal3D(grouph: number, qw: number, qx: number, qy: number, qz: number): Entity;
  addDistance(grouph: number, value: number, workplane: Entity): Entity;
  addLine2D(grouph: number, ptA: Entity, ptB: Entity, workplane: Entity): Entity;
  addLine3D(grouph: number, ptA: Entity, ptB: Entity): Entity;
  addCubic(grouph: number, ptA: Entity, ptB: Entity, ptC: Entity, ptD: Entity, workplane: Entity): Entity;
  addArc(grouph: number, normal: Entity, center: Entity, start: Entity, end: Entity, workplane: Entity): Entity;
  addCircle(grouph: number, normal: Entity, center: Entity, radius: Entity, workplane: Entity): Entity;
  addWorkplane(grouph: number, origin: Entity, nm: Entity): Entity;
  addBase2D(grouph: number): Entity;

  addConstraint(
    grouph: number,
    type: number,
    workplane: Entity,
    val: number,
    ptA: Entity,
    ptB: Entity,
    entityA: Entity,
    entityB: Entity,
    entityC: Entity,
    entityD: Entity,
    other: boolean,
    other2: boolean,
  ): Constraint;

  coincident(grouph: number, entityA: Entity, entityB: Entity, workplane: Entity): Constraint;
  distance(grouph: number, entityA: Entity, entityB: Entity, value: number, workplane: Entity): Constraint;
  equal(grouph: number, entityA: Entity, entityB: Entity, workplane: Entity): Constraint;
  equalAngle(grouph: number, entityA: Entity, entityB: Entity, entityC: Entity, entityD: Entity, workplane: Entity): Constraint;
  equalPointToLine(grouph: number, entityA: Entity, entityB: Entity, entityC: Entity, entityD: Entity, workplane: Entity): Constraint;
  ratio(grouph: number, entityA: Entity, entityB: Entity, value: number, workplane: Entity): Constraint;
  symmetric(grouph: number, entityA: Entity, entityB: Entity, entityC: Entity): Constraint;
  symmetricH(grouph: number, ptA: Entity, ptB: Entity, workplane: Entity): Constraint;
  symmetricV(grouph: number, ptA: Entity, ptB: Entity, workplane: Entity): Constraint;
  midpoint(grouph: number, ptA: Entity, ptB: Entity, workplane: Entity): Constraint;
  horizontal(grouph: number, entityA: Entity, workplane: Entity, entityB: Entity): Constraint;
  vertical(grouph: number, entityA: Entity, workplane: Entity, entityB: Entity): Constraint;
  diameter(grouph: number, entityA: Entity, value: number): Constraint;
  sameOrientation(grouph: number, entityA: Entity, entityB: Entity): Constraint;
  angle(grouph: number, entityA: Entity, entityB: Entity, value: number, workplane: Entity, inverse: boolean): Constraint;
  perpendicular(grouph: number, entityA: Entity, entityB: Entity, workplane: Entity, inverse: boolean): Constraint;
  parallel(grouph: number, entityA: Entity, entityB: Entity, workplane: Entity): Constraint;
  tangent(grouph: number, entityA: Entity, entityB: Entity, workplane: Entity): Constraint;
  distanceProj(grouph: number, ptA: Entity, ptB: Entity, value: number): Constraint;
  lengthDiff(grouph: number, entityA: Entity, entityB: Entity, value: number, workplane: Entity): Constraint;
  dragged(grouph: number, ptA: Entity, workplane: Entity): Constraint;

  getParamValue(ph: number): number;
  setParamValue(ph: number, value: number): number;
  solveSketch(hgroup: number, calculateFaileds: boolean): SolveResult;
  clearSketch(): void;
}

declare function ModuleLoader(): Promise<SlvsModule>;

export default ModuleLoader;
