#ifndef CRONIOT_LOG_RINGBUFFER_H
#define CRONIOT_LOG_RINGBUFFER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace croniot::log {

// Fixed-capacity write-behind queue: push() never blocks and never fails.
// If a producer outpaces the consumer, the oldest unread record is evicted
// (never a silent drop of new data - see droppedCount()). This is the
// anillo en RAM of the plan: it decouples whoever calls the log hook from
// the flash/SD/radio writers, which are slow and synchronous.
//
// T must be trivially copyable (see LogRecord) - the same buffer shape is
// reused verbatim for the .noinit black box in the platform layer (Fase 2),
// where storing anything with a heap pointer would dangle across a reboot.
template <typename T, std::size_t Capacity>
class RingBuffer {
public:
    static_assert(Capacity > 0, "RingBuffer capacity must be positive");

    void push(const T& value) {
        buf_[writeIndex_ % Capacity] = value;
        ++writeIndex_;
        if (writeIndex_ - readIndex_ > Capacity) {
            // Producer lapped the consumer: the slot we just wrote into
            // held an unread record. Advance the read cursor past it and
            // count the loss - never drop it in silence.
            readIndex_ = writeIndex_ - Capacity;
            ++dropped_;
        }
    }

    bool empty() const { return readIndex_ == writeIndex_; }
    std::size_t size() const { return writeIndex_ - readIndex_; }
    static constexpr std::size_t capacity() { return Capacity; }
    uint32_t droppedCount() const { return dropped_; }

    std::optional<T> pop() {
        if (empty()) return std::nullopt;
        T value = buf_[readIndex_ % Capacity];
        ++readIndex_;
        return value;
    }

private:
    std::array<T, Capacity> buf_{};
    std::size_t writeIndex_ = 0;
    std::size_t readIndex_ = 0;
    uint32_t dropped_ = 0;
};

}  // namespace croniot::log

#endif
