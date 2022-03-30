#pragma once

#include <httpserver.hpp>
#include <utility>
#include "perf_monitor.hpp"

namespace resources
{
    using httpserver::http_resource;
    using httpserver::http_response;
    using httpserver::http_request;

    template<class T>
    using Ref = std::shared_ptr<T>;

    class clean_resource : public http_resource
    {
        const std::string p_endpoint;
        const bool p_family;
    public:
        clean_resource(std::string endpoint, bool family) : p_endpoint(std::move(endpoint)), p_family(family) {}

        [[nodiscard]] inline const std::string& endpoint() const noexcept { return p_endpoint; }
        [[nodiscard]] inline bool family() const noexcept { return p_family; }
    };

    #define SIMPLE_RESOURCE(name, method, endpoint, family) class name : public clean_resource \
    {                                                                                          \
    public:                                                                                    \
        name() : clean_resource(endpoint, family) {}                                           \
        const Ref<http_response> method(const http_request& req) override;                     \
    }

    #define METHOD_SIG(method) const Ref<http_response> method(const http_request& req) override

    constexpr const char* OPAQUE = "11733b200778ce33060f31c9af70a870ba96ddd4";

    class digest_test : public clean_resource
    {
        Ref<PerfData> data;
    public:
        explicit digest_test(Ref<PerfData> data_ptr) : clean_resource("/test_digest", false), data(std::move(data_ptr)) {}

        inline void push_access(const AccessData& accessData)
        {
            data->access_queue.push(accessData);
        }

        METHOD_SIG(render_GET);
    };

    SIMPLE_RESOURCE(echo_test, render, "/echo", true);
    SIMPLE_RESOURCE(empty_test, render_GET, "/empty", false);
    SIMPLE_RESOURCE(big_workload, render_GET, "/work", false);

    std::vector<Ref<clean_resource>> resources(const Ref<PerfData>& data);
}
