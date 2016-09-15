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

#ifndef WSPC_TYPE_SERVICE_HANDLER_HPP_GUARD
#define WSPC_TYPE_SERVICE_HANDLER_HPP_GUARD

#include "wspc/service_handler.hpp"
#include "wspc/type_description.hpp"

#include <kl/json_convert.hpp>
#include <kl/type_traits.hpp>
#include <kl/tuple.hpp>

#include <functional>
#include <memory>
#include <string>

#if defined(_MSC_VER)
#  pragma warning(push)
   // MSVC complains about using comma inside []
#  pragma warning(disable: 4709)
#endif

namespace wspc {
namespace detail {
// https://www.reddit.com/r/cpp/comments/52o99u/a_generic_workaround_for_voidreturning_functions/
// Basically, the overloaded comma operator has an interesting property in that
// it can take a parameter of type void. If it is the case, then the built-in
// comma operator is used.
struct empty_response_t
{
    constexpr auto operator[](empty_response_t) const
    {
        return empty_response_t{};
    }
    
    template <typename T>
    constexpr T&& operator[](T&& t) const
    { 
        return std::forward<T>(t);
    }

    template <typename T>
    constexpr friend T&& operator,(T&& t, empty_response_t)
    {
        return std::forward<T>(t);
    }
};

static constexpr auto empty_response = empty_response_t{};
} // namespace detail
} // namespace wspc

namespace kl {
template <>
json11::Json to_json(const wspc::detail::empty_response_t&)
{
    return json11::Json::array{};
}
} // namespace kl

namespace wspc {

namespace detail {

// Implementation of service handler that doesn't read any request data (i.e its
// argument is void type), calls appropriate handle() method and finally
// serializes outgoing response.
template <typename Return>
class service_handler_void : public wspc::service_handler
{
protected:
    using return_type = Return;

public:
    json11::Json operator()(const json11::Json&) override
    {
        // If handle() returns a non-void, overloaded operator comma kicks in
        // returning what handle() returns in the process. Otherwise built-in
        // comma operator is used resulting in resp_obj to be of type
        // empty_response_t.
        const auto resp_obj = empty_response[handle(), empty_response];
        return kl::to_json(resp_obj);
    }

    std::string request_description() const override
    {
        return "void";
    }

    std::string response_description() const override
    {
        return get_type_info<return_type>();
    }

protected:
    virtual Return handle() = 0;
};

// Functional wrapper over service_handler_void
template <typename Func,
          typename Return = typename kl::func_traits<Func>::return_type>
class service_handler_void_func : public service_handler_void<Return>
{
    using super_type = service_handler_void<Return>;
    using return_type = typename super_type::return_type;

public:
    service_handler_void_func(Func func) : call_{std::move(func)} {}

protected:
    return_type handle() override { return call_(); }

private:
    std::function<return_type()> call_;
};

// Implementation of service_handler that automatically deserializes
// incoming JSON into proper tuple, forwards it to handle() method and finally
// serializes outgoing response.
template <typename Signature>
class service_handler_tup;

template <typename Return, typename... Args>
class service_handler_tup<Return(Args...)> : public wspc::service_handler
{
protected:
    using tuple_type = std::tuple<Args...>;
    using return_type = Return;

public:
    json11::Json operator()(const json11::Json& request) override
    {
        try
        {
            auto req_obj = kl::from_json<tuple_type>(request);
            const auto resp_obj =
                empty_response[handle(req_obj), empty_response];
            return kl::to_json(resp_obj);
        }
        catch (kl::json_deserialize_exception& ex)
        {
            using namespace std::string_literals;
            throw invalid_parameters_exception{"invalid method params: "s +
                                               ex.what()};
        }
    }

    std::string request_description() const override
    {
        return get_type_info<tuple_type>();
    }

    std::string response_description() const override
    {
        return get_type_info<return_type>();
    }

protected:
    virtual Return handle(const tuple_type& args) = 0;
};

// Functional wrapper over service_handler_tup
template <typename Func,
          typename Signature = typename kl::func_traits<Func>::signature_type>
class service_handler_tup_func : public service_handler_tup<Signature>
{
    using super_type = service_handler_tup<Signature>;
    using tuple_type = typename super_type::tuple_type;
    using return_type = typename super_type::return_type;

public:
    service_handler_tup_func(Func func) : call_{std::move(func)} {}

protected:
    return_type handle(const tuple_type& args) override
    {
        return kl::tuple::apply_fn::call(
            args, [&](const auto&... args) {
                return this->call_(args...);
            });
    }

private:
    std::function<Signature> call_;
};

// Implementation of service_handler that automatically deserializes
// incoming JSON into proper struct (key-value) and serializes outgoing
// response.
template <typename Signature>
class service_handler_kv;

template <typename Response, typename Request>
class service_handler_kv<Response(Request)> : public wspc::service_handler
{
protected:
    using request_type = Request;
    using response_type = Response;

public:
    service_handler_kv() = default;

