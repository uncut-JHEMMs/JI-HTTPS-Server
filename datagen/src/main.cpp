#include <filesystem>
#include <iostream>

#include "datagen.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <file.csv> <database_directory>\n";
        return 1;
    }

    /**
     * Make sure a flat file had been passed in, and is indeed a file.
     */
    if (!fs::is_regular_file(argv[1]))
    {
        std::cerr << argv[1] << " is not a file!\n";
        return 1;
    }

    /**
     * If the database_directory path does exist, make sure it is a directory and not a file.
     */
    if (fs::exists(argv[2]) && !fs::is_directory(argv[2]))
    {
        std::cerr << argv[2] << " is not a directory!\n";
        return 1;
    }

    datagen::process(argv[1], argv[2]);

    return 0;
}
