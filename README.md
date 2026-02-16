# STM32H743VIT6 Audio Processor System - LVGL Frontend Development Guide


## 1. Project Overview

### 1.1 System Architecture

This project is an audio processing system based on STM32H743VIT6, utilizing LVGL v8.3 as the GUI framework. The system includes three core functionalities: file management, audio playback, and real-time audio processing.

<img width="1694" height="566" alt="QQ20260216-233810" src="https://github.com/user-attachments/assets/3a174d27-aba5-459b-844d-dd804a8298c7" />

### 1.2 Hardware Specifications

| Parameter | Specification | Description |
|-----------|---------------|-------------|
| MCU | STM32H743VIT6 | Cortex-M7 @ 480MHz |
| Flash | 2MB | Program storage |
| RAM | 1MB | Runtime memory |
| Display Resolution | 460x460 | RGB LCD interface |
| Color Depth | 16-bit | RGB565 |
| Touchscreen | Capacitive | I2C interface |

## 2. Quick Start

### 2.1 Prerequisites

- **Windows**: MinGW (gcc v12.2.0+), CMake 3.15+
- **Linux**: `build-essential`, `libsdl2-dev`, `cmake`
- **macOS**: Xcode Command Line Tools, CMake

### 2.2 Building

#### Clone and Setup
```bash
# Clone the project
git clone <repository-url>
cd lvgl_simulator

# Create build directory
mkdir build && cd build
cmake ..
```

#### Windows (MinGW)
```bash
# Add MinGW to PATH, then:
cmake --build . --parallel
# Copy SDL2.dll to bin directory
copy ..\mingw64\x86_64-w64-mingw32\bin\SDL2.dll bin\

# Run
.\bin\main.exe
```

#### Linux/macOS
```bash
cmake --build . --parallel
./bin/main
```

#### Docker
```bash
docker build -t lvgl_simulator .
docker run lvgl_simulator
```

### 2.3 IDE Setup

**VSCode:**
1. Install C/C++ Extension Pack and CMake Tools
2. Open project folder
3. Select GCC compiler when prompted
4. Click Build and Run

**Eclipse CDT:**
1. File → Import → General → Existing Project into Workspace
2. Select project directory
3. Build and run

## 3. LVGL v8.3 Integration

### 3.1 Key Interfaces

#### Display Driver
```c
void lv_port_disp_init(void);  /* Initialize display port */
static void disp_flush_cb(lv_disp_drv_t * disp_drv, 
        const lv_area_t * area, 
        lv_color_t * color_p);  /* Flush callback */
```

#### Input Device
```c
void lv_port_indev_init(void);  /* Initialize input device */
static bool touchpad_read_cb(lv_indev_drv_t * indev_drv, 
        lv_indev_data_t * data);  /* Touch callback */
```

#### Memory Management
```c
#define LV_MEM_SIZE (128 * 1024)  /* 128KB memory pool */
void * lv_mem_alloc(size_t size);
void lv_mem_free(void * data);
```

### 3.2 Configuration (lv_conf.h)

```c
#define LV_COLOR_DEPTH          16      /* RGB565 */
#define LV_HOR_RES_MAX          460     /* Horizontal resolution */
#define LV_VER_RES_MAX          460     /* Vertical resolution */
#define LV_MEM_CUSTOM           1       /* Custom memory */
#define LV_MEM_SIZE             (128U * 1024U)
#define LV_USE_ANIMATION        1       /* Enable animations */
#define LV_USE_FILESYSTEM       1       /* File system support */
```

## 4. Hardware Interface Implementation

### 4.1 File System Interface

The embedded team must implement:

```c
typedef struct {
  char name[64];      /* Filename */
  char path[256];     /* Full path */
  uint8_t is_dir;     /* Is directory */
  uint32_t size;      /* File size */
} fs_file_info_t;

int fs_open_dir(const char *path);
int fs_read_dir(int dir_handle, fs_file_info_t *file_info);
int fs_close_dir(int dir_handle);
int fs_delete_file(const char *path);
```

