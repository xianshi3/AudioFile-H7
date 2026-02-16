# STM32H743VIT6 音频处理器系统 - LVGL前端开发文档（修订版）

## 文档版本历史

| 版本 | 日期 | 作者 | 修改说明 |
|------|------|------|---------|
| V1.1 | 2026-02-16 | LVGL前端组 | 移除显存配置优化和对象池优化说明，添加中文支持说明 |

## 1. 项目概述

### 1.1 系统架构

本项目是基于STM32H743VIT6的音频处理系统，采用LVGL v8.3作为图形用户界面框架。系统包含文件管理、音频播放、实时音频处理三大核心功能。

### 1.2 硬件资源规格

| 参数 | 规格 | 说明 |
|------|------|------|
| MCU | STM32H743VIT6 | Cortex-M7 @ 480MHz |
| Flash | 2MB | 程序存储 |
| RAM | 1MB | 运行内存 |
| 显示分辨率 | 460x460 | RGB LCD接口 |
| 显示颜色深度 | 16-bit | RGB565 |
| 触摸屏 | 电容式 | I2C接口 |

### 1.3 内存分配方案

```
总RAM: 1MB (1048576字节)
├── LVGL显存缓冲区: 64x1024 = 65536字节 (双缓冲区)
│   ├── 缓冲区1: 32KB
│   └── 缓冲区2: 32KB
├── LVGL动态内存池: 128KB
├── 音频处理双重缓冲区: 64KB
│   ├── ADC缓冲区: 32KB
│   └── DAC缓冲区: 32KB
├── 文件系统缓存: 32KB
├── 任务栈空间: 64KB
│   ├── 主任务栈: 16KB
│   ├── 音频处理任务栈: 32KB
│   └── LVGL任务栈: 16KB
└── 剩余可用内存: ~700KB
```

## 2. LVGL v8.3 移植指南

### 2.1 LVGL版本说明

当前模拟器使用的LVGL版本为v8.3，与STM32H743目标板保持版本一致。

**版本特征：**

- 内存优化：支持自定义内存分配器
- 抗锯齿：支持边缘平滑渲染
- 动画系统：内置补间动画
- 字体引擎：支持多种字体格式

### 2.2 关键接口说明

#### 2.2.1 显示驱动接口

```c
/* lv_port_disp.h - 显示端口配置 */
typedef struct {
  void (*flush_cb)(struct _lv_disp_drv_t * disp_drv, 
           const lv_area_t * area, 
           lv_color_t * color_p);  /* 刷新回调 */
  uint32_t draw_buf_size;                    /* 缓冲区大小 */
  lv_color_t *buf_1;                         /* 缓冲区1 */
  lv_color_t *buf_2;                         /* 缓冲区2 */
} lv_disp_drv_t;

/* 初始化函数原型 */
void lv_port_disp_init(void);
```

#### 2.2.2 输入设备接口

```c
/* lv_port_indev.h - 输入设备端口 */
typedef struct {
  bool (*read_cb)(struct _lv_indev_drv_t * indev_drv, 
          lv_indev_data_t * data);    /* 读取回调 */
  lv_indev_type_t type;                        /* 设备类型 */
} lv_indev_drv_t;

/* 初始化函数原型 */
void lv_port_indev_init(void);
```

#### 2.2.3 内存接口

```c
/* lv_mem.h - 内存管理 */
void * lv_mem_alloc(size_t size);    /* 分配内存 */
void lv_mem_free(void * data);        /* 释放内存 */
void * lv_mem_realloc(void * data_p, size_t new_size); /* 重新分配 */

/* 自定义内存池配置 */
#define LV_MEM_SIZE (128 * 1024)      /* 128KB LVGL内存池 */
#define LV_MEM_ADR 0                   /* 自动分配 */
```

### 2.3 目标板移植配置

#### 2.3.1 lv_conf.h 关键配置

