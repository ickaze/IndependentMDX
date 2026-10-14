// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
#include <cstddef>
namespace mdx {
// Refill in submission order, including across the ring boundary. A later
// free slot must never overtake the oldest slot after multiple completions.
template<class Submit>
void refillAudioBuffers(size_t &next, size_t count, Submit submit) {
    for(size_t n=0;n<count;++n) {
        if(!submit(next)) break;
        next=(next+1)%count;
    }
}
}
