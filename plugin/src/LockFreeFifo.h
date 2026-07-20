// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "JuceHeader.h"

/* single channel SPSC lock-free queue */
struct LockFreeFifo
{
    juce::AbstractFifo fifo { 1 };
    std::vector<float> data;
    
    void resize(int size) {
        fifo.setTotalSize(size);
        data.assign(static_cast<size_t>(size), 0.0f);
    }
    
    void write(const float* src, int n) {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(n, start1, size1, start2, size2);
        
        if (size1 > 0) {
            std::memcpy(data.data() + start1, src, static_cast<size_t>(size1) * sizeof(float));
        }
        if (size2 > 0) {
            std::memcpy(data.data() + start2, src + size1, static_cast<size_t>(size2) * sizeof(float));
        }
        
        fifo.finishedWrite(size1 + size2);
        
        jassert(size1 + size2 == n);
    }
    
    int read(float* dst, int n)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(n, start1, size1, start2, size2);
        
        if (size1 > 0) {
            std::memcpy(dst, data.data() + start1, static_cast<size_t>(size1) * sizeof(float));
        }
        if (size2 > 0) {
            std::memcpy(dst + size1, data.data() + start2, static_cast<size_t>(size2) * sizeof(float));
        }
        
        fifo.finishedRead(size1 + size2);
        
        return size1 + size2;
    }
    
    int getNumReady()
    {
        return fifo.getNumReady();
    }
};
