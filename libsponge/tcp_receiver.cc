#include "tcp_receiver.hh"

#include <cassert>
#include <iostream>

void TCPReceiver::segment_received(const TCPSegment &seg) {
    bool is_syn = seg.header().syn;
    bool is_fin = seg.header().fin;
    WrappingInt32 seq_no = seg.header().seqno;
    std::string payload = seg.payload().copy();

    if (_state == LISTEN) {
        if (!is_syn)
            return;
        // If there is a payload associated with the SYN segment, the payload should be
        // pushed by the reassembler.
        _state = SYN_RECV;
        _isn = WrappingInt32(seq_no);
    }

    uint64_t abs_seqno = unwrap(seq_no, _isn, _reassembler.assembled_idx());

    // The abs_seqno of 0 is for SYN, which indicates that 0 is an illegal index for a segment without the SYN
    // flag.
    if (abs_seqno == 0 && !is_syn)
        return;

    if (_state == SYN_RECV) {
        if (is_fin)
            _state = FIN_RECV;
        // For the payload associated with the SYN segment, the index for writing is abs_seqno (which is exactly 0).
        // For later payloads, the index is abs_seqno - 1 (check out the handout of lab2).
        _reassembler.push_substring(payload, is_syn ? 0 : abs_seqno - 1, is_fin);
    }

    if (_state == FIN_RECV) {
        // Some previous segments may have been lost.
        if (!_reassembler.stream_out().input_ended())
            _reassembler.push_substring(payload, abs_seqno - 1, is_fin);
        return;
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_state == LISTEN)
        return std::nullopt;
    else if (_reassembler.unassembled_bytes() == 0 && _state == FIN_RECV)
        return wrap(_reassembler.assembled_idx() + 2, _isn);
    else
        return wrap(_reassembler.assembled_idx() + 1, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - (_reassembler.stream_out().buffer_size()); }
