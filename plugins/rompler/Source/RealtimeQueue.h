#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace aod
{

/**
    Bounded, single-producer/single-consumer FIFO ring buffer.

    One slot is deliberately unused so full and empty states are distinct.
*/
template <typename T, std::size_t N>
class SpscQueue
{
    static_assert (N >= 2, "SpscQueue needs one usable slot and one sentinel slot");
    static_assert (std::is_trivially_copyable_v<T>, "SpscQueue values must be trivially copyable");

public:
    [[nodiscard]] bool tryPush (const T& value) noexcept
    {
        const auto write = writeIndex_.load (std::memory_order_relaxed);
        const auto next = increment (write);
        if (next == readIndex_.load (std::memory_order_acquire))
            return false;

        buffer_[write] = value;
        writeIndex_.store (next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool tryPop (T& value) noexcept
    {
        const auto read = readIndex_.load (std::memory_order_relaxed);
        if (read == writeIndex_.load (std::memory_order_acquire))
            return false;

        value = buffer_[read];
        readIndex_.store (increment (read), std::memory_order_release);
        return true;
    }

private:
    [[nodiscard]] static constexpr std::size_t increment (std::size_t index) noexcept
    {
        return (index + 1) % N;
    }

    std::array<T, N> buffer_ {};
    // Apple Silicon uses 128-byte cache lines.  Keeping each producer/consumer
    // index on a separate line avoids false sharing in the audio/UI handoff.
    alignas (128) std::atomic<std::size_t> writeIndex_ { 0 };
    alignas (128) std::atomic<std::size_t> readIndex_ { 0 };
};

} // namespace aod
