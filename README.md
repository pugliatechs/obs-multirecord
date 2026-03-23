# OBS Multi Source Recorder

An OBS Studio plugin that records individual video+audio source pairs to separate files, independent of the main program recording.

## Features

- **Multiple source pairs** - record any combination of video and audio sources simultaneously
- **Independent from program recording** - source recordings run alongside (or without) the standard OBS recording
- **Per-pair configuration** - each recording pair has its own codec, bitrate, container format, and output directory
- **Dock panel UI** - convenient table-based interface docked in OBS for managing all recording pairs
- **Auto-detection** - enumerates available sources, video encoders (x264, NVENC, etc.), and audio encoders (AAC, etc.)
- **Config persistence** - recording pairs are saved and restored with the OBS profile

## How It Works

Each recording pair creates an independent pipeline:

```
Video Source -> texrender -> stagesurface -> virtual video_output -> video encoder -+
                                                                                    +-> ffmpeg_muxer -> file
Audio Source -> audio_output_info callback -> virtual audio_output -> audio encoder -+
```

This approach captures the raw source output (before scene composition and filters applied at the scene level), so you get clean isolated recordings per source.

## Building

### Prerequisites

- OBS Studio 28+ development headers/libraries
- CMake 3.28+
- Qt5 or Qt6 (Widgets module)
- A C17/C++17 compiler

### Build Steps

```bash
# Clone
git clone https://github.com/pugliatechs/obs-multirecord.git
cd obs-multirecord

# Configure (adjust OBS paths as needed)
cmake -B build \
  -DCMAKE_PREFIX_PATH="/usr/lib/cmake/obs-studio;/usr/lib/cmake/qt6" \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Install (to OBS plugin directory)
sudo cmake --install build --prefix /usr
```

### Flatpak OBS

If you use the Flatpak version of OBS, install the plugin into:
```
~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-multi-record/bin/64bit/
```

### Arch Linux / Manjaro

```bash
# Install OBS dev headers (if not already present)
sudo pacman -S obs-studio qt6-base cmake

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr
```

## Usage

1. Open OBS Studio
2. Go to **View > Docks > Multi Recorder** (or find it in the dock area)
3. Click **Add Pair** to add a new recording entry
4. Select a **Video Source** and optionally a separate **Audio Source**
5. Set the **Output Directory** (click `...` to browse)
6. Choose container format (MKV recommended), video/audio encoders, and bitrates
7. Click **Start** on individual rows, or **Start All**
8. Click **Stop** when done. Files are written to the configured directory

### Filename Format

Files are named using the pattern: `{SourceName}_{YYYYMMDD}_{HHMMSS}.{ext}`

## Architecture

| File | Purpose |
|------|---------|
| `src/plugin-main.c` | Module entry point (`obs_module_load`) |
| `src/record-pipeline.h/c` | Per-source recording pipeline (video/audio capture, encoding, muxing) |
| `src/multi-record-dock.hpp/cpp` | Qt dock panel UI and config persistence |
| `src/source-combo-delegate.hpp/cpp` | Combo box delegate utility |
| `data/locale/en-US.ini` | Localisation strings |

## Disclaimer

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS, COPYRIGHT HOLDERS, MARCO PENNELLI, OR PUGLIATECHS APS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Use this software at your own risk. The author and the organization assume no responsibility for any damages, data loss, security incidents, or other consequences resulting from the use or misuse of this software.

## Author

**Marco Pennelli** | [PugliaTechs APS](https://www.pugliatechs.com)

## License

GPLv2 - same as OBS Studio.