```c
/* lv_conf.h - v8.3版本配置 */

/* 1. 基本配置 */
#define LV_USE_PERF_MONITOR     0       /* 关闭性能监控 */
#define LV_USE_MEM_MONITOR      0       /* 关闭内存监控 */
#define LV_COLOR_DEPTH          16      /* 16位RGB565 */
#define LV_COLOR_16_SWAP        0       /* 不交换字节顺序 */

/* 2. 内存配置 */
#define LV_MEM_CUSTOM           1       /* 使用自定义内存管理 */
#define LV_MEM_SIZE             (128U * 1024U)  /* 128KB LVGL专用内存 */

/* 3. 显示缓冲区配置 */
#define LV_VER_RES_MAX          460     /* 垂直分辨率 */
#define LV_HOR_RES_MAX          460     /* 水平分辨率 */

/* 4. 字体配置 - 只启用需要的字体以节省空间 */
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_36   1
#define LV_FONT_DEFAULT          &lv_font_montserrat_14

/* 5. 功能模块配置 - 只启用必要模块 */
#define LV_USE_THEME_DEFAULT    1       /* 默认主题 */
#define LV_USE_LOG              0       /* 关闭日志 */
#define LV_USE_ANIMATION        1       /* 启用动画 */
#define LV_USE_FILESYSTEM       1       /* 启用文件系统 */
```

#### 2.3.2 显示驱动移植代码

```c
/* lv_port_disp.c - 核心显示驱动 */

/* 双缓冲区定义 - 64KB对齐到DMA可访问内存 */
static lv_color_t buf_1[LV_HOR_RES_MAX * 100] __attribute__((section(".sram")));
static lv_color_t buf_2[LV_HOR_RES_MAX * 100] __attribute__((section(".sram")));

/**
 * @brief 初始化显示端口
 * @note 必须在LVGL初始化后调用
 */
void lv_port_disp_init(void)
{
  /* 初始化显示缓冲区 */
  static lv_disp_draw_buf_t draw_buf;
  lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, LV_HOR_RES_MAX * 100);

  /* 注册显示驱动 */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  disp_drv.antialiasing = 1;  /* 启用抗锯齿 */

  lv_disp_drv_register(&disp_drv);
}

/**
 * @brief 显示刷新回调
 * @param disp_drv 显示驱动指针
 * @param area 需要刷新的区域
 * @param color_p 像素数据指针
 * @note 由LVGL自动调用，通过DMA传输至LCD
 */
static void disp_flush_cb(lv_disp_drv_t * disp_drv, 
              const lv_area_t * area, 
              lv_color_t * color_p)
{
  /* 计算参数 */
  uint32_t width = lv_area_get_width(area);
  uint32_t height = lv_area_get_height(area);
  uint32_t x_start = area->x1;
  uint32_t y_start = area->y1;
  
  /* 启动DMA传输 */
  lcd_dma2d_transfer(x_start, y_start, width, height, (uint16_t*)color_p);
  
  /* 通知LVGL刷新完成 */
  lv_disp_flush_ready(disp_drv);
}
```

#### 2.3.3 触摸屏驱动移植

```c
/* lv_port_indev.c - 输入设备驱动 */

/**
 * @brief 初始化输入设备
 */
void lv_port_indev_init(void)
{
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read_cb;
  lv_indev_drv_register(&indev_drv);
}

/**
 * @brief 触摸读取回调
 * @param indev_drv 输入驱动指针
 * @param data 数据输出
 * @return false - 无数据, true - 有数据
 */
static bool touchpad_read_cb(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
  static uint16_t last_x = 0;
  static uint16_t last_y = 0;
  
  /* 读取触摸控制器 (例如FT5336) */
  if (touch_ic_get_point(&last_x, &last_y)) {
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  
  return false;  /* 没有更多数据要读取 */
}
```

### 2.4 性能优化配置

#### 2.4.1 渲染优化

```c
/* lv_conf.h - 渲染优化 */

#define LV_DISP_ROT_MAX_BUF      (10U * 1024U)  /* 旋转缓冲区10KB */
#define LV_USE_PERF_MONITOR      0               /* 禁用性能监控 */
#define LV_USE_REFR_DEBUG        0               /* 禁用调试 */

/* 启用硬件加速 */
#define LV_USE_GPU_ARM2D         1               /* 使用ARM-2D加速 */
#define LV_USE_GPU_STM32_DMA2D   1               /* 使用DMA2D加速 */
```

