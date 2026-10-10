#!/bin/zsh
# Makes the standalone Mac game and its disk image, from nothing: cleans the last build away, compiles, cooks,
# stages, and writes NaijaHustle-mac-arm64.dmg (Shipping, Apple Silicon, signed to run locally, not notarized).
#
#   Scripts/package_mac.sh
#
# It works on the APFS image on the external drive, because the internal disk is too small to cook on:
#   hdiutil attach ~/LumiTB_mnt/NaijaHustle-build/work.sparseimage -nobrowse     (mounts /Volumes/NHBuild)
# Override the places with UE_ROOT, NH_WORK and NH_OUT. The Unreal Editor must be closed.
#
# Three things here are workarounds, each for something that went wrong the first time (PROGRESS.md, "Standalone Mac build"):
#   - UnrealBuildTool's own Xcode step fails on this Mac, so the game is built with UE_BUILD_FROM_XCODE=1 and that
#     step is run by hand, and BuildCookRun is run without -build
#   - the cook writes into Unreal's Zen storage folder, which stops below 2 GB free; it is pointed at the drive for
#     the length of the cook and put back afterwards, whatever happens
#   - -CookOutputDir has to end in Mac, or the stage step looks one folder deeper than the cook wrote
set -e
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
WORK="${NH_WORK:-/Volumes/NHBuild}"
HERE="${0:A:h}"
PROJECT_DIR="${HERE:h}"
PROJECT="$PROJECT_DIR/NaijaHustle.uproject"
OUT="${NH_OUT:-$WORK/project-files/builds}"
ZEN="$HOME/Library/Application Support/Epic/UnrealEngine/Common/Zen"
LOGS="$WORK/logs"
DMG="NaijaHustle-mac-arm64.dmg"

[[ -d "$WORK" ]] || { echo "The work volume $WORK is not mounted. See the top of this script."; exit 1; }
pgrep -f "UnrealEditor" >/dev/null && { echo "Close the Unreal Editor (and any running game) first."; exit 1; }
mkdir -p "$LOGS" "$OUT"
step() { echo "== $1"; }

step "clean: the last cook, stage and archive"
rm -rf "$WORK/Cooked" "$WORK/Staged" "$WORK/Archive" "$WORK/dmgroot" "$WORK/$DMG"

step "Zen storage to the drive for the cook"
"$UE_ROOT/Engine/Binaries/Mac/zen" down >/dev/null 2>&1 || true
sleep 2
restore_zen() {
  "$UE_ROOT/Engine/Binaries/Mac/zen" down >/dev/null 2>&1 || true
  sleep 2
  if [[ -L "$ZEN/Data" && -d "$ZEN/Data.internal" ]]; then
    rm "$ZEN/Data" && mv "$ZEN/Data.internal" "$ZEN/Data" && echo "== Zen storage put back on the internal disk"
  fi
}
trap restore_zen EXIT
if [[ ! -L "$ZEN/Data" ]]; then
  mkdir -p "$WORK/ZenData"
  mv "$ZEN/Data" "$ZEN/Data.internal"
  ln -s "$WORK/ZenData" "$ZEN/Data"
fi

step "compile: the editor (the cook runs in it) and the Shipping game"
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" NaijaHustleEditor Mac Development -project="$PROJECT" > "$LOGS/pkg_build_editor.log" 2>&1 || { tail -20 "$LOGS/pkg_build_editor.log"; exit 1; }
UE_BUILD_FROM_XCODE=1 "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" NaijaHustle Mac Shipping -project="$PROJECT" -architecture=arm64 > "$LOGS/pkg_build_game.log" 2>&1 || { tail -20 "$LOGS/pkg_build_game.log"; exit 1; }
(cd "$PROJECT_DIR/Intermediate/ProjectFiles" && UBT_NO_POST_DEPLOY=true xcodebuild build -workspace NaijaHustle_Mac_NaijaHustle.xcworkspace -scheme NaijaHustle -configuration Shipping \
  -destination generic/platform=macOS CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES UE_XCODE_BUILD_MODE=PostBuildSync -hideShellScriptEnvironment > "$LOGS/pkg_xcode.log" 2>&1) || { tail -20 "$LOGS/pkg_xcode.log"; exit 1; }

step "cook, stage, package, archive (this is the long part)"
caffeinate -ims "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJECT" -platform=Mac -specifiedarchitecture=arm64 -clientconfig=Shipping \
  -cook -stage -pak -iostore -package -archive -archivedirectory="$WORK/Archive" -stagingdirectory="$WORK/Staged" -CookOutputDir="$WORK/Cooked/Mac" \
  -nop4 -utf8output -unattended > "$LOGS/pkg_uat.log" 2>&1 || { grep -v "^    export " "$LOGS/pkg_uat.log" | grep -E "Error|ERROR|failed" | tail -15; exit 1; }
grep -E "Cooked packages [0-9]+ Packages Remain 0|Success - |BUILD SUCCESSFUL" "$LOGS/pkg_uat.log" | tail -3

APP="$WORK/Archive/Mac/NaijaHustle-Mac-Shipping.app"
[[ -d "$APP" ]] || { echo "No app came out at $APP"; exit 1; }

step "the disk image"
mkdir "$WORK/dmgroot"
cp -cR "$APP" "$WORK/dmgroot/Naija Hustle.app"
ln -s /Applications "$WORK/dmgroot/Applications"
# The seal has come out broken once ("file modified: pakchunk0-Mac.ucas": the pak rewritten after the app was signed).
# It is signed to run locally anyway, so it is signed again here, and has to verify after that.
if ! codesign --verify --deep --strict "$WORK/dmgroot/Naija Hustle.app" 2>/dev/null; then
  echo "== the app's signature did not verify: signing it again"
  codesign --force --deep --sign - "$WORK/dmgroot/Naija Hustle.app" 2>&1 | tail -1
fi
codesign --verify --deep --strict "$WORK/dmgroot/Naija Hustle.app"
hdiutil create -volname "Naija Hustle" -srcfolder "$WORK/dmgroot" -fs HFS+ -format UDZO -ov "$WORK/$DMG" 2>&1 | grep -v WARNING | tail -1
hdiutil verify "$WORK/$DMG" 2>&1 | tail -1

step "replace the old image"
cp "$WORK/$DMG" "$OUT/$DMG"
# and a copy straight on the drive, beside the work image, where it can be read without mounting anything
[[ -d "$HOME/LumiTB_mnt/NaijaHustle-build" ]] && cp "$WORK/$DMG" "$HOME/LumiTB_mnt/NaijaHustle-build/$DMG" && echo "== copied to ~/LumiTB_mnt/NaijaHustle-build/$DMG"
ls -lh "$OUT/$DMG"
du -sh "$APP" | sed 's/^/app: /'
echo "== done: $OUT/$DMG"
