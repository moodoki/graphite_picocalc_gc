# The smallest SD app that proves the whole path (6B.15/6B.16).
#
# It draws, so it takes the screen: what is on the panel when this ends
# is what this script put there, not an output pane. ESC goes back to
# the launcher.

import calc

BG = (0, 0, 60)

calc.clear_screen(BG)
calc.draw_rect(16, 24, 288, 190, "white")
calc.draw_text(32, 44, "Hello from the SD card", "white", BG)
calc.draw_text(32, 74, "This app is a directory:", "gray", BG)
calc.draw_text(32, 94, "/picocalc/apps/hello/", "cyan", BG)

# An app is not a sandbox — it is the same interpreter the RUN key uses,
# reaching the same calculator. Anything stored here is still there
# afterwards, on the home screen.
calc.store("k", 7)
calc.draw_text(32, 134, "k = 7, so 2^k = " + str(calc.eval("2^k")), "yellow", BG)

calc.draw_text(32, 184, "ESC returns to the launcher", "gray", BG)
