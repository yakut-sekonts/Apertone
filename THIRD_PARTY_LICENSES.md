# Third-party dependencies

Luma's original source is Apache-2.0. Dependencies retain their own licenses.

- **Qt 6.10.3** — https://www.qt.io / https://download.qt.io/archive/qt/6.10/6.10.3/
  Core, Gui, Qml, Quick, QuickControls2, native dialogs, tools and SVG plugins.
  Used open-source modules are dynamically linked under **LGPL-3.0**, with
  module-specific BSD/Apache/MIT components and exceptions in the official SDK.
  GPL-only modules such as Qt Charts are not used. Official license files and
  `qt_attribution.json` from the used SDK must accompany redistributed bundles.
- **LibRaw 0.21.5** — https://www.libraw.org / https://github.com/LibRaw/LibRaw
  RAW sensor decoding and camera metadata. Dual LGPL-2.1/CDDL-1.0; this project
  chooses **LGPL-2.1** and links it dynamically. Optional GPL demosaic packs,
  RawSpeed and proprietary DNG SDK are disabled. `LICENSE.LGPL`, `COPYRIGHT` and
  the exact source archive accompany the bundle.
- **SQLite 3.50.4** — https://sqlite.org/releaselog/3_50_4.html
  Transactional catalog. **Public domain**. Statically compiled from the pinned
  amalgamation; no dependency on the host SQLite library.
- **LittleCMS 2.17** — https://github.com/mm2/Little-CMS/tree/lcms2.17
  ICC conversion. **MIT**. Statically compiled, with the upstream LICENSE notice.
- **Apple system frameworks** — CommonCrypto, AppKit and ColorSync from the
  system SDK. Used for SHA-256 and monitor profiles; no Apple SDK or framework
  binary is redistributed.

Build tools are not part of the app runtime. The locked toolchain includes
aqtinstall (MIT), CMake (BSD-3-Clause), Ninja (Apache-2.0), clang-format/clang-tidy
(Apache-2.0 with LLVM exception) and their Python dependencies. See the lock file
for exact versions and each upstream distribution for its license notices.

Redistribution must preserve notices, provide the corresponding LGPL sources
and permit replacement/relinking of LGPL libraries. A local debug build does not
justify omitting those materials from a public release. The packaging script
includes them; release verification must audit the final bundle.
