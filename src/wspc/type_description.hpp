/*
 *  Copyright (c) 2016 Kajetan Swierk
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#ifndef WSPC_TYPE_DESCRIPTION_HPP_GUARD
#define WSPC_TYPE_DESCRIPTION_HPP_GUARD

#include <kl/ctti.hpp>
#include <kl/enum_reflector.hpp>
#include <kl/enum_traits.hpp>
#include <kl/stream_join.hpp>
#include <kl/index_sequence.hpp>
#include <kl/tuple.hpp>

#include <sstream>
#include <type_traits>

namespace wspc {

template <typename T>
std::string get_type_info();

namespace detail {

template <typename T>
using is_reflectable_enum =
    std::integral_constant<bool, std::is_enum<T>::value &&
                                     kl::enum_traits<T>::support_range>;

void sanitize_html(std::stringstream& strm, const char* in);

struct visitor
{
    explicit visitor(std::stringstream& strm) : strm_{strm} {}

    template <typename FieldInfo>
    void operator()(FieldInfo f)
    {
        if (current_field_index_)
            strm_ << ", ";
        strm_ << "  " << f.name() << " :: ";

        sanitize_html(strm_, kl::ctti::name<typename FieldInfo::type>());

        visit_enum(std::forward<FieldInfo>(f),
                   is_reflectable_enum<typename FieldInfo::type>{});
        ++current_field_index_;
    }

private:
    template <typename FieldInfo>
    void visit_enum(FieldInfo, std::true_type)
    {
        using enum_type = std::remove_cv_t<typename FieldInfo::type>;
        using enum_reflector = kl::enum_reflector<enum_type>;
        using enum_traits = kl::enum_traits<enum_type>;
        strm_ << " (";
        auto joiner = kl::make_outstream_joiner(strm_, ", ");
        for (auto i = enum_traits::min_value(); i < enum_traits::max_value();
             ++i)
        {
            if (const auto str =
                    enum_reflector::to_string(static_cast<enum_type>(i)))
            {
                joiner = str;
            }
        }
        strm_ << ")";
    }

    template <typename FieldInfo>
    void visit_enum(FieldInfo, std::false_type) {}

    std::stringstream& strm_;
    std::size_t current_field_index_{0};
};

template <typename T>
std::string get_type_info_tuple_impl(std::false_type /*is_tuple*/)
{
    std::stringstream strm;
    sanitize_html(strm, kl::ctti::name<T>());
    return strm.str();
}

template <typename T, typename Joiner, std::size_t... Is>
void get_type_info_tuple_elems(Joiner& joiner, kl::index_sequence<Is...>)
{
    using swallow = std::initializer_list<int>;
    (void)swallow{
        (joiner = get_type_info<std::tuple_element_t<Is, T>>(), 0)...};
}

template <typename T>
std::string get_type_info_tuple_impl(std::true_type /*is_tuple*/)
{
    std::stringstream strm;
    strm << "[ ";
    auto joiner = kl::make_outstream_joiner(strm, ", ");
    get_type_info_tuple_elems<T>(joiner, kl::make_tuple_indices<T>{});
    strm << " ]";
    return strm.str();
}

template <typename T>
struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
std::string get_type_info_impl(std::false_type /*is_reflectable*/)
{
    return get_type_info_tuple_impl<T>(is_tuple<T>{});
}

template <typename T>
std::string get_type_info_impl(std::true_type /*is_reflectable*/)
{
    std::stringstream strm;
    strm << kl::ctti::name<T>() << " { ";
    kl::ctti::reflect<T>(visitor{strm});
    strm << " }";
    return strm.str();
}
} // namespace detail

template <typename T>
std::string get_type_info()
{
    return detail::get_type_info_impl<T>(kl::is_reflectable<T>{});
}
} // namespace wspc

#endif
