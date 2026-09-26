#include "UniqueFd.hpp"
#include <unistd.h>

UniqueFd::UniqueFd() : fd(-1) {}

UniqueFd::UniqueFd(int fd) : fd(fd) {}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd(other.fd) {
    other.fd = -1;
}

UniqueFd::~UniqueFd() {
    if (fd != -1) {
        close(fd);
    }
}

int UniqueFd::get() const {
    return fd;
}

void UniqueFd::reset(int newFd) {
    if (fd != -1 && fd != newFd) {
        close(fd);
    }
    fd = newFd;
}

int UniqueFd::release() {
    int oldFd = fd;
    fd = -1;
    return oldFd;
}