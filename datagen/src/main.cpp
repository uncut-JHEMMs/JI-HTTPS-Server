#include <filesystem>
#include <iostream>

#include "datagen.hpp"

namespace fs = std::filesystem;

constexpr std::array<std::string_view, 5> data_files {"states.csv", "merchants.ssv", "cards.ssv", "users.ssv", "transactions.csv"};

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <data_directory> <database_directory>\n";
        return 1;
    }

    /**
     * Make sure a data dir had been passed in, and is contains the required files
     */
    if (!fs::is_directory(argv[1]))
    {
        std::cerr << argv[1] << " is not a directory!\n";
        return 1;
    }

    fs::path data_dir = argv[1];
    for (const auto& data_file : data_files)
    {
        if (!fs::is_regular_file(data_dir / data_file))
        {
            std::cerr << "Required data file '" << data_file << "' is not available!\n";
            return 1;
        }
    }

    /**
     * If the database_directory path does exist, make sure it is a directory and not a file.
     */
    if (fs::exists(argv[2]) && !fs::is_directory(argv[2]))
    {
        std::cerr << argv[2] << " is not a directory!\n";
        return 1;
    }

    datagen::process(data_dir, argv[2]);

    return 0;
}
