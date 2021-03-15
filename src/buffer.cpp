#include <bco/buffer.h>

namespace bco {

namespace detail {

BufferBase::BufferBase(const std::span<uint8_t> data)
    : buffer_({data.begin(), data.end()})
{
}

BufferBase::BufferBase(std::vector<uint8_t>&& data)
    : buffer_({std::move(data)})
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
        throw std::exception {"Buffer is empty"};
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (chunk.size() + curr_pos > index) {
            return chunk[index - curr_pos];
        }
        curr_pos += chunk.size();
    }
    throw std::exception { "Out of index" };
}

std::vector<std::span<uint8_t>> BufferBase::data()
{
    std::vector<std::span<uint8_t>> slices(buffer_.size());
    for (auto& chunk : buffer_) {
        slices.emplace_back(chunk.data(), chunk.size());
    }
    return std::move(slices);
}

//const std::vector<std::span<uint8_t>> BufferBase::cdata() const
//{
//    std::vector<std::span<uint8_t>> slices(buffer_.size());
//    for (auto& chunk : buffer_) {
//        slices.emplace_back(chunk.data(), chunk.size());
//    }
//    return std::move(slices);
//}



} // namespace detail

Buffer::Buffer()
    : base_(new detail::BufferBase)
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

size_t Buffer::size() const
{
    return base_->size();
}

void Buffer::push_back(const std::span<uint8_t> data, bool new_slice)
{
    base_->push_back(data, new_slice);
}

void Buffer::push_back(std::vector<uint8_t>&& data, bool new_slice)
{
    base_->push_back(std::move(data), new_slice);
}

void Buffer::insert(size_t index, const std::span<uint8_t> data)
{
    base_->insert(index, data);
}

void Buffer::insert(size_t index, std::vector<uint8_t>&& data)
{
    base_->insert(index, std::move(data));
}

uint8_t& Buffer::operator[](size_t index)
{
    return base_->operator[](index);
}

std::vector<std::span<uint8_t>> Buffer::data()
{
    return base_->data();
}

const std::vector<std::span<uint8_t>> Buffer::cdata() const
{
    return base_->data();
}

} // namespace bco
