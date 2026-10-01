#pragma once

#include <atomic>
#include <cstddef>

namespace ppf42::test {

extern std::atomic<bool>   gTrackAllocations;
extern std::atomic<size_t> gAllocationCount;
extern std::atomic<size_t> gAllocatedBytes;

struct ScopedAllocationGuard {
    ScopedAllocationGuard() noexcept {
        gAllocationCount.store(0, std::memory_order_seq_cst);
        gAllocatedBytes.store(0, std::memory_order_seq_cst);
        gTrackAllocations.store(true, std::memory_order_seq_cst);
    }

    ~ScopedAllocationGuard() noexcept {
        gTrackAllocations.store(false, std::memory_order_seq_cst);
    }

    ScopedAllocationGuard(const ScopedAllocationGuard&) = delete;
    ScopedAllocationGuard& operator=(const ScopedAllocationGuard&) = delete;

    [[nodiscard]] size_t getCount() const noexcept {
        return gAllocationCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t getBytes() const noexcept {
        return gAllocatedBytes.load(std::memory_order_relaxed);
    }
};

} // namespace ppf42::test
