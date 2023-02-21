#include <cassert>
#include <filesystem>
#include <ownkit/util.h>
#include <stdexcept>

void ownkit::CreateDirectoryIfNotExist(const std::string &path)
{
    if (std::filesystem::exists(path)) {
        return;
    }

    assert(!path.empty());
    auto cmd = "mkdir " + path;


    auto ret = std::system(cmd.c_str());
    if (0 != ret) {
        throw std::runtime_error("execute system cmd [mkdir] error");
    }
}