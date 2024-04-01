#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _current_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return next_seqno_absolute() - _bytes_acked; }

// Suggested practice: specifying the params explicitly (i.e. true/false rather than an expression)
void TCPSender::_send_segment(bool syn, bool fin) {
    // TCP sender writes all of the fields of the TCPSegment that were relevant to the TCPReceiver in Lab 2:
    // namely, the sequence number, the SYN flag, the payload, and the FIN flag.
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    seg.header().syn = syn;
    seg.header().fin = fin;

    // Assign the payload.
    // TODO: the sender should handle the case where SYN segment needs to be sent but receive window size is zero.
    // For now, we simply assume that this case cannot happend.
    if (!syn) {
        size_t payload_len =
            (_window_right - _next_seqno == 0)
                ? 1
                : min(TCPConfig::MAX_PAYLOAD_SIZE,
                      min(static_cast<size_t>(_window_right - _next_seqno), stream_in().buffer_size()));
        seg.payload() = stream_in().read(payload_len);
    }

    _next_seqno += seg.length_in_sequence_space();

    if (!_is_timer_started)
        _is_timer_started = true;

    _segments_out.push(seg);
    _segments_outstanding.push_back(seg);
}

void TCPSender::_handle_closed() {
    if (next_seqno_absolute() != 0) {
        _state = SERROR;
        return;
    }
    bool is_fin = stream_in().input_ended() &&
                  next_seqno_absolute() + stream_in().buffer_size() == stream_in().bytes_written() + 1;
    _send_segment(true, is_fin);
    _state = SYN_SENT;
}

void TCPSender::_handle_transmission() {
    // std::cout <<  _next_seqno << _window_right << std::endl;
    while (_next_seqno < _window_right) {
        if (!stream_in().input_ended() && stream_in().buffer_size() == 0)
            break;

        // SYN hasn't been sent, which is an error in this state.
        if (next_seqno_absolute() == 0) {
            _state = SERROR;
            break;
        }

        // A subtle point here: Don't send FIN by itself if the window is full
        // For instance, consider the bytes "ABC<EOF>" and a window size of 3. This segment won't be marked as 1 even it
        // statifies the first two conditions below.
        bool is_fin = stream_in().input_ended() &&
                      next_seqno_absolute() + stream_in().buffer_size() == stream_in().bytes_written() + 1 &&
                      next_seqno_absolute() + stream_in().buffer_size() < _window_right;

        _send_segment(false, is_fin);
        if (is_fin) {
            _state = FIN_SENT;
            break;
        }
    }

    if (_next_seqno == _window_right)
        std::cout << _state << std::endl;
}

void TCPSender::fill_window() {
    if (_state == CLOSED)
        _handle_closed();

    if (_state == SYN_ACKED)
        _handle_transmission();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // Defensive programming: an invalid ackno will simply be abandoned.
    if (unwrap(ackno, _isn, _bytes_acked) > next_seqno_absolute())
        return;

    // Only reset the timer if a new segment has been acked.
    if (unwrap(ackno, _isn, _bytes_acked) > _bytes_acked) {
        _bytes_acked = unwrap(ackno, _isn, _bytes_acked);
        _timer_million_seconds = 0;
        _current_retransmission_timeout = _initial_retransmission_timeout;
        _retransmission_times = 0;
    }

    for (auto it = _segments_outstanding.begin(); it < _segments_outstanding.end(); it++) {
        uint64_t abs_ackno = unwrap(ackno, _isn, _bytes_acked);
        uint64_t abs_outstanding_seg_right =
            unwrap((*it).header().seqno + (*it).length_in_sequence_space(), _isn, _bytes_acked);
        if (abs_outstanding_seg_right <= abs_ackno) {
            if ((*it).header().syn)
                _state = SYN_ACKED;
            if ((*it).header().fin)
                _state = FIN_ACKED;
            _segments_outstanding.erase(it);
        }
    }

    // Reset the timer. Specially, if all of the outstanding segments have been acknowledged, stop the timer.
    if (!_segments_outstanding.size())
        _is_timer_started = false;

    // The TCPSender should fill the window again if new space has opened up.
    _window_right = unwrap(ackno, _isn, _window_right) + static_cast<uint64_t>(window_size);

    if (_window_right > _next_seqno)
        fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_is_timer_started)
        return;

    _timer_million_seconds += ms_since_last_tick;
    if (_timer_million_seconds >= _current_retransmission_timeout) {
        if (_retransmission_times <= TCPConfig::MAX_RETX_ATTEMPTS) {
            _timer_million_seconds = 0;
            _current_retransmission_timeout *= 2;
            _retransmission_times++;
            _segments_out.push(_segments_outstanding.front());
        } else {
            // The number of consecutive retransmissions exceeds MAX_RETX_ATTEMPTS.
            // The connection should be aborted.
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_times; }

void TCPSender::send_empty_segment() {}