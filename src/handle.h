#ifndef SOVLESPACE_HANDLE_H
#define SOVLESPACE_HANDLE_H

#include <functional>
#include <type_traits>

namespace SolveSpace {

/// Trait indicating which types are handle types and should get the associated operators.
/// Specialize for each handle type and inherit from std::true_type.
template<class T>
struct IsHandleOracle : std::false_type {};

// Equality-compare any two instances of a handle type.
template<class T>
static inline typename std::enable_if<IsHandleOracle<T>::value, bool>::type
operator==(T const &lhs, T const &rhs) {
    return lhs.v == rhs.v;
}

// Inequality-compare any two instances of a handle type.
template<class T>
static inline typename std::enable_if<IsHandleOracle<T>::value, bool>::type
operator!=(T const &lhs, T const &rhs) {
    return !(lhs == rhs);
}

// Less-than-compare any two instances of a handle type.
template<class T>
static inline typename std::enable_if<IsHandleOracle<T>::value, bool>::type
operator<(T const &lhs, T const &rhs) {
    return lhs.v < rhs.v;
}

template<class T>
struct HandleHasher {
    static_assert(IsHandleOracle<T>::value, "Not a valid handle type");

    inline size_t operator()(const T &h) const {
        using Hasher = std::hash<decltype(T::v)>;
        return Hasher{}(h.v);
    }
};

} // namespace SolveSpace

#endif // !SOVLESPACE_HANDLE_H
