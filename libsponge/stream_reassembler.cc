#include "stream_reassembler.hh"

#include <iostream>

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _auxillary(), _index_assembled(0), _unassembled(0), _eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string &data, const uint64_t index, const bool eof) {
    uint64_t start_idx = index;
    uint64_t end_idx = index + data.size();
    std::string data_peeled = data;
    decltype(_auxillary.begin()) target_insert = _auxillary.end();
    std::vector<decltype(_auxillary.begin())> target_replace;
    bool valid = true;

    // Invalid data: empty, has been assembled, no capacity.
    if (data.size() == 0 || end_idx <= _index_assembled ||
        _index_assembled + _output.remaining_capacity() - start_idx <= 0)
        valid = false;

    // Part of the data has been assembled.
    if (valid && index < _index_assembled) {
        start_idx = _index_assembled;
        data_peeled = data_peeled.substr(_index_assembled - index);
    }

    // Part of the data should be dropped because there isn't enough space to hold it in the buffer.
    if (valid && end_idx > _index_assembled + _output.remaining_capacity()) {
        data_peeled = data_peeled.substr(0, _index_assembled + _output.remaining_capacity() - start_idx);
        end_idx = _index_assembled + _output.remaining_capacity();
    } else if (eof)
        _eof = true;

    // Peel the data to fit it in the auxillary vector.
    for (auto it = _auxillary.begin(); it != _auxillary.end(); it++) {
        // The data is a subset of another interval held in the auxillary store.
        if (start_idx >= (*it).start && end_idx <= (*it).end) {
            valid = false;
            break;
        }

        // Have intersections.
        if (start_idx <= (*it).end && (*it).start <= start_idx) {
            data_peeled = data_peeled.substr((*it).end - start_idx);
            start_idx += (*it).end - start_idx;
        }
        if (end_idx > (*it).start && (*it).end >= end_idx) {
            data_peeled = data_peeled.substr(0, data_peeled.size() - (end_idx - (*it).start));
            end_idx -= (end_idx - (*it).start);
        }

        // Find a hole to fit the data in.
        if ((*it).end > start_idx && target_insert == _auxillary.end())
            target_insert = it;

        // The data covers an interval included previously.
        if ((*it).start >= start_idx && (*it).end <= end_idx)
            target_replace.push_back(it);
    }

    // If it's a valid data, insert it into the auxillary vector.
    BytesInterval bi(start_idx, end_idx);
    if (valid) {
        bi.data = data_peeled;
        _auxillary.insert(target_insert, bi);
        _unassembled += data_peeled.size();

        for (auto it : target_replace) {
            _unassembled -= (*it).data.size();
            _auxillary.erase(it);
        }
    }

    // Stage the auxillary substrings to the byte stream.
    while (_auxillary.size() && _index_assembled == _auxillary.front().start) {
        BytesInterval &cand = _auxillary.front();
        _output.write(cand.data);
        _unassembled -= cand.data.size();
        _index_assembled = cand.end;
        _auxillary.pop_front();
    }

    if (_auxillary.size() == 0 && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _unassembled == 0; }
