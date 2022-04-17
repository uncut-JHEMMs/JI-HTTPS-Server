#include "datagen.hpp"

#include <set>
#include <fstream>
#include <iostream>

#include <boost/tokenizer.hpp>
#include <lmdb++.h>

#include "generation.hpp"

void datagen::process(const fs::path& data_path, const fs::path& db_dir)
{
    /**
     * LMDB expects the directory you pass into it to already exist, it won't create
     * it for you. So that's what I'm doing here, making the directory if it doesn't
     * already exist.
     */
    if (!fs::is_directory(db_dir))
        fs::create_directories(db_dir);

    auto env = lmdb::env::create();
    mdb_env_set_maxdbs(env, 5);
    env.open(db_dir.c_str());

    auto transaction = lmdb::txn::begin(env);
    MDB_dbi users_dbi = lmdb::dbi::open(transaction, "users", MDB_CREATE);
    MDB_dbi cards_dbi = lmdb::dbi::open(transaction, "cards", MDB_CREATE);

    std::set<std::string> users;
    std::set<std::pair<std::string, std::string>> cards;
    std::ifstream file{data_path};

    auto start = std::chrono::system_clock::now();
    {
        std::string line;
        // Ignore the first line of the csv, since it's only column headers
        getline(file, line);

        while (getline(file, line))
        {
            using separator = boost::escaped_list_separator<char>;
            using tokenizer = boost::tokenizer<separator>;

            /**
             * boost:tokenizer will turn my line into an iterator of elements,
             * much like a CSV Parser would, with the added benefit that it supports
             * quoted strings.
             */
            tokenizer tok(line);
            auto it = tok.begin();
            auto next = [&it]() -> tokenizer::value_type
            {
                /**
                 * You can't directly index the tokenizer's iterator, so this lambda here
                 * is a little helper function to just step through it.
                 */
                auto elem = *it;
                it++;
                return elem;
            };

            auto user_id = next();
            auto card_id = next();

            /**
             * TODO(Jordan):
             *   I might be able to get some optimization here if I convert the user user_id to an
             *   unsigned integer, and store that. I need to test that later.
             */
            if (!users.count(user_id))
            {
                users.emplace(user_id);

                auto user = generation::generate_user();
                auto serialized = user.serialize();
                MDB_val key{ user_id.size(), user_id.data() };
                MDB_val val = serialized;

                mdb_put(transaction, users_dbi, &key, &val, 0);
            }

            /**
             * TODO(Jordan):
             *  Ditto as Users
             */
            if (!cards.count(std::make_pair(user_id, card_id)))
            {
                cards.emplace(user_id, card_id);

                std::string key_str = user_id;
                key_str += ".";
                key_str += card_id;

                auto card = generation::generate_card();
                auto serialized = card.serialize();
                MDB_val key{key_str.size(), key_str.data() };
                MDB_val val = serialized;

                mdb_put(transaction, cards_dbi, &key, &val, 0);
            }
        }
    }
    auto end = std::chrono::system_clock::now();

    transaction.commit();
    using ToSeconds = std::chrono::duration<double>;
    auto timeTook = ToSeconds(end - start).count();

    std::cout << "Finished processing in " << timeTook << " seconds\n";
}
