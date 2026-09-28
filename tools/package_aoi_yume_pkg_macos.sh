#!/bin/zsh
set -euo pipefail

repo_root="${0:A:h:h}"
build_root="${BUILD_ROOT:-$repo_root/build-plugin/plugins/rompler/EONDS50_artefacts/RelWithDebInfo}"
dist_root="${DIST_ROOT:-$repo_root/dist}"
pkg_path="${PKG_PATH:-$dist_root/Aoi-YUME-macOS.pkg}"
pkg_identifier="${PKG_IDENTIFIER:-com.eonlab.aoi-yume.pkg}"
pkg_version="${PKG_VERSION:-}"
skip_signature="${SKIP_SIGNATURE:-0}"
pkg_sign_identity="${PKG_SIGN_IDENTITY:-}"
require_pkg_signature="${REQUIRE_PKG_SIGNATURE:-0}"

if [[ -z "$pkg_version" ]]; then
    pkg_version="$(sed -nE 's/^project\([^)]*VERSION[[:space:]]+([^[:space:]\)]+).*/\1/p' "$repo_root/CMakeLists.txt" | head -1)"
fi
pkg_version="${pkg_version:-0.1.0}"

pkgbuild_bin="${PKGBUILD_BIN:-$(command -v pkgbuild || true)}"
pkgutil_bin="${PKGUTIL_BIN:-$(command -v pkgutil || true)}"
[[ -n "$pkgbuild_bin" ]] || { print -u2 "pkgbuild is required to create a macOS package"; exit 1; }
[[ -n "$pkgutil_bin" ]] || { print -u2 "pkgutil is required to inspect the created package"; exit 1; }
if [[ "$require_pkg_signature" == "1" && -z "$pkg_sign_identity" ]]; then
    print -u2 "REQUIRE_PKG_SIGNATURE=1 requires PKG_SIGN_IDENTITY"
    exit 1
fi

canonical_fonts=("Crystal Legacy.sf2" "Natural Stage.sf2" "Studio Essentials.sf2")
bundle_specs=(
    "VST3|Aoi YUME.vst3|Library/Audio/Plug-Ins/VST3"
    "AU|Aoi YUME.component|Library/Audio/Plug-Ins/Components"
    "Standalone|Aoi YUME.app|Applications"
)

require_canonical_fonts() {
    local bundle="$1"
    local font_dir="$bundle/Contents/Resources/SoundFonts"
    [[ -d "$font_dir" ]] || { print -u2 "Missing SoundFonts directory: $font_dir"; return 1; }

    local expected actual
    expected="$(printf '%s\n' "${canonical_fonts[@]}" | sort)"
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
    if ! plutil -lint "$plist" >/dev/null 2>&1; then
        print -u2 "Invalid Info.plist: $plist"
        return 1
    fi
    local metadata
    metadata="$(plutil -p "$plist")"
    [[ "$metadata" == *'"CFBundleName" => "Aoi YUME"'* ]] || { print -u2 "Unexpected product name in $plist"; return 1; }
    [[ "$metadata" == *'"CFBundleIdentifier" => "com.eonlab.aoi-yume"'* ]] || { print -u2 "Unexpected bundle ID in $plist"; return 1; }
    if [[ "$metadata" == *'"AudioComponents"'* ]]; then
        [[ "$metadata" == *'"manufacturer" => "EonL"'* ]] || { print -u2 "Unexpected AU manufacturer in $plist"; return 1; }
        [[ "$metadata" == *'"subtype" => "AoYu"'* ]] || { print -u2 "Unexpected AU subtype in $plist"; return 1; }
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

stage_root="$(mktemp -d "${TMPDIR:-/tmp}/aoi-yume-pkg.XXXXXX")"
trap 'rm -rf "$stage_root"' EXIT
payload_root="$stage_root/payload"

for spec in "${bundle_specs[@]}"; do
    format="${spec%%|*}"
    rest="${spec#*|}"
    bundle_name="${rest%%|*}"
    install_dir="${rest#*|}"
    verify_bundle "$format" "$bundle_name"
    mkdir -p "$payload_root/$install_dir"
    ditto --norsrc --noextattr --noacl --noqtn \
        "$build_root/$format/$bundle_name" "$payload_root/$install_dir/$bundle_name"
done

# The build tree can carry download quarantine/provenance attributes from the
# source SoundFonts. Do not copy those attributes into the staging root. macOS
# may still attach its own com.apple.provenance marker to newly-created files;
# that marker is harmless and does not affect the bundle's code signature.

mkdir -p "${pkg_path:h}"
pkgbuild_args=(
    --root "$payload_root"
    --identifier "$pkg_identifier"
    --version "$pkg_version"
    --install-location /
    --ownership recommended
)
if [[ -n "$pkg_sign_identity" ]]; then
    pkgbuild_args+=(--sign "$pkg_sign_identity")
fi
"$pkgbuild_bin" "${pkgbuild_args[@]}" "$pkg_path"

payload_listing="$($pkgutil_bin --payload-files "$pkg_path")"
for expected in \
    "Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3/Contents/Resources/SoundFonts/Crystal Legacy.sf2" \
    "Library/Audio/Plug-Ins/Components/Aoi YUME.component/Contents/Resources/SoundFonts/Natural Stage.sf2" \
    "Applications/Aoi YUME.app/Contents/Resources/SoundFonts/Studio Essentials.sf2"; do
    [[ "$payload_listing" == *"$expected"* ]] || {
        print -u2 "Package payload is missing: $expected"
        exit 1
    }
done
for legacy in \
    'Sonic_Mania_-_Korg_M1_Legacy_Soundfont.sf2' \
    'Live HQ Natural SoundFont GM.sf2' \
    'SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2'; do
    [[ "$payload_listing" != *"$legacy"* ]] || { print -u2 "Package contains legacy filename: $legacy"; exit 1; }
done

if [[ -n "$pkg_sign_identity" ]]; then
    "$pkgutil_bin" --check-signature "$pkg_path"
    package_signature="signed with $pkg_sign_identity"
else
    package_signature="unsigned installer package (set PKG_SIGN_IDENTITY for Developer ID Installer signing)"
fi

package_sha256="$(shasum -a 256 "$pkg_path" | awk '{print $1}')"
manifest="$dist_root/Aoi-YUME-macOS.pkg.manifest.txt"
{
    print "Product: Aoi YUME"
    print "Vendor: EON LAB"
    print "Package: $pkg_path"
    print "PackageIdentifier: $pkg_identifier"
    print "PackageVersion: $pkg_version"
    print "PackageSignature: $package_signature"
    print "PackageSHA256: $package_sha256"
    print "InstallLocations: /Library/Audio/Plug-Ins/VST3, /Library/Audio/Plug-Ins/Components, /Applications"
    for spec in "${bundle_specs[@]}"; do
        format="${spec%%|*}"
        rest="${spec#*|}"
        bundle_name="${rest%%|*}"
        install_dir="${rest#*|}"
        print "Bundle: $format/$bundle_name -> /$install_dir/$bundle_name"
        print "SoundFonts: ${(j:, :)canonical_fonts}"
    done
} > "$manifest"

print "Created $pkg_path"
print "Manifest: $manifest"
print "PackageSHA256: $package_sha256"
