#pragma once
#include <string>

namespace bco
{

namespace detail
{

template <typename T, typename Enable = void>
T default_value();

template <typename T, std::enable_if_t<std::is_integral_v<T>>>
T default_value()
{
    return 0;
}

template <>
std::string default_value()
{
    return "";
}

template <typename T, typename Enable = void>
T default_value(const T& val);

template <typename T, std::enable_if_t<std::is_integral_v<T>>>
T default_value(const T&)
{
    return 0;
}

template <>
std::string default_value(const std::string&)
{
    return "";
}



} // namespace detail

} // namespace bco
