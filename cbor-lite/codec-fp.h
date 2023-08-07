#pragma once
// This file is part of CBOR-lite which is copyright Isode Limited
// and others and released under a MIT license. For details, see the
// COPYRIGHT.md file in the top-level folder of the CBOR-lite software
// distribution.

// This copy of CBOR-lite has been modified for 32-bit MSVC for Project 64 Legacy.

#include "codec.h"

#ifndef __BYTE_ORDER__
#ifdef _MSC_VER
// PJ64: Byte order hack for MSVC
#define __ORDER_LITTLE_ENDIAN__ 0
#define __ORDER_BIG_ENDIAN__ 1
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#else
#error __BYTE_ORDER__ not defined
#endif
#elif (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error __BYTE_ORDER__ neither __ORDER_BIG_ENDIAN__ nor __ORDER_LITTLE_ENDIAN__
#endif

namespace CborLite {

template <typename Buffer, typename Type>
typename std::enable_if<std::is_class<Buffer>::value && std::is_floating_point<Type>::value, std::size_t>::type encodeSingleFloat(
    Buffer& buffer, const Type& t) {
    static_assert(sizeof(float) == 4, "sizeof(float) expected to be 4");
    auto len = encodeTagAndAdditional(buffer, Major::floatingPoint, Minor::singleFloat);
    const char* p;
    float ft;
    if (sizeof(t) == sizeof(ft)) {
        p = reinterpret_cast<const char*>(&t);
    } else {
        ft = static_cast<decltype(ft)>(t);
        p = reinterpret_cast<char*>(&ft);
    }
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (auto i = 0u; i < sizeof(ft); ++i) {
        buffer.push_back(p[i]);
    }
#else
    for (auto i = 1u; i <= sizeof(ft); ++i) {
        buffer.push_back(p[sizeof(ft) - i]);
    }
#endif
    return len + sizeof(ft);
}

template <typename Buffer, typename Type>
typename std::enable_if<std::is_class<Buffer>::value && std::is_floating_point<Type>::value, std::size_t>::type encodeDoubleFloat(
    Buffer& buffer, const Type& t) {
    static_assert(sizeof(double) == 8, "sizeof(double) expected to be 8");
    auto len = encodeTagAndAdditional(buffer, Major::floatingPoint, Minor::doubleFloat);
    const char* p;
    double ft;
    if (sizeof(t) == sizeof(ft)) {
        p = reinterpret_cast<const char*>(&t);
    } else {
        ft = t;
        p = reinterpret_cast<char*>(&ft);
    }
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (auto i = 0u; i < sizeof(ft); ++i) {
        buffer.push_back(p[i]);
    }
#else
    for (auto i = 1u; i <= sizeof(ft); ++i) {
        buffer.push_back(p[sizeof(ft) - i]);
    }
#endif
    return len + sizeof(ft);
}

template <typename InputIterator, typename Type>
typename std::enable_if<std::is_class<InputIterator>::value && std::is_floating_point<Type>::value && !std::is_const<Type>::value,
    std::size_t>::type
decodeSingleFloat(InputIterator& pos, InputIterator end, Type& t, Flags flags = Flag::none) {
    static_assert(sizeof(float) == 4, "sizeof(float) expected to be 4");
    auto tag = undefined;
    auto value = undefined;
    auto len = decodeTagAndAdditional(pos, end, tag, value, flags);
    if (tag != Major::floatingPoint) throw Exception("not floating-point");
    if (value != Minor::singleFloat) throw Exception("not single-precision floating-point");
    if (std::distance(pos, end) < static_cast<int>(sizeof(float))) throw Exception("not enough input");

    char* p;
    float ft;
    if (sizeof(t) == sizeof(ft)) {
        p = reinterpret_cast<char*>(&t);
    } else {
        ft = static_cast<decltype(ft)>(t);
        p = reinterpret_cast<char*>(&ft);
    }

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (auto i = 0u; i < sizeof(ft); ++i) {
        p[i] = *(pos++);
    }
#else
    for (auto i = 1u; i <= sizeof(ft); ++i) {
        p[sizeof(ft) - i] = *(pos++);
    }
#endif
    if (sizeof(t) != sizeof(ft)) t = ft;
    return len + sizeof(ft);
}

template <typename InputIterator, typename Type>
typename std::enable_if<std::is_class<InputIterator>::value && std::is_floating_point<Type>::value && !std::is_const<Type>::value,
    std::size_t>::type
decodeDoubleFloat(InputIterator& pos, InputIterator end, Type& t, Flags flags = Flag::none) {
    static_assert(sizeof(double) == 8, "sizeof(double) expected to be 8");
    auto tag = undefined;
    auto value = undefined;
    auto len = decodeTagAndAdditional(pos, end, tag, value, flags);
    if (tag != Major::floatingPoint) throw Exception("not floating-point");
    if (value != Minor::doubleFloat) throw Exception("not double-precision floating-point");
    if (std::distance(pos, end) < static_cast<int>(sizeof(double))) throw Exception("not enough input");

    char* p;
    double ft;
    if (sizeof(t) == sizeof(ft)) {
        p = reinterpret_cast<char*>(&t);
    } else {
        ft = t;
        p = reinterpret_cast<char*>(&ft);
    }

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (auto i = 0u; i < sizeof(ft); ++i) {
        p[i] = *(pos++);
    }
#else
    for (auto i = 1u; i <= sizeof(ft); ++i) {
        p[sizeof(ft) - i] = *(pos++);
    }
#endif

    if (sizeof(t) != sizeof(ft)) t = ft;
    return len + sizeof(ft);
}

} // namespace CborLite
