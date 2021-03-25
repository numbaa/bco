#include <bco/buffer.h>
#include <bco/utils.h>
#include "magic.h"

namespace bco {

namespace detail {

void BCO_UNIQUE_FUNC(bco_buffer)
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    bco::Buffer buf;
    buf.read_big_endian_at(0, u8);
    buf.read_big_endian_at(0, u16);
    buf.read_big_endian_at(0, u32);
    buf.read_big_endian_at(0, u64);
    buf.read_little_endian_at(0, u8);
    buf.read_little_endian_at(0, u16);
    buf.read_little_endian_at(0, u32);
    buf.read_little_endian_at(0, u64);
    buf.write_big_endian_at(0, u8);
    buf.write_big_endian_at(0, u16);
    buf.write_big_endian_at(0, u32);
    buf.write_big_endian_at(0, u64);
    buf.write_little_endian_at(0, u8);
    buf.write_little_endian_at(0, u16);
    buf.write_little_endian_at(0, u32);
    buf.write_little_endian_at(0, u64);
}

BufferBase::BufferBase(size_t size)
    : buffer_({ std::vector<uint8_t>(size) })
{
}
BufferBase::BufferBase(const std::span<uint8_t> data)
    : buffer_({ data.begin(), data.end() })
{
}

BufferBase::BufferBase(std::vector<uint8_t>&& data)
    : buffer_({ std::move(data) })
{
}

size_t BufferBase::size() const
{
    size_t s = 0;
    for (const auto& chunk : buffer_) {
        s += chunk.size();
    }
    return s;
}

void BufferBase::push_back(const std::span<uint8_t> data, bool new_slice)
{
    if (new_slice || buffer_.empty()) {
        buffer_.push_back(std::vector<uint8_t> { data.begin(), data.end() });
    } else {
        auto& back = buffer_.back();
        size_t old_size = back.size();
        back.resize(old_size + data.size());
        ::memcpy(back.data() + old_size, data.data(), data.size());
    }
}

void BufferBase::push_back(std::vector<uint8_t>&& data, bool new_slice)
{
    if (new_slice || buffer_.empty()) {
        buffer_.push_back(std::move(data));
    } else {
        auto& back = buffer_.back();
        size_t old_size = back.size();
        back.resize(old_size + data.size());
        ::memcpy(back.data() + old_size, data.data(), data.size());
    }
}

void BufferBase::insert(size_t index, const std::span<uint8_t> data)
{
    size_t buffer_size = size();
    if (index == buffer_size) {
        push_back(data, true);
        return;
    } else {
        size_t curr_pos = 0;
        for (auto it = buffer_.begin(); it != buffer_.end(); ++it) {
            if (curr_pos == index) {
                buffer_.insert(it, std::vector<uint8_t>(data.begin(), data.end()));
                return;
            } else if (it->size() + curr_pos > index) {
                it->insert(it->begin() + index - curr_pos, data.begin(), data.end());
                return;
            } else {
                curr_pos += it->size();
            }
        }
    }
}

void BufferBase::insert(size_t index, std::vector<uint8_t>&& data)
{
    size_t buffer_size = size();
    if (index == buffer_size) {
        push_back(data, true);
        return;
    } else {
        size_t curr_pos = 0;
        for (auto it = buffer_.begin(); it != buffer_.end(); ++it) {
            if (curr_pos == index) {
                buffer_.insert(it, std::move(data));
                return;
            } else if (it->size() + curr_pos > index) {
                //optimize
                it->insert(it->begin() + index - curr_pos, data.begin(), data.end());
                return;
            } else {
                curr_pos += it->size();
            }
        }
    }
}

uint8_t& BufferBase::operator[](size_t index)
{
    if (buffer_.empty())
        throw std::exception { "Buffer is empty" };
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (chunk.size() + curr_pos > index) {
            return chunk[index - curr_pos];
        }
        curr_pos += chunk.size();
    }
    throw std::exception { "Out of index" };
}

