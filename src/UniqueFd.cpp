#include "UniqueFd.hpp"
#include <unistd.h>

UniqueFd::UniqueFd() : fd_(-1) {}

UniqueFd::UniqueFd(int fd) : fd_(fd) {}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UniqueFd::~UniqueFd() {
    if (fd_ != -1) {
        close(fd_);
    }
}

int UniqueFd::get() const {
    return fd_;
}

void UniqueFd::reset(int newFd) {
    if (fd_ != -1 && fd_ != newFd) {
        close(fd_);
    }
    fd_ = newFd;
}

int UniqueFd::release() {
    int oldFd = fd_;
    fd_ = -1;
    return oldFd;
}