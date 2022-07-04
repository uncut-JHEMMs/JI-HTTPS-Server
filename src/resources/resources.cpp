#include "resources.hpp"
#include <spdlog/spdlog.h>

#include "helpers/utilities.hpp"
#include "helpers/xml_builder.hpp"

namespace resources
{
    std::vector<Ref<clean_resource>> resources(const Ref<PerfData>& perf_data, const Ref<Statistics>& stat_data, Ref<lmdb::env>& env)
    {
        return {
            std::make_shared<digest_test>(stat_data, perf_data),
            std::make_shared<echo_test>(stat_data),
            std::make_shared<empty_test>(stat_data),
            std::make_shared<big_workload>(stat_data),
            std::make_shared<model::get_user>(stat_data, env),
            std::make_shared<model::get_transaction_types>(stat_data, env),
            std::make_shared<analytics::get_top5_transactions_by_zip>(stat_data, env),
            std::make_shared<analytics::get_top5_transactions_by_city>(stat_data, env),
            std::make_shared<analytics::query_transactions>(stat_data, env),
            std::make_shared<analytics::total_fraud_free_transactions>(stat_data, env)
        };
    }

    const Ref<http_response> digest_test::process(const http_request& req)
    {
        using httpserver::digest_auth_fail_response;
        using httpserver::string_response;

        if (req.get_digested_user().empty())
        {
            push_access(AccessData { req.get_requestor(), std::string{}, AccessStatus::NoUserProvided });
            return std::make_shared<digest_auth_fail_response>("FAIL: No username provided!", "test@localhost", OPAQUE, true);
        }
        else
        {
            bool reload_nonce = false;
            if (!req.check_digest_auth("test@localhost", "mypass", 300, &reload_nonce))
            {
                push_access(AccessData { req.get_requestor(), req.get_digested_user(), AccessStatus::InvalidUserOrPassword });
                return std::make_shared<digest_auth_fail_response>("FAIL: Invalid username or password!", "test@localhost", OPAQUE, reload_nonce);
            }
        }

        push_access(AccessData { req.get_requestor(), req.get_digested_user(), AccessStatus::Success });
        return std::make_shared<string_response>("SUCCESS!", 200, "text/plain");
    }

    const Ref<http_response> echo_test::process(const http_request& req)
    {
        using httpserver::string_response;

        std::stringstream ss;
        ss << "Method: " << req.get_method() << "\n";
        ss << "Path: " << req.get_path() << "\n";
        ss << "Headers:\n";
        for (const auto& header : req.get_headers())
        {
            ss << "\t" << header.first << ": " << header.second << "\n";
        }
        ss << "Arguments:\n";
        for (const auto& arg : req.get_args())
        {
            ss << "\t" << arg.first << ": " << arg.second << "\n";
        }
        ss << "Cookies:\n";
        for (const auto& cookie : req.get_cookies())
        {
            ss << "\t" << cookie.first << ": " << cookie.second << "\n";
        }

        return std::make_shared<string_response>(ss.str(), 200, "text/plain");
    }

    const Ref<http_response> empty_test::process(const http_request&)
    {
        return std::make_shared<httpserver::string_response>("", 200, "text/plain");
    }

    const Ref<http_response> big_workload::process(const http_request&)
    {
        using namespace std::literals::chrono_literals;
        using httpserver::string_response;

        std::this_thread::sleep_for(5s);
        return std::make_shared<string_response>("Complete!", 200, "text/plain");
    }
}
