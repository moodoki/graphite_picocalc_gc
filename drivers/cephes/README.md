# cephes (vendored subset)

Special functions for the Phase 3C probability distributions (task
3C.1, `phase3-spec.md` §5.3), cherry-picked from the **cephes
`cprob`** package.

- **Source**: https://www.netlib.org/cephes/cprob.tgz (fetched
  2026-07-19)
- **Author**: Stephen L. Moshier. Copyright 1984-1995 by Stephen L.
  Moshier. Distribution statement from the netlib archive readme:
  "What you see here may be used freely but it comes with no support
  or guarantee." (Full statement: https://www.netlib.org/cephes/readme
  — see also `NOTICE.md` at the repo root.)
- **Vendored files** (unmodified, read-only per AGENTS.md):
  `mconf.h` (UNK portable arithmetic — correct for both host and ARM),
  `const.c`, `polevl.c`, `mtherr.c`, `gamma.c` (gamma, lgam),
  `ndtr.c` (ndtr, erf, erfc), `expx2.c`, `ndtri.c`,
  `igam.c` (igam, igamc), `igami.c`, `incbet.c`, `incbi.c`.
- **Not vendored**: the integer-df convenience wrappers (`stdtr.c`,
  `chdtr.c`, `fdtr.c`, `bdtr.c`, `pdtr.c`) — `src/math/dist.cpp`
  builds t/chi-square/F/binomial/Poisson directly on
  `incbet`/`incbi`/`igam`/`igamc`/`ndtr`/`ndtri`, which supports
  real-valued degrees of freedom.

## Build notes

The CMake target `cephes` compiles these with symbol renames
(`gamma=cephes_gamma`, `erf=cephes_erf`, `erfc=cephes_erfc`) so they
can never collide with the libm/newlib symbols of the same names; the
renames are applied uniformly to every TU in the target, so
intra-library references stay consistent. Callers (`src/math/dist.cpp`)
declare the renamed symbols `extern "C"`. Vendored code is compiled
with warnings suppressed (`-w`) — it predates modern warning sets and
is treated as read-only third-party code.
