#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include "runtime_support.hpp"
namespace Config {
constexpr const char *CONFIG_PATH = "/data/adb/zygisk-hide/config.json";
constexpr size_t MAX_CONFIG_SIZE = 65536;
using Targets = std::map<std::string, bool>;
enum class Request : uint8_t { SWITCH_NS = 1, CONFIG_QUERY = 2 };
// Strict schema: flat object of plain package/process names and JSON booleans.
// Reject duplicates, escapes, unsupported values and trailing input.
bool parse(std::string_view json, Targets &targets);
bool matches(const Targets &targets, const std::string &process, const std::string &package);
bool isPackageHidden(const std::string &process, const std::string &package);
bool queryIsPackageHidden(int fd, const std::string &process, const std::string &package);
bool readString(int fd, std::string &value, Runtime::Deadline until);
bool writeString(int fd, const std::string &value, Runtime::Deadline until);
}
