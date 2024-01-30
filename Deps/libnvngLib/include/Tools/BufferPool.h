#pragma once

#include <stdint.h>
class BufferPool
{
public :
        BufferPool(int64_t bufferSize, int64_t initSize, int64_t incSize);
        ~BufferPool();

        void* Malloc();
        void Free(void* p);

private :
        class impl;
        impl* this_;
};

class BufferPoolImpl
{
public :
        BufferPoolImpl(int64_t minSize, int64_t maxSize);
        ~BufferPoolImpl();

        void* Malloc(int64_t size);
        void Free(void* p);

private :
        class impl;
        impl* this_;
};

// vim: fenc=utf8:expandtab:ts=8:noma
