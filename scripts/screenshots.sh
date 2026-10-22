#!/usr/bin/env bash
# Runs inside the Android emulator job: installs the debug app and takes
# screenshots of each screen into ./shots (shown on the "screens" branch).
set -x
APK=app/build/outputs/apk/debug/app-debug.apk
PKG=np.sunai
mkdir -p shots
adb install -r -g "$APK"
adb shell settings put system font_scale 1.0
shot() { sleep 4; adb exec-out screencap -p > "shots/$1.png"; }
start() { adb shell am start -W -S -n $PKG/.ui.MainActivity "$@"; }
start;                                    shot 1_home
start --es open connect;                  shot 2_connect
start --es open test --ez demo true;      shot 3_test
start --es open helper;                   shot 4_helper_locked
start --es open helper --ez unlock true;  shot 5_helper
adb shell input swipe 500 1600 500 400 300; shot 6_helper_scrolled
adb logcat -d -s AndroidRuntime:E SunaiAudio:* > shots/logcat.txt || true
ls -la shots
