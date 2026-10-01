#include "AllocationTracker.h"
#include <cstdlib>
#include <new>

namespace ppf42::test {
std::atomic<bool>   gTrackAllocations { false };
std::atomic<size_t> gAllocationCount  { 0 };
std::atomic<size_t> gAllocatedBytes  { 0 };
} // namespace ppf42::test

void* operator new(size_t size) {
    if (ppf42::test::gTrackAllocations.load(std::memory_order_relaxed)) {
        ppf42::test::gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        ppf42::test::gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

void* operator new[](size_t size) {
    if (ppf42::test::gTrackAllocations.load(std::memory_order_relaxed)) {
        ppf42::test::gAllocationCount.fetch_add(1, std::memory_order_relaxed);
        ppf42::test::gAllocatedBytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}
