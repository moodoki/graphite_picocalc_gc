#include "apps/help_screen.hpp"

#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/catalog.hpp"

namespace apps {

namespace {

constexpr int kTabCount = 3;
const char* const kTabNames[kTabCount] = {"FUNC", "KEYS", "SYNTAX"};

constexpr int kTopY = 24;
constexpr int kLineH = 16;
constexpr int kVisibleLines = 16;  // kTopY + 16*16 = 280, above softkeys

// Lines starting with '#' render as green section headers.
const char* const kKeysLines[] = {
    "#EVERY SCREEN",
    "F1 EDIT  editor (Y=/PAR/POL)",
    "F2 WIN   window settings",
    "F3 MODE  mode settings",
    "F4 TRC   trace (opens graph)",
    "F5 GRPH  graph <-> table",
    "HOME     back to home screen",
    "ESC      back / cancel edit",
    "#COMMANDS (type on home)",
    "help (?) this help",
    "diag     diagnostics screen",
    "files    SD file list",
    "lists    data list editor",
    "stats    statistics screen",
    "         (list/stat also ok)",
    "dist     distribution helper",
    "test     inference (infer ok)",
    "plot     stat plot setup",
    "calc     graph analysis menu",
    "cls      clear screen (keeps",
    "         input history)",
    "clrhist  erase all history",
    "#HOME",
    "UP       recall last entry",
    "UP/DOWN  walk input history",
    "Alt/Ctrl+UP/DOWN scroll view",
    "#GRAPH",
    "F4 TRC   toggle trace",
    "F5 TBL   value table",
    "Alt+F5   split graph|table",
    "- / =    zoom out / in",
    "S / T    ZStandard / ZTrig",
    "F        ZoomFit (y to curves)",
    "Z        ZoomStat (stat plots)",
    "L        toggle axis labels",
    "LT/RT    move trace cursor",
    "UP/DOWN  next curve (trace)",
    "F6 CALC  analyze: value zero",
    "min max intersect dy/dx int;",
    "ENTER places bounds/points,",
    "ESC cancels; root -> x, ans",
    "#TABLE",
    "UP/DOWN  scroll rows",
    "LT/RT    scroll columns",
    "ENTER    add value (ASK mode)",
    "DEL      delete row (ASK)",
    "F2 SETP  table setup",
    "Alt+F5   split graph|table",
    "#SPLIT (graph|table)",
    "F5       switch focused pane",
    "Alt+F5 / ESC  back to full",
    "F4       trace (graph pane)",
    "trace <-> table row sync",
    "#EDITORS (Y=, PAR, POLAR, SEQ)",
    "ENTER    edit field",
    "SPACE    toggle enable",
    "DEL      clear field",
    "F5       graph",
    "#LIST EDITOR (lists cmd)",
    "arrows   move cell",
    "ENTER/type  edit or append",
    "DEL      delete row",
    "F6/F7 (Shift+F1/F2) sort",
    "F8 (Shift+F3) clear list",
    "#STATS (stats cmd)",
    "UP/DOWN  select row",
    "LT/RT    change value",
    "ENTER    calculate (last row)",
    "results: UP/DOWN scroll",
    "#DIST (dist cmd)",
    "LT/RT    distribution / fn",
    "ENTER    edit param field",
    "DEL      clear + edit empty",
    "ENTER    calculate (last row)",
    "result -> ans; call shown",
    "#TEST (test cmd)",
    "LT/RT    test / option cycle",
    "ENTER    edit field / calc",
    "Data/Stats source where avail",
    "#STAT PLOTS (plot cmd)",
    "3 slots: scatter, xy-line,",
    "histogram, box, norm prob",
    "draw with funcs on graph;",
    "Z on graph = ZoomStat",
    "#WINDOW / TABLE SETUP",
    "ENTER    edit value",
    "DEL      clear + edit empty",
    "#MODE",
    "LT/RT    change value",
    "ENTER    select / reboot row",
};
constexpr int kKeysCount = sizeof(kKeysLines) / sizeof(kKeysLines[0]);

const char* const kSyntaxLines[] = {
    "#CASE",
    "input is case-sensitive:",
    "functions, vars, commands",
    "are all lowercase",
    "#STORE",
    "expr->a     store result in a",
    "works for a-z and theta",
    "#CONSTANTS",
    "pi, e (Euler's number)",
    "e is not a variable; 1e10 or",
    "1E10 = scientific literal",
    "#VARIABLES",
    "a-z, theta, ans (lowercase)",
    "ans = last result",
    "#FACTORIAL",
    "n! or fac(n)",
    "#ANGLE MODE",
    "MODE sets RADIAN or DEGREE;",
    "trig functions follow it",
    "#GRAPH MODES",
    "MODE > Graph mode:",
    "FUNC / PARAM / POLAR / SEQ",
    "PARAM plots X1T(t), Y1T(t)",
    "over Tmin..Tmax (see WINDOW)",
    "POLAR plots r(theta) over",
    "THmin..THmax; angle mode",
    "applies to theta",
    "SEQ plots u/v/w(n): e.g.",
    "u(n)=u(n-1)+1 with seed",
    "u(nMin); lags n-1, n-2;",
    "MODE > Seq plot: TIME/WEB",
    "#HISTORY",
    "UP on empty input recalls;",
    "UP/DOWN walks past entries",
    "#LISTS (l1..l6)",
    "{1,2,3}->l1  store a list",
    "l1+2*l2      element-wise",
    "{1,2,3}+2    literals too",
    "range(1,9)   list 1..9",
    "range(0,1,.1) with step",
    "sum/prod/length(l) scalar",
    "mean/median/stdev(l)",
    "args: l1..l6, range(...),",
    "{...} or any list expr",
    "sort_asc(l1) sorts in place",
    "cumsum(l1), delta_list(l1)",
    "seq(x^2,x,1,10,1)->l1",
    "type list(s) for the editor",
    "#STATS (stat cmd)",
    "1-Var/2-Var over l1..l6,",
    "optional freq list (1-Var)",
    "10 regressions: Lin Quad",
    "Cubic Quart Ln Exp Pwr",
    "Logistic Sin Med-Med",
    "Store to fills a y slot",
    "with the fitted model",
    "#DISTRIBUTIONS (dist cmd)",
    "normal, t, chisq, f:",
    " _pdf, _cdf(lo,hi,..), _inv",
    "cdf is P(lo<=X<=hi); use",
    "-1e99/1e99 for open tails",
    "inv takes lower-tail area",
    "binomial, poisson,",
    "geometric: _pmf, _cdf(k..)",
    "k, n must be integers",
    "see FUNC tab for signatures",
};
constexpr int kSyntaxCount = sizeof(kSyntaxLines) / sizeof(kSyntaxLines[0]);

// Column where the function summary starts ("seq(f,v,lo,hi,st)" = 17
// chars is the widest signature; summaries then get ~20 chars).
constexpr int kSummaryCol = 19;

void draw_text_line(gfx::Framebuffer& fb, const gfx::Font& font, int y, const char* text) {
    using namespace platform::colors;
    if (text[0] == '#') {
        font.draw_string(fb, 4, y, text + 1, kGreen);
    } else {
        font.draw_string(fb, 12, y, text, kWhite);
    }
}

}  // namespace

int HelpScreen::line_count() const {
    if (tab_ == 0) {
        int n = 0;
        math::catalog(&n);
        return n;
    }
    return tab_ == 1 ? kKeysCount : kSyntaxCount;
}

int HelpScreen::max_scroll() const {
    const int extra = line_count() - kVisibleLines;
    return extra > 0 ? extra : 0;
}

bool HelpScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kLeft:
            tab_ = (tab_ + kTabCount - 1) % kTabCount;
            scroll_ = 0;
            return true;
        case Key::kRight:
            tab_ = (tab_ + 1) % kTabCount;
            scroll_ = 0;
            return true;
        case Key::kUp:
            if (scroll_ > 0) {
                --scroll_;
            }
            return true;
        case Key::kDown:
            if (scroll_ < max_scroll()) {
                ++scroll_;
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void HelpScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);

