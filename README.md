# ASCII Player Reborn

Based on my past [ASCII-PLAYER](https://github.com/Azrielx86/ASCII-PLAYER) project.

This version gets every frame with [OpenCV](https://opencv.org/), converts it to ASCII and print it on terminal in real
time, wihtout needing a helper file like in the past version.

Also for this version [FFmpeg](https://ffmpeg.org/) and [miniaudio](https://miniaud.io/) are used for the audio
playback!

## Program options

```
Usage: ./AsciiPlayerReborn [options]
Options:
  -w, --width WIDTH   Specify the desired width.
  -h, --height HEIGHT Specift the desired height.
  -f, --file PATH     Video source file path.
  -c, --charset       Selects the charset:
                         (1) Long charset:  .'`^",:;Il!i><~+_-?][}{1)(|\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$
                         (2) Short charset:  .:-=+*#%@
```

## Showcase

### Bad Apple

![Bad Apple](./docs/bad_apple.gif)

### Lagtrain

![Lagtrain](./docs/lagtrain.gif)