    json11::Json operator()(const json11::Json& request) override
    {
        try
        {
            auto req_obj = kl::from_json<std::decay_t<Request>>(request);
            const auto resp_obj =
                empty_response[handle(std::move(req_obj)), empty_response];
            return kl::to_json(resp_obj);
        }
        catch (kl::json_deserialize_exception& ex)
        {
            using namespace std::string_literals;
            throw invalid_parameters_exception{"invalid method params: "s +
                                               ex.what()};
        }
    }

    std::string request_description() const override
    {
        return get_type_info<std::decay_t<Request>>();
    }

    std::string response_description() const override
    {
        return get_type_info<std::decay_t<Response>>();
    }

protected:
    virtual Response handle(Request req) = 0;
};

// Functional wrapper over service_handler_kv
template <typename Func,
          typename Signature = typename kl::func_traits<Func>::signature_type>
class service_handler_kv_func : public service_handler_kv<Signature>
{
    using super_type = service_handler_kv<Signature>;
    using response_type = typename super_type::response_type;
    using request_type = typename super_type::request_type;

public:
    service_handler_kv_func(Func func) : call_{std::move(func)} {}

protected:
    response_type handle(request_type req) override
    {
        return call_(std::forward<request_type>(req));
    }

private:
    std::function<Signature> call_;
};

template <typename Func>
using decayed_first_arg =
    std::decay_t<typename kl::func_traits<Func>::template arg<0>::type>;

// There's a little ambiguity with classifying functions with only one argument
// when it binds to reflectable struct with no fields declared.
// To overcome this we check for referenceness of a first  argument as
// references (const or no-const) can't be part of a (de-)serialized tuple.
// To force using key-value with a struct with no (reflectable) fields defined
// make sure to define the argument as a [const] reference
template <typename Func>
using is_first_arg_reference =
    std::integral_constant<bool, std::is_reference<typename kl::func_traits<
                                     Func>::template arg<0>::type>::value>;

// One argument only and this argument is reflectable
template <typename Func>
using is_key_value_arg = std::integral_constant<
    bool, kl::func_traits<Func>::arity == 1 &&
              (kl::is_reflectable<decayed_first_arg<Func>>::value &&
               ((kl::ctti::total_num_fields<decayed_first_arg<Func>>() > 0) ||
                is_first_arg_reference<Func>::value))>;

// So tuple 'branch' with take over when:
// - There's more than one argument to given function Func or
// - That argument is not reflectable (such as int) or
// - it has no defined reflectable fields (using KL_DEFINE_REFLECTABLE macro)
//   and its not declared as a reference

// Tag-dispatching based on request's type category: tuple, key-value or parameterless
enum class request_category { tuple, key_value, void_ };
using tuple_type =
    std::integral_constant<request_category, request_category::tuple>;
using key_value_type =
    std::integral_constant<request_category, request_category::key_value>;
using void_type =
    std::integral_constant<request_category, request_category::void_>;

template <typename Func, typename = kl::void_t<>>
struct get_request_type : std::conditional_t<is_key_value_arg<Func>::value,
                                             key_value_type, tuple_type>
{};
template <typename Func>
struct get_request_type<
    Func, kl::void_t<std::enable_if_t<kl::func_traits<Func>::arity == 0>>>
    : void_type
{};

template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func, tuple_type)
{
    return std::make_unique<wspc::detail::service_handler_tup_func<Func>>(
        std::forward<Func>(func));
}

template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func, key_value_type)
{
    return std::make_unique<wspc::detail::service_handler_kv_func<Func>>(
        std::forward<Func>(func));
}

template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func, void_type)
{
    return std::make_unique<wspc::detail::service_handler_void_func<Func>>(
        std::forward<Func>(func));
}
} // namespace detail

// Factory for service_handler_func or service_handler_kv_func.
// Usage: make_service_handler([&](int a0, double a1, bool a2)
//                             { return ...; });
// Usage: make_service_handler_kv([&](const some_request& req)
//                                { return some_response{...}; });
// Factory function autodetects:
//  - request's argument and response types. None of them can be void.
//  - Request and Response types iff lambda is defined in terms of
//    <Response(Request)>. 
template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func)
{
    return detail::make_service_handler(std::forward<Func>(func),
                                        detail::get_request_type<Func>{});
}
} // namespace wspc

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

#endif
