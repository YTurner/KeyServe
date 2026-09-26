// RAII owner for a POSIX file descriptor.
//* Ownership is unique; copying is disabled to prevent double-close.
class UniqueFd {
  public:
    UniqueFd();
    explicit UniqueFd(int fd);
    UniqueFd(UniqueFd&& other) noexcept;
    ~UniqueFd();

    int get() const;
    void reset(int newFd);
    // Gives ownership of the descriptor to the caller without closing it.
    int release();

    // File descriptor ownership is unique; copying would cause double-close.
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd& operator=(UniqueFd&& other) noexcept = delete;

  private:
    int fd_;
};