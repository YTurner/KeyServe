class UniqueFd {
  public:
    UniqueFd();
    explicit UniqueFd(int fd);
    UniqueFd(UniqueFd&& other) noexcept;
    ~UniqueFd();

    int get() const;
    void reset(int newFd);
    int release();

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd& operator=(UniqueFd&& other) noexcept = delete;

  private:
    int fd;
};