#pragma once

#include <atomic>
#include <cstddef>

namespace x10::instrument::test
{

/**
    Counts every operator new/delete call in this process, so a test can prove
    a "no allocation" claim instead of asserting it in a comment.

    RegionIndex::match() is documented as safe to call from the audio thread
    specifically because it never allocates. That claim is easy to get wrong
    silently — a std::vector copy or an implicit std::string construction
    slipped into a refactor would not fail any functional test, only this one.

    The counter is defined in AllocationGuard.cpp, along with the global
    operator new/delete overrides that drive it. Clang rejects a replacement
    operator new/delete declared inline (-Winline-new-delete), so those must
    live in a single translation unit rather than here; this header only
    declares the read side.

    Global new/delete overrides are process-wide once linked in, which is fine
    for a dedicated test binary: Catch2's own allocations happen outside the
    measured window, since tests only compare the counter's delta across a
    specific call, not its absolute value.
*/
std::atomic<std::size_t>& allocationCount() noexcept;

class AllocationScope
{
public:
    AllocationScope() noexcept;

    [[nodiscard]] std::size_t allocationsSoFar() const noexcept;

private:
    std::size_t before_;
};

} // namespace x10::instrument::test
