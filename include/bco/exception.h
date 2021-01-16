#pragma once
#include <stdexcept>

namespace bco {

class NetworkException : public std::runtime_error {
public:
    NetworkException(const std::string& message)
        : std::runtime_error { message }
    {
    }
};

} // namespace bco
