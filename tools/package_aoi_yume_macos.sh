#!/bin/zsh
set -euo pipefail

repo_root="${0:A:h:h}"
build_root="${BUILD_ROOT:-$repo_root/build-plugin/plugins/rompler/EONDS50_artefacts/RelWithDebInfo}"
dist_root="${DIST_ROOT:-$repo_root/dist}"
archive_path="${ARCHIVE_PATH:-$dist_root/Aoi-YUME-macOS.zip}"
skip_signature="${SKIP_SIGNATURE:-0}"

canonical_fonts=("Crystal Legacy.sf2" "Natural Stage.sf2" "Studio Essentials.sf2")
bundle_specs=(
    "VST3|Aoi YUME.vst3"
    "AU|Aoi YUME.component"
    "Standalone|Aoi YUME.app"
)

require_canonical_fonts() {
    local bundle="$1"
    local font_dir="$bundle/Contents/Resources/SoundFonts"
    [[ -d "$font_dir" ]] || { print -u2 "Missing SoundFonts directory: $font_dir"; return 1; }

    local expected actual
    expected="$(printf '%s\n' "${canonical_fonts[@]}")"
    actual="$(find "$font_dir" -maxdepth 1 -type f -name '*.sf2' -print | sed 's|.*/||' | sort)"
    [[ "$actual" == "$expected" ]] || {
        print -u2 "Unexpected SoundFonts in $bundle:\n$actual"
        return 1
    }
}

require_identity() {
    local bundle="$1"
    local plist="$bundle/Contents/Info.plist"
    [[ -f "$plist" ]] || { print -u2 "Missing Info.plist: $plist"; return 1; }
    local metadata
    if plutil -lint "$plist" >/dev/null 2>&1; then
        metadata="$(plutil -p "$plist")"
        [[ "$metadata" == *'"CFBundleName" => "Aoi YUME"'* ]] || { print -u2 "Unexpected product name in $plist"; return 1; }
        [[ "$metadata" == *'"CFBundleIdentifier" => "com.eonlab.aoi-yume"'* ]] || { print -u2 "Unexpected bundle ID in $plist"; return 1; }
        if [[ "$metadata" == *'"AudioComponents"'* ]]; then
            [[ "$metadata" == *'"manufacturer" => "EonL"'* ]] || { print -u2 "Unexpected AU manufacturer in $plist"; return 1; }
            [[ "$metadata" == *'"subtype" => "AoYu"'* ]] || { print -u2 "Unexpected AU subtype in $plist"; return 1; }
        fi
    fi
}

verify_bundle() {
    local format="$1"
    local bundle_name="$2"
    local bundle="$build_root/$format/$bundle_name"
    [[ -d "$bundle" ]] || { print -u2 "Missing built bundle: $bundle"; return 1; }
    require_identity "$bundle"
    require_canonical_fonts "$bundle"
    if [[ "$skip_signature" != "1" ]]; then
        codesign --verify --deep --strict "$bundle"
    fi
}

stage_root="$(mktemp -d "${TMPDIR:-/tmp}/aoi-yume-package.XXXXXX")"
trap 'rm -rf "$stage_root"' EXIT
package_root="$stage_root/Aoi-YUME-macOS"
mkdir -p "$package_root"

for spec in "${bundle_specs[@]}"; do
    format="${spec%%|*}"
    bundle_name="${spec#*|}"
    verify_bundle "$format" "$bundle_name"
    mkdir -p "$package_root/$format"
    ditto --norsrc --noqtn "$build_root/$format/$bundle_name" "$package_root/$format/$bundle_name"
done

mkdir -p "$dist_root"
ditto -c -k --norsrc --keepParent "$package_root" "$archive_path"

manifest="$dist_root/Aoi-YUME-macOS.manifest.txt"
{
    print "Product: Aoi YUME"
    print "Vendor: EON LAB"
    print "Archive: $archive_path"
    for spec in "${bundle_specs[@]}"; do
        format="${spec%%|*}"
        bundle_name="${spec#*|}"
        print "Bundle: $format/$bundle_name"
        print "SoundFonts: ${(j:, :)canonical_fonts}"
    done
} > "$manifest"

listing="$(unzip -Z1 "$archive_path")"
[[ "$listing" != *'__MACOSX/'* ]] || { print -u2 "Archive contains __MACOSX metadata"; exit 1; }
for legacy in \
    'Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2' \
    'Live HQ Natural SoundFont GM.sf2' \
    'SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2'; do
    [[ "$listing" != *"$legacy"* ]] || { print -u2 "Archive contains legacy filename: $legacy"; exit 1; }
done

print "Created $archive_path"
print "Manifest: $manifest"
