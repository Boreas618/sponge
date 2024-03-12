#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

using namespace std;

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

    if (data.size() == 0 || end_idx <= _index_assembled ||
        _index_assembled + _output.remaining_capacity() - start_idx <= 0)
        valid = false;

    if (valid && index < _index_assembled) {
        start_idx = _index_assembled;
        data_peeled = data_peeled.substr(_index_assembled - index);
    }

    std::cout << _index_assembled << data_peeled << std::endl;

    if (valid && end_idx > _index_assembled + _output.remaining_capacity()) {
        data_peeled = data_peeled.substr(0, _index_assembled + _output.remaining_capacity() - start_idx);
        end_idx = _index_assembled + _output.remaining_capacity();
    } else if (eof)
        _eof = true;

    for (auto it = _auxillary.begin(); it != _auxillary.end(); it++) {
        if (start_idx >= (*it).start && end_idx <= (*it).end) {
            valid = false;
            break;
        }
        if (start_idx <= (*it).end && (*it).start <= start_idx) {
            data_peeled = data_peeled.substr((*it).end - start_idx);
            start_idx += (*it).end - start_idx;
        }
        if (end_idx > (*it).start && (*it).end >= end_idx) {
            data_peeled = data_peeled.substr(0, data_peeled.size() - (end_idx - (*it).start));
            end_idx -= (end_idx - (*it).start);
        }
        if ((*it).end > start_idx && target_insert == _auxillary.end())
            target_insert = it;
        if ((*it).start >= start_idx && (*it).end <= end_idx)
            target_replace.push_back(it);
    }

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
        _output.write(_auxillary.front().data);
        _unassembled -= _auxillary.front().data.size();
        _index_assembled = _auxillary.front().end;
        _auxillary.pop_front();
    }

    if (_auxillary.size() == 0 && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return {}; }