#### 2.4.2 任务调度优化

```c
/* 主循环优化 - 嵌入式系统典型配置 */
void main_loop(void)
{
  uint32_t tick_last = HAL_GetTick();
  
  while(1) {
    /* LVGL定时器处理 */
    lv_timer_handler();
    
    /* 自适应延时 - 根据处理时间动态调整 */
    uint32_t tick_now = HAL_GetTick();
    uint32_t process_time = tick_now - tick_last;
    tick_last = tick_now;
    
    if (process_time < 20) {
      HAL_Delay(20 - process_time);  /* 保持20ms周期 */
    }
    
    /* 音频处理任务 */
    audio_process_task();
  }
}
```

## 3. 中文支持说明

### 3.1 当前限制说明

> **重要提示：** 当前LVGL配置未包含中文字库，无法显示中文文件名和中文界面文字。

当前模拟器代码中所有界面文字均为英文：
- 应用标题： "Audio Processor", "File Manager" 等
- 按钮文字： "Delete", "Cancel" 等
- 提示信息： "Not playing", "No tracks" 等
- 效果器名称： "Reverb", "Echo" 等

### 3.2 中文文件名显示问题

当音乐文件名为中文时，当前系统会出现以下情况：
- 文件列表中将显示乱码或空白
- "Now Playing" 区域无法正确显示歌曲名
- 通知消息中的文件名无法正常显示

### 3.3 解决方案（如需中文支持）

如需支持中文显示，嵌入式组需在LVGL中集成中文字库：

#### 3.3.1 方案一：使用LVGL内置CJK字体（推荐）

```c
/* lv_conf.h - 启用CJK统一汉字字体 */
#define LV_FONT_SIMSUN_16_CJK    1  /* 16像素宋体CJK */
#define LV_FONT_SIMSUN_16_CJK    1  /* 也可使用 */

/* 或使用更完整的CJK字体 */
#define LV_USE_FONT_COMPRESSED   1  /* 启用字体压缩 */
#define LV_FONT_MONTSERRAT_14     1  /* 保留英文字体 */
```

#### 3.3.2 方案二：自定义字体文件

```c
/* 1. 使用LVGL字体转换工具(lv_font_conv)生成中文字库 */
/* 命令示例： */
/*
lv_font_conv --font msyh.ttf --size 16 --format lvgl \
       --bpp 4 --no-compress --no-prefilter \
       -o my_font_16.c --symbols "中文常用字符集"
*/

/* 2. 在代码中声明外部字体 */
LV_FONT_DECLARE(my_font_16);

/* 3. 设置默认字体 */
lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE),
            lv_palette_main(LV_PALETTE_RED),
            false, &my_font_16);
```

#### 3.3.3 方案三：使用字体回退机制

```c
/* lv_conf.h */
#define LV_USE_FONT_FALLBACK     1  /* 启用字体回退 */

/* 代码中设置字体回退链 */
lv_font_t * font_fallback = &my_chinese_font;
lv_font_set_fallback(&lv_font_montserrat_14, font_fallback);
```

### 3.4 内存占用考虑

| 字体方案 | 内存占用 | 说明 |
|---------|---------|------|
| 纯英文字体 | ~30KB | 当前配置 |
| CJK基本集(常用3500字) | ~200KB | 16px, 4bpp |
| CJK完整集(2万字+) | ~1.2MB | 超出STM32H743内存 |

**建议：** 如需中文支持，只包含项目中实际使用的中文字符，或仅包含GB2312常用汉字集。

## 4. 前端接口定义

### 4.1 模拟数据与真实数据映射

#### 4.1.1 文件管理器接口

