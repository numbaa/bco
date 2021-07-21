#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <list>

namespace bco {

namespace detail {

class BufferBase {

public:
    BufferBase() = default;
    BufferBase(size_t size);
    BufferBase(const std::span<uint8_t> data);
    BufferBase(std::vector<uint8_t>&& data);
    size_t size() const;
    void push_back(const std::span<const uint8_t> data, bool new_slice = false);
    void push_back(std::vector<uint8_t>&& data, bool new_slice = false);
    void insert(size_t index, const std::span<const uint8_t> data);
    void insert(size_t index, std::vector<uint8_t>&& data);
    uint8_t& operator[](size_t index);
    std::vector<std::span<uint8_t>> data(size_t start, size_t end);
    const std::vector<std::span<const uint8_t>> cdata(size_t start, size_t end) const;

    template <typename T>
    bool read_big_endian_at(size_t index, T& value);
    
    template <typename T>
    bool write_big_endian_at(size_t index, T value);

    template <typename T>
    bool read_little_endian_at(size_t index, T& value);

    template <typename T>
    bool write_little_endian_at(size_t index, T value);

private:
    std::list<std::vector<uint8_t>> buffer_;
};

} // namespace detail


class Buffer {
public:
    Buffer();
    explicit Buffer(size_t size);
    Buffer(const std::span<uint8_t> data);
    Buffer(std::vector<uint8_t>&& data);
    size_t size() const;
    bool is_subbuf() const;
    Buffer subbuf(size_t start, size_t count);
    void push_back(const std::span<const uint8_t> data, bool new_slice = false);
    void push_back(std::vector<uint8_t>&& data, bool new_slice = false);
    void insert(size_t index, const std::span<const uint8_t> data);
    void insert(size_t index, std::vector<uint8_t>&& data);
    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;
    std::vector<std::span<uint8_t>> data();
    const std::vector<std::span<const uint8_t>> data() const;

    template <typename T>
    bool read_big_endian_at(size_t index, T& value);

    template <typename T>
    bool write_big_endian_at(size_t index, T value);

    template <typename T>
    bool read_little_endian_at(size_t index, T& value);

    template <typename T>
    bool write_little_endian_at(size_t index, T value);

private:
    Buffer(size_t start, size_t end, std::shared_ptr<detail::BufferBase> base);

private:
    size_t start_ = 0;
    size_t end_ = std::numeric_limits<size_t>::max();
    std::shared_ptr<detail::BufferBase> base_;
};

} // namespace bco
