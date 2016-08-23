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

#include "wspc/transport.hpp"

#if !defined(_MSC_VER) || _MSC_VER >= 1900
#  define _WEBSOCKETPP_NOEXCEPT_
#endif
#define _WEBSOCKETPP_CPP11_CHRONO_
#define _WEBSOCKETPP_CPP11_THREAD_
#define _WEBSOCKETPP_CPP11_FUNCTIONAL_
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_MEMORY_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <set>

namespace wspc {

using server_backend = websocketpp::config::asio;
using asio_server = websocketpp::server<server_backend>;

class transport_impl
{
public:
    transport_impl(wspc::processor& processor)
        : processor_{&processor}
    {
        server_.init_asio();

        server_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            connections_.insert(hdl);
        });

        server_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            connections_.erase(hdl);
        });

        server_.set_message_handler([this](websocketpp::connection_hdl hdl,
                                           asio_server::message_ptr msg) {
            auto resp = processor_->process_message(msg->get_payload());
            if (!resp.empty())
            {
                std::error_code ignored_ec;
                server_.send(hdl, resp, websocketpp::frame::opcode::text,
                             ignored_ec);
            }
        });

        server_.set_http_handler([this](websocketpp::connection_hdl hdl) {
            asio_server::connection_ptr con = server_.get_con_from_hdl(hdl);
            try
            {
                con->set_body(processor_->process_http());
                con->set_status(websocketpp::http::status_code::ok);
            }
            catch (std::exception& ex)
            {
                con->set_body(ex.what());
                con->set_status(
                    websocketpp::http::status_code::internal_server_error);
            }
        });
    }

    ~transport_impl() = default;

    void close()
    {
        for (auto& hdl : connections_)
        {
            asio_server::connection_ptr con = server_.get_con_from_hdl(hdl);
            con->close(websocketpp::close::status::service_restart,
                       "connection closed");
        }
    }

    void accept(std::uint16_t port)
    {
        if (port_ != 0)
            return;
        server_.listen(port);
        server_.start_accept();
        port_ = port;
    }

    void poll()
    {
        server_.poll();
    }

    void run(std::uint16_t port)
    {
        if (port_ != 0)
            return;
        accept(port);

        server_.run();
    }

    void stop()
    {
        server_.stop();
    }

    void broadcast(const std::string& payload)
    {
        for (auto& hdl : connections_)
            server_.send(hdl, payload, websocketpp::frame::opcode::text);
    }

    int num_clients() const
    {
        return connections_.size();
    }

private:
    wspc::processor* processor_;
    asio_server server_;
    std::set<websocketpp::connection_hdl,
             std::owner_less<websocketpp::connection_hdl>>
        connections_;
    std::uint16_t port_{0};
};

transport::transport(wspc::processor& processor)
    : impl_{std::make_shared<wspc::transport_impl>(processor)}
{
}

transport::~transport() = default;

void transport::accept(std::uint16_t port) { impl_->accept(port); }

void transport::poll() { impl_->poll(); }

void transport::close() { impl_->close(); }

void transport::run(std::uint16_t port) { impl_->run(port); }

void transport::stop() { impl_->stop(); }

wspc::broadcaster transport::get_broadcaster()
{
    return wspc::broadcaster{impl_};
}

int transport::num_clients() const { return impl_->num_clients(); }

void broadcaster::broadcast(const std::string& payload)
{
    impl_->broadcast(payload);
}
} // namespace wspc
