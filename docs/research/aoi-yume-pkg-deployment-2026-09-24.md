# Aoi YUME macOS `.pkg` deployment — 2026-09-24

## Result

The current `plugin` preset artifacts were packaged as:

`dist/Aoi-YUME-macOS.pkg`

The package contains all three formats from the same
`build-plugin/plugins/rompler/EONDS50_artefacts/RelWithDebInfo` tree:

| Format | Installer destination |
|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3` |
| AU | `/Library/Audio/Plug-Ins/Components/Aoi YUME.component` |
| Standalone | `/Applications/Aoi YUME.app` |

Each bundle is checked for the product identity, AU metadata where present,
the three canonical SoundFonts (`Crystal Legacy.sf2`, `Natural Stage.sf2`,
`Studio Essentials.sf2`), and an ad-hoc code signature before staging. The
package payload was inspected with `pkgutil --payload-files`; all three
install paths and canonical SoundFont names are present, and the legacy
SoundFont names are absent.

The package is currently unsigned as an installer package because this machine
does not have a Developer ID Installer identity. The embedded plugin bundles
remain ad-hoc signed for local host loading. A signed installer can be created
with:

```zsh
PKG_SIGN_IDENTITY="Developer ID Installer: Example, Inc. (TEAMID)" \
  tools/package_aoi_yume_pkg_macos.sh
```

`REQUIRE_PKG_SIGNATURE=1` makes the script fail if that identity is not set.
The package is not installed by the packaging command; installing it is a
separate administrator-authorized action in macOS Installer.

## Reproduction

```zsh
cmake --build --preset plugin
SKIP_SIGNATURE=0 tools/package_aoi_yume_pkg_macos.sh
tools/test_aoi_yume_packaging.sh
```

The generated manifest records the package version, installer signature state,
install destinations, and SHA256 at
`dist/Aoi-YUME-macOS.pkg.manifest.txt`.
