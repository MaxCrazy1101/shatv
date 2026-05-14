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

### Binary AUR package draft: `shatv-bin`

`shatv-bin` should be published only for immutable versioned releases, not for
the continuously overwritten `alpha` release. The package should consume a fixed
release asset URL and a fixed `sha256sums` value.

The binary package should match the current AUR source-package policy:

- package name: `shatv-bin`;
- default ASR support enabled;
- install `/usr/bin/shatv`;
- provide `shatv` and `shatv-asr`;
- conflict with `shatv`, `shatv-git`, `shatv-asr`, and `shatv-asr-bin`;
- exclude ASR model archives and extracted model files.

The release asset used by `shatv-bin` should be built in an Arch Linux container
rather than repacked from the Ubuntu `.deb`. The asset should contain the
install prefix payload directly:

```text
usr/bin/shatv
usr/lib/shatv/...
usr/share/applications/top.shanana.shatv.desktop
usr/share/metainfo/top.shanana.shatv.metainfo.xml
usr/share/icons/...
usr/share/doc/shatv/...
```

Suggested release asset names:

- `shatv-arch-x86_64-asr.tar.zst`
- `SHA256SUMS`

`shatv-bin` should depend on Arch runtime packages for Qt, FFmpeg, libarchive,
and other system libraries. It should not depend on AUR `sherpa-onnx` or
`onnxruntime` if the release asset intentionally bundles the selected
sherpa-onnx/ONNX Runtime shared libraries under `/usr/lib/shatv`.

Recommended `shatv-bin` dependency direction:

- runtime `depends`: `qt6-base`, `qt6-declarative`, `qt6-multimedia`,
  `qt6-shadertools`, `ffmpeg`, `libarchive`, `zlib`;
- no runtime dependency on `sherpa-onnx` or `onnxruntime` while ASR native
  libraries are bundled in the release asset.

The initial `PKGBUILD` shape should be:

```bash
pkgname=shatv-bin
pkgver=0.1.0_alpha
pkgrel=1
_release_tag=v0.1.0-alpha
pkgdesc='Qt-based IPTV player with bundled sherpa-onnx speech recognition'
arch=('x86_64')
url='https://github.com/MaxCrazy1101/shatv'
license=('MIT')
depends=(
  'ffmpeg'
  'gcc-libs'
  'glibc'
  'hicolor-icon-theme'
  'libarchive'
  'qt6-base'
  'qt6-declarative'
  'qt6-multimedia'
  'qt6-shadertools'
  'zlib'
)
provides=('shatv' 'shatv-asr')
conflicts=('shatv' 'shatv-git' 'shatv-asr' 'shatv-asr-bin')
source=("https://github.com/MaxCrazy1101/shatv/releases/download/${_release_tag}/shatv-arch-x86_64-asr.tar.zst")
sha256sums=('...')

package() {
  cp -a "$srcdir/usr" "$pkgdir/"
}
```

Use `_release_tag` so the AUR `pkgver` can use Arch-compatible underscores
without guessing the upstream GitHub tag name.

### `shatv-bin` release workflow

The dedicated `shatv-bin` workflow runs on immutable version tags. It should
not run on pull requests, branch pushes, or the mutable `alpha` release.

Trigger:

```yaml
on:
  push:
    tags:
      - 'v*'
```

Required behavior:

1. Ignore the mutable `alpha` tag.
2. Build the ASR-enabled binary payload in an Arch Linux container.
3. Create `shatv-arch-x86_64-asr.tar.zst` from the installed `usr/` payload.
4. Compute `sha256sum` for the tarball.
5. Create the versioned GitHub release if it does not already exist.
6. Upload the tarball and `SHA256SUMS` to the versioned GitHub release.
7. Generate `shatv-bin/PKGBUILD` and `.SRCINFO` with the release tag and hash.
8. Push those two files to `ssh://aur@aur.archlinux.org/shatv-bin.git`.

Required repository secrets:

- `AUR_SSH_PRIVATE_KEY`: SSH private key for the AUR account that maintains
  `shatv-bin`.

The workflow should use a pinned `aur.archlinux.org` host key in
`known_hosts`. Do not use `StrictHostKeyChecking=no`.

Draft safety rules:

- fail if the release tag is `alpha`;
- fail if the release asset already exists with a different checksum;
- fail if `.SRCINFO` was not generated from the same `PKGBUILD`;
- fail if `shatv-bin` would point at a mutable release asset;
- do not publish `shatv-bin` from pull requests or branch pushes.

Reserve `shatv-asr-bin` only if the project later needs both non-ASR and ASR
binary AUR packages. Under the current policy, `shatv-bin` is the ASR-capable
binary package.

### Publication requirements

- The upstream Git repository must be public or otherwise anonymously
  cloneable; AUR users cannot build from a private GitHub source URL.
