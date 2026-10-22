# सुनाइ · Sunai

A free, **picture-first sound amplifier** for Android, made for people in villages who
cannot afford hearing aids and may not be able to read. The phone's microphone listens,
the app cleans and balances the sound in real time, and sends it to the user's earphones.

- **Live amplification** with very low delay (C++ audio engine on Google's Oboe library)
- **AI noise reduction** (RNNoise, ~100 KB, runs on the phone)
- **Ear test** that tunes each ear separately
- **Battery saver**, automatic below 20 %
- **100 % offline**: the app has no internet permission at all, and nothing is recorded
- **Voice help** on every screen (Nepali first; Maithili, Bhojpuri, Tharu ready for recordings)
- **Tiny**: no AndroidX, no analytics, no outside libraries; target APK size 3–5 MB

> Sunai is a listening helper, **not a medical device**. People with ear pain, discharge or
> sudden hearing loss should visit a health post.

---

## How the sound works

```
phone mic → low-cut filter → AI noise reduction → compressor/expander (WDRC)
          → howl guard → own-voice reduction → per-ear equaliser (from the ear test) → balance
          → ear-safety limiter → earphone
```

| Stage | What it does | Delay added |
|---|---|---|
| Low-cut | Removes rumble and wind | 0 |
| AI noise reduction | Removes background noise, keeps voices (Talk and Crowd modes) | 10 ms (off in Home, battery saver and low-delay mode) |
| WDRC | Soft speech louder, loud sounds only a little louder; mic hiss pushed down in silence | 0 |
| Howl guard | Detects squealing (feedback) and pulls the gain down | 0 |
| Own-voice reduction | Turns the wearer's own voice down 18 dB (it is much louder at the phone mic than other people). Threshold learned on the Helper screen with **मेरो आवाज सिकाउनुहोस्** | 0 |
| Equaliser | 6 bands per ear (250 Hz – 8 kHz), boosted where the ear test found weak hearing | 0 |
| Limiter | Output can never exceed −6 dBFS (ear protection on) or −1 dBFS (off) | 0 |

Every stage works sample by sample, so the only buffering is Android's own (two audio
bursts, usually 4–10 ms) plus the optional 10 ms of AI.

### Delay you can expect

| Earphone | Total delay |
|---|---|
| Wired / USB-C | about 10–25 ms (feels instant) |
| LE Audio or hearing-aid (ASHA) Bluetooth | about 30–50 ms |
| Ordinary Bluetooth earbuds | 150 ms or more; the app shows ⏳ and suggests wired earphones |

The app measures real delay with Oboe timestamps and shows ⚡ (fast) or ⏳ (slow).

### Battery saver

Switched on by the helper, or automatically below 20 % battery (off again at 25 % or when
charging). It turns off the AI stage, keeps only the three speech bands of the equaliser,
skips equaliser work during silence, and stops all screen updates while the screen is off.
It does **not** enlarge audio buffers, so the delay stays the same.

---

## Project layout

```
app/src/main/cpp/
  dsp/            sound chain (plain C++, tested on any computer)
  audio/          Oboe full-duplex engine: low-latency streams, routing, restart on disconnect
  jni_bridge.cpp  functions called from Kotlin
  third_party/    Oboe 1.9.3 (Apache-2.0) and RNNoise 0.1.1 (BSD-3), built from source
app/src/main/java/np/sunai/
  core/           Sunai controller, settings, earphone monitor, voice help
  audio/          NativeAudio bridge, foreground ListenService
  ui/             Home, Connect, Ear test, Helper screens
native-tests/     checks for the sound chain (limiter, delay, ear boosts, howl guard, speed)
```

## Building

**In the cloud:** every push to `main` runs GitHub Actions, which runs the sound-engine tests
and builds the debug and release APKs. Download them from the run's **Artifacts**
(`sunai-apk`). The APK sizes are shown on the run summary.

**On a computer:** open the folder in Android Studio (it will create the Gradle wrapper),
or with Gradle 8.14 installed:

```bash
gradle assembleDebug          # app/build/outputs/apk/debug/app-debug.apk
gradle assembleRelease        # small, optimised build
```

Sound-engine tests (no phone needed):

```bash
cmake -S native-tests -B native-tests/build && cmake --build native-tests/build
./native-tests/build/sunai_tests
```

### Release signing

Without secrets, the release APK is signed with the debug key (fine for testing and sharing
with field testers, **not** for Google Play). For Play Store releases add these repository
secrets: `SUNAI_KEYSTORE_B64` (the .jks file, base64), `SUNAI_KEYSTORE_PASSWORD`,
`SUNAI_KEY_ALIAS`, `SUNAI_KEY_PASSWORD`.

---

## Recording the voice help (important)

Until recordings are added, the app reads its Nepali help text with the phone's offline
text-to-speech, if the phone has a Nepali voice. Real recordings by a warm local speaker
are much better for older listeners.

1. Record each clip with a native speaker, slowly and kindly. The Nepali text for each clip
   is in `app/src/main/res/values/strings.xml` (`say_…` entries).
2. Save as **Ogg Opus, mono, 24 kbps** (about 10–20 KB per clip).
3. Put them in `app/src/main/res/raw/` named `voice_<language>_<key>.ogg`:

| key | when it plays |
|---|---|
| `home` | voice button on the home screen |
| `connect` | voice button on the connect screen |
| `test` | voice button on the ear test |
| `helper` | voice button on the helper screen |
| `no_earphone` | listening started without an earphone |
| `connected` | an earphone was connected |
| `saver_on` | battery saver turned on automatically |
| `loud` | one hour at top volume |
| `test_done` | ear test finished |

Languages: `ne` (Nepali), `mai` (Maithili), `bho` (Bhojpuri), `tha` (Tharu).
Example: `voice_mai_home.ogg`.

---

## Testing on real phones

- Use cheap Android 8–11 phones with 2–3 GB RAM, the kind people actually own.
- Measure the real delay and glitches with Google's free **OboeTester** app.
- Check battery drain with Android Studio's Power Profiler.
- Test the icons and voice help with older villagers **before** wide release.
- Ask an audiologist to review the ear-test levels (`TestActivity`: −42 dBFS test tone,
  +12 dB boost when not heard) and the volume steps (`Processor.cpp`).

## Roadmap

- Bluetooth "call mode" (SCO / LE Audio voice) as an extra low-delay route
- Two-level ear test (soft, then louder) for finer tuning
- Recorded voice clips in all four languages
- Adaptive notch filter for feedback, in addition to the howl guard

## Licences

- Oboe — Apache License 2.0 (`app/src/main/cpp/third_party/oboe/LICENSE`)
- RNNoise — BSD 3-Clause (`app/src/main/cpp/third_party/rnnoise/COPYING`)
