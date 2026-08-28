// Entry point for the host build (Phase 6.4.0).
//
// This is deliberately NOT a port of src/main.cpp. That file is 885 lines
// and most of them are about a board: the watchdog reboot cause, the
// fault record, stack painting, the SD DET hot-plug poll, the D14/D26
// late-init retry heartbeat, and six 30-second telemetry printouts. None
// of it has a referent here, and copying it would be the fork's mistake
// in miniature -- code that looks maintained because it compiles.
//
// What is kept is the startup *sequence*, in the firmware's order, so the
// two files can be read side by side and any divergence is a diff rather
// than an archaeology problem.
//
// Usage: graphite-shot --shot <file.ppm>

#include <cstdio>
#include <cstring>
#include <vector>

#include "platform/app_registry.hpp"
#include "platform/keyboard.hpp"
#include "platform/platform.hpp"
#include "platform/power.hpp"
#include "platform/sd_apps.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/screen_manager.hpp"
#include "math/lists.hpp"
#include "math/matrix.hpp"
#include "math/named_lists.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_model.hpp"
#include "apps/home_screen.hpp"
#include "apps/notepad_screen.hpp"
#include "apps/program_screen.hpp"
#include "host/display_headless.hpp"
#include "host/keyscript.hpp"
#include "scripting/calc_canvas.hpp"
#include "scripting/micropython_embed.hpp"

namespace host {

// Defined in platform_host.cpp -- see the comment there for why main has to
// be the one to capture this.
void set_stack_top(char* addr);

namespace {
bool g_exit_requested = false;
}

// Called by the MODE screen's reboot row, which has no bootloader to
// reach on a desktop (see the guard in apps/mode_screen.cpp).
void request_exit() {
    g_exit_requested = true;
}

bool exit_requested() {
    return g_exit_requested;
}

}  // namespace host

namespace {

// Mirrors register_builtin_apps() in src/main.cpp. An explicit list, not
// per-translation-unit self-registration, so the launcher's row order is
// visible in one place (D67) -- and so the host shows the same rows in
// the same order as the board, which a screenshot depends on.
void register_builtin_apps() {
    platform::AppEntry notepad = {};
    notepad.name = "Notepad";
    notepad.kind = platform::AppKind::kBuiltIn;
    notepad.launch = [](const platform::AppEntry&) {
        ui::screen_manager().push(&apps::notepad_screen());
    };
    platform::AppRegistry::register_app(notepad);

    platform::AppEntry python = {};
    python.name = "Python";
    python.kind = platform::AppKind::kBuiltIn;
    python.launch = [](const platform::AppEntry&) {
        apps::program_screen().open_editor();
        ui::screen_manager().push(&apps::program_screen());
    };
    platform::AppRegistry::register_app(python);

    platform::AppEntry files = {};
    files.name = "Files";
    files.kind = platform::AppKind::kBuiltIn;
    files.launch = [](const platform::AppEntry&) {
        ui::screen_manager().push(&apps::files_screen());
    };
    platform::AppRegistry::register_app(files);
}

// The one thunk every SD-discovered app shares (6B.16, D67). Same shape as
// src/main.cpp's, and the reason AppLaunchFn takes the entry rather than
// being a bare void(*)(): none of these apps exists until the scan runs.
void launch_sd_app(const platform::AppEntry& self) {
    apps::program_screen().queue_app(self.path, self.name);
    ui::screen_manager().push(&apps::program_screen());
}

void usage(const char* argv0) {
    std::fprintf(
        stderr,
        "usage: %s [--eval <e>] [--key <k>] [--keyscript <f>] [--run <s.py>] --shot <out.ppm>\n"
        "\n"
        "  --eval   submit a line to the home screen before rendering,\n"
        "           exactly as PICOCALC_SERIAL_INJECT does on the board.\n"
        "           Repeatable; order is preserved.\n"
        "  --run    run a Python file through the same exec_file() the\n"
        "           program screen uses for an SD app.\n"
        "  --key    queue one key (a name like up/enter/esc/f1, or a\n"
        "           single character). Repeatable; order is preserved.\n"
        "  --keyscript  replay a file of key names (D97). Same names,\n"
        "           whitespace-separated, # comments to end of line.\n"
        "\n"
        "Storage root: $PICOCALC_HOME, else $HOME/.picocalc\n",
        argv0);
}

}  // namespace

