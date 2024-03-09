#include "byte_stream.hh"

#include <vector>

// Dummy implementation of a flow-controlled in-memory byte stream.

ByteStream::ByteStream(const size_t cap)
    : capacity(cap), buffer(), read_idx(0), write_idx(0), _input_ended(false), _output_ended(false), _error(false) {
    buffer.resize(cap);
}

size_t ByteStream::write(const std::string &data) {
    size_t cnt = 0;
    for (; cnt < data.size() && remaining_capacity() > 0; write_idx++, cnt++)
        buffer[write_idx % capacity] = data[cnt];
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const {
    size_t i = read_idx;
    size_t cnt = 0;
    std::string peek;

    // There isn't enough content to be copied.
    if (read_idx + len > write_idx)
        return peek;

    for (; i < write_idx && cnt < len; i++, cnt++)
        peek.push_back(buffer[i % capacity]);
    return peek;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (read_idx + len > write_idx) {
        _error = true;
        return;
    }
    read_idx += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t cnt = 0;
    std::string str_read;

    // There isn't enough content to read.
    if (read_idx + len > write_idx) {
        _error = true;
        return str_read;
    }
    for (; read_idx < write_idx && cnt < len; read_idx++, cnt++)
        str_read.push_back(buffer[read_idx % capacity]);
    return str_read;
}

void ByteStream::end_input() {
    if (_input_ended) {
        _error = true;
        return;
    }
    buffer[write_idx % capacity] = -1;
    _input_ended = true;
}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return write_idx - read_idx; }

bool ByteStream::buffer_empty() const { return write_idx == read_idx; }

bool ByteStream::eof() const { return buffer[read_idx % capacity] == -1; }

size_t ByteStream::bytes_written() const { return write_idx; }

size_t ByteStream::bytes_read() const { return read_idx; }

size_t ByteStream::remaining_capacity() const { return capacity - (write_idx - read_idx); }