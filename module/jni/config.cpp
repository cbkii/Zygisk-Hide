#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.hpp"
#include "logging.hpp"

bool Config::parse(std::string_view json, Targets &targets) {
    targets.clear();
    if (json.size() > MAX_CONFIG_SIZE) return false;
    Targets parsed;
    size_t pos = 0;
    auto space = [&]() {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' ||
               json[pos] == '\r' || json[pos] == '\t')) ++pos;
    };
    auto token = [&](char c) {
        space();
        if (pos == json.size() || json[pos] != c) return false;
        ++pos;
        return true;
    };
    if (!token('{')) return false;
    space();
    if (pos < json.size() && json[pos] != '}') {
        for (;;) {
            if (!token('"')) return false;
            size_t start = pos;
            while (pos < json.size() && json[pos] != '"') ++pos;
            std::string key(json.substr(start, pos - start));
            if (!Runtime::validName(key, true) || !token('"') || !token(':')) return false;
            space();
            bool value;
            if (json.substr(pos, 4) == "true") { value = true; pos += 4; }
            else if (json.substr(pos, 5) == "false") { value = false; pos += 5; }
            else return false;
            if (!parsed.emplace(key, value).second || parsed.size() > 1024) return false;
            space();
            if (pos >= json.size() || json[pos] != ',') break;
            ++pos;
        }
    }
    if (!token('}')) return false;
    space();
    if (pos != json.size()) return false;
    targets = std::move(parsed);
    return true;
}

bool Config::matches(const Targets &targets, const std::string &process, const std::string &package) {
    auto enabled = [&](const std::string &key) {
        auto it = targets.find(key);
        return it != targets.end() && it->second;
    };
    if (!Runtime::validName(process, true)) return false;
    if (enabled(process)) return true;
    if (Runtime::validName(package) && enabled(package)) return true;
    // Conventional package:private_process naming, including isolated helpers.
    // Custom names use app_data_dir or the manager's denylist flag.
    auto colon = process.find(':');
    return colon != std::string::npos && enabled(process.substr(0, colon));
}

bool Config::isPackageHidden(const std::string &process, const std::string &package) {
    int fd = open(CONFIG_PATH, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != 0 ||
        (st.st_mode & 0022) || st.st_size < 0 || st.st_size > static_cast<off_t>(MAX_CONFIG_SIZE)) {
        close(fd);
        LOGW("Rejected unsafe or oversized target configuration");
        return false;
    }
    std::string json;
    char block[4096];
    ssize_t n;
    while ((n = read(fd, block, sizeof(block))) != 0) {
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || json.size() + static_cast<size_t>(n) > MAX_CONFIG_SIZE) { close(fd); return false; }
        json.append(block, static_cast<size_t>(n));
    }
    close(fd);
    Targets targets;
    if (!parse(json, targets)) {
        LOGW("Invalid target configuration; manager denylist remains independent");
        return false;
    }
    return matches(targets, process, package);
}

bool Config::readString(int fd, std::string &value, Runtime::Deadline until) {
    uint16_t length = 0;
    if (!Runtime::readExact(fd, &length, sizeof(length), until) || length > 512) return false;
    value.resize(length);
    return Runtime::readExact(fd, value.data(), length, until);
}
bool Config::writeString(int fd, const std::string &value, Runtime::Deadline until) {
    if (value.size() > 512) return false;
    uint16_t length = static_cast<uint16_t>(value.size());
    return Runtime::writeExact(fd, &length, sizeof(length), until) &&
           Runtime::writeExact(fd, value.data(), length, until);
}
bool Config::queryIsPackageHidden(int fd, const std::string &process, const std::string &package) {
    auto until = Runtime::deadline();
    uint8_t tag = static_cast<uint8_t>(Request::CONFIG_QUERY), result = 0;
    return Runtime::writeExact(fd, &tag, sizeof(tag), until) &&
           writeString(fd, process, until) && writeString(fd, package, until) &&
           Runtime::readExact(fd, &result, sizeof(result), until) && result == 1;
}
