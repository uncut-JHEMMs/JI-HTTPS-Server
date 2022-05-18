#pragma once

#include <filesystem>

namespace datagen
{
    namespace fs = std::filesystem;

    void process(const fs::path& data_path, const fs::path& db_dir);
}