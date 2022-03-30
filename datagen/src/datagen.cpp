#include "datagen.hpp"

#include <set>
#include <fstream>
#include <iostream>

#include <boost/tokenizer.hpp>
#include <lmdb++.h>

#include "generation.hpp"

void datagen::process(const fs::path& path)
{
    if (!fs::is_directory("db"))
        fs::create_directories("db");

    auto env = lmdb::env::create();
    mdb_env_set_maxdbs(env, 50);
    mdb_env_set_mapsize(env, (size_t)1048576 * (size_t)100000);
    env.open("db");
    auto transaction = lmdb::txn::begin(env);

    using separator = boost::escaped_list_separator<char>;
    using tokenizer = boost::tokenizer<separator>;

    std::ifstream file{path};
    std::string line;

    std::set<std::string> users;
    MDB_dbi users_dbi = lmdb::dbi::open(transaction, "users", MDB_DUPSORT | MDB_CREATE);

    auto start = std::chrono::system_clock::now();
    {
        // Ignore the first line of the csv, since it's only column headers
        getline(file, line);

        while (getline(file, line))
        {
            tokenizer tok(line);
            auto it = tok.begin();
            auto next = [&it]() -> tokenizer::value_type
            {
                auto elem = *it;
                it++;
                return elem;
            };

            auto id = next();

            /**
             * TODO(Jordan):
             *   I might be able to get some optimization if I perform a search that's optimized
             *   for unsigned integers...
             */
            if (!users.count(id))
            {
                users.emplace(id);

                auto user = generation::generate_user();
                auto serialized = user.serialize();
                MDB_val key{ id.size(), id.data() };
                MDB_val val = serialized;

                mdb_put(transaction, users_dbi, &key, &val, MDB_NODUPDATA);
            }
        }
    }
    auto end = std::chrono::system_clock::now();

    transaction.commit();
    using ToSeconds = std::chrono::duration<double>;
    auto timeTook = ToSeconds(end - start).count();

    std::cout << "Finished processing in " << timeTook << " seconds\n";
}
