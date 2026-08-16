#include "AllocationGuard.h"

#include <cstdlib>
#include <new>

namespace x10::instrument::test
{

std::atomic<std::size_t>& allocationCount() noexcept
{
    static std::atomic<std::size_t> count { 0 };
    return count;
}

AllocationScope::AllocationScope() noexcept : before_ (allocationCount().load (std::memory_order_relaxed)) {}

std::size_t AllocationScope::allocationsSoFar() const noexcept
{
    return allocationCount().load (std::memory_order_relaxed) - before_;
}

} // namespace x10::instrument::test

// Global replacement operators, deliberately not inline (Clang rejects an
// inline replacement operator new/delete). This links only into the test
// binary, never into x10_instrument itself.
void* operator new (std::size_t size)
{
    x10::instrument::test::allocationCount().fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size))
        return p;
    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

void* operator new[] (std::size_t size)
{
    x10::instrument::test::allocationCount().fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size))
        return p;
    throw std::bad_alloc();
}

void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
