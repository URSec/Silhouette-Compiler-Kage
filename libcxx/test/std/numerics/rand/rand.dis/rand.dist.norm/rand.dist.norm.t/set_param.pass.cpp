//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <random>

// template<class RealType = double>
// class student_t_distribution;

// void param(const param_type& parm);

#include <random>
#include <cassert>

int main()
{
    {
        typedef std::student_t_distribution<> D;
        typedef D::param_type P;
        P p(0.25);
        D d(0.75);
        d.param(p);
        assert(d.param() == p);
    }
}