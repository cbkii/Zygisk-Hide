#include <cassert>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#include "config.hpp"

int main() {
    Config::Targets t;
    assert(Config::parse(R"({"com.google.android.gms":true,"com.example":false})", t));
    assert(Config::matches(t, "com.google.android.gms.unstable:worker", "com.google.android.gms"));
    assert(Config::matches(t, "com.google.android.gms:unstable", ""));
    assert(!Config::matches(t, "com.google.android.gms.fake:worker", ""));
    assert(!Config::matches(t, "com.example", ""));
    assert(Config::matches(t, "com.google.android.gms_zygote", "", true));
    assert(!Config::matches(t, "com.google.android.gms_zygote", "", false));
    assert(!Config::matches(t, "com.google.android.gms.fake_zygote", "", true));
    assert(Config::queryCleanup(-1, "", "", Config::MANAGER_SELECTED) == Config::Cleanup::Local);
    assert(Config::queryCleanup(-1, "", "", 0) == Config::Cleanup::None);
    assert(!Config::matches(t, "invalid/name", "com.google.android.gms"));
    for (const auto *bad : {"", "[]", "{\"a\":trueX}", "{\"a\":true,}",
        "{\"a\":true,\"a\":false}", "{\"a\":1}", "{\"a\":{}}", "{}garbage",
        "{\"a\\u002eb\":true}", "{\"a\":false,\"b\":true"}) {
        assert(!Config::parse(bad, t)); assert(t.empty());
    }
    assert(!Config::parse(std::string(Config::MAX_CONFIG_SIZE + 1, ' '), t));
    assert(Config::parse(" \n {\"com.example:isolated:worker\":true} \t", t));
    assert(Config::matches(t, "com.example:isolated:worker", ""));
    assert(Config::parse("{}", t) && t.empty());
    assert(Runtime::packageFromDataDir("/data/user/10/com.example", 1010123) == "com.example");
    assert(Runtime::packageFromDataDir("/data/user_de/0/com.example", 99123) == "com.example");
    assert(Runtime::packageFromDataDir("/data/data/com.example", 10123) == "com.example");
    assert(Runtime::packageFromDataDir("/data/data/com.example", 1010123).empty());
    assert(Runtime::packageFromDataDir("/data/user/0/com.example/nested", 10123).empty());
    assert(Runtime::packageFromDataDir("/data/user/1/com.example", 10123).empty());
    assert(Runtime::pathWithin("/data/adb/module", "/data/adb"));
    assert(Runtime::pathWithin("/data/adb", "/data/adb"));
    assert(!Runtime::pathWithin("/data/adblock", "/data/adb"));
    assert(Runtime::childSucceeded(0));
    assert(!Runtime::childSucceeded(1 << 8)); // Original WIFEXITED-only bug.
    assert(!Runtime::childSucceeded(-1));
    assert(!Runtime::childSucceeded(9));

    int pair[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    auto sender = std::thread([&]() {
        for (char c : std::string("abcdef")) {
            assert(Runtime::writeExact(pair[1], &c, 1, Runtime::deadline()));
            usleep(1000);
        }
        shutdown(pair[1], SHUT_WR);
    });
    char bytes[6]; assert(Runtime::readExact(pair[0], bytes, 6, Runtime::deadline()));
    assert(std::string(bytes, 6) == "abcdef");
    assert(!Runtime::readExact(pair[0], bytes, 1, Runtime::deadline()));
    sender.join(); close(pair[0]); close(pair[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    auto start = Runtime::Clock::now();
    assert(!Runtime::readExact(pair[0], bytes, 1, Runtime::deadline(25)));
    assert(Runtime::Clock::now() - start < std::chrono::seconds(1));
    int file = open("/dev/null", O_RDONLY); assert(file >= 0);
    assert(Runtime::sendFd(pair[0], file, Runtime::deadline()));
    int received = Runtime::receiveFd(pair[1], Runtime::deadline());
    assert(received >= 0 && received != file);
    assert(fcntl(received, F_GETFD) & FD_CLOEXEC);
    close(received); close(file);
    assert(Config::writeString(pair[0], "com.example:worker", Runtime::deadline()));
    std::string result;
    assert(Config::readString(pair[1], result, Runtime::deadline()) && result == "com.example:worker");
    uint16_t oversize = 513;
    assert(Runtime::writeExact(pair[0], &oversize, sizeof(oversize), Runtime::deadline()));
    assert(!Config::readString(pair[1], result, Runtime::deadline()));
    close(pair[1]);
    assert(!Runtime::writeExact(pair[0], bytes, 1, Runtime::deadline())); // No SIGPIPE.
    close(pair[0]);
}
