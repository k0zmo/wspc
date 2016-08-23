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

#ifndef WSPC_TRANSPORT_HPP_GUARD
#define WSPC_TRANSPORT_HPP_GUARD

#include <cstdint>
#include <memory>
#include <cassert>
#include <string>
#include <stdexcept>

namespace wspc {

// Forward declarations
class transport_impl;
class processor;
class broadcaster;

class transport
{
public:
    transport(wspc::processor& processor);
    ~transport();

    void close();

    // Poll based interface
    void accept(std::uint16_t port);
    void poll();

    // Asio's loop based interface
    void run(std::uint16_t port);
    void stop();

    wspc::broadcaster get_broadcaster();
    int num_clients() const;

private:
    std::shared_ptr<wspc::transport_impl> impl_;
};

// Sends given message to all listening clients
class broadcaster
{
public:
    void broadcast(const std::string& payload);

private:
    friend wspc::broadcaster transport::get_broadcaster();

    explicit broadcaster(std::shared_ptr<wspc::transport_impl> svr)
        : impl_{std::move(svr)}
    {
        assert(impl_);
        if (!impl_)
            throw std::runtime_error{"svr is null"};
    }

private:
    std::shared_ptr<transport_impl> impl_;
};

class processor
{
public:
    virtual std::string process_http() = 0;
    virtual std::string process_message(const std::string& payload) = 0;

protected:
    ~processor() = default;
};
} // namespace wspc

#endif
