# Third-Party Notices

The [root MIT License](LICENSE) applies to AriaAgent's own code. Third-party
libraries, SDKs and runtime files keep their own licenses.

[dependencies.json](dependencies.json) records requests and exact resolved
versions/revisions. The table describes its current entries, not every possible
source/target/version override. See the [dependency guide](docs/dependencies.md)
and [English update guide](docs/dependency-updates.en.md).

| Component | Current resolution | License | Use |
|---|---|---|---|
| [Aria](https://github.com/dqsjqian/Aria) | 3.1.0 | MIT | Framework, bindings and Qt adapter. |
| [Mira](https://github.com/dqsjqian/Mira) | 1.0.0 | MIT | HTTP/1.1 and TLS client transport; HTTP/2 and HTTP/3 are disabled. |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | Messages, tools, settings and storage. |
| [OpenSSL](https://github.com/openssl/openssl) | 4.0.3 | Apache-2.0 | TLS through Mira; the bundled source build is static. |
| [Qt Core, Gui and Widgets](https://doc.qt.io/qt-6/licensing.html) | Selected installed Qt 6 SDK | LGPL-3.0-only, with GPL/commercial alternatives as specified upstream | Desktop UI and platform plugins. |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT; embedded portions under Boost-1.0 | Available in the shared dependency file for Aria tests; embedded Aria tests are disabled by default. |

Aria's selected [LICENSE](https://github.com/dqsjqian/Aria/blob/7f957a8764e69d02e8487a2d128ace9f0605ccdd/LICENSE)
and third-party notices apply to its code. Mira's
[LICENSE](https://github.com/dqsjqian/Mira/blob/9386d89d2a259303a0a2c3b1539a5cb0e3fc0be5/LICENSE)
credits Copyright (c) 2026 dqsjqian. nlohmann/json's
[LICENSE.MIT](https://github.com/nlohmann/json/blob/55f93686c01528224f448c19128836e7df245f72/LICENSE.MIT)
credits Copyright (c) 2013–2025 Niels Lohmann; embedded MIT portions additionally
credit Evan Nemerson (2016–2021, Hedley), Florian Loitsch (2009, `to_chars`),
Björn Hoehrmann (2008–2009), and The Abseil Authors (2018).
Mira 1.0.0 also supplies its own third-party inventory; WebSocket, HTTP/2 and
HTTP/3 are disabled here, so their zlib/ng-series dependencies are not added to
this build. The enabled HTTP/1 and TLS path requires OpenSSL.
OpenSSL's selected
[LICENSE.txt](https://github.com/openssl/openssl/blob/af1775b60dfa141a4ad762585052cabeb9f37e9e/LICENSE.txt)
contains Apache-2.0; that source archive has no separate `NOTICE` file.
OpenSSL's Text::Template 1.56 is a build-only tool under GPL-1.0-or-later or
Artistic-1.0, not an AriaAgent runtime library.

## Distribution boundary

The source repository does not contain the downloaded dependency caches or Qt
SDK. Fetching dependencies for a local build is distinct from distributing them
as source, static code, DLLs or plugins. Include full applicable licenses and
attributions in any binary package; references in this document do not replace
those copies. Statically incorporated JSON, Mira and OpenSSL code still needs
its applicable notices.

The app build stages this project's and the selected Aria source's available
`LICENSE`/`THIRD_PARTY_NOTICES.md` beside the executable under `licenses/`.
It also copies the actual selected Mira/JSON/OpenSSL source licenses and
`NOTICE*`/`THIRD_PARTY_NOTICES*` files, and collects all JSON header SPDX attributions in UTF-8.
A parent-provided target without a known source produces a warning; obtain its
matching license materials before distribution.
`scripts/deploy-dlls.ps1` copies Qt/plugins and other runtime DLLs; this staging
does not assemble a complete third-party license bundle, corresponding Qt
source, or relinking materials. Qt's bundled third-party components and compiler runtimes
must be inventoried for the actual kit and deployed file set; they are not all
MIT. The current CI uploads configuration diagnostics, not application binaries.

Using Qt under LGPLv3 requires the specified notices and LGPLv3/GPLv3 license
copies. An appropriate replaceable shared-library mechanism is one route under
section 4(d)(1); static linking can instead use section 4(d)(0) with corresponding
library source and application material suitable for relinking. Distributing
Qt binaries also requires a permitted way to provide their corresponding source,
including applicable modifications/build material; installation information may
be required in the circumstances stated by the license. This project does not
claim to provide that complete Qt delivery package. Read the actual
[LGPLv3](https://doc.qt.io/qt-6/lgpl.html), [GPLv3 section 6](https://doc.qt.io/qt-6/gpl.html)
and [Qt third-party notices](https://doc.qt.io/qt-6/third-party-libraries.html)
for the selected SDK.

GCC runtimes remain subject to their licenses and
[Runtime Library Exception](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html).
Microsoft runtime files follow
[Microsoft's redistribution terms](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170).
Do not treat a local debug deployment directory as a ready-to-publish release.
Changing dependencies or the deployment format requires reviewing the resulting
licenses and materials again; keeping AriaAgent's own code MIT does not waive
third-party obligations.
