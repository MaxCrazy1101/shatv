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

### AUR package: `shatv-git`

The AUR source package should be `shatv-git`:

- build from the upstream Git repository;
- require the upstream Git repository to be anonymously cloneable before AUR
  publication;
- install through the same CMake install rules used by the Debian package;
- enable `SHATV_ENABLE_ASR=ON`;
- use `/usr` as `SHATV_SHERPA_ONNX_ROOT` and `SHATV_ONNXRUNTIME_ROOT`;
- depend on the Arch/AUR `sherpa-onnx` and `onnxruntime` package contracts
  instead of bundling native ASR libraries;
- install `/usr/bin/shatv`, `top.shanana.shatv.desktop`,
  `top.shanana.shatv.metainfo.xml`, the placeholder icon, and package notices;
- do not include ASR model archives or extracted model files.
- provide `shatv` and `shatv-asr`;
- conflict with `shatv`, `shatv-bin`, `shatv-asr`, and `shatv-asr-bin`.

Recommended Arch dependencies:

- runtime `depends`: `qt6-base`, `qt6-declarative`, `qt6-multimedia`,
  `qt6-shadertools`, `ffmpeg`, `libarchive`, `sherpa-onnx`, `onnxruntime`,
  `zlib`;
- build `makedepends`: `cmake`, `git`, `ninja`, `pkgconf`, `qt6-tools`,
  `toml11`;
- validation `checkdepends`: `desktop-file-utils`, `appstream`, optionally
  `namcap`.

`onnxruntime` may be satisfied by a provider package such as
`onnxruntime-cuda`. The package should keep that dependency as the virtual
`onnxruntime` name so pacman can resolve the provider.

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

### Binary AUR packages

Reserve `shatv-bin` for a future binary AUR package. Do not use `shatv-bin`
for the first source package.

Reserve `shatv-asr-bin` only if an ASR-capable binary package intentionally
consumes upstream binary sherpa-onnx/ONNX Runtime artifacts. Binary packages
must still exclude model archives and extracted model files.

### Publication requirements

- The upstream Git repository must be public or otherwise anonymously
  cloneable; AUR users cannot build from a private GitHub source URL.
