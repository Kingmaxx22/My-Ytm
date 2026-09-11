#pragma once

#include <string>
#include <variant>

namespace myytm::youtube {

enum class ErrorKind {
    Network,
    Timeout,
    Auth,
    RateLimited,
    Parse,
    NotFound,
    Unknown,
};

struct Error {
    ErrorKind kind = ErrorKind::Unknown;
    std::string message; // user-facing, no secrets
    int httpStatus = 0;

    static Error network(std::string msg) { return {ErrorKind::Network, std::move(msg), 0}; }
    static Error timeout() { return {ErrorKind::Timeout, "Request timed out. Check your connection.", 0}; }
    static Error auth() { return {ErrorKind::Auth, "Authentication failed. Please re-authenticate.", 401}; }
    static Error rateLimited() { return {ErrorKind::RateLimited, "Rate limited. Please try again later.", 429}; }
    static Error parse(std::string msg) { return {ErrorKind::Parse, std::move(msg), 0}; }
};

template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(Error e) { return Result(std::move(e)); }

    [[nodiscard]] bool isOk() const noexcept { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool isErr() const noexcept { return !isOk(); }

    [[nodiscard]] T& value() { return std::get<T>(data_); }
    [[nodiscard]] const T& value() const { return std::get<T>(data_); }
    [[nodiscard]] Error& error() { return std::get<Error>(data_); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(data_); }

    [[nodiscard]] T valueOr(T fallback) const
    {
        if (isOk()) return value();
        return std::move(fallback);
    }

private:
    explicit Result(T v) : data_(std::move(v)) {}
    explicit Result(Error e) : data_(std::move(e)) {}
    std::variant<T, Error> data_;
};

// Specialization for void
template <>
class Result<void> {
public:
    static Result ok() { return Result(true, {}); }
    static Result err(Error e) { return Result(false, std::move(e)); }

    [[nodiscard]] bool isOk() const noexcept { return ok_; }
    [[nodiscard]] bool isErr() const noexcept { return !ok_; }
    [[nodiscard]] Error& error() { return err_; }
    [[nodiscard]] const Error& error() const { return err_; }

private:
    Result(bool ok, Error e) : ok_(ok), err_(std::move(e)) {}
    bool ok_ = false;
    Error err_;
};

} // namespace myytm::youtube
