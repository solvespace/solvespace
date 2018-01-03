//-----------------------------------------------------------------------------
// Inlined implementations of some math operations using Eigen.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2018 Ryan Pavlik.
//-----------------------------------------------------------------------------
#ifndef __UTILEIGEN_H
#define __UTILEIGEN_H

#include "solvespace.h"

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <unsupported/Eigen/AlignedVector3>

namespace SolveSpace {

/// A 3D vector of doubles using Eigen
using EiVector3 = Eigen::Vector3d;

/// A 3D vector of doubles using Eigen, with an extra double making it alignable for Eigen's SIMD
///
/// Higher performance, but harder to store in a structure
/// because it needs to be over-aligned.
using EiAlignedVector3 = Eigen::AlignedVector3<double>;

/// A 4D vector of doubles using Eigen
using EiVector4 = Eigen::Vector4d;

/// A (normalized) quaternion using Eigen, each component in doubles.
using EiQuaternion = Eigen::Quaterniond;

/// Wrap a SolveSpace::Vector in an Eigen map object so Eigen can use it like an EiVector3 without
/// copies.
///
/// You can use this to initialize a new EiAlignedVector3 (or EiVector3) if you want a fast local
/// copy.
inline Eigen::Map<EiVector3> map(Vector& v) {
    return EiVector3::Map(&v.x);
}

/// Wrap a (constant) SolveSpace::Vector in an Eigen map object so Eigen can use it like an
/// EiVector3 without copies.
///
/// You can use this to initialize a new EiAlignedVector3 (or EiVector3) if you want a fast local
/// copy.
inline Eigen::Map<const EiVector3> map(Vector const& v) {
    return EiVector3::Map(&v.x);
}

/// Copy a SolveSpace::Vector4 to an Eigen EiVector4.
///
/// Can't map here, unfortunately, since element order differs.
inline EiVector4 to_eigen(Vector4 const& v) {
    return EiVector4(v.x, v.y, v.z, v.w);
}

inline double DistanceToLine(EiAlignedVector3 const& point, EiAlignedVector3 const& p0,
                             EiAlignedVector3 const& dp) {
    return (point - p0).cross(dp).norm() / dp.norm();
}

inline double DistanceToLine(Vector const& point, Vector const& p0, Vector const& dp) {
    EiAlignedVector3 epoint = map(point);
    EiAlignedVector3 ep0    = map(p0);
    EiAlignedVector3 edp    = map(dp);
    return DistanceToLine(epoint, ep0, edp);
}


inline double DistanceToLineFromEndpoints(Vector const& point, Vector const& p0, Vector const& p1) {
    EiAlignedVector3 epoint = map(point);
    EiAlignedVector3 ep0    = map(p0);
    EiAlignedVector3 dp     = map(p1) - ep0;
    return DistanceToLine(epoint, ep0, dp);
}

} // namespace SolveSpace
#endif
