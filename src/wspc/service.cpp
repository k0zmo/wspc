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

#include "wspc/service.hpp"

#include <exception>
#include <sstream>

namespace wspc {

service::service()
    : transport_{std::make_unique<wspc::transport>(*this)},
      broadcaster_{transport_->get_broadcaster()}
{
}

service::service(std::uint16_t port)
    : service{}
{
    transport_->accept(port);
}

std::string service::process_http()
{
    std::stringstream ss;
    ss <<
R"(<!doctype html>
<html><head><title>WebSocket Test Service</title></head>
<body><p>List of supported remote procedures: </p>
<ul>)";

    for (const auto& kv : handlers_)
    {
        ss << "<li>" << kv.first << ": </li>\n";
        ss << "<ul><li>takes: " << kv.second->request_description()
           << "</li>\n";
        ss << "<li>returns: " << kv.second->response_description()
           << "</li></ul>\n";
    }

    ss << "</ul>\n<p>List of supported notifications: </p><ul>\n";
    for (const auto& desc : event_descriptions_)
        ss << "<li>" << desc << "</li>\n";

    ss << "</ul>\n</body></html>";

    return ss.str();
}

namespace {

enum class fault_code
{
    parse_error = -32700,
    invalid_request = -32600,
    method_not_found = -32601,
    invalid_params = -32602,
    internal_error = -32603
};

json11::Json make_error_response(const json11::Json& id, fault_code code,
                                 const std::string& error_message)
{
    return json11::Json::object{
        {"id", id},
        {"error", json11::Json::object{{"code", static_cast<int>(code)},
                                       {"message", error_message}}}};
}

std::string wrap_response(const json11::Json& response)
{
    static std::string empty;
    return !response["id"].is_null() ? response.dump() : empty;
}
} // namespace anonymous

std::string service::process_message(const std::string& payload)
{
    std::string err;
    json11::Json json{json11::Json::parse(payload, err)};

    if (!err.empty())
    {
        return make_error_response(nullptr, fault_code::parse_error, err)
            .dump();
    }
    // Check for existance of 'method' string value
    if (!json.has_shape({{"method", json11::Json::STRING}}, err))
    {
        return make_error_response(nullptr, fault_code::invalid_request, err)
            .dump();
    }

    const auto& id = json["id"];
    const auto& method = json["method"].string_value();

    auto handler_ = handlers_.find(method);
    if (handler_ == end(handlers_))
    {
        return wrap_response(make_error_response(
            id, fault_code::method_not_found, "procedure not found"));
    }

    try
    {
        auto& handler = *handler_->second;
        const auto& params = json["params"];
        // If params is an object we treat them as a struct (we can get fields
        // names in reflectable struct in contrast to function/lambdas
        // arguments)
        if (params.is_object())
        {
            return wrap_response(
                json11::Json::object{{"result", handler(params)}, {"id", id}});
        }
        else if (params.is_array())
        {
            static const auto empty_json_object =
                json11::Json{json11::Json::object{}};
            const auto& arr = params.array_items();

            return wrap_response(json11::Json::object{
                {"result",
                 handler(arr.size() == 1 ? arr[0] : empty_json_object)},
                {"id", id}});
        }
        else
        {
            return wrap_response(make_error_response(
                id, fault_code::invalid_params,
                "wrong type of 'params' - expceted array or object"));
        }
    }
    catch (invalid_parameters_exception& ex)
    {
        return wrap_response(
            make_error_response(id, fault_code::invalid_params, ex.what()));
    }
    catch (std::exception& ex)
    {
        return wrap_response(
            make_error_response(id, fault_code::internal_error, ex.what()));
    }
}

void service::register_handler(const std::string& procedureName,
                               wspc::service_handler_ptr handler)
{
    handlers_[procedureName] = std::move(handler);
}
} // namespace wspc
