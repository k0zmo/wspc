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
template <typename T>
void get_type_info(std::stringstream& strm);

namespace detail {

void sanitize_html(std::stringstream& strm, const char* in);

template <typename T>
struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
using is_reflectable_enum =
    kl::bool_constant<std::is_enum<T>::value &&
                      kl::enum_traits<T>::support_range>;

template <typename T>
using is_custom_type_info =
    kl::bool_constant<kl::is_reflectable<T>::value || is_tuple<T>::value ||
                      is_reflectable_enum<T>::value>;

struct type_info_writer
{
    template <typename T, kl::enable_if<kl::is_reflectable<T>> = 0>
    static void write(std::stringstream& strm)
    {
        sanitize_html(strm, kl::ctti::name<T>());
        strm << " { ";
        kl::ctti::reflect<T>(visitor{strm});
        strm << " }";
    }

    template <typename T, kl::enable_if<is_tuple<T>> = 0>
    static void write(std::stringstream& strm)
    {
        write_tuple<T>(strm, kl::make_tuple_indices<T>{});
    }

    template <typename T, kl::enable_if<is_reflectable_enum<T>> = 0>
    static void write(std::stringstream& strm)
    {
        sanitize_html(strm, kl::ctti::name<T>());

        using enum_type = std::remove_cv_t<T>;
        using enum_reflector = kl::enum_reflector<enum_type>;
        using enum_traits = kl::enum_traits<enum_type>;
        strm << " (";
        auto joiner = kl::make_outstream_joiner(strm, ", ");
        for (auto i = enum_traits::min_value(); i < enum_traits::max_value();
             ++i)
        {
            if (const auto str =
                    enum_reflector::to_string(static_cast<enum_type>(i)))
            {
                joiner = str;
            }
        }
        strm << ")";
    }

private:
    struct visitor
    {
        explicit visitor(std::stringstream& strm) : strm_{strm} {}

        template <typename FieldInfo>
        void operator()(FieldInfo f)
        {
            if (current_field_index_)
                strm_ << ", ";
            strm_ << "  ";
            sanitize_html(strm_, f.name());
            strm_ << " :: ";
            get_type_info<typename FieldInfo::type>(strm_);
            ++current_field_index_;
        }

    private:
        std::stringstream& strm_;
        std::size_t current_field_index_{0};
    };

    template <typename T, std::size_t... Is>
    static void write_tuple(std::stringstream& strm, kl::index_sequence<Is...>)
    {
        strm << "[ ";
        auto joiner = kl::make_outstream_joiner(strm, ", ");
        using swallow = std::initializer_list<int>;
        (void)swallow{
            (joiner = get_type_info<std::tuple_element_t<Is, T>>(), 0)...};
        strm << " ]";
    }
};

template <typename T>
void get_type_info_impl(std::stringstream& strm,
                        std::true_type /*is_custom_type_info*/)
{
    type_info_writer::write<T>(strm);
}

template <typename T>
void get_type_info_impl(std::stringstream& strm,
                        std::false_type /*is_custom_type_info*/)
{
    sanitize_html(strm, kl::ctti::name<T>());
}
} // namespace detail

template <typename T>
void get_type_info(std::stringstream& strm)
{
    detail::get_type_info_impl<T>(strm, detail::is_custom_type_info<T>{});
}

template <typename T>
std::string get_type_info()
{
    std::stringstream strm;
    get_type_info<T>(strm);
    return strm.str();
}
} // namespace wspc

#endif
