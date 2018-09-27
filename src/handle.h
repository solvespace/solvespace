//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_HANDLE_H
#define SOLVESPACE_HANDLE_H



namespace SolveSpace {

// All of the hWhatever handles are a 32-bit ID, that is used to represent
// some data structure in the sketch.
class hGroup {
public:
    // bits 15: 0   -- group index
    uint32_t v;

    inline hEntity entity(int i) const;
    inline hParam param(int i) const;
    inline hEquation equation(int i) const;
};
class hRequest {
public:
    // bits 15: 0   -- request index
    uint32_t v;

    inline hEntity entity(int i) const;
    inline hParam param(int i) const;

    inline bool IsFromReferences() const;
};
class hEntity {
public:
    // bits 15: 0   -- entity index
    //      31:16   -- request index
    uint32_t v;

    inline bool isFromRequest() const;
    inline hRequest request() const;
    inline hGroup group() const;
    inline hEquation equation(int i) const;
};
class hParam {
public:
    // bits 15: 0   -- param index
    //      31:16   -- request index
    uint32_t v;

    inline hRequest request() const;
};

class hStyle {
public:
    uint32_t v;
};

class hConstraint {
public:
    uint32_t v;

    inline hEquation equation(int i) const;
    inline hParam param(int i) const;
};

class hEquation {
public:
    uint32_t v;

    inline bool isFromConstraint() const;
    inline hConstraint constraint() const;
};

inline hEntity hGroup::entity(int i) const
{ hEntity r; r.v = 0x80000000 | (v << 16) | (uint32_t)i; return r; }
inline hParam hGroup::param(int i) const
{ hParam r; r.v = 0x80000000 | (v << 16) | (uint32_t)i; return r; }
inline hEquation hGroup::equation(int i) const
{ hEquation r; r.v = (v << 16) | 0x80000000 | (uint32_t)i; return r; }

inline bool hRequest::IsFromReferences() const {
    if(v == Request::HREQUEST_REFERENCE_XY.v) return true;
    if(v == Request::HREQUEST_REFERENCE_YZ.v) return true;
    if(v == Request::HREQUEST_REFERENCE_ZX.v) return true;
    return false;
}
inline hEntity hRequest::entity(int i) const
{ hEntity r; r.v = (v << 16) | (uint32_t)i; return r; }
inline hParam hRequest::param(int i) const
{ hParam r; r.v = (v << 16) | (uint32_t)i; return r; }

inline bool hEntity::isFromRequest() const
{ if(v & 0x80000000) return false; else return true; }
inline hRequest hEntity::request() const
{ hRequest r; r.v = (v >> 16); return r; }
inline hGroup hEntity::group() const
{ hGroup r; r.v = (v >> 16) & 0x3fff; return r; }
inline hEquation hEntity::equation(int i) const
{ hEquation r; r.v = v | 0x40000000 | (uint32_t)i; return r; }

inline hRequest hParam::request() const
{ hRequest r; r.v = (v >> 16); return r; }


inline hEquation hConstraint::equation(int i) const
{ hEquation r; r.v = (v << 16) | (uint32_t)i; return r; }
inline hParam hConstraint::param(int i) const
{ hParam r; r.v = v | 0x40000000 | (uint32_t)i; return r; }

inline bool hEquation::isFromConstraint() const
{ if(v & 0xc0000000) return false; else return true; }
inline hConstraint hEquation::constraint() const
{ hConstraint r; r.v = (v >> 16); return r; }


}

#endif //SOLVESPACE_HANDLE_H
