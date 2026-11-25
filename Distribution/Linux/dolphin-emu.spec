Name:           dolphin-emu
Version:        2609.069808a8f2
Release:        1%{?dist}
Summary:        GameCube / Wii / Triforce emulator

License:        GPL-2.0-or-later
URL:            https://dolphin-emu.org/

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  golang
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  ffmpeg-devel
BuildRequires:  glslang-devel
BuildRequires:  SDL3-devel
BuildRequires:  hidapi-devel
BuildRequires:  libspng-devel
BuildRequires:  libcurl-devel
BuildRequires:  mbedtls-devel
BuildRequires:  minizip-ng-devel
BuildRequires:  lz4-devel
BuildRequires:  lzo-devel
BuildRequires:  libzstd-devel
BuildRequires:  bzip2-devel
BuildRequires:  xz-devel
BuildRequires:  pugixml-devel
BuildRequires:  fmt-devel
BuildRequires:  bluez-libs-devel
BuildRequires:  libevdev-devel
BuildRequires:  systemd-devel
BuildRequires:  libusb1-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  pipewire-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  vulkan-loader-devel
BuildRequires:  vulkan-headers
BuildRequires:  libX11-devel
BuildRequires:  libXrandr-devel
BuildRequires:  libXi-devel

%description
Dolphin is an emulator for two recent Nintendo video game consoles:
the GameCube and the Wii.

%prep
cp -a %{_sourcedir}/COPYING %{_sourcedir}/LICENSES .

%build
cmake -S %{_sourcedir} -B %{_builddir}/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix}
ninja -C %{_builddir}/build

%install
DESTDIR=%{buildroot} cmake --install %{_builddir}/build

%files
%license COPYING LICENSES/
%{_bindir}/dolphin-emu
%{_bindir}/dolphin-tool
%{_datadir}/dolphin-emu/
%{_datadir}/applications/dolphin-emu.desktop
%{_datadir}/icons/hicolor/*/apps/dolphin-emu.*
%{_datadir}/locale/*/LC_MESSAGES/dolphin-emu.mo
%{_mandir}/man6/dolphin-emu*.6*

%changelog
* Wed Nov 25 2026 Sharath <sharath@localhost> - 2603-1
- Initial Linux RPM spec for Dolphin-Extra
