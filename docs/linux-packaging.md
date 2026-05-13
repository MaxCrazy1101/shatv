# Linux Packaging

ShaTV publishes Debian packages for the first Linux alpha packaging path. The
initial target is Ubuntu 24.04 on amd64.

## Packages

- `shatv-ubuntu-24.04-amd64.deb`
  - Debian package name: `shatv`
  - Installs `/usr/bin/shatv`
  - Does not bundle ASR native runtime libraries
  - Does not bundle ASR model files or model archives

- `shatv-ubuntu-24.04-amd64-asr.deb`
  - Debian package name: `shatv-asr`
  - Installs `/usr/bin/shatv`
  - Bundles sherpa-onnx and ONNX Runtime under the private app library directory
  - Uses the system `libarchive` package for ASR model archive extraction
  - Does not bundle ASR model files or model archives

The `shatv` and `shatv-asr` Debian packages conflict with and replace each
other because both packages own `/usr/bin/shatv`.

## Installed Files

Both packages install:

- `/usr/bin/shatv`
- `top.shanana.shatv.desktop`
- `top.shanana.shatv.metainfo.xml`
- a placeholder `shatv.svg` icon
- package notices and third-party source notes under `/usr/share/doc`

The placeholder icon is temporary and should be replaced with final branding
before a stable release.

## CI Policy

The Linux CI job builds both Debian packages on `ubuntu-24.04`.

- Pull requests upload both `.deb` files as a short-retention artifact.
- Main-branch pushes upload both `.deb` files directly to the `alpha` release
  with overwrite semantics.
- The workflow is push/PR driven and does not require manual dispatch.

## AUR

Arch Linux AUR packaging is deferred until the Linux install rules and Debian
package behavior are stable. The AUR path should publish a `PKGBUILD` source
build entry rather than a GitHub Release binary artifact.
