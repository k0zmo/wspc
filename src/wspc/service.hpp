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

#ifndef WSPC_SERVICE_HPP_GUARD
#define WSPC_SERVICE_HPP_GUARD

#include "wspc/transport.hpp"
#include "wspc/service_handler.hpp"
#include "wspc/type_description.hpp"

#include <kl/ctti.hpp>
#include <kl/json_convert.hpp>

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace wspc {

// RCP service implementation using WebSocket and JSON-based messages
class service : public wspc::processor
{
public:
    service();
    explicit service(std::uint16_t port);

    void run(std::uint16_t port) { transport_->run(port); }
    void update() { transport_->poll(); }
    void close() { transport_->close(); }

    // Broadcast given event for all listening clients
    template <typename Event>
    void broadcast(Event&& event)
    {
        if (transport_->num_clients() == 0)
            return;

        const auto event_json = json11::Json{json11::Json::object{
            {"method", kl::ctti::name<Event>()},
            {"params", kl::to_json(std::forward<Event>(event))}}};
        broadcaster_.broadcast(event_json.dump());
    }

    // Register handler for given, named procedure
    void register_handler(const std::string& procedure_name,
                          wspc::service_handler_ptr handler);

    template <typename Event>
    void register_event()
    {
        event_descriptions_.push_back(get_type_info<Event>());
    }

private:
    // "wspc::processor" interface implementation
    std::string process_http() override;
    std::string process_message(const std::string& payload) override;

private:
    std::unique_ptr<wspc::transport> transport_;
    wspc::broadcaster broadcaster_;
    std::unordered_map<std::string, wspc::service_handler_ptr> handlers_;
    std::vector<std::string> event_descriptions_;
};
} // namespace wspc

#endif
