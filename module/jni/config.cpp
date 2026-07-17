#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <unistd.h>

#include "config.hpp"
#include "logging.hpp"

using namespace Config;

namespace
{
    // Reads the whole config file into memory. Returns an empty string if
    // missing or unreadable, which callers treat as "no apps configured".
    std::string readConfigFile()
    {
        std::ifstream file(CONFIG_PATH, std::ios::binary);
        if (!file)
            return {};

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    /*
     * Looks for the exact quoted key "packageName" in a flat JSON object
     * and checks whether its value is the literal `true`.
     *
     * This is intentionally NOT a general JSON parser -- it's a narrow
     * scanner purpose-built for the one format our own WebUI ever writes:
     * a single-level object mapping package name strings to booleans,
     * e.g. { "com.example.app": true }. Package names can't contain a
     * literal `"`, so a `"pkg"` substring match is unambiguous.
     */
    bool findTruthyKey(const std::string &json, const std::string &packageName)
    {
        const std::string needle = "\"" + packageName + "\"";
        size_t pos = 0;
        while ((pos = json.find(needle, pos)) != std::string::npos)
        {
            size_t cursor = pos + needle.size();

            while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])))
                cursor++;

            if (cursor >= json.size() || json[cursor] != ':')
            {
                pos += needle.size();
                continue;
            }
            cursor++;

            while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])))
                cursor++;

            if (json.compare(cursor, 4, "true") == 0)
                return true;

            // Key matched but the value wasn't `true` (e.g. explicit
            // `false`). Keep scanning in case of a duplicate key further
            // down; otherwise this key is simply "not hidden".
            pos += needle.size();
        }
        return false;
    }
}

bool Config::isPackageHidden(const std::string &packageName)
{
    if (packageName.empty())
        return false;

    std::string json = readConfigFile();
    if (json.empty())
        return false;

    return findTruthyKey(json, packageName);
}

bool Config::queryIsPackageHidden(int companionFd, const std::string &packageName)
{
    if (companionFd == -1)
        return false;

    uint8_t tag = static_cast<uint8_t>(Request::CONFIG_QUERY);
    uint16_t len = static_cast<uint16_t>(std::min<size_t>(packageName.size(), UINT16_MAX));

    if (write(companionFd, &tag, sizeof(tag)) != (ssize_t)sizeof(tag))
        return false;
    if (write(companionFd, &len, sizeof(len)) != (ssize_t)sizeof(len))
        return false;
    if (len > 0 && write(companionFd, packageName.data(), len) != (ssize_t)len)
        return false;

    uint8_t result = 0;
    if (read(companionFd, &result, sizeof(result)) != (ssize_t)sizeof(result))
        return false;

    return result != 0;
}