    // Title bar with tabs; the active one is highlighted.
    fb.fill_rect(0, 0, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "HELP", kGrayLine);
    int tx = 4 + 6 * font.width();
    for (int t = 0; t < kTabCount; ++t) {
        const int w = font.text_width(kTabNames[t]);
        if (t == tab_) {
            fb.fill_rect(tx - 2, 0, w + 4, 16, platform::Color::from_rgb(0, 0, 90));
        }
        font.draw_string(fb, tx, 2, kTabNames[t], t == tab_ ? kWhite : kGrayLine);
        tx += w + 12;
    }

    const int count = line_count();
    for (int row = 0; row < kVisibleLines; ++row) {
        const int i = scroll_ + row;
        if (i >= count) {
            break;
        }
        const int y = kTopY + row * kLineH;
        if (tab_ == 0) {
            int n = 0;
            const math::FnDescriptor* cat = math::catalog(&n);
            font.draw_string(fb, 4, y, cat[i].signature, kGreen);
            // Long signatures (normal_cdf(lo,hi,mu,sd) = 23 chars, 3C)
            // push their summary right instead of overlapping.
            const int sig = static_cast<int>(std::strlen(cat[i].signature));
            const int col = sig + 1 > kSummaryCol ? sig + 1 : kSummaryCol;
            font.draw_string(fb, 4 + col * font.width(), y, cat[i].summary, kWhite);
        } else {
            draw_text_line(fb, font, y, tab_ == 1 ? kKeysLines[i] : kSyntaxLines[i]);
        }
    }

    // Scroll indicator when content overflows (right end of the title bar).
    if (max_scroll() > 0) {
        char pos[16];
        std::snprintf(pos, sizeof(pos), "%d/%d", scroll_ + 1, max_scroll() + 1);
        font.draw_string(fb, platform::kScreenW - 4 - font.text_width(pos), 2, pos, kGrayLine);
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "LT/RT:TAB  UP/DN:SCROLL  ESC:BACK", kGrayLine);
}

HelpScreen& help_screen() {
    static HelpScreen instance;
    return instance;
}

}  // namespace apps
