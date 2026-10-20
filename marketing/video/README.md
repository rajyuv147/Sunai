# Sunai product video (Nepali)

- `sunai_nepali_product_video.mp4` — 1920×1080, 30 fps, about 1 min 34 s. Nepali voiceover, burned-in Nepali subtitles, music and UI sounds.
- `sunai_nepali.srt` — the same subtitles as a separate file for YouTube or Facebook.

The voice is synthetic (offline Piper voice `ne_NP-google-medium`, via sherpa-onnx). Replace it with a native speaker's recording before wide release.

## Rebuilding (`source/`)

Needs Python (`sherpa-onnx`, `soundfile`, `numpy`), Node with Playwright and Chromium, ffmpeg, and the Noto Sans Devanagari font.

```bash
cd marketing/video/source
python3 icons.py ../../../app/src/main/res/drawable icons.js   # app icons -> SVG
# Voice model: github.com/k2-fsa/sherpa-onnx/releases/tag/tts-models (vits-piper-ne_NP-google-medium)
mkdir -p vo && python3 tts.py <model-dir> script.json vo 0 0.92   # narration -> vo/*.wav
python3 timeline.py .                                             # scene timing + subtitles
python3 audio.py .                                                # mix.wav (voice + music + sounds)
node render.js video silent.mp4 30                                # frames -> silent.mp4
ffmpeg -i silent.mp4 -i mix.wav -c:v copy -c:a aac -b:a 192k -shortest -movflags +faststart out.mp4
```

The narration text is in `script.json`. The on-screen app screens are rebuilt from `app/src/main/res` (layouts, colours, strings and icons).
