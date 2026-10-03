// Global allocation counter for T19. Replaces operator new and delete for the
// whole executable; counting is active only while allocCounting is true.

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>

namespace sph::test
{
std::atomic<bool> allocCounting { false };
std::atomic<long> allocCount { 0 };
} // namespace sph::test

namespace
{
inline void note() noexcept
{
    if (sph::test::allocCounting.load (std::memory_order_relaxed))
        sph::test::allocCount.fetch_add (1, std::memory_order_relaxed);
}

void* allocate (std::size_t n)
{
    note();
    if (void* p = std::malloc (n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}

void* allocateAligned (std::size_t n, std::align_val_t a)
{
    note();
    const std::size_t al = std::max (sizeof (void*), (std::size_t) a);
    void* p = nullptr;
    if (posix_memalign (&p, al, n == 0 ? al : n) != 0)
        throw std::bad_alloc();
    return p;
}

void release (void* p) noexcept
{
    if (p != nullptr)
        note();
    std::free (p);
}
} // namespace

void* operator new (std::size_t n) { return allocate (n); }
void* operator new[] (std::size_t n) { return allocate (n); }
void* operator new (std::size_t n, const std::nothrow_t&) noexcept
{
    try { return allocate (n); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept
{
    try { return allocate (n); } catch (...) { return nullptr; }
}
void* operator new (std::size_t n, std::align_val_t a) { return allocateAligned (n, a); }
void* operator new[] (std::size_t n, std::align_val_t a) { return allocateAligned (n, a); }
void operator delete (void* p) noexcept { release (p); }
void operator delete[] (void* p) noexcept { release (p); }
void operator delete (void* p, std::size_t) noexcept { release (p); }
void operator delete[] (void* p, std::size_t) noexcept { release (p); }
void operator delete (void* p, std::align_val_t) noexcept { release (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { release (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { release (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { release (p); }
