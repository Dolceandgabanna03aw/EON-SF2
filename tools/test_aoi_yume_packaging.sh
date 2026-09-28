#!/bin/zsh
set -euo pipefail

script_dir="${0:A:h}"
repo_root="${script_dir:h}"
fixture_root="$(mktemp -d "${TMPDIR:-/tmp}/aoi-yume-package-test.XXXXXX")"
trap 'rm -rf "$fixture_root"' EXIT

cmake_source="$(<"$repo_root/plugins/rompler/CMakeLists.txt")"
[[ "$cmake_source" == *$'PLUGIN_MANUFACTURER_CODE    EonL'* ]]
[[ "$cmake_source" == *$'PLUGIN_CODE                 AoYu'* ]]
[[ "$cmake_source" == *$'BUNDLE_ID                   com.eonlab.aoi-yume'* ]]

build_root="$fixture_root/build"
dist_root="$fixture_root/dist"
for pair in \
    "VST3/Aoi YUME.vst3" \
    "AU/Aoi YUME.component" \
    "Standalone/Aoi YUME.app"; do
    bundle="$build_root/$pair"
    mkdir -p "$bundle/Contents/Resources/SoundFonts"
    cat > "$bundle/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Aoi YUME</string>
    <key>CFBundleIdentifier</key>
    <string>com.eonlab.aoi-yume</string>
</dict>
</plist>
PLIST
    : > "$bundle/Contents/Resources/SoundFonts/Crystal Legacy.sf2"
    : > "$bundle/Contents/Resources/SoundFonts/Natural Stage.sf2"
    : > "$bundle/Contents/Resources/SoundFonts/Studio Essentials.sf2"
done

archive="$dist_root/Aoi-YUME-macOS.zip"
BUILD_ROOT="$build_root" DIST_ROOT="$dist_root" ARCHIVE_PATH="$archive" \
    SKIP_SIGNATURE=1 "$repo_root/tools/package_aoi_yume_macos.sh"

listing="$(unzip -Z1 "$archive")"
[[ "$listing" != *'__MACOSX/'* ]]
[[ "$listing" == *'Aoi-YUME-macOS/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts/Crystal Legacy.sf2'* ]]
[[ "$listing" == *'Aoi-YUME-macOS/AU/Aoi YUME.component/Contents/Resources/SoundFonts/Natural Stage.sf2'* ]]
[[ "$listing" == *'Aoi-YUME-macOS/Standalone/Aoi YUME.app/Contents/Resources/SoundFonts/Studio Essentials.sf2'* ]]
[[ "$listing" != *'Sonic_Mania'* ]]
[[ "$listing" != *'Live HQ Natural'* ]]
[[ "$listing" != *'SGM-v2.01'* ]]

if command -v pkgbuild >/dev/null 2>&1 && command -v pkgutil >/dev/null 2>&1; then
    pkg="$dist_root/Aoi-YUME-macOS.pkg"
    BUILD_ROOT="$build_root" DIST_ROOT="$dist_root" PKG_PATH="$pkg" \
        PKG_VERSION=0.1.0 SKIP_SIGNATURE=1 "$repo_root/tools/package_aoi_yume_pkg_macos.sh"

    payload="$(pkgutil --payload-files "$pkg")"
    [[ "$payload" == *'Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts/Crystal Legacy.sf2'* ]]
    [[ "$payload" == *'Library/Audio/Plug-Ins/Components/Aoi YUME.component/Contents/Resources/SoundFonts/Natural Stage.sf2'* ]]
    [[ "$payload" == *'Applications/Aoi YUME.app/Contents/Resources/SoundFonts/Studio Essentials.sf2'* ]]
    [[ "$payload" != *'Sonic_Mania'* ]]
    [[ "$payload" != *'Live HQ Natural'* ]]
    [[ "$payload" != *'SGM-v2.01'* ]]
else
    echo "pkgbuild/pkgutil unavailable; pkg fixture skipped"
fi

install_root="$fixture_root/install"
backup_root="$fixture_root/backups"
mkdir -p "$install_root/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts"
: > "$install_root/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts/Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2"
mkdir -p "$install_root/Components/Aoi YUME.component/Contents/Resources/SoundFonts"
: > "$install_root/Components/Aoi YUME.component/Contents/Resources/SoundFonts/Live HQ Natural SoundFont GM.sf2"

BUILD_ROOT="$build_root" INSTALL_ROOT="$install_root" BACKUP_ROOT="$backup_root" \
    SKIP_SIGNATURE=1 "$repo_root/tools/install_aoi_yume_macos.sh"

installed_fonts="$install_root/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts"
[[ -f "$installed_fonts/Crystal Legacy.sf2" ]]
[[ -f "$installed_fonts/Natural Stage.sf2" ]]
[[ -f "$installed_fonts/Studio Essentials.sf2" ]]
[[ ! -f "$installed_fonts/Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2" ]]
backup_legacy_count="$(find "$backup_root" -type f -name 'Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2' | wc -l | tr -d ' ')"
[[ "$backup_legacy_count" == "1" ]]

installed_components="$install_root/Components/Aoi YUME.component/Contents/Resources/SoundFonts"
[[ -f "$installed_components/Crystal Legacy.sf2" ]]
[[ -f "$installed_components/Natural Stage.sf2" ]]
[[ -f "$installed_components/Studio Essentials.sf2" ]]
[[ ! -f "$installed_components/Live HQ Natural SoundFont GM.sf2" ]]
backup_live_count="$(find "$backup_root" -type f -name 'Live HQ Natural SoundFont GM.sf2' | wc -l | tr -d ' ')"
[[ "$backup_live_count" == "1" ]]

echo "packaging fixture passed"