```c
/* 模拟器版本 - 使用本地文件系统 */
static void load_directory(const char *path, lv_obj_t *list)
{
  DIR *dir = opendir(path);  /* PC本地目录 */
  // ... 处理逻辑
}

/* 真实硬件版本 - 需嵌入式组提供接口 */

/* ========== 嵌入式组需实现 ========== */
/**
 * @brief 文件信息结构体 - 与前端对齐
 */
typedef struct {
  char name[64];      /* 文件名 - 可能为UTF-8编码的中文 */
  char path[256];     /* 完整路径 */
  uint8_t is_dir;     /* 是否为目录 */
  uint32_t size;      /* 文件大小 */
} fs_file_info_t;

/**
 * @brief 文件系统操作接口
 * @note 嵌入式组需实现这些函数供前端调用
 */
int fs_open_dir(const char *path);                          /* 打开目录 */
int fs_read_dir(int dir_handle, fs_file_info_t *file_info); /* 读取目录项 */
int fs_close_dir(int dir_handle);                           /* 关闭目录 */
int fs_delete_file(const char *path);                       /* 删除文件 */
/* ========== 嵌入式组需实现 END ========== */
```

#### 4.1.2 音频播放器接口

```c
/* 模拟器版本 - 模拟播放进度 */
static void audio_player_timer_cb(lv_timer_t *timer)
{
  static int progress = 0;
  progress = (progress + 1) % 101;  /* 模拟进度 */
  lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
}

/* 真实硬件版本 - 需嵌入式组提供音频服务接口 */

/* ========== 嵌入式组需实现 ========== */
/**
 * @brief 音频播放状态
 */
typedef enum {
  AUDIO_STATE_STOPPED,
  AUDIO_STATE_PLAYING,
  AUDIO_STATE_PAUSED,
  AUDIO_STATE_LOADING
} audio_state_t;

/**
 * @brief 音频播放接口
 */
int audio_play(const char *file_path);           /* 播放音频文件 */
int audio_pause(void);                           /* 暂停播放 */
int audio_resume(void);                          /* 恢复播放 */
int audio_stop(void);                            /* 停止播放 */
int audio_get_position(uint32_t *current, uint32_t *total); /* 获取播放位置 */
int audio_set_position(uint32_t position);       /* 设置播放位置 */
audio_state_t audio_get_state(void);             /* 获取播放状态 */

/**
 * @brief 音频文件信息
 */
typedef struct {
  char file_name[64];    /* 文件名 - 可能为UTF-8编码的中文 */
  uint32_t duration;     /* 总时长(秒) */
  uint32_t sample_rate;  /* 采样率 */
  uint8_t channels;      /* 声道数 */
} audio_file_info_t;

int audio_get_file_info(const char *file_path, audio_file_info_t *info);
/* ========== 嵌入式组需实现 END ========== */
```

#### 4.1.3 实时音频处理接口

```c
/* 模拟器版本 - 仅界面效果 */
static void on_config_slider_change(lv_event_t *e)
{
  /* 仅更新UI显示值，无实际音频处理 */
  lv_label_set_text_fmt(data->value_label, "%d", (int)value);
}

/* 真实硬件版本 - 需嵌入式组提供音频DSP接口 */

/* ========== 嵌入式组需实现 ========== */
/**
 * @brief 效果器参数 - 与前端结构对齐
 */
typedef struct {
  uint8_t effect_id;           /* 效果器ID */
  uint8_t enabled;             /* 是否启用 */
  int32_t param1;              /* 参数1 */
  int32_t param2;              /* 参数2 */
  int32_t param3;              /* 参数3 */
  char name[32];               /* 效果器名称 - 英文 */
} dsp_effect_t;

/**
 * @brief DSP处理接口
 */
int dsp_init(void);                                 /* 初始化DSP */
int dsp_set_effect_chain(dsp_effect_t *effects, uint8_t count); /* 设置效果链 */
int dsp_update_effect_param(uint8_t index, uint8_t param_id, int32_t value); /* 更新参数 */
int dsp_bypass_effect(uint8_t index, uint8_t bypass); /* 旁路效果器 */
int dsp_get_processing_load(uint8_t *load_percent);   /* 获取处理负载 */

/**
 * @brief 音频数据流接口
 * @note 双重缓冲区管理
 */
typedef struct {
  int16_t *adc_buffer;        /* ADC输入缓冲区 */
  int16_t *dac_buffer;        /* DAC输出缓冲区 */
  uint32_t buffer_size;       /* 缓冲区大小(样本数) */
  volatile uint8_t buffer_ready; /* 缓冲区就绪标志 */
} audio_buffer_t;

int audio_stream_start(void);                        /* 启动音频流 */
int audio_stream_stop(void);                         /* 停止音频流 */
audio_buffer_t* audio_get_next_buffer(void);         /* 获取下一个缓冲区 */
/* ========== 嵌入式组需实现 END ========== */
```

