#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>

#pragma STDC FENV_ACCESS ON

#define NW_FPU_NEAREST(expr) \
    unsigned csr = _mm_getcsr(); \
    _mm_setcsr(csr & ~0x6000u); \
    float r = (expr); \
    _mm_setcsr(csr); \
    return r
#else
#define NW_FPU_NEAREST(expr) return (expr)
#endif

static void nw_fpu_guard(float *d, float *t)
{
    uint32_t a, b;
    memcpy(&a, d, sizeof(a));
    memcpy(&b, t, sizeof(b));
    int diff = (int)(a >> 23 & 0xFF) - (int)(b >> 23 & 0xFF);
    if (diff >= 25) {
        b &= 0x80000000u;
    } else if (diff > 0) {
        b &= 0xFFFFFFFFu << (diff - 1);
    } else if (diff <= -25) {
        a &= 0x80000000u;
    } else if (diff < 0) {
        a &= 0xFFFFFFFFu << (-diff - 1);
    }
    memcpy(d, &a, sizeof(a));
    memcpy(t, &b, sizeof(b));
}

float nw_fpu_add(float a, float b)
{
    nw_fpu_guard(&a, &b);
    return a + b;
}

float nw_fpu_sub(float a, float b)
{
    nw_fpu_guard(&a, &b);
    return a - b;
}

float nw_fpu_div(float a, float b)
{
    NW_FPU_NEAREST(a / b);
}

float nw_fpu_sqrt(float x)
{
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    if ((bits & 0x7F800000u) == 0) {
        bits &= 0x80000000u;
        memcpy(&x, &bits, sizeof(bits));
        return x;
    }
    NW_FPU_NEAREST(sqrtf(fabsf(x)));
}
