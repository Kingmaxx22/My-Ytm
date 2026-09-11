#include "ui/terminal.h"
#include "platform/terminal/ITerminal.h"

namespace myytm::ui {

Terminal::Terminal() : impl_(platform::makeTerminal()) {}
Terminal::~Terminal() = default;

void Terminal::init() { impl_->init(); }
void Terminal::restore() { impl_->restore(); }
void Terminal::clear() const { impl_->clear(); }
void Terminal::hideCursor() const { impl_->hideCursor(); }
void Terminal::showCursor() const { impl_->showCursor(); }
void Terminal::moveTo(int row, int col) const { impl_->moveTo(row, col); }
void Terminal::write(const std::string& text) const { impl_->write(text); }
void Terminal::writeLine(const std::string& text) const { impl_->writeLine(text); }
int Terminal::width() const noexcept { return impl_->width(); }
int Terminal::height() const noexcept { return impl_->height(); }
void Terminal::refreshSize() { impl_->refreshSize(); }

} // namespace myytm::ui
