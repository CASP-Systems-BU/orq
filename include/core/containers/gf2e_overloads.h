#pragma once

#include <NTL/GF2E.h>

#include <cassert>

namespace NTL {

// Dummy overloads for GF2E operators that are undefined. Throws an assertion if called.

inline GF2E operator&(const GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E operator|(const GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E operator^(const GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}

inline GF2E operator&(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E operator|(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E operator^(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}

inline GF2E& operator&=(GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E& operator|=(GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E& operator^=(GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}

inline GF2E& operator&=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E& operator|=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E& operator^=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}

inline GF2E operator~(const GF2E& value) {
    assert(false);
    return value;
}
inline bool operator!(const GF2E& value) {
    assert(false);
    return false;
}

inline GF2E operator%(const GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E operator%(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E& operator%=(GF2E& lhs, const GF2E&) {
    assert(false);
    return lhs;
}
inline GF2E& operator%=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}

inline GF2E operator>>(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E operator<<(const GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E& operator>>=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}
inline GF2E& operator<<=(GF2E& lhs, long) {
    assert(false);
    return lhs;
}

inline bool operator>(const GF2E&, const GF2E&) {
    assert(false);
    return false;
}
inline bool operator>=(const GF2E&, const GF2E&) {
    assert(false);
    return false;
}
inline bool operator<(const GF2E&, const GF2E&) {
    assert(false);
    return false;
}
inline bool operator<=(const GF2E&, const GF2E&) {
    assert(false);
    return false;
}

inline bool operator>(const GF2E&, long) {
    assert(false);
    return false;
}
inline bool operator>=(const GF2E&, long) {
    assert(false);
    return false;
}
inline bool operator<(const GF2E&, long) {
    assert(false);
    return false;
}
inline bool operator<=(const GF2E&, long) {
    assert(false);
    return false;
}

}  // namespace NTL
