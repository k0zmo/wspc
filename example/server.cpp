#include "wspc/service.hpp"
#include "wspc/typed_service_handler.hpp"

#include <kl/ctti.hpp>
#include <kl/enum_reflector.hpp>
#include <kl/enum_traits.hpp>

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include <thread>
#include <string>
#include <iostream>
#include <chrono>
#include <string>
#include <stdexcept>

enum class operation
{
    add = 1,
    subtract,
    multiply,
    divide
};
KL_DEFINE_ENUM_REFLECTOR(operation, (
    add, subtract, multiply, divide
))

namespace kl {
template <>
struct enum_traits<operation>
    : enum_trait_support_range<operation, operation::add, operation::divide,
                               false>
{
};
} // namespace kl

struct work_request
{
    double arg1;
    double arg2;
    operation op;
    boost::optional<std::string> comment;
};
KL_DEFINE_REFLECTABLE(work_request, (
    arg1, arg2, op, comment
))

// Just to get prettier output on type_description
KL_DEFINE_REFLECTABLE(std::string, _)
KL_DEFINE_REFLECTABLE(boost::optional<std::string>, _)

struct work_response
{
    double result;
};
KL_DEFINE_REFLECTABLE(work_response, (
    result
))

struct ping_request {};
KL_DEFINE_REFLECTABLE(ping_request, _)

struct pong_response
{
    std::string response;
    unsigned tick;
};
KL_DEFINE_REFLECTABLE(pong_response, (
    response, tick
))

struct ping_event
{
    unsigned tick;
};
KL_DEFINE_REFLECTABLE(ping_event, (
    tick
))

int main()
{
    wspc::service service;

    service.register_handler(
        "calculate2",
        // Can't use boost::optional<> here since to_json() counts
        // array elements and bail outs if it doesn't match with a tuple's size
        wspc::make_service_handler([](double arg1, double arg2, operation op) {
            std::cout << "calculate(" << arg1 << ", " << arg2 << ", op: "
                      << kl::enum_reflector<operation>::to_string(op) << ")\n";

            const auto ret = [&] {
                switch (op)
                {
                case operation::add:
                    return arg1 + arg2;
                case operation::subtract:
                    return arg1 - arg2;
                case operation::multiply:
                    return arg1 * arg2;
                case operation::divide:
                    return arg1 / arg2;
                }
                throw std::logic_error{"internal error"};
            }();

            std::cout << "result: " << ret << std::endl;
            return ret;
        }));

    service.register_handler(
        "ping",
        wspc::make_service_handler([](const ping_request&) {
            using namespace std::string_literals;
            using namespace std::chrono;
            auto tick = static_cast<unsigned>(
                duration_cast<seconds>(steady_clock::now().time_since_epoch())
                    .count());
            return pong_response{"pong"s, tick};
        }));

    service.register_handler(
        "calculate",
        wspc::make_service_handler([](const work_request& work) {
            std::cout << "calculate(" << work.arg1 << ", " << work.arg2
                      << ", op: "
                      << kl::enum_reflector<operation>::to_string(work.op)
                      << "), comment: " << work.comment << '\n';

            const auto ret = [&] {
                switch (work.op)
                {
                case operation::add:
                    return work.arg1 + work.arg2;
                case operation::subtract:
                    return work.arg1 - work.arg2;
                case operation::multiply:
                    return work.arg1 * work.arg2;
                case operation::divide:
                    return work.arg1 / work.arg2;
                }
                throw std::logic_error{"internal error"};
            }();

            std::cout << "result: " << ret << std::endl;

            return work_response{ret};
        }));

    service.register_event<ping_event>();

    std::thread th{[&] {
        while (true)
        {
            using namespace std::chrono;

            std::this_thread::sleep_for(seconds{3});

            auto tick = static_cast<unsigned>(
                duration_cast<seconds>(steady_clock::now().time_since_epoch())
                .count());
            service.broadcast(ping_event{tick});
        }
    }};

    service.run(9001);
    th.join();
}