std::vector<std::span<uint8_t>> BufferBase::data(size_t start, size_t end)
{
    //TODO: 测试这个函数
    std::vector<std::span<uint8_t>> slices(buffer_.size());
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (curr_pos >= end)
            break;
        if (curr_pos == start) {
            slices.emplace_back(chunk.data(), chunk.size());
        } else if (curr_pos + chunk.size() <= start) {
            ;
        } else if (curr_pos < start && curr_pos + chunk.size() > start) {
            //比较 end 和 curr_pos + chunk.size()的大小，取小的
            slices.emplace_back(chunk.data() + start - curr_pos, start - curr_pos);
        } else if (curr_pos > start && curr_pos + chunk.size() <= end) {
            slices.emplace_back(chunk.data(), chunk.size());
        } else if (curr_pos > start && curr_pos + chunk.size() > end) {
            slices.emplace_back(chunk.data(), end - curr_pos);
        } else {
            std::abort();
        }
        curr_pos += chunk.size();
    }
    return slices;
}

//以下几个函数由使用者保证正确调用，不再做越界判断

template <typename T>
inline bool BufferBase::read_big_endian_at(size_t index, T& value)
{
    read_big_endian(&operator[](index), value);
    return true;
}

template <typename T>
bool BufferBase::write_big_endian_at(size_t index, T value)
{
    write_big_endian(&operator[](index), value);
    return true;
}

template <typename T>
bool BufferBase::read_little_endian_at(size_t index, T& value)
{
    read_little_endian(&operator[](index), value);
    return true;
}

template <typename T>
bool BufferBase::write_little_endian_at(size_t index, T value)
{
    write_little_endian(&operator[](index), value);
    return true;
}

} // namespace detail

Buffer::Buffer()
    : base_(new detail::BufferBase)
{
}

Buffer::Buffer(size_t size)
    : base_(new detail::BufferBase { size })
{
}

Buffer::Buffer(const std::span<uint8_t> data)
    : base_(new detail::BufferBase { data })
{
}

Buffer::Buffer(std::vector<uint8_t>&& data)
    : base_(new detail::BufferBase { std::move(data) })
{
}

Buffer::Buffer(size_t start, size_t end, std::shared_ptr<detail::BufferBase> base)
    : start_(start)
    , end_(end)
    , base_(base)
{
}

size_t Buffer::size() const
{
    if (is_subbuf()) {
        return end_ - start_;
    } else {
        return base_->size();
    }
}

bool Buffer::is_subbuf() const
{
    return not(start_ == 0 && end_ == std::numeric_limits<decltype(end_)>::max());
}

Buffer Buffer::subbuf(size_t start, size_t end)
{
    return Buffer { start, end, base_ };
}

void Buffer::push_back(const std::span<uint8_t> data, bool new_slice)
{
    if (is_subbuf()) {
        throw std::exception { "Unsupported function" };
    }
    base_->push_back(data, new_slice);
}

void Buffer::push_back(std::vector<uint8_t>&& data, bool new_slice)
{
    if (is_subbuf()) {
        throw std::exception { "Unsupported function" };
    }
    base_->push_back(std::move(data), new_slice);
}

void Buffer::insert(size_t index, const std::span<uint8_t> data)
{
    if (is_subbuf()) {
        throw std::exception { "Unsupported function" };
    }
    base_->insert(index, data);
}

void Buffer::insert(size_t index, std::vector<uint8_t>&& data)
{
    if (is_subbuf()) {
        throw std::exception { "Unsupported function" };
    }
    base_->insert(index, std::move(data));
}

uint8_t& Buffer::operator[](size_t index)
{
    if (is_subbuf() && index >= (end_ - start_)) {
        throw std::exception { "Out of index" };
    }
    return base_->operator[](index + start_);
}

const uint8_t& Buffer::operator[](size_t index) const
{
    if (is_subbuf() && index >= (end_ - start_)) {
        throw std::exception { "Out of index" };
    }
    return base_->operator[](index + start_);
}

std::vector<std::span<uint8_t>> Buffer::data()
{
    return base_->data(start_, end_);
}

const std::vector<std::span<uint8_t>> Buffer::data() const
{
    return base_->data(start_, end_);
}

template <typename T>
bool bco::Buffer::read_big_endian_at(size_t index, T& value)
{
    return base_->read_big_endian_at(index + start_, value);
}

template <typename T>
bool bco::Buffer::write_big_endian_at(size_t index, T value)
{
    return base_->write_big_endian_at(index + start_, value);
}

template <typename T>
bool bco::Buffer::read_little_endian_at(size_t index, T& value)
{
    return base_->read_little_endian_at(index + start_, value);
}

template <typename T>
bool bco::Buffer::write_little_endian_at(size_t index, T value)
{
    return base_->write_little_endian_at(index + start_, value);
}

} // namespace bco
