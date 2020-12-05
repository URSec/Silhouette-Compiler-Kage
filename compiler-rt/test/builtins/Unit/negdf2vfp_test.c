// RUN: %clang_builtins %s %librt -o %t && %run %t
//===-- negdf2vfp_test.c - Test __negdf2vfp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file tests __negdf2vfp for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
extern COMPILER_RT_ABI double __negdf2vfp(double a);

int test__negdf2vfp(double a)
{
    double actual = __negdf2vfp(a);
    double expected = -a;
    if (actual != expected)
        printf("error in test__negdf2vfp(%f) = %f, expected %f\n",
               a, actual, expected);
    return actual != expected;
}
#endif

int main()
{
#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
    if (test__negdf2vfp(1.0))
        return 1;
    if (test__negdf2vfp(HUGE_VALF))
        return 1;
    if (test__negdf2vfp(0.0))
        return 1;
    if (test__negdf2vfp(-1.0))
        return 1;
#else
    printf("skipped\n");
#endif
    return 0;
}
