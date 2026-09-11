#pragma once
#include <memory>
#include <string>

namespace myytm::platform {

class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual void init() = 0;
    virtual void restore() = 0;
    virtual void clear() const = 0;
    virtual void hideCursor() const = 0;
    virtual void showCursor() const = 0;
    virtual void moveTo(int row, int col) const = 0;
    virtual void write(const std::string& text) const = 0;
    virtual void writeLine(const std::string& text) const = 0;
    [[nodiscard]] virtual int width() const noexcept = 0;
    [[nodiscard]] virtual int height() const noexcept = 0;
    virtual void refreshSize() = 0;
};

std::unique_ptr<ITerminal> makeTerminal();

} // namespace myytm::platform
