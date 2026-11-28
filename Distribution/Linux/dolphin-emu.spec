%global debug_package %{nil}

Name:           dolphin-emu
Version:        %(t=$(git -C %{_sourcedir} tag --list '[0-9][0-9][0-9][0-9]' --sort=-v:refname | head -n1); echo "${t:-2609}.$(git -C %{_sourcedir} rev-list --count ${t:-2609}..HEAD)")
Release:        1%{?dist}
Summary:        GameCube / Wii / Triforce emulator

License:        GPL-2.0-or-later
URL:            https://dolphin-emu.org/

Requires:       %{name}-data = %{version}-%{release}

Provides:       bundled(expr)
Provides:       bundled(FatFs)
Provides:       bundled(FreeSurround)
Provides:       bundled(rangeset)
Provides:       bundled(cpp-optparse)
Provides:       bundled(imgui) = 1.91
Provides:       bundled(implot) = 0.16
Provides:       bundled(cpp-ipc)
Provides:       bundled(watcher) = 0.14.5
Provides:       bundled(tinygltf)
Provides:       bundled(soundtouch)
Provides:       bundled(bochs)
Provides:       bundled(mbedtls) = 2.28
Provides:       bundled(mgba)
Provides:       bundled(rcheevos) = 11.6.0
Provides:       bundled(rclone) = 1.75.0
Provides:       bundled(enet)
Provides:       bundled(xxhash)
Provides:       bundled(cubeb)

BuildRequires:  cmake
BuildRequires:  clang
BuildRequires:  ccache
BuildRequires:  golang
BuildRequires:  gettext
BuildRequires:  glslang-devel
BuildRequires:  spirv-tools-devel
BuildRequires:  SDL3-devel
BuildRequires:  libspng-devel
BuildRequires:  libcurl-devel
BuildRequires:  mbedtls-devel
BuildRequires:  minizip-ng-devel
BuildRequires:  lzo-devel
BuildRequires:  libzstd-devel >= 1.4.0
BuildRequires:  xz-devel
BuildRequires:  pugixml-devel
BuildRequires:  fmt-devel >= 8
BuildRequires:  libevdev-devel
BuildRequires:  systemd-devel
BuildRequires:  pipewire-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  vulkan-headers
BuildRequires:  libxkbcommon-devel
BuildRequires:  libX11-devel
BuildRequires:  libXrandr-devel
BuildRequires:  libXi-devel

%if 0%{?suse_version}
BuildRequires:  ninja
BuildRequires:  lld
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-base-private-devel
BuildRequires:  qt6-svg-devel
BuildRequires:  qt6-declarative-devel
BuildRequires:  ffmpeg-devel
BuildRequires:  spirv-headers
BuildRequires:  libhidapi-devel
BuildRequires:  liblz4-devel >= 1.8
BuildRequires:  libbz2-devel
BuildRequires:  bluez-devel
BuildRequires:  libusb-1_0-devel
BuildRequires:  libpulse-devel
BuildRequires:  vulkan-devel
BuildRequires:  Mesa-libGL-devel
%else
BuildRequires:  ninja-build
BuildRequires:  lld
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  libavcodec-free-devel
BuildRequires:  libavformat-free-devel
BuildRequires:  libavutil-free-devel
BuildRequires:  libswscale-free-devel
BuildRequires:  spirv-headers-devel
BuildRequires:  hidapi-devel
BuildRequires:  lz4-devel >= 1.8
BuildRequires:  bzip2-devel
BuildRequires:  bluez-libs-devel
BuildRequires:  libusb1-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  vulkan-loader-devel
BuildRequires:  mesa-libGL-devel
%endif

%description
Dolphin is an emulator for two recent Nintendo video game consoles:
the GameCube and the Wii.

%package data
Summary:        Data files for %{name}
BuildArch:      noarch

%description data
Data files for %{name}.

%package tool
Summary:        CLI tool for %{name}

%description tool
Command-line tool for %{name} (dolphin-tool).

%prep
cp -a %{_sourcedir}/COPYING %{_sourcedir}/LICENSES .

%build
cmake -S %{_sourcedir} -B %{_builddir}/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja -C %{_builddir}/build

%install
DESTDIR=%{buildroot} cmake --install %{_builddir}/build

%files
%license COPYING LICENSES/
%{_bindir}/dolphin-emu
%{_datadir}/applications/dolphin-emu.desktop
%{_datadir}/icons/hicolor/*/apps/dolphin-emu.*
%{_datadir}/dolphin-emu/Sys/Resources/
%{_datadir}/dolphin-emu/Sys/Themes/
%{_datadir}/locale/*/LC_MESSAGES/dolphin-emu.mo
%{_mandir}/man6/dolphin-emu.6*
%exclude %{_mandir}/man6/dolphin-emu-nogui.*

%files data
%license COPYING LICENSES/
%exclude %{_datadir}/dolphin-emu/Sys/Resources/
%exclude %{_datadir}/dolphin-emu/Sys/Themes/
%{_datadir}/dolphin-emu/

%files tool
%license COPYING LICENSES/
%{_bindir}/dolphin-tool

%changelog
* Wed Nov 25 2026 Sharath <sharath@localhost> - 2603-1
- Initial Linux RPM spec for Dolphin-Extra
