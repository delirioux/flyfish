/* SPDX-License-Identifier: GPL-3.0-only */
/* TUNA's few library calls. The FM-1's build (tools/build.py, TUNA_FREESTANDING=1) has no C library and maybe
 * no <math.h> or <string.h>: memcpy and memset come from Felucca's libc.c, the maths from tuna_libm.c. Everywhere
 * else (the host tests, the browser build) the system's headers. */
#ifndef TUNA_SYS_H
#define TUNA_SYS_H
#if TUNA_FREESTANDING
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
float fabsf(float x);
long lrintf(float x);
float sqrtf(float x);
float exp2f(float x);
float sinf(float x);
#else
#include <math.h>
#include <string.h>
#endif
#endif
