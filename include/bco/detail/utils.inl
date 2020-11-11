
namespace bco
{

namespace detail
{

template <typename T = void>
inline T default_value();

template <typename T>
inline T default_value<std::enable_if_t<std::is_integral_v<T>, T>>()
{
    return 0;
}

template <>
inline std::string default_value<std::string>()
{
    return "";
}

} // namespace detail

} // namespace bco
