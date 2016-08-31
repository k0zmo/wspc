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

namespace wspc {

namespace detail {

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
            const auto resp = handle(req_obj);
            return kl::to_json(resp);
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
            const auto resp_obj = handle(std::move(req_obj));
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
class service_handler_kv_func
    : public service_handler_kv<Signature>
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

template <typename Func>
wspc::service_handler_ptr
    make_service_handler(Func&& func, std::false_type /*is_key_value_arg*/)
{
    return std::make_unique<wspc::detail::service_handler_tup_func<Func>>(
        std::forward<Func>(func));
}

template <typename Func>
wspc::service_handler_ptr
    make_service_handler(Func&& func, std::true_type /*is_key_value_arg*/)
{
    return std::make_unique<wspc::detail::service_handler_kv_func<Func>>(
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
//    <Response(Request)>. Neither Response nor Request cannot be void types
// ### TODO: Lift up these constraints of non-void
template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func)
{
    return detail::make_service_handler(std::forward<Func>(func),
                                        detail::is_key_value_arg<Func>{});
}
} // namespace wspc

#endif
