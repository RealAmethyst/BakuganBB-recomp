# Prism dependency notices

## Upstream source and license

These changes modify `source/backends/orca.cpp` from [Prism](https://github.com/ethindp/prism), version 0.18.3, revision `b68e9c94240cefb1f08b2306c26376e3ab300a7a`.

The affected file retains its `SPDX-License-Identifier: MPL-2.0` header. The patch is distributed under MPL-2.0, reproduced unchanged in [LICENSE.prism](LICENSE.prism). The project's BSD license does not replace those terms. [NOTICE.prism](NOTICE.prism) is the unchanged upstream notice and preserves its dependency and author attribution.

## Local changes and authorship

The local patch requires an explicit existing Unix session-bus address, prevents D-Bus service activation, bounds connection and method waits, and checks reader ownership before speech operations. It changes no public Prism API.

The local modifications were authored by Codex. They have not been submitted to or accepted by Prism upstream, and no upstream endorsement is claimed. Prism's pinned contribution guidelines reject wholly AI-authored upstream contributions; this patch must not be represented as human-authored or submitted on that basis.

## Binary packaging

Retain the upstream `LICENSE`, `NOTICE` and complete `LICENSES` directory when packaging a built Prism library. Keep the exact source revision and patch available with the corresponding-source information. The upstream CMake install rules do not install this license tree automatically. The source tree also includes Highway's license even though the upstream notice does not list Highway in its prose; preserve it.

This directory contains source changes and notices only. It does not include Prism binaries or bundled dependency source. System libraries keep their own licenses and distribution requirements.
