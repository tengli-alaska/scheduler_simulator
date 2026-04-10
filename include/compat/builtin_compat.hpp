#pragma once

// Compatibility shims for older Apple Clang/libc++ combinations where
// libc++ headers use __builtin_ctzg/__builtin_clzg but the compiler does
// not provide them yet.

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_ctzg)
template <typename T>
inline int __compat_builtin_ctzg(T value, int fallback) {
    if (value == 0) return fallback;
    return __builtin_ctzll(static_cast<unsigned long long>(value));
}
#define __builtin_ctzg(x, fallback) __compat_builtin_ctzg((x), (fallback))
#endif

#if !__has_builtin(__builtin_clzg)
template <typename T>
inline int __compat_builtin_clzg(T value, int fallback) {
    if (value == 0) return fallback;
    constexpr int bits = static_cast<int>(sizeof(T) * 8);
    return __builtin_clzll(static_cast<unsigned long long>(value)) - (64 - bits);
}
#define __builtin_clzg(x, fallback) __compat_builtin_clzg((x), (fallback))
#endif
