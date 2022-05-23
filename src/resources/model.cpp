#include "resources.hpp"

#include <charconv>

#include "helpers/xml_builder.hpp"
#include "helpers/utilities.hpp"

#include "models.hpp"

namespace resources::model
{
    using httpserver::string_response;

    const Ref<http_response> get_user::process(const http_request& req)
    {
        auto& path_pieces = req.get_path_pieces();
        auto& args = req.get_args();
        XmlBuilder builder;

        builder
                .add_signature()
                .add_child("Data")
                .add_child("Users");

        if (path_pieces.size() > 2)
            return std::make_shared<string_response>("Too many path elements! Expected /user/{id}/!", 400,
                    "text/plain");

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto dbi = lmdb::dbi::open(rtxn, "users");
        auto cursor = lmdb::cursor::open(rtxn, dbi);

        auto read_string = [](uint8_t*& raw)
        {
            uint8_t size = *raw;
            raw++;
            std::string str;
            str.resize(size);
            memcpy(str.data(), raw, size);
            raw += size;
            return str;
        };

        if (path_pieces.size() == 1 && (args.empty() || args.find("id") == args.end()))
        {
            MDB_val key;
            MDB_val value;
            bool first = true;
            while (mdb_cursor_get(cursor, &key, &value, first ? MDB_FIRST : MDB_NEXT) == MDB_SUCCESS)
            {
                if (first)
                    first = false;

                uint16_t id = *(uint16_t*)key.mv_data;
                auto* raw = (uint8_t*)value.mv_data;

                std::string first_name = read_string(raw);
                std::string last_name = read_string(raw);
                std::string email = read_string(raw);

                builder
                        .add_child("User", {{ "ID", std::to_string(id) }})
                        .add_string("FirstName", first_name)
                        .add_string("LastName", last_name)
                        .add_string("Email", email)
                        .step_up();
            }
        }
        else
        {
            std::string id = path_pieces.size() == 1 ? args.at("id") : path_pieces.at(1);
            if (!util::is_integer(id))
                return std::make_shared<string_response>("Id must be an integer!", 400, "text/plain");

            uint16_t id_val;
            std::from_chars(id.c_str(), id.c_str() + id.size(), id_val);

            MDB_val mdb_id{ sizeof(id_val), &id_val };
            MDB_val result;
            if (mdb_cursor_get(cursor, &mdb_id, &result, MDB_SET) == MDB_NOTFOUND)
            {
                cursor.close();
                rtxn.abort();
                return std::make_shared<string_response>("Could not find a user with that id!", 404, "text/plain");
            }

            auto* raw = (uint8_t*)result.mv_data;

            std::string first_name = read_string(raw);
            std::string last_name = read_string(raw);
            std::string email = read_string(raw);

            builder
                    .add_child("User", {{ "ID", id }})
                    .add_string("FirstName", first_name)
                    .add_string("LastName", last_name)
                    .add_string("Email", email);
        }

        cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }

    const Ref<http_response> get_transaction_types::process(const http_request&)
    {
        XmlBuilder builder;

        builder
            .add_signature()
            .add_child("Data")
                .add_array("TransactionTypes", {
                    models::TransactionType::Swipe,
                    models::TransactionType::Online,
                    models::TransactionType::Chip
                }, [](XmlBuilder& b, const auto& t) -> void {
                    b.add_string("TransactionType", models::transaction_type_to_string(t));
                });

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }
}