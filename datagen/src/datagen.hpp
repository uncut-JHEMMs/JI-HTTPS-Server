#pragma once

#include <filesystem>

namespace datagen
{
    namespace fs = std::filesystem;
    void process(const fs::path& path);
}