### 4.2 前端回调函数定义

前端已定义好的回调函数，嵌入式组需在适当的时机调用：

```c
/* main.c - 前端已实现的事件回调 */

/**
 * @brief 应用点击回调
 * @param e LVGL事件对象
 * @note 嵌入式组无需修改
 */
static void on_app_click(lv_event_t *e);

/**
 * @brief 文件点击回调
 * @param e LVGL事件对象
 * @note 文件删除操作会调用fs_delete_file接口
 */
static void on_file_click(lv_event_t *e);

/**
 * @brief 音频文件点击回调
 * @param e LVGL事件对象
 * @note 会触发音频播放，调用audio_play接口
 */
static void on_audio_file_click(lv_event_t *e);

/**
 * @brief 播放/暂停按钮回调
 * @param e LVGL事件对象
 * @note 调用audio_pause/audio_resume接口
 */
static void on_play_click(lv_event_t *e);

/**
 * @brief 效果器参数更新回调
 * @param e LVGL事件对象
 * @note 调用dsp_update_effect_param接口
 */
static void on_config_slider_change(lv_event_t *e);
```

## 5. 关键组件实现

### 5.1 效果器链管理

```c
/* 前端效果器状态管理 - 完整实现参考main.c */

/**
 * @brief 效果器预设配置
 * @note 嵌入式组需根据实际DSP能力调整参数范围
 */
static const effect_config_t effect_presets[] = {
  {
    .name = "Reverb",
    .type = EFFECT_REVERB,
    .default_param1 = 50,
    .param_names = {"Mix", "", ""},
    .param_min = {0, 0, 0},
    .param_max = {100, 0, 0}
  },
  {
    .name = "Echo",
    .type = EFFECT_ECHO,
    .default_param1 = 30,
    .param_names = {"Delay", "", ""},
    .param_min = {0, 0, 0},
    .param_max = {100, 0, 0}
  },
  // ... 更多效果器配置
};

/**
 * @brief 更新效果链显示
 * @note 前端UI自动调用，嵌入式组可通过此函数监控效果链状态
 */
static void update_effect_chain_display(void)
{
  static char chain_text[128];
  chain_text[0] = '\0';
  int enabled_count = 0;
  
  for (int i = 0; i < 5; i++) {
    if (app_ctx->effects[i].enabled) {
      if (enabled_count > 0) strcat(chain_text, " → ");
      strcat(chain_text, app_ctx->effects[i].name);
      enabled_count++;
    }
  }
  
  lv_label_set_text(app_ctx->chain_label, 
            enabled_count ? chain_text : "No active effects");
}
```

### 5.2 定时器管理