int main(int argc, char** argv) {
    // FIRST, before any other frame exists: MicroPython's GC scans for roots
    // from here down, so this must sit above everything the calculator will
    // run in. The firmware gets it from the linker (__StackTop); a desktop
    // has to take it from main's own frame.
    char stack_anchor = 0;
    host::set_stack_top(&stack_anchor + 1);

    const char* shot_path = nullptr;
    std::vector<const char*> eval_lines;
    const char* run_script = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shot_path = argv[++i];
        } else if (std::strcmp(argv[i], "--eval") == 0 && i + 1 < argc) {
            eval_lines.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "--run") == 0 && i + 1 < argc) {
            run_script = argv[++i];
        } else if (std::strcmp(argv[i], "--keyscript") == 0 && i + 1 < argc) {
            if (!host::run_keyscript(argv[++i])) {
                return 1;
            }
        } else if (std::strcmp(argv[i], "--key") == 0 && i + 1 < argc) {
            if (!host::queue_key(argv[++i])) {
                std::fprintf(stderr, "unknown key name: %s\n", argv[i]);
                return 2;
            }
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (shot_path == nullptr) {
        usage(argv[0]);
        return 2;
    }

    const platform::InitStatus status = platform::init();

    // Launched on the board so core 1 can DMA strips; a no-op here, and
    // kept because the call is part of the sequence rather than because
    // it does anything (see the guard in gfx/framebuffer.cpp).
    gfx::start_display_service();

    apps::home_screen().load_state();
    apps::load_graph_state();
    // Every one of these is all-or-nothing and returns false while
    // storage is down -- which it always is until 6.4.2. The firmware's
    // late-init loop retries; there is nothing to retry here yet.
    (void)math::lists().load(platform::storage());
    (void)math::named_lists().load(platform::storage());
    (void)math::matrices().load(platform::storage());
    (void)math::load_ans(platform::storage());
    (void)platform::power::load(platform::storage());

    register_builtin_apps();
    // Tier 2: a directory under <root>/apps/ with an app.txt is a launcher
    // tile, same as on the card. This is the path 6.4.2 exists to make
    // reachable -- SD app loading is the newest code in the tree and the
    // hardest to exercise, because it needs a card prepared by hand.
    const int sd_apps = platform::scan_sd_apps(&launch_sd_app);

    auto& mgr = ui::screen_manager();
    mgr.push(&apps::home_screen());

    // Mirrors the firmware's serial-injection path (main.cpp, under
    // PICOCALC_SERIAL_INJECT): submit to the home screen, report what came
    // back. Not a key script -- that is 6.4.4, and it reaches screens this
    // cannot. This reaches the evaluator, which is what persistence and a
    // home-screen screenshot need.
    for (const char* line : eval_lines) {
        const char* result = nullptr;
        const char* kind = nullptr;
        const bool ok = apps::home_screen().submit_line(line, &result, &kind);
        if (!ok) {
            std::fprintf(stderr, "eval: rejected \"%s\"\n", line);
            return 1;
        }
        if (result == nullptr) {
            std::printf("eval: \"%s\" -> command\n", line);
        } else {
            std::printf("eval: \"%s\" -> \"%s\" kind=%s\n", line, result, kind);
        }
    }

    // Same entry point the program screen uses for an SD app (6B.15/16), so
    // this exercises the streaming file reader rather than a shortcut.
    if (run_script != nullptr) {
        auto& py = scripting::python();
        if (!py.init()) {
            std::fprintf(stderr, "run: interpreter would not start\n");
            return 1;
        }
        // No output callback. mp_port.c's mp_hal_stdout_tx_strn_cooked
        // already printfs everything a script prints AND fans it out to the
        // callback -- the callback exists so the on-device output pane sees
        // it too. Registering one here just printed every line twice.
        const bool ok = py.exec_file(run_script);
        if (!ok) {
            std::fprintf(stderr, "run: %s raised\n", run_script);
            return 1;
        }
    }

    // Drain whatever the key script queued into the UI, exactly as the
    // firmware's main loop does -- render only when something changed, and
    // let HOME pop to root. Bounded by the queue, not by a frame budget:
    // there is no user here to out-type us.
    while (host::keys_pending()) {
        const platform::KeyEvent ev = platform::keyboard().poll();
        if (ev.key == platform::Key::kNone || !ev.pressed) {
            continue;
        }
        if (ev.key == platform::Key::kHome && mgr.current() != &apps::home_screen()) {
            mgr.pop_to_root();
        } else {
            mgr.handle_key(ev);
        }
    }

    // A script that took the panel keeps it (D80): repainting the chrome
    // over its canvas is exactly what the firmware refuses to do, and here
    // it would silently replace the thing we came to photograph.
    if (!scripting::canvas::owns_display()) {
        mgr.render_frame();
    }

    if (!host::write_ppm(shot_path)) {
        std::fprintf(stderr, "graphite-shot: could not write %s\n", shot_path);
        return 1;
    }
    std::printf("graphite-shot: wrote %s (storage=%d psram=%d sd-apps=%d)\n", shot_path,
                status.storage ? 1 : 0, status.psram ? 1 : 0, sd_apps);
    return 0;
}
