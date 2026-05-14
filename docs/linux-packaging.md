# Linux Packaging

ShaTV publishes Debian packages for the first Linux alpha packaging path. The
initial target is Ubuntu 26.04 on amd64.

## Packages

- `shatv-ubuntu-26.04-amd64.deb`
  - Debian package name: `shatv`
  - Installs `/usr/bin/shatv`
  - Does not bundle ASR native runtime libraries
  - Does not bundle ASR model files or model archives

- `shatv-ubuntu-26.04-amd64-asr.deb`
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

The Linux CI job builds both Debian packages inside an `ubuntu:26.04`
container. The GitHub-hosted runner may still use another supported Linux
label because GitHub Actions does not provide an `ubuntu-26.04` runner label
yet.

- Pull requests upload both `.deb` files as a short-retention artifact.
- Main-branch pushes upload both `.deb` files directly to the `alpha` release
  with overwrite semantics.
- The workflow is push/PR driven and does not require manual dispatch.

## AUR

Arch Linux AUR packaging should be a source-build path, not a repack of the
Debian package and not a GitHub Release binary artifact.

### First package: `shatv-git`

The first AUR package should be `shatv-git`:

- build from the upstream Git repository;
- require the upstream Git repository to be anonymously cloneable before AUR
  publication;
- install through the same CMake install rules used by the Debian package;
- keep `SHATV_ENABLE_ASR=OFF`;
- install `/usr/bin/shatv`, `top.shanana.shatv.desktop`,
  `top.shanana.shatv.metainfo.xml`, the placeholder icon, and package notices;
- do not include ASR model archives or extracted model files.
- provide `shatv`;
- conflict with `shatv` and the future binary package name `shatv-bin`.

Recommended Arch dependencies:

- runtime `depends`: `qt6-base`, `qt6-declarative`, `qt6-multimedia`,
  `qt6-shadertools`, `ffmpeg`, `libarchive`, `zlib`;
- build `makedepends`: `cmake`, `git`, `ninja`, `pkgconf`, `qt6-tools`,
  `toml11`;
- validation `checkdepends`: `desktop-file-utils`, `appstream`, optionally
  `namcap`.

Expected local validation:

```bash
cd packaging/arch/shatv-git
makepkg --verifysource
makepkg -Ccrs
makepkg --printsrcinfo > .SRCINFO
namcap PKGBUILD
namcap shatv-*.pkg.tar.zst
pacman -Qlp shatv-*.pkg.tar.zst
sudo pacman -U ./shatv-*.pkg.tar.zst
desktop-file-validate /usr/share/applications/top.shanana.shatv.desktop
appstreamcli validate --no-net /usr/share/metainfo/top.shanana.shatv.metainfo.xml
sudo pacman -Rns shatv-git
```

### Binary and ASR-capable AUR packages

Reserve `shatv-bin` for a future binary AUR package. Do not use `shatv-bin`
for the first source package.

Defer ASR-capable AUR packages until the native ASR dependency path is decided
for Arch. The current release package bundles sherpa-onnx and ONNX Runtime for
ASR-capable artifacts, but the AUR package should not depend on an unpublished
or unstable local SDK path.

Later options:

- `shatv-asr`: source-build ShaTV package that depends on verified Arch/AUR
  `sherpa-onnx` and `onnxruntime` packages;
- `shatv-asr-bin`: only if an ASR-capable package intentionally consumes upstream binary
  sherpa-onnx/ONNX Runtime artifacts.

Both ASR options must still exclude model archives and extracted model files.

### Publication requirements

- The upstream Git repository must be public or otherwise anonymously
  cloneable; AUR users cannot build from a private GitHub source URL.