```c
/* 前端定时器管理 - 嵌入式组需根据实际硬件配置 */

/**
 * @brief 音频播放器定时器回调
 * @note 嵌入式组需修改为从硬件获取实际播放进度
 */
static void audio_player_timer_cb(lv_timer_t *timer)
{
  app_context_t *ctx = (app_context_t *)timer->user_data;
  if (!ctx || !ctx->timer_running || !ctx->is_playing) return;

  /* 嵌入式组需替换为: audio_get_position(&current, &total) */
  static int progress = 0;
  progress = (progress + 1) % 101;  /* 模拟数据 - 待替换 */
  lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
  
  /* 嵌入式组需替换为实际总时长 */
  int total = 180;  /* 模拟数据 - 待替换 */
  int current = (progress * total) / 100;
  
  char time_str[16];
  sprintf(time_str, "%02d:%02d", current / 60, current % 60);
  lv_label_set_text(ctx->time_label, time_str);
}

/**
 * @brief 音频处理器定时器回调
 * @note 可用于更新DSP负载显示
 */
static void audio_processor_timer_cb(lv_timer_t *timer)
{
  /* 嵌入式组可在此添加DSP监控功能 */
  uint8_t load = 0;
  if (dsp_get_processing_load(&load) == 0) {
    /* 可选：显示DSP负载 */
  }
}
```

## 6. 字体优化建议

### 6.1 当前字体配置

```c
/* 当前启用的字体大小 - 全部为英文字体 */
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_36   1
```

### 6.2 字体大小使用统计

| 字体大小 | 使用场景 |
|---------|---------|
| 12px | 底部提示、通知消息 |
| 14px | 文件列表、按钮文字、参数标签 |
| 16px | 标题栏、列表标题 |
| 18px | 页面主标题 |
| 20px | 效果器图标 |
| 22px | 主屏幕标题 |
| 24px | 歌曲名显示 |
| 28px | 控制按钮图标 |
| 32px | 功能卡片图标 |
| 36px | 播放按钮图标 |

## 7. 模拟数据替换指南

### 7.1 文件列表加载

**模拟器代码(pc_simulator):**

```c
static void load_directory(const char *path, lv_obj_t *list)
{
  DIR *dir = opendir(path);  /* PC文件系统 */
  // ...
}
```

**需替换为硬件代码:**

```c
static void load_directory(const char *path, lv_obj_t *list)
{
  /* 1. 调用嵌入式组提供的文件系统接口 */
  int dir_handle = fs_open_dir(path);
  if (dir_handle < 0) {
    show_notification("Cannot open directory", lv_color_hex(0xe74c3c));
    return;
  }

  lv_obj_clean(list);
  app_ctx->file_count = 0;

  /* 2. 添加上级目录项 */
  if (strcmp(path, "/") != 0) {
    lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_DIRECTORY, "..");
    lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  }

  /* 3. 循环读取目录项 */
  fs_file_info_t file_info;
  while (fs_read_dir(dir_handle, &file_info) == 0 && 
       app_ctx->file_count < MAX_FILES) {
    
    file_info_t *file = &app_ctx->files[app_ctx->file_count];
    strcpy(file->name, file_info.name);
    strcpy(file->path, file_info.path);
    file->is_dir = file_info.is_dir;
    file->size = file_info.size;

    const char *icon = file->is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
    lv_obj_t *btn = lv_list_add_btn(list, icon, file_info.name);
    lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, 
               (void *)(intptr_t)app_ctx->file_count);
    
    app_ctx->file_count++;
  }
  
  /* 4. 关闭目录句柄 */
  fs_close_dir(dir_handle);
}
```

### 7.2 音频播放进度

**模拟器代码:**

```c
static void audio_player_timer_cb(lv_timer_t *timer)
{
  static int progress = 0;
  progress = (progress + 1) % 101;  /* 模拟递增 */
  lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
}
```

**需替换为硬件代码:**

```c
static void audio_player_timer_cb(lv_timer_t *timer)
{
  app_context_t *ctx = (app_context_t *)timer->user_data;
  if (!ctx || !ctx->timer_running || !ctx->is_playing) return;

  /* 获取实际播放位置 */
  uint32_t current_ms = 0;
  uint32_t total_ms = 0;
  
  if (audio_get_position(&current_ms, &total_ms) == 0 && total_ms > 0) {
    /* 计算进度百分比 */
    uint32_t progress = (current_ms * 100) / total_ms;
    lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
    
    /* 更新时间显示 */
    char time_str[16];
    uint32_t current_sec = current_ms / 1000;
    uint32_t total_sec = total_ms / 1000;
    sprintf(time_str, "%02d:%02d", current_sec / 60, current_sec % 60);
    lv_label_set_text(ctx->time_label, time_str);
  }
}
```

