#include <filesystem>
#include <iostream>

#include "datagen.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <file.csv>\n";
        return 1;
    }

    if (!fs::is_regular_file(argv[1]))
    {
        std::cerr << argv[1] << " is not a file!\n";
        return 1;
    }

    datagen::process(argv[1]);

    return 0;
}
