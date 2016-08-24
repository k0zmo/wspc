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

// Get std::tuple<...> from type_pack<...>
template <typename T>
struct type_pack_to_tuple;

template <typename... Args>
struct type_pack_to_tuple<kl::type_pack<Args...>>
{
    using type = std::tuple<Args...>;
};

// For given Callable returns std::tuple<Args...>
template <typename Func>
struct get_tuple_from_args
{
    using type_pack_type = typename kl::func_traits<Func>::args_type;
    using type = typename type_pack_to_tuple<type_pack_type>::type;
};
} // namespace detail

// Implementation of service_handler that automatically deserializes
// incoming JSON into proper tuple, forwards it to handle() method and finally
// serializes outgoing response.
template <typename Signature>
class typed_service_handler;

template <typename Return, typename... Args>
class typed_service_handler<Return(Args...)> : public wspc::service_handler
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

    // ### TODO
    /*std::string request_description() const override
    {
        return get_type_info<std::decay_t<Request>>();
    }

    std::string response_description() const override
    {
        return get_type_info<std::decay_t<Response>>();
    }*/

protected:
    virtual Return handle(const tuple_type& args) = 0;
};

template <typename Func,
          typename Signature = typename kl::func_traits<Func>::signature_type>
class typed_service_handler_func : public wspc::typed_service_handler<Signature>
{
    using super_type = wspc::typed_service_handler<Signature>;
    using tuple_type = typename super_type::tuple_type;
    using return_type = typename super_type::return_type;

public:
    typed_service_handler_func(Func func) : call_{std::move(func)} {}

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

// Factory for service_handler_func.
// Usage: make_service_handler([&](int a0, double a1, bool a2)
//                             { return ...; });
// Factory function autodetects request's argument and response types. None of
// them can be void.
// ### TODO: Lift up this constraint
template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func)
{
    return std::make_unique<wspc::typed_service_handler_func<Func>>(
        std::forward<Func>(func));
}

// Implementation of service_handler that automatically deserializes
// incoming JSON into proper struct (key-value) and serializes outgoing
// response.
template <typename Signature>
class typed_service_handler_kv;

template <typename Response, typename Request>
class typed_service_handler_kv<Response(Request)> : public wspc::service_handler
{
protected:
    using request_type = Request;
    using response_type = Response;

public:
    typed_service_handler_kv() = default;

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

// Functional wrapper over typed_service_handler_kv
template <typename Func,
          typename Signature = typename kl::func_traits<Func>::signature_type>
class typed_service_handler_kv_func
    : public wspc::typed_service_handler_kv<Signature>
{
    using super_type = wspc::typed_service_handler_kv<Signature>;
    using response_type = typename super_type::response_type;
    using request_type = typename super_type::request_type;

public:
    typed_service_handler_kv_func(Func func) : call_{std::move(func)} {}

protected:
    response_type handle(request_type req) override
    {
        return call_(std::forward<request_type>(req));
    }

private:
    std::function<Signature> call_;
};

// Factory for service_handler_kv_func.
// Usage: make_service_handler_kv([&](const some_request& req)
//                                { return some_response{...}; });
// Factory function autodetects Request and Response types iff lambda is defined
// in terms of <Response(Request)>. Neither Response nor Request cannot be void
// types
// ### TODO: Lift up this constraint
template <typename Func>
wspc::service_handler_ptr make_service_handler_kv(Func&& func)
{
    return std::make_unique<wspc::typed_service_handler_kv_func<Func>>(
        std::forward<Func>(func));
}
} // namespace wspc

#endif
