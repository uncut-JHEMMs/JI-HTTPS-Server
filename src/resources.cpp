#include "resources.hpp"
#include <spdlog/spdlog.h>

namespace resources
{
    std::vector<Ref<clean_resource>> resources(const Ref<PerfData>& data)
    {
        return {
            std::make_shared<digest_test>(data),
            std::make_shared<echo_test>(),
            std::make_shared<empty_test>(),
            std::make_shared<big_workload>()
        };
    }

    const Ref<http_response> digest_test::render_GET(const http_request& req)
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

    const Ref<http_response> echo_test::render(const http_request& req)
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

    const Ref<http_response> empty_test::render_GET(const http_request& req)
    {
        return std::make_shared<httpserver::string_response>("", 200, "text/plain");
    }

    const Ref<http_response> big_workload::render_GET(const http_request& req)
    {
        using namespace std::literals::chrono_literals;
        using httpserver::string_response;

        std::this_thread::sleep_for(5s);
        return std::make_shared<string_response>("Complete!", 200, "text/plain");
    }
}
