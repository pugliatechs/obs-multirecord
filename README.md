# OBS Multi Source Recorder

An OBS Studio plugin that records individual video sources to separate files, independent of the main program recording. Audio is captured from the OBS main audio mix.

## Features

- **Multiple source recordings** - record any number of video sources simultaneously to separate files
- **Independent from program recording** - source recordings run alongside (or without) the standard OBS recording
- **Per-entry configuration** - each recording has its own video/audio codec, bitrate, container format, and output directory
- **Native source resolution** - sources are recorded at their native resolution without scaling
- **Dock panel UI** - compact table-based interface docked in OBS with a settings dialog per entry
- **Auto-detection** - enumerates available video sources and encoders (x264, VAAPI, etc.) at runtime
- **Config persistence** - recording entries are saved and restored with the OBS profile

## How It Works

Each recording entry creates an independent pipeline using OBS views:

```
Video Source -> obs_view -> OBS render loop -> video encoder -+
                                                              +-> ffmpeg_muxer -> file
OBS main audio mix -----------------------> audio encoder ----+
```

The `obs_view` approach renders the source natively through OBS's internal render loop with no manual GPU readback. Audio is captured from the OBS main audio mix (all active audio sources).

## Building

### Prerequisites

- OBS Studio 28+ development headers/libraries
- CMake 3.28+
- Qt5 or Qt6 (Widgets module)
- A C17/C++17 compiler

### Build Steps

```bash
git clone https://github.com/pugliatechs/obs-multirecord.git
cd obs-multirecord

cmake -B build \
  -DCMAKE_PREFIX_PATH="/usr/lib/cmake/obs-studio;/usr/lib/cmake/qt6" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel

sudo cmake --install build --prefix /usr
```

### Flatpak OBS

If you use the Flatpak version of OBS, install the plugin into:
```
~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-multi-record/bin/64bit/
```

### Arch Linux / Manjaro

```bash
sudo pacman -S obs-studio qt6-base cmake

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr
```

## Usage

1. Open OBS Studio
2. Go to **View > Docks > Multi Recorder**
3. Click **Add** to create a new recording entry
4. Select a **Video Source** from the dropdown (only video-capable sources are listed)
5. Set the **Output Directory** (click `...` to browse)
6. Choose container format (MKV recommended), video/audio encoders, and bitrates
7. Click the play button on individual rows, or **Rec All**
8. Click the stop button when done. Files are written to the configured directory

Double-click any row to edit its settings.

### Audio

Audio is captured from the OBS main audio mix. All active audio sources (Desktop Audio, Mic, etc.) are included in the recording, matching what the standard OBS recording captures.

### Filename Format

Files are named using the pattern: `{SourceName}_{YYYYMMDD}_{HHMMSS}.{ext}`

Spaces in source names are replaced with underscores.

## Architecture

| File | Purpose |
|------|---------|
| `src/plugin-main.c` | Module entry point (`obs_module_load`) |
| `src/record-pipeline.h/c` | Per-source recording pipeline using `obs_view` |
| `src/multi-record-dock.hpp/cpp` | Qt dock panel UI, settings dialog, and config persistence |
| `src/source-combo-delegate.hpp/cpp` | Combo box delegate utility |
| `data/locale/en-US.ini` | Localisation strings |

## Disclaimer

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS, COPYRIGHT HOLDERS, MARCO PENNELLI, OR PUGLIATECHS APS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Use this software at your own risk. The author and the organization assume no responsibility for any damages, data loss, security incidents, or other consequences resulting from the use or misuse of this software.

## Author

**Marco Pennelli** | [PugliaTechs APS](https://www.pugliatechs.com)

## License

GPLv2 - same as OBS Studio.
