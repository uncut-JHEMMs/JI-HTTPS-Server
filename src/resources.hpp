#pragma once

#include <httpserver.hpp>
#include <utility>
#include <lmdb++.h>
#include "monitors/perf_monitor.hpp"
#include "monitors/stat_monitor.hpp"

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
        std::shared_ptr<Statistics> p_stats;
    public:
        clean_resource(std::string endpoint, bool family, const Ref<Statistics>& stat_data) : p_endpoint(std::move(endpoint)), p_family(family), p_stats(stat_data) {}

        [[nodiscard]] inline const std::string& endpoint() const noexcept { return p_endpoint; }
        [[nodiscard]] inline bool family() const noexcept { return p_family; }
        [[nodiscard]] inline std::shared_ptr<Statistics> stats() const noexcept { return p_stats; }

        inline const http_request& log_request(const http_request& req)
        {
            p_stats->requestsPerSecond += (unsigned int)req.get_content().size();
            return req;
        }

        inline const Ref<http_response>& log_response(const Ref<http_response>& res)
        {
            using httpserver::string_response;
            using httpserver::digest_auth_fail_response;

            auto* resp = res->get_raw_response();
            char* raw_resp = (char*)resp;
            raw_resp += sizeof(char*) * 5;
            raw_resp += sizeof(pthread_mutex_t);
            raw_resp += sizeof(uint64_t) * 3;
            size_t size = *((size_t*)raw_resp);

            p_stats->responsePerSecond += (unsigned int)size;
            return res;
        }
    };

    #define SIMPLE_RESOURCE(name, method, endpoint, family) class name : public clean_resource \
    {                                                                                          \
    public:                                                                                    \
        explicit name(const Ref<Statistics>& stat_data) : clean_resource(endpoint, family, stat_data) {}\
        const Ref<http_response> method(const http_request& req) override                      \
        {                                                                                      \
            return log_response(process(log_request(req)));                                    \
        }                                                                                      \
                                                                                               \
        const Ref<http_response> process(const http_request& req);                             \
    }

    #define LMDB_RESOURCE(name, method, endpoint, family) class name : public clean_resource \
    {                                                                                        \
        Ref<lmdb::env> p_env;                                                                \
    public:                                                                                  \
        name(const Ref<Statistics>& stat_data, std::shared_ptr<lmdb::env> env) :             \
            clean_resource(endpoint, family, stat_data), p_env(std::move(env))               \
        {}                                                                                   \
                                                                                             \
        const Ref<http_response> method(const http_request& req) override                    \
        {                                                                                    \
            return log_response(process(log_request(req)));                                  \
        }                                                                                    \
                                                                                             \
        const Ref<http_response> process(const http_request& req);                           \
    }

    #define METHOD_SIG(method) const Ref<http_response> method(const http_request& req) override

    constexpr const char* OPAQUE = "11733b200778ce33060f31c9af70a870ba96ddd4";

    class digest_test : public clean_resource
    {
        Ref<PerfData> data;
    public:
        explicit digest_test(const Ref<Statistics>& stat_data, Ref<PerfData> data_ptr) : clean_resource("/test_digest", false, stat_data), data(std::move(data_ptr)) {}

        inline void push_access(const AccessData& accessData)
        {
            data->access_queue.push(accessData);
        }

        METHOD_SIG(render_GET)
        {
            return log_response(process(log_request(req)));
        }

        const Ref<http_response> process(const http_request& req);
    };

    SIMPLE_RESOURCE(echo_test, render, "/echo", true);
    SIMPLE_RESOURCE(empty_test, render_GET, "/empty", false);
    SIMPLE_RESOURCE(big_workload, render_GET, "/work", false);

    namespace model
    {
        LMDB_RESOURCE(get_user, render_GET, "/user", true);
        LMDB_RESOURCE(get_transaction_types, render_GET, "/transaction_types", true);
    }

    namespace analytics
    {
        // TODO: Merge these all of these into the `query_transactions` method.
        LMDB_RESOURCE(get_top5_transactions_by_zip, render_GET, "/top5/transactions/zip", true);
        LMDB_RESOURCE(get_top5_transactions_by_city, render_GET, "/top5/transactions/city", true);
        LMDB_RESOURCE(query_transactions, render_GET, "/query/transactions", true);
        LMDB_RESOURCE(total_fraud_free_transactions, render_GET, "/query/fraud_free_transactions", true);
    }

    std::vector<Ref<clean_resource>> resources(const Ref<PerfData>& perf_data, const Ref<Statistics>& stat_data, Ref<lmdb::env>& env);
}
