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

#ifndef WSPC_SERVICE_HANDLER_HPP_GUARD
#define WSPC_SERVICE_HANDLER_HPP_GUARD

#include <string>
#include <memory>
#include <stdexcept>

// Forward declaration
namespace json11 { class Json; }

namespace wspc {

// Base class for named handlers for RPC service
class service_handler
{
public:
    virtual ~service_handler();
    virtual json11::Json operator()(const json11::Json& request) = 0;

    virtual std::string request_description() const;
    virtual std::string response_description() const;
};

using service_handler_ptr = std::unique_ptr<service_handler>;

struct invalid_parameters_exception : std::domain_error
{
    explicit invalid_parameters_exception(const std::string& message)
        : std::domain_error{message}
    {
    }
};
} // namespace wspc

#endif
