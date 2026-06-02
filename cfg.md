# Configuration Options

Configuration file location: `{store_dir}/cfg/sdlretro.json`

## Resolution

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `res_w` | uint32 | 640 (320 on GCW) | Window/screen width |
| `res_h` | uint32 | 480 (240 on GCW) | Window/screen height |

## Display

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `fullscreen` | bool | false | Enable fullscreen mode |
| `scale` | int | 2 (1 on GCW) | Integer scale factor (1x-5x) |
| `scaling_mode` | uint32 | 0 | SDL1-only: 0=IPU scaling, 1=Screen center |
| `integer_scaling` | bool | false | SDL2-only: Force integer-only scaling ratio |
| `linear` | bool | true | SDL2-only: Use hardware linear rendering |

## Audio

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `mono_audio` | bool | false | Use mono audio output (stereo -> mono) |
| `sample_rate` | uint32 | 0 | Output sample rate (0=auto, uses source rate with integer multiplier) |
| `resampler_quality` | uint32 | 0 | libsamplerate quality (0=Linear, 1=Zero Order Hold, 2=Fastest, 3=Medium, 4=Best) |

## Timing

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `frame_limit` | float | 0.0 | Frame rate limit in FPS (0=use core FPS, e.g. 30.0 for 30fps cap) |
| `save_check` | uint32 | 0 | SRAM save check interval in seconds (0=disabled) |

## Language

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `language` | int | 0 | UI language (see `enum retro_language` in libretro.h) |

## Directories

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `datadir` | string | - | Static data directory path |
| `storedir` | string | - | Dynamic data directory path (saves, configs) |

## Example Config

```json
{
    "res_w": 640,
    "res_h": 480,
    "fullscreen": false,
    "scale": 2,
    "scaling_mode": 0,
    "integer_scaling": false,
    "linear": true,
    "mono_audio": false,
    "sample_rate": 0,
    "resampler_quality": 0,
    "frame_limit": 0.0,
    "save_check": 0,
    "language": 0
}
```