### 4.2 Audio Playback Interface

```c
typedef enum {
  AUDIO_STATE_STOPPED,
  AUDIO_STATE_PLAYING,
  AUDIO_STATE_PAUSED,
  AUDIO_STATE_LOADING
} audio_state_t;

int audio_play(const char *file_path);
int audio_pause(void);
int audio_resume(void);
int audio_stop(void);
int audio_get_position(uint32_t *current, uint32_t *total);
audio_state_t audio_get_state(void);
```

### 4.3 Audio Processing Interface

```c
typedef struct {
  uint8_t effect_id;    /* Effect ID */
  uint8_t enabled;      /* Enable flag */
  int32_t param1;       /* Parameter 1 */
  int32_t param2;       /* Parameter 2 */
  int32_t param3;       /* Parameter 3 */
  char name[32];        /* Effect name */
} dsp_effect_t;

int dsp_init(void);
int dsp_set_effect_chain(dsp_effect_t *effects, uint8_t count);
int dsp_update_effect_param(uint8_t index, uint8_t param_id, int32_t value);
int dsp_get_processing_load(uint8_t *load_percent);
```

## 5. Chinese Font Support

### 5.1 Current Status

The current configuration supports **English only**. Chinese filenames and UI text will display as garbled characters or spaces.

### 5.2 Adding Chinese Support

#### Option 1: LVGL Built-in CJK Font (Recommended)
```c
#define LV_FONT_SIMSUN_16_CJK    1
#define LV_USE_FONT_COMPRESSED   1
```

#### Option 2: Custom Font File
```bash
# Generate Chinese font using lv_font_conv
lv_font_conv --font msyh.ttf --size 16 --format lvgl \
     --bpp 4 --no-compress -o my_font_16.c
```

#### Memory Considerations
| Font Scheme | Memory | Description |
|------------|--------|-------------|
| English only | ~30KB | Current |
| CJK basic (3500 chars) | ~200KB | 16px, 4bpp |
| Full CJK (20k+ chars) | ~1.2MB | Exceeds STM32H743 |

**Recommendation**: Include only characters used in your project.

## 6. Memory Layout

```
Total RAM: 1MB
├── LVGL Display Buffer: 65KB
├── LVGL Memory Pool: 128KB
├── Audio Processing Buffer: 64KB
├── File System Cache: 32KB
├── Task Stacks: 64KB
└── Available: ~700KB
```

## 7. Project Structure

```
lvgl_simulator/
├── main.c                    # Frontend implementation
├── lv_conf.h                 # LVGL configuration
├── lv_drv_conf.h             # Driver configuration
├── CMakeLists.txt            # Build configuration
├── Dockerfile                # Docker setup
├── lvgl/                     # LVGL v8.3 library
├── lv_drivers/               # Display/input drivers
└── bin/                      # Compiled output
```

## 8. Troubleshooting

| Issue | Solution |
|-------|----------|
| GCC not found | Verify MinGW PATH environment variable |
| CMake build error | Delete `build/` folder and rebuild |
| No display output | Check `disp_flush_cb()` is called |
| Touch not responding | Verify I2C communication and `touchpad_read_cb()` |
| Chinese text shows garbled | Add Chinese font support (Section 5) |
| Out of memory | Reduce font sizes or buffer sizes in `lv_conf.h` |

## 9. Contributing

1. Fork the repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Commit changes: `git commit -am 'Add feature'`
4. Push to branch: `git push origin feature/my-feature`
5. Submit Pull Request

## 10. References

- [LVGL Documentation](https://docs.lvgl.io/)
- [LVGL GitHub](https://github.com/lvgl/lvgl)
- [Online Simulator](https://sim.lvgl.io/)
- [PC Simulator Template (v8.3)](https://github.com/lvgl/lv_port_pc_eclipse/tree/release/v8.3)

## 11. License

See `LICENSE` file for details.

---

**Note**: Project paths must not contain Chinese characters or spaces!
