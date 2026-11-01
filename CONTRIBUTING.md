# Contributing to Sunai

Thank you for helping. Sunai exists for people in villages who cannot afford
hearing aids and may not be able to read, so every change is judged by one
question: **does it make the app easier, safer or clearer for them?**

## Ground rules

- **Pictures first.** Every action must work without reading. Words are small
  hints for helpers. One colour = one meaning (green on/heard, red off/not heard,
  blue volume/choice, amber help/warning).
- **Stay offline.** Do not add the `INTERNET` permission, analytics, ads or
  any network library. Sound is never recorded or stored.
- **Stay small and fast.** No AndroidX, Compose, Flutter or large libraries.
  Release APK should stay under ~5 MB. Do not add audio buffering or
  look-ahead that increases delay.
- **Ear safety is not optional.** Never remove or raise the output limiter
  ceiling without an audiologist's review.
- **Not a medical device.** Do not add diagnosis, medical claims or the words
  "hearing aid" in the app or store listing.

## Helping without code

- **Record voice clips** in Nepali, Maithili, Bhojpuri, Tharu or another local
  language (see "Recording the voice help" in the README).
- **Test on cheap phones** and report: phone model, Android version, wired or
  Bluetooth earphones, and a screenshot of the Helper screen's status text.
- **Test the pictures** with older people who cannot read, and tell us which
  icons they did not understand.
- **Audiologists:** review the ear-test levels and volume steps.

## Building and testing

See the README. Before opening a pull request:

```bash
cmake -S native-tests -B native-tests/build && cmake --build native-tests/build
./native-tests/build/sunai_tests        # all checks must pass
```

GitHub Actions builds the APK, runs these tests and takes emulator screenshots
of every screen for each pull request and push.

## Licence

By contributing you agree that your contribution is licensed under the
Apache License 2.0 (see LICENSE).