### 7.3 效果器参数更新

**模拟器代码:**

```c
static void on_config_slider_change(lv_event_t *e)
{
  int32_t value = lv_slider_get_value(slider);
  lv_label_set_text_fmt(data->value_label, "%d", (int)value);
  
  /* 仅更新本地存储 */
  effect->param1 = value;
}
```

**需替换为硬件代码:**

```c
static void on_config_slider_change(lv_event_t *e)
{
  lv_obj_t *slider = lv_event_get_current_target(e);
  param_slider_data_t *data = (param_slider_data_t *)lv_event_get_user_data(e);
  
  if (!data) return;
  
  int32_t value = lv_slider_get_value(slider);
  
  /* 更新UI显示 */
  if (data->value_label && lv_obj_is_valid(data->value_label)) {
    lv_label_set_text_fmt(data->value_label, "%d", (int)value);
  }
  
  /* 更新本地存储 */
  if (data->effect_index >= 0 && data->effect_index < 6) {
    effect_t *effect = &app_ctx->effects[data->effect_index];
    switch (data->param_index) {
      case 0: effect->param1 = value; break;
      case 1: effect->param2 = value; break;
      case 2: effect->param3 = value; break;
    }
    
    /* 调用DSP接口更新实际效果器参数 */
    dsp_update_effect_param(data->effect_index, data->param_index, value);
  }
}
```

## 8. 调试与测试

### 8.1 内存监控

```c
/* 调试辅助函数 - 可选 */
void debug_print_memory_info(void)
{
  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  
  printf("LVGL Memory:\n");
  printf("  Total: %d bytes\n", mon.total_size);
  printf("  Used: %d bytes (%d%%)\n", mon.used_size, mon.used_pct);
  printf("  Frag: %d%%\n", mon.frag_pct);
}
```

### 8.2 性能分析

```c
/* FPS监控 */
void debug_fps_monitor(void)
{
  static uint32_t last_tick = 0;
  static uint32_t frame_count = 0;
  static uint32_t fps = 0;
  
  frame_count++;
  uint32_t now = HAL_GetTick();
  
  if (now - last_tick >= 1000) {
    fps = frame_count;
    frame_count = 0;
    last_tick = now;
    printf("FPS: %d\n", fps);
  }
}
```

## 9. 版本兼容性说明

### 9.1 LVGL v8.3 API变更

| v7.x API | v8.3 API | 说明 |
|----------|----------|------|
| lv_scr_act() | lv_scr_act() | 兼容 |
| lv_obj_del() | lv_obj_del() | 兼容 |
| lv_label_set_text() | lv_label_set_text() | 兼容 |
| lv_btn_create() | lv_btn_create() | 兼容 |
| lv_cont_create() | lv_obj_create() | 容器改用通用对象 |

### 9.2 项目文件结构

```
lvgl_simulator/
├── main.c                      # 前端主程序 (59,971 bytes)
├── lv_conf.h                   # LVGL配置文件 (24,421 bytes)
├── lv_drv_conf.h               # 驱动配置文件 (15,441 bytes)
├── mouse_cursor_icon.c         # 鼠标光标资源 (21,242 bytes)
├── CMakeLists.txt              # CMake构建配置
├── Makefile                    # Make构建脚本
├── confdef.txt                 # 配置定义文件
├── Dockerfile                  # Docker容器配置
├── licence.txt                 # 许可证信息
├── pc_simulator.launch         # IDE启动配置
├── .project                    # IDE项目文件
├── .cproject                   # IDE C项目文件
├── .editorconfig               # 编辑器配置
├── .gitignore                  # Git忽略规则
├── .gitmodules                 # Git子模块配置
├── lvgl/                       # LVGL v8.3核心库
│   ├── src/                    # 源代码目录
│   ├── examples/               # LVGL示例
│   └── ...                     # 其他LVGL文件
├── lv_drivers/                 # LVGL驱动层
│   ├── display/                # 显示驱动
│   ├── indev/                  # 输入设备驱动
│   └── ...                     # 其他驱动文件
├── build/                      # CMake编译输出目录
│   ├── CMakeCache.txt
│   ├── CMakeFiles/
│   ├── Makefile
│   └── ...                     # 编译中间文件
├── bin/                        # 编译后的可执行文件
│   └── lvgl_simulator          # 最终可执行程序
├── .github/                    # GitHub配置目录
│   └── workflows/              # CI/CD工作流
└── LVGL应用开发文档.md          # 本文档
```

