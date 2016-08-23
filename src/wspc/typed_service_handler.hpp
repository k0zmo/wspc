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

#include <functional>
#include <memory>
#include <string>

namespace wspc {

template <typename Signature>
class typed_service_handler;

template <typename Response, typename Request>
class typed_service_handler<Response(Request)> : public wspc::service_handler
{
protected:
    using request_type = Request;
    using response_type = Response;

public:
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

namespace detail {

template <typename Func>
struct get_signature
{
    using return_type = typename kl::func_traits<Func>::return_type;
    using arg_type = typename kl::func_traits<Func>::template arg<0>::type;
    using type = return_type(arg_type);
};
} // namespace detail

template <typename Func,
          typename Signature = typename detail::get_signature<Func>::type>
class typed_service_handler_func : public wspc::typed_service_handler<Signature>
{
    using super_type = wspc::typed_service_handler<Signature>;
    using response_type = typename super_type::response_type;
    using request_type = typename super_type::request_type;

public:
    typed_service_handler_func(Func func) : call_{std::move(func)} {}

protected:
    response_type handle(request_type req) override
    {
        return call_(std::forward<request_type>(req));
    }

private:
    std::function<Signature> call_;
};

template <typename Func>
wspc::service_handler_ptr make_service_handler(Func&& func)
{
    return std::make_unique<wspc::typed_service_handler_func<Func>>(
        std::forward<Func>(func));
}
} // namespace wspc

#endif
