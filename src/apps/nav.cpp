#include "apps/nav.hpp"

#include "ui/screen_manager.hpp"
#include "apps/graph_screen.hpp"
#include "apps/param_editor.hpp"
#include "apps/polar_editor.hpp"
#include "apps/seq_editor.hpp"
#include "apps/y_editor.hpp"
#include "graph/graph_state.hpp"

namespace apps {

void push_mode_editor() {
    switch (graph::state().mode) {
        case graph::Mode::kParametric:
            ui::screen_manager().push(&param_editor_screen());
            break;
        case graph::Mode::kPolar:
            ui::screen_manager().push(&polar_editor_screen());
            break;
        case graph::Mode::kSeq:
            ui::screen_manager().push(&seq_editor_screen());
            break;
        default:
            ui::screen_manager().push(&y_editor_screen());
            break;
    }
}

void goto_graph_trace() {
    graph_screen().start_trace();
    ui::screen_manager().switch_to(&graph_screen());
}

}  // namespace apps
