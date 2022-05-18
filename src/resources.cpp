#include "resources.hpp"
#include <spdlog/spdlog.h>
#include <libxml/xmlwriter.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/dom/DOM.hpp>

#include "utilities.hpp"

namespace resources
{
    std::vector<Ref<clean_resource>> resources(const Ref<PerfData>& perf_data, const Ref<Statistics>& stat_data, Ref<lmdb::env>& env)
    {
        return {
            std::make_shared<digest_test>(stat_data, perf_data),
            std::make_shared<echo_test>(stat_data),
            std::make_shared<empty_test>(stat_data),
            std::make_shared<big_workload>(stat_data),
            std::make_shared<get_user>(stat_data, env)
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

    static std::map<std::string, lmdb::dbi> dbi_cache;

    const Ref<http_response> get_user::process(const http_request& req)
    {
        using httpserver::string_response;

        auto& path_pieces = req.get_path_pieces();
        auto& args = req.get_args();

        if (path_pieces.size() > 2)
            return std::make_shared<string_response>("Too many path elements! Expected /user/{id}/!", 400, "text/plain");
        if (path_pieces.size() == 1 && (args.empty() || args.find("id") == args.end()))
            return std::make_shared<string_response>("Expected an id in the form of /user/{id}/ or /user?id={id}!", 400, "text/plain");

        std::string id = path_pieces.size() == 1 ? args.at("id") : path_pieces.at(1);
        if (!util::is_integer(id))
            return std::make_shared<string_response>("Id must be an integer!", 400, "text/plain");

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        if (!dbi_cache.count("users"))
            dbi_cache.insert(std::make_pair("users", lmdb::dbi::open(rtxn, "users")));

        auto& dbi = dbi_cache.at("users");
        auto cursor = lmdb::cursor::open(rtxn, dbi);

        MDB_val mdb_id{id.size(), id.data()};
        MDB_val result;
        if (mdb_cursor_get(cursor, &mdb_id, &result, MDB_SET) == MDB_NOTFOUND)
            return std::make_shared<string_response>("Could not find a user with that id!", 404, "text/plain");

        const auto* raw = (const uint8_t*)result.mv_data;
        auto read_string = [&raw]() {
            uint8_t size = *raw;
            raw++;
            std::string str;
            str.resize(size);
            memcpy(str.data(), raw, size);
            raw += size;
            return str;
        };

        std::string first_name = read_string();
        std::string last_name = read_string();
        std::string email = read_string();
        cursor.close();
        rtxn.abort();

        using namespace xercesc;
        XMLPlatformUtils::Initialize();

        auto impl = DOMImplementationRegistry::getDOMImplementation(X("LS"));
        auto doc = impl->createDocument(nullptr, X("Envelope"), nullptr);
        auto root = doc->getDocumentElement();
        root->setAttribute(X("xmlns"), X("urn:envelope"));

        auto user = doc->createElement(X("User"));
        user->setAttribute(X("ID"), X(id.c_str()));
        user
            ->appendChild(doc->createElement(X("FirstName")))
            ->appendChild(doc->createTextNode(X(first_name.c_str())));
        user
            ->appendChild(doc->createElement(X("LastName")))
            ->appendChild(doc->createTextNode(X(last_name.c_str())));
        user
            ->appendChild(doc->createElement(X("Email")))
            ->appendChild(doc->createTextNode(X(email.c_str())));

        root
            ->appendChild(doc->createElement(X("Data")))
            ->appendChild(user);

        auto signed_doc = util::sign_document(doc);

        /*auto buf = xmlAllocOutputBuffer(nullptr);
        auto writer = xmlNewTextWriter(buf);

        xmlTextWriterStartDocument(writer, nullptr, nullptr, nullptr);
        {
            xmlTextWriterStartElementNS(writer, nullptr, BAD_CAST "Envelope", BAD_CAST "urn:envelope");
            {
                xmlTextWriterStartElement(writer, BAD_CAST "User");
                xmlTextWriterWriteAttribute(writer, BAD_CAST "id", BAD_CAST id.c_str());
                {
                    xmlTextWriterWriteElement(writer, BAD_CAST "FirstName", BAD_CAST first_name.c_str());
                    xmlTextWriterWriteElement(writer, BAD_CAST "LastName", BAD_CAST last_name.c_str());
                    xmlTextWriterWriteElement(writer, BAD_CAST "Email", BAD_CAST email.c_str());
                }
                xmlTextWriterEndElement(writer);
            }
            xmlTextWriterEndElement(writer);
        }
        xmlTextWriterEndDocument(writer);

        auto signed_doc = util::sign_document(xmlOutputBufferGetSize(buf), xmlOutputBufferGetContent(buf));
        xmlFreeTextWriter(writer);*/

        XMLPlatformUtils::Terminate();
        return std::make_shared<string_response>(signed_doc, 200, "application/xml");
    }
}
