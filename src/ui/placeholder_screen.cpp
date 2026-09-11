#include "ui/placeholder_screen.h"
#include "ui/renderer.h"

namespace myytm::ui {

void PlaceholderScreen::render(const Renderer& r)
{
    const int w = r.terminal().width();
    const int h = r.terminal().height();
    r.text(1, 1, Renderer::bold(title_));
    r.hline(2, 1, w);
    r.text(4, 1, body_);
    r.text(6, 1, Renderer::dim("(Coming in later phases)"));
    r.text(h - 1, 1, Renderer::dim("Press q to go back, ?:Help"));
}

} // namespace myytm::ui
