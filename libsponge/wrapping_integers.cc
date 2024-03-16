#include "wrapping_integers.hh"

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + uint32_t(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t offset;
    if (n.raw_value() < isn.raw_value())
        offset = UINT32_MAX + 1 - (isn.raw_value() - n.raw_value());
    else
        offset = n.raw_value() - isn.raw_value();

    uint64_t offset_cast = static_cast<uint64_t>(offset);

    // Since 64-bit absolute offsets wrap around at 2^32, the true offset can be expressed as k * 2^32 + offset, where k
    // is determined by the checkpoint as described above.

    uint64_t ckpt_rounded_down = (checkpoint) & 0xfffffffe00000000;
    uint64_t cand_0;
    uint64_t cand_1 = ckpt_rounded_down + offset_cast;

    if (checkpoint < offset_cast)
        return offset_cast;
    while (cand_1 <= checkpoint)
        cand_1 += (uint64_t)UINT32_MAX + 1;
    cand_0 = cand_1 - ((uint64_t)UINT32_MAX + 1);

    if (checkpoint - cand_0 <= cand_1 - checkpoint)
        return cand_0;
    else
        return cand_1;
}