**重要文件说明：**

| 文件 | 大小 | 用途 |
|------|------|------|
| main.c | ~60KB | 前端核心实现，包含UI逻辑和事件处理 |
| lv_conf.h | ~24KB | LVGL全局配置，包含显示、内存、字体等设置 |
| lv_drv_conf.h | ~15KB | 显示和输入驱动配置 |
| mouse_cursor_icon.c | ~21KB | PC模拟器鼠标光标资源文件 |
| CMakeLists.txt | - | 跨平台编译配置（推荐方式） |
| Makefile | - | 传统Make编译脚本（可选） |
| Dockerfile | - | Docker容器打包配置 |

**编译步骤：**

```bash
# 方式一：使用CMake（推荐）
mkdir build && cd build
cmake ..
make

# 方式二：使用Makefile
make

# 执行
./bin/lvgl_simulator
```

## 10. 附录

### 10.1 常用LVGL API参考

```c
/* 对象创建 */
lv_obj_t * lv_obj_create(lv_obj_t * parent);
lv_obj_t * lv_btn_create(lv_obj_t * parent);
lv_obj_t * lv_label_create(lv_obj_t * parent);
lv_obj_t * lv_list_create(lv_obj_t * parent);
lv_obj_t * lv_slider_create(lv_obj_t * parent);
lv_obj_t * lv_switch_create(lv_obj_t * parent);

/* 对象属性设置 */
void lv_obj_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h);
void lv_obj_set_pos(lv_obj_t * obj, lv_coord_t x, lv_coord_t y);
void lv_obj_align(lv_obj_t * obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);
void lv_obj_set_style_bg_color(lv_obj_t * obj, lv_color_t color, lv_style_selector_t sel);
void lv_obj_set_style_text_color(lv_obj_t * obj, lv_color_t color, lv_style_selector_t sel);
void lv_obj_set_style_radius(lv_obj_t * obj, lv_coord_t radius, lv_style_selector_t sel);
void lv_obj_add_flag(lv_obj_t * obj, lv_obj_flag_t f);
void lv_obj_clear_flag(lv_obj_t * obj, lv_obj_flag_t f);

/* 事件处理 */
void lv_obj_add_event_cb(lv_obj_t * obj, lv_event_cb_t event_cb, lv_event_code_t filter, void * user_data);

/* 定时器 */
lv_timer_t * lv_timer_create(lv_timer_cb_t timer_cb, uint32_t period, void * user_data);
void lv_timer_del(lv_timer_t * timer);
```

### 10.2 常见问题排查

| 问题 | 可能原因 | 解决方案 |
|------|--------|---------|
| 屏幕无显示 | 刷新回调未调用 | 检查DMA传输是否完成，调用lv_disp_flush_ready() |
| 触摸无响应 | 读取回调未更新数据 | 检查I2C通信，确保data->state正确设置 |
| 内存不足 | LV_MEM_SIZE过小 | 增加内存池大小，减少不必要的对象 |
| 渲染卡顿 | 缓冲区太小 | 增加缓冲区行数，启用DMA2D加速 |
| 字体显示异常 | 字体未编译 | 检查lv_conf.h中对应字体宏定义 |
| 中文显示乱码 | 未启用中文字体 | 参考第3章添加中文字库支持 |

### 10.3 联系方式

| 角色 | 负责人 | 联系方式 |
|------|--------|---------|
| LVGL前端组 | Reality | - |
| 嵌入式组 | - | - |
| 项目负责人 | - | - |