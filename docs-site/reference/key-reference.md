# Key Reference

> **Generated file — do not edit by hand.** Regenerate with
> `python3 scripts/gen-doc-reference.py` (source: `src/apps/help_screen.cpp (kKeysLines)`).

Mirrors the on-device `F5` HELP screen's tab of the same name.
Lines are curated for a narrow on-device display column; wrapped
continuations (marked by a leading space in the source) are
rejoined below, everything else is kept as its own line verbatim.

## EVERY SCREEN

- `F1 EDIT  editor (Y=/PAR/POL)`
- `F2 WIN   window settings`
- `F3 MODE  mode settings`
- `F4 TRC   trace (opens graph)`
- `F5 GRPH  graph <-> table`
- `F6 APPS  app launcher (home)`
- `HOME     back to home screen`
- `ESC      back / cancel edit`

## COMMANDS (type on home)

- `help (?) this help`
- `apps     app launcher (also F6; app also ok)`
- `diag     diagnostics screen`
- `files    SD file browser`
- `cas      CAS operations menu`
- `lists    data list editor`
- `stats    statistics screen (list/stat also ok)`
- `dist     distribution helper`
- `test     inference (infer ok)`
- `plot     stat plot setup`
- `calc     graph analysis menu`
- `py <stmt> run one line of Python; state is kept`
- `cls      clear screen (keeps input history)`
- `clrhist  erase all history`

## HOME

- `UP       recall last entry`
- `UP/DOWN  walk input history`
- `Alt/Ctrl+UP/DOWN scroll view`
- `Alt+ENTER  decimal result; on an empty line, redo the last exact one as decimal`

## GRAPH

- `F4 TRC   toggle trace`
- `F5 TBL   value table`
- `Alt+F5   split graph|table`
- `- / =    zoom out / in`
- `S / T    ZStandard / ZTrig`
- `F        ZoomFit (y to curves)`
- `Z        ZoomStat (stat plots)`
- `D / Q    ZDecimal / ZSquare`
- `B        ZBox (2-corner zoom)`
- `H        Shade(lower,upper);`
- `Y= 'S'   shade above/below`
- `L        toggle axis labels`
- `LT/RT    move trace cursor`
- `UP/DOWN  next curve (trace)`
- `F6 CALC  analyze: value zero`
- `min max intersect dy/dx int;`
- `ENTER places bounds/points,`
- `ESC cancels; root -> x, ans`

## TABLE

- `UP/DOWN  scroll rows`
- `LT/RT    scroll columns`
- `ENTER    add value (ASK mode)`
- `DEL      delete row (ASK)`
- `F2 SETP  table setup`
- `Alt+F5   split graph|table`

## SPLIT (graph|table)

- `F5       switch focused pane`
- `Alt+F5 / ESC  back to full`
- `F4       trace (graph pane)`
- `trace <-> table row sync`

## EDITORS (Y=, PAR, POLAR, SEQ)

- `ENTER    edit field`
- `SPACE    toggle enable`
- `DEL      clear field`
- `F5       graph`

## APPS (F6 or apps cmd)

- `UP/DOWN  select app`
- `ENTER    open (1-9 also ok)`
- `ESC      back to home`
- `in an app, ESC returns to`
- `the launcher, HOME to home`

## NOTEPAD (Apps > Notepad)

- `arrows   move cursor`
- `F2 SAVE  F3 LOAD  F4 NEW`
- `ESC      leave (twice if unsaved)`
- `notes are .txt files in`
- `/picocalc/notes`

## FILES (files cmd)

- `UP/DOWN  select entry`
- `ENTER    open folder, or open a file in its app`
- `LEFT/ESC up one folder; ESC at the top leaves`
- `HOME     leave from any depth`
- `F2 CUT   mark for moving`
- `F3 MOVE  move it here`
- `F4 REN   rename`
- `F5 MKDIR new folder`
- `DEL      delete (confirms; folders must be empty)`

## LIST EDITOR (lists cmd)

- `arrows   move cell`
- `ENTER/type  edit or append`
- `DEL      delete row`
- `F6/F7 (Shift+F1/F2) sort`
- `F8 (Shift+F3) clear list`

## STATS (stats cmd)

- `UP/DOWN  select row`
- `LT/RT    change value`
- `ENTER    calculate (last row)`
- `results: UP/DOWN scroll`

## DIST (dist cmd)

- `LT/RT    distribution / fn`
- `ENTER    edit param field`
- `DEL      clear + edit empty`
- `ENTER    calculate (last row)`
- `result -> ans; call shown`

## TEST (test cmd)

- `LT/RT    test / option cycle`
- `ENTER    edit field / calc`
- `Data/Stats source where avail`

## STAT PLOTS (plot cmd)

- `3 slots: scatter, xy-line,`
- `histogram, box, norm prob`
- `draw with funcs on graph;`
- `Z on graph = ZoomStat`

## WINDOW / TABLE SETUP

- `ENTER    edit value`
- `DEL      clear + edit empty`

## MODE

- `LT/RT    change value`
- `ENTER    select / reboot row`
