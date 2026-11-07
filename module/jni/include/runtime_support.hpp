#pragma once
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

// Host-testable contracts; no Android-private structure offsets.
namespace Runtime {
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
inline Deadline deadline(int ms = 1200) { return Clock::now() + std::chrono::milliseconds(ms); }
inline bool transfer(int fd, void *buffer, size_t length, Deadline until, bool writing) {
    auto *bytes = static_cast<uint8_t *>(buffer);
    while (length) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now()).count();
        if (remaining <= 0) { errno = ETIMEDOUT; return false; }
        pollfd p{fd, static_cast<short>(writing ? POLLOUT : POLLIN), 0};
        int ready = poll(&p, 1, static_cast<int>(std::min<int64_t>(remaining, 1200)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready == 0) continue;
        if (ready < 0 || (p.revents & (POLLERR | POLLNVAL))) return false;
        if (!(p.revents & p.events)) return false;
        ssize_t n = writing ? send(fd, bytes, length, MSG_DONTWAIT | MSG_NOSIGNAL)
                            : recv(fd, bytes, length, MSG_DONTWAIT);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return false;
        bytes += n;
        length -= static_cast<size_t>(n);
    }
    return true;
}
inline bool readExact(int fd, void *p, size_t n, Deadline until) { return transfer(fd, p, n, until, false); }
inline bool writeExact(int fd, const void *p, size_t n, Deadline until) {
    return transfer(fd, const_cast<void *>(p), n, until, true);
}
inline bool waitSocket(int fd, short events, Deadline until) {
    for (;;) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now()).count();
        if (ms <= 0) { errno = ETIMEDOUT; return false; }
        pollfd p{fd, events, 0};
        int n = poll(&p, 1, static_cast<int>(std::min<int64_t>(ms, 1200)));
        if (n < 0 && errno == EINTR) continue;
        if (n == 0) continue;
        return n > 0 && !(p.revents & (POLLERR | POLLNVAL)) && (p.revents & events);
    }
}
inline bool sendFd(int socket, int fd, Deadline until) {
    char byte = 'N';
    iovec io{&byte, 1};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &io; message.msg_iovlen = 1;
    message.msg_control = control; message.msg_controllen = sizeof(control);
    auto *c = CMSG_FIRSTHDR(&message);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
    __builtin_memcpy(CMSG_DATA(c), &fd, sizeof(fd));
    while (waitSocket(socket, POLLOUT, until)) {
        ssize_t n = sendmsg(socket, &message, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n == 1) return true;
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        break;
    }
    return false;
}
inline int receiveFd(int socket, Deadline until) {
    while (waitSocket(socket, POLLIN, until)) {
        char byte = 0;
        iovec io{&byte, 1};
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * 4)]{};
        msghdr message{};
        message.msg_iov = &io; message.msg_iovlen = 1;
        message.msg_control = control; message.msg_controllen = sizeof(control);
        ssize_t n = recvmsg(socket, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        int result = -1;
        unsigned count = 0;
        for (auto *c = CMSG_FIRSTHDR(&message); n > 0 && c; c = CMSG_NXTHDR(&message, c)) {
            if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS || c->cmsg_len < CMSG_LEN(0)) continue;
            size_t total = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t i = 0; i < total; ++i) {
                int fd;
                __builtin_memcpy(&fd, CMSG_DATA(c) + i * sizeof(int), sizeof(fd));
                if (count++ == 0) result = fd;
                else close(fd);
            }
        }
        if (n == 1 && byte == 'N' && count == 1 && !(message.msg_flags & MSG_CTRUNC)) return result;
        if (result >= 0) close(result);
        return -1;
    }
    return -1;
}
inline bool childSucceeded(int status) {
    return status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
inline bool pathWithin(std::string_view path, std::string_view root) {
    return path == root || (path.starts_with(root) && path.size() > root.size() && path[root.size()] == '/');
}
inline bool validName(std::string_view name, bool process = false) {
    if (name.empty() || name.size() > (process ? 512u : 255u)) return false;
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.' || (process && c == ':'))) return false;
    }
    return true;
}
inline std::string packageFromDataDir(std::string_view path, int uid) {
    const std::string user = std::to_string(uid / 100000);
    for (const auto &prefix : {std::string("/data/user/") + user + "/",
                               std::string("/data/user_de/") + user + "/",
                               uid < 100000 ? std::string("/data/data/") : std::string()}) {
        if (prefix.empty() || !path.starts_with(prefix)) continue;
        auto package = path.substr(prefix.size());
        if (validName(package)) return std::string(package);
    }
    return {};
}
}
