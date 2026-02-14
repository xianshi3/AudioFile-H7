/**
 * @file main.c
 * @brief 音频文件处理器 - LVGL图形用户界面
 * @version 2.2
 * @date 2024-01-20
 */

/*********************
 *      头文件包含
 *********************/
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

/* 平台检测 */
#if defined(__linux__) || defined(__APPLE__)
    #define HAVE_EXECINFO 1
    #include <execinfo.h>
#else
    #define HAVE_EXECINFO 0
#endif

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/sdl/sdl.h"

/*********************
 *      宏定义
 *********************/
#define MAX_PATH 256
#define MAX_FILES 128
#define MAX_TIMERS 10
#define MAX_EFFECTS 8
#define TIMER_STACK_SIZE 8192
#define LOG_DIR "./logs"
#define LOG_BUFFER_SIZE 4096
#define LOG_FLUSH_INTERVAL 50

#define HEADER_HEIGHT 50
#define FOOTER_HEIGHT 85
#define BUTTON_WIDTH 220
#define BUTTON_HEIGHT 60
#define BACK_BTN_SIZE 50

/* 日志级别控制 */
#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG  /* 开发阶段用DEBUG */
#endif

#ifndef ENABLE_DEBUG_LOGS
#define ENABLE_DEBUG_LOGS 1  /* 1:开启调试日志 */
#endif

/* 优化的日志宏 */
#if CURRENT_LOG_LEVEL <= LOG_LEVEL_DEBUG && ENABLE_DEBUG_LOGS
    #define LOG_DEBUG(fmt, ...) \
        do { \
            if (log_ctx && log_ctx->initialized) { \
                log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
            } \
        } while(0)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if CURRENT_LOG_LEVEL <= LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...) \
        do { \
            if (log_ctx && log_ctx->initialized) { \
                log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
            } \
        } while(0)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

#if CURRENT_LOG_LEVEL <= LOG_LEVEL_WARN
    #define LOG_WARN(fmt, ...) \
        do { \
            if (log_ctx && log_ctx->initialized) { \
                log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
            } \
        } while(0)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#define LOG_ERROR(fmt, ...) \
    do { \
        if (log_ctx && log_ctx->initialized) { \
            log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/* 简化的检查宏 - 只在DEBUG模式下启用 */
#if ENABLE_DEBUG_LOGS
    #define CHECK_NULL(ptr, msg) do { \
        if ((ptr) == NULL) { \
            LOG_ERROR("Null pointer: %s at %s:%d", msg, __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

    #define CHECK_NULL_VOID(ptr, msg) do { \
        if ((ptr) == NULL) { \
            LOG_ERROR("Null pointer: %s at %s:%d", msg, __FILE__, __LINE__); \
            return; \
        } \
    } while(0)
#else
    #define CHECK_NULL(ptr, msg) ((void)0)
    #define CHECK_NULL_VOID(ptr, msg) ((void)0)
#endif

#define CREATE_LABEL(parent, text, font, align, x, y) \
    ({ \
        lv_obj_t *label = lv_label_create(parent); \
        if (label) { \
            lv_label_set_text(label, text); \
            lv_obj_set_style_text_font(label, font, 0); \
            lv_obj_align(label, align, x, y); \
        } \
        label; \
    })

#define CREATE_BTN(parent, width, height, event_cb, user_data) \
    ({ \
        lv_obj_t *btn = lv_btn_create(parent); \
        if (btn) { \
            lv_obj_set_size(btn, width, height); \
            lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data); \
        } \
        btn; \
    })

/**********************
 *      类型定义
 **********************/
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

typedef enum {
    APP_NONE = 0,
    APP_FILE_MANAGER,
    APP_AUDIO_PLAYER,
    APP_AUDIO_PROCESSOR
} app_type_t;

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_REVERB,
    EFFECT_ECHO,
    EFFECT_DISTORTION,
    EFFECT_EQ,
    EFFECT_FILTER
} effect_type_t;

typedef struct {
    char name[64];
    char path[MAX_PATH];
    int is_dir;
    int size;
} file_info_t;

typedef struct {
    const char *name;
    effect_type_t type;
    int default_param1;
    int default_param2;
    int default_param3;
    const char *param_names[3];
} effect_config_t;

typedef struct {
    effect_type_t type;
    int enabled;
    int param1;
    int param2;
    int param3;
    char name[32];
    lv_obj_t *btn;
} effect_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *header;
    lv_obj_t *back_btn;
    lv_obj_t *title;
    lv_obj_t *main_cont;
    lv_obj_t *list;
} screen_components_t;

/* 日志系统上下文 - 独立于应用上下文 */
typedef struct {
    FILE *file;
    char path[MAX_PATH];
    int initialized;
    int log_count;
    time_t start_time;
    char buffer[LOG_BUFFER_SIZE];
    int flush_counter;
} log_context_t;

typedef struct {
    /* 主屏幕 */
    lv_obj_t *main_screen;
    
    /* 当前状态 */
    app_type_t current_app;
    screen_components_t screen;
    
    /* 文件相关 */
    file_info_t files[MAX_FILES];
    int file_count;
    char current_path[MAX_PATH];
    
    /* 播放器相关 */
    int is_playing;
    int current_track;
    lv_obj_t *play_btn;
    lv_obj_t *progress_bar;
    lv_obj_t *time_label;
    lv_obj_t *now_playing_label;
    
    /* 效果器相关 */
    effect_t effects[MAX_EFFECTS];
    int effect_count;
    lv_obj_t *effect_cont;
    lv_obj_t *param_cont;
    
    /* 定时器管理 */
    lv_timer_t *app_timer;
    int timer_running;
    
    /* 初始化标志 */
    int initialized;
} app_context_t;

/* 效果器预设配置 */
static const effect_config_t effect_presets[] = {
    {"Reverb", EFFECT_REVERB, 50, 0, 0, {"Mix", "", ""}},
    {"Echo", EFFECT_ECHO, 30, 0, 0, {"Delay", "", ""}},
    {"Distortion", EFFECT_DISTORTION, 70, 0, 0, {"Drive", "", ""}},
    {"Equalizer", EFFECT_EQ, 50, 50, 50, {"Low", "Mid", "High"}},
    {"Filter", EFFECT_FILTER, 1000, 0, 0, {"Freq", "", ""}}
};

/**********************
 *      静态变量
 **********************/
static app_context_t *app_ctx = NULL;
static log_context_t *log_ctx = NULL;
static int crash_handlers_installed = 0;

/**********************
 *      静态函数声明
 **********************/
/* 硬件抽象层 */
static void hal_init(void);

/* 屏幕创建函数 */
static void create_main_screen(void);
static void create_app_screen(app_type_t app_type);
static void setup_header(lv_obj_t *screen, const char *title);
static void setup_file_manager_screen(void);
static void setup_audio_player_screen(void);
static void setup_audio_processor_screen(void);

/* 定时器回调 */
static void file_manager_timer_cb(lv_timer_t *timer);
static void audio_player_timer_cb(lv_timer_t *timer);
static void audio_processor_timer_cb(lv_timer_t *timer);

/* 文件操作 */
static void load_directory(const char *path, lv_obj_t *list);
static void load_audio_files(const char *path, lv_obj_t *list);

/* 事件处理 */
static void on_app_click(lv_event_t *e);
static void on_back_click(lv_event_t *e);
static void on_file_click(lv_event_t *e);
static void on_audio_file_click(lv_event_t *e);
static void on_delete_confirm(lv_event_t *e);
static void on_effect_click(lv_event_t *e);
static void on_play_click(lv_event_t *e);
static void on_stop_click(lv_event_t *e);
static void on_slider_change(lv_event_t *e);

/* UI辅助函数 */
static void show_notification(const char *msg, lv_color_t color);
static void cleanup_app(void);

/* 日志系统函数 */
static int init_logging(void);
static void close_logging(void);
static void log_message(int level, const char *file, int line, 
                       const char *func, const char *fmt, ...);
static void flush_log(void);
static void create_log_directory(void);

/* 崩溃处理 */
static void crash_handler(int sig);
static void install_crash_handlers(void);
static void print_crash_info(int sig);

/* 初始化检查 */
static int check_initialization(void);

/**********************
 *      全局函数
 **********************/
int main(int argc, char **argv)
{
    printf("Starting Audio File Processor...\n");
    printf("Current working directory: %s\n", getcwd(NULL, 0));
    
    /* 创建日志目录 */
    create_log_directory();
    
    /* 初始化日志系统（必须在其他初始化之前） */
    if (init_logging() < 0) {
        fprintf(stderr, "Failed to initialize logging system\n");
        return -1;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("Audio File Processor starting...");
    LOG_INFO("Version: 2.2");
    LOG_INFO("Platform: %s", 
        #if defined(_WIN32)
            "Windows"
        #elif defined(__linux__)
            "Linux"
        #elif defined(__APPLE__)
            "macOS"
        #else
            "Unknown"
        #endif
    );
    
    /* 安装崩溃处理程序 */
    install_crash_handlers();
    
    /* 检查命令行参数 */
    for (int i = 0; i < argc; i++) {
        LOG_DEBUG("argv[%d]: %s", i, argv[i]);
    }
    
    /* 初始化LVGL */
    LOG_INFO("Initializing LVGL...");
    lv_init();
    LOG_DEBUG("LVGL initialized successfully");
    
    /* 初始化硬件抽象层 */
    LOG_INFO("Initializing HAL...");
    hal_init();
    LOG_DEBUG("HAL initialized successfully");
    
    /* 分配应用上下文 */
    LOG_INFO("Allocating application context...");
    app_ctx = (app_context_t *)calloc(1, sizeof(app_context_t));
    if (app_ctx == NULL) {
        LOG_ERROR("Failed to allocate application context");
        close_logging();
        return -1;
    }
    
    /* 初始化应用上下文 */
    LOG_DEBUG("Initializing application context...");
    strcpy(app_ctx->current_path, "./");
    app_ctx->main_screen = NULL;
    app_ctx->initialized = 1;
    
    LOG_DEBUG("Current working directory: %s", app_ctx->current_path);
    
    /* 创建主屏幕 */
    LOG_INFO("Creating main screen...");
    create_main_screen();
    
    if (app_ctx->main_screen == NULL) {
        LOG_ERROR("Failed to create main screen");
        free(app_ctx);
        close_logging();
        return -1;
    }
    
    LOG_INFO("Application started successfully");
    LOG_INFO("========================================");
    
    /* 主循环 */
    int loop_count = 0;
    while(1) {
        lv_timer_handler();
        usleep(5 * 1000);
        
        /* 每10000次循环输出一次调试信息（降低频率） */
        loop_count++;
        if (loop_count % 10000 == 0) {
            LOG_DEBUG("Main loop running... (count: %d)", loop_count);
        }
        
        /* 定期刷新日志 */
        if (loop_count % 1000 == 0) {
            flush_log();
        }
    }
    
    /* 清理资源（实际上不会执行到这里） */
    close_logging();
    
    return 0;
}

/**********************
 *      日志系统实现
 **********************/

/**
 * @brief 创建日志目录
 */
static void create_log_directory(void)
{
    struct stat st = {0};
    
    if (stat(LOG_DIR, &st) == -1) {
        #if defined(_WIN32)
            mkdir(LOG_DIR);
        #else
            mkdir(LOG_DIR, 0755);
        #endif
        printf("Created log directory: %s\n", LOG_DIR);
    }
}

/**
 * @brief 初始化日志系统
 */
static int init_logging(void)
{
    /* 分配日志上下文 */
    log_ctx = (log_context_t *)calloc(1, sizeof(log_context_t));
    if (log_ctx == NULL) {
        fprintf(stderr, "Failed to allocate log context\n");
        return -1;
    }
    
    /* 创建日志文件名 */
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    
    snprintf(log_ctx->path, sizeof(log_ctx->path),
             "%s/audio_processor_%04d%02d%02d_%02d%02d%02d.log",
             LOG_DIR,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    log_ctx->file = fopen(log_ctx->path, "w");
    if (log_ctx->file == NULL) {
        fprintf(stderr, "Failed to create log file: %s (errno: %d)\n", 
                log_ctx->path, errno);
        free(log_ctx);
        log_ctx = NULL;
        return -1;
    }
    
    /* 设置缓冲 */
    setvbuf(log_ctx->file, log_ctx->buffer, _IOFBF, LOG_BUFFER_SIZE);
    
    log_ctx->start_time = time(NULL);
    log_ctx->initialized = 1;
    log_ctx->flush_counter = 0;
    
    fprintf(log_ctx->file, 
            "Audio File Processor Log File\n"
            "Log file: %s\n"
            "Started at: %s"
            "========================================\n\n",
            log_ctx->path, ctime(&now));
    fflush(log_ctx->file);
    
    printf("Log file: %s\n", log_ctx->path);
    return 0;
}

/**
 * @brief 关闭日志系统
 */
static void close_logging(void)
{
    if (!log_ctx) return;
    
    if (log_ctx->file) {
        time_t now = time(NULL);
        
        fprintf(log_ctx->file, 
                "\n========================================\n"
                "Application terminated at: %s"
                "Total log entries: %d\n"
                "Runtime: %ld seconds\n"
                "========================================\n",
                ctime(&now),
                log_ctx->log_count,
                now - log_ctx->start_time);
        
        fflush(log_ctx->file);
        fclose(log_ctx->file);
        log_ctx->file = NULL;
    }
    
    log_ctx->initialized = 0;
    free(log_ctx);
    log_ctx = NULL;
}

/**
 * @brief 刷新日志缓冲区
 */
static void flush_log(void)
{
    if (log_ctx && log_ctx->file) {
        fflush(log_ctx->file);
    }
}

/**
 * @brief 日志消息处理
 */
static void log_message(int level, const char *file, int line, 
                       const char *func, const char *fmt, ...)
{
    if (!log_ctx || !log_ctx->initialized || !log_ctx->file) {
        return;
    }
    
    /* 获取当前时间 */
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    const char *level_str[] = {
        [LOG_LEVEL_DEBUG] = "DBG",
        [LOG_LEVEL_INFO]  = "INF",
        [LOG_LEVEL_WARN]  = "WRN",
        [LOG_LEVEL_ERROR] = "ERR"
    };
    
    /* 写入日志 */
    fprintf(log_ctx->file, "[%s] [%s] %s:%d (%s): ", 
            time_str, level_str[level], 
            strrchr(file, '/') ? strrchr(file, '/') + 1 : file,
            line, func);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(log_ctx->file, fmt, args);
    va_end(args);
    
    fprintf(log_ctx->file, "\n");
    
    log_ctx->log_count++;
    log_ctx->flush_counter++;
    
    /* 定期刷新 */
    if (log_ctx->flush_counter >= LOG_FLUSH_INTERVAL) {
        fflush(log_ctx->file);
        log_ctx->flush_counter = 0;
    }
    
    /* 错误日志同时输出到stderr */
    if (level >= LOG_LEVEL_WARN) {
        fprintf(stderr, "[%s] %s\n", time_str, level_str[level]);
    }
}

/**
 * @brief 打印崩溃信息（跨平台版本）
 */
static void print_crash_info(int sig)
{
    if (!log_ctx || !log_ctx->file) return;
    
    fprintf(log_ctx->file, "\n");
    fprintf(log_ctx->file, "========================================\n");
    fprintf(log_ctx->file, "CRASH DETECTED!\n");
    fprintf(log_ctx->file, "Signal: %d\n", sig);
    
    switch(sig) {
        case SIGSEGV:
            fprintf(log_ctx->file, "Type: Segmentation Fault\n");
            fprintf(log_ctx->file, "Cause: Invalid memory access\n");
            break;
        case SIGABRT:
            fprintf(log_ctx->file, "Type: Abort\n");
            fprintf(log_ctx->file, "Cause: Assertion failed or abort() called\n");
            break;
        case SIGFPE:
            fprintf(log_ctx->file, "Type: Floating Point Exception\n");
            fprintf(log_ctx->file, "Cause: Division by zero or invalid operation\n");
            break;
        case SIGILL:
            fprintf(log_ctx->file, "Type: Illegal Instruction\n");
            break;
        case SIGINT:
            fprintf(log_ctx->file, "Type: Interrupt\n");
            fprintf(log_ctx->file, "Cause: User pressed Ctrl+C\n");
            break;
        case SIGTERM:
            fprintf(log_ctx->file, "Type: Terminate\n");
            break;
        default:
            fprintf(log_ctx->file, "Type: Unknown\n");
            break;
    }
    
    /* 输出一些调试信息 */
    fprintf(log_ctx->file, "\nSystem Information:\n");
    fprintf(log_ctx->file, "  Time: %s", ctime(&(time_t){time(NULL)}));
    
    if (app_ctx) {
        fprintf(log_ctx->file, "  Current app: %d\n", app_ctx->current_app);
        fprintf(log_ctx->file, "  File count: %d\n", app_ctx->file_count);
        fprintf(log_ctx->file, "  Timer running: %d\n", app_ctx->timer_running);
        fprintf(log_ctx->file, "  Current path: %s\n", app_ctx->current_path);
    }
    
    #if HAVE_EXECINFO
    /* 如果系统支持execinfo，打印堆栈 */
    void *buffer[100];
    int size = backtrace(buffer, 100);
    char **symbols = backtrace_symbols(buffer, size);
    
    fprintf(log_ctx->file, "\nStack trace (%d frames):\n", size);
    for (int i = 0; i < size; i++) {
        fprintf(log_ctx->file, "  #%d: %s\n", i, symbols[i]);
    }
    free(symbols);
    #else
    fprintf(log_ctx->file, "\nStack trace not available on this platform\n");
    #endif
    
    fprintf(log_ctx->file, "========================================\n");
    fflush(log_ctx->file);
}

/**
 * @brief 崩溃信号处理器
 */
static void crash_handler(int sig)
{
    /* 恢复默认信号处理 */
    signal(sig, SIG_DFL);
    
    /* 打印崩溃信息 */
    print_crash_info(sig);
    
    /* 关闭日志 */
    close_logging();
    
    /* 重新抛出信号 */
    raise(sig);
}

/**
 * @brief 安装崩溃处理器
 */
static void install_crash_handlers(void)
{
    if (crash_handlers_installed) return;
    
    /* 只安装关键信号的处理器 */
    signal(SIGSEGV, crash_handler);  // 段错误
    signal(SIGABRT, crash_handler);  // 中止
    signal(SIGFPE, crash_handler);   // 浮点异常
    signal(SIGILL, crash_handler);   // 非法指令
    signal(SIGINT, crash_handler);   // 中断 (Ctrl+C)
    signal(SIGTERM, crash_handler);  // 终止
    
    crash_handlers_installed = 1;
    LOG_DEBUG("Crash handlers installed");
}

/**
 * @brief 检查初始化状态
 */
static int check_initialization(void)
{
    if (app_ctx == NULL) {
        if (log_ctx && log_ctx->initialized) {
            LOG_ERROR("Application context is NULL");
        }
        return -1;
    }
    
    if (!app_ctx->initialized) {
        LOG_ERROR("Application not properly initialized");
        return -1;
    }
    
    return 0;
}

/**********************
 *      硬件抽象层
 **********************/
static void hal_init(void)
{
    LOG_DEBUG("Initializing SDL...");
    sdl_init();
    LOG_DEBUG("SDL initialized");

    /* 显示缓冲区 */
    LOG_DEBUG("Initializing display buffer...");
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[SDL_HOR_RES * 100];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, SDL_HOR_RES * 100);
    LOG_DEBUG("Display buffer initialized (size: %d)", SDL_HOR_RES * 100);

    /* 显示设备 */
    LOG_DEBUG("Registering display device...");
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL) {
        LOG_ERROR("Failed to register display device");
        return;
    }
    LOG_DEBUG("Display device registered (resolution: %dx%d)", 
              SDL_HOR_RES, SDL_VER_RES);

    /* 主题设置 */
    LOG_DEBUG("Setting up theme...");
    lv_theme_t *th = lv_theme_default_init(disp, 
        lv_palette_main(LV_PALETTE_BLUE), 
        lv_palette_main(LV_PALETTE_RED), 
        LV_THEME_DEFAULT_DARK, 
        LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);
    LOG_DEBUG("Theme initialized");

    /* 输入设备组 */
    LOG_DEBUG("Creating input device group...");
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    /* 鼠标输入 */
    LOG_DEBUG("Registering mouse input...");
    static lv_indev_drv_t mouse_drv;
    lv_indev_drv_init(&mouse_drv);
    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = sdl_mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&mouse_drv);
    if (mouse_indev == NULL) {
        LOG_WARN("Failed to register mouse input");
    }

    /* 键盘输入 */
    LOG_DEBUG("Registering keyboard input...");
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t *kb_indev = lv_indev_drv_register(&kb_drv);
    if (kb_indev != NULL) {
        lv_indev_set_group(kb_indev, g);
    } else {
        LOG_WARN("Failed to register keyboard input");
    }

    /* 编码器输入 */
    LOG_DEBUG("Registering encoder input...");
    static lv_indev_drv_t enc_drv;
    lv_indev_drv_init(&enc_drv);
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = sdl_mousewheel_read;
    lv_indev_t *enc_indev = lv_indev_drv_register(&enc_drv);
    if (enc_indev != NULL) {
        lv_indev_set_group(enc_indev, g);
    } else {
        LOG_WARN("Failed to register encoder input");
    }
    
    LOG_INFO("HAL initialization complete");
}

/**
 * @brief 创建主屏幕
 */
static void create_main_screen(void)
{
    LOG_INFO("Creating main screen");
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot create main screen: not initialized");
        return;
    }
    
    app_ctx->main_screen = lv_obj_create(NULL);
    if (app_ctx->main_screen == NULL) {
        LOG_ERROR("Failed to create main screen object");
        return;
    }
    
    LOG_DEBUG("Loading main screen");
    lv_scr_load(app_ctx->main_screen);
    
    /* 标题 */
    LOG_DEBUG("Creating title label");
    lv_obj_t *title = CREATE_LABEL(app_ctx->main_screen, "Audio File Processor", 
                                   &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, 10);
    if (title == NULL) {
        LOG_WARN("Failed to create title label");
    }

    /* 按钮容器 */
    LOG_DEBUG("Creating button container");
    lv_obj_t *btn_cont = lv_obj_create(app_ctx->main_screen);
    if (btn_cont == NULL) {
        LOG_ERROR("Failed to create button container");
        return;
    }
    
    lv_obj_set_size(btn_cont, LV_PCT(90), LV_PCT(70));
    lv_obj_center(btn_cont);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_cont, 20, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);

    /* 应用按钮定义 */
    struct {
        const char *text;
        const char *symbol;
        app_type_t type;
    } app_btns[] = {
        {"File Manager", LV_SYMBOL_DIRECTORY, APP_FILE_MANAGER},
        {"Audio Player", LV_SYMBOL_PLAY, APP_AUDIO_PLAYER},
        {"Audio Processor", LV_SYMBOL_SETTINGS, APP_AUDIO_PROCESSOR}
    };

    /* 创建应用按钮 */
    for (int i = 0; i < 3; i++) {
        LOG_DEBUG("Creating button %d: %s", i, app_btns[i].text);
        
        lv_obj_t *btn = CREATE_BTN(btn_cont, BUTTON_WIDTH, BUTTON_HEIGHT, 
                                   on_app_click, (void *)(intptr_t)app_btns[i].type);
        
        if (btn == NULL) {
            LOG_ERROR("Failed to create button %d", i);
            continue;
        }
        
        char btn_text[64];
        snprintf(btn_text, sizeof(btn_text), "%s %s", app_btns[i].symbol, app_btns[i].text);
        
        lv_obj_t *label = lv_label_create(btn);
        if (label != NULL) {
            lv_label_set_text(label, btn_text);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_center(label);
        } else {
            LOG_WARN("Failed to create label for button %d", i);
        }
    }
    
    LOG_INFO("Main screen created successfully");
}

/**
 * @brief 创建应用屏幕（通用入口）
 */
static void create_app_screen(app_type_t app_type)
{
    LOG_INFO("Creating application screen: %d", app_type);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot create app screen: not initialized");
        return;
    }
    
    LOG_DEBUG("Cleaning up previous app");
    cleanup_app();
    
    app_ctx->current_app = app_type;
    app_ctx->screen.screen = lv_obj_create(NULL);
    
    if (app_ctx->screen.screen == NULL) {
        LOG_ERROR("Failed to create app screen object");
        return;
    }
    
    const char *titles[] = {
        [APP_FILE_MANAGER] = "File Manager",
        [APP_AUDIO_PLAYER] = "Audio Player",
        [APP_AUDIO_PROCESSOR] = "Audio Processor"
    };
    
    LOG_DEBUG("Setting up header with title: %s", titles[app_type]);
    setup_header(app_ctx->screen.screen, titles[app_type]);
    
    /* 主内容容器 */
    LOG_DEBUG("Creating main content container");
    app_ctx->screen.main_cont = lv_obj_create(app_ctx->screen.screen);
    if (app_ctx->screen.main_cont == NULL) {
        LOG_ERROR("Failed to create main content container");
        return;
    }
    
    lv_obj_set_size(app_ctx->screen.main_cont, LV_PCT(100), LV_PCT(85));
    lv_obj_align(app_ctx->screen.main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(app_ctx->screen.main_cont, 10, 0);
    lv_obj_set_style_border_width(app_ctx->screen.main_cont, 0, 0);
    lv_obj_set_style_bg_opa(app_ctx->screen.main_cont, LV_OPA_TRANSP, 0);

    /* 根据应用类型创建具体界面 */
    LOG_DEBUG("Setting up specific screen for app type %d", app_type);
    switch (app_type) {
        case APP_FILE_MANAGER:
            setup_file_manager_screen();
            break;
        case APP_AUDIO_PLAYER:
            setup_audio_player_screen();
            break;
        case APP_AUDIO_PROCESSOR:
            setup_audio_processor_screen();
            break;
        default:
            LOG_ERROR("Unknown app type: %d", app_type);
            break;
    }

    LOG_DEBUG("Loading app screen");
    lv_scr_load(app_ctx->screen.screen);
    LOG_INFO("Application screen created successfully");
}

/**
 * @brief 创建通用标题栏
 */
static void setup_header(lv_obj_t *screen, const char *title)
{
    LOG_DEBUG("Setting up header: %s", title);
    
    if (!screen || !title) {
        LOG_ERROR("Invalid parameters in setup_header");
        return;
    }
    
    lv_obj_t *header = lv_obj_create(screen);
    if (header == NULL) {
        LOG_ERROR("Failed to create header object");
        return;
    }
    
    lv_obj_set_size(header, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    /* 返回按钮 */
    LOG_DEBUG("Creating back button");
    lv_obj_t *back_btn = CREATE_BTN(header, BACK_BTN_SIZE, HEADER_HEIGHT - 10, 
                                    on_back_click, NULL);
    if (back_btn == NULL) {
        LOG_WARN("Failed to create back button");
    } else {
        lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
        
        lv_obj_t *back_label = lv_label_create(back_btn);
        if (back_label != NULL) {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT);
            lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
            lv_obj_center(back_label);
        }
    }

    /* 标题 */
    LOG_DEBUG("Creating title label");
    lv_obj_t *title_label = CREATE_LABEL(header, title, &lv_font_montserrat_16, 
                                         LV_ALIGN_CENTER, 0, 0);
    if (title_label == NULL) {
        LOG_WARN("Failed to create title label");
    }

    /* 保存到上下文 */
    app_ctx->screen.header = header;
    app_ctx->screen.back_btn = back_btn;
    
    LOG_DEBUG("Header setup complete");
}

/**
 * @brief 设置文件管理器界面
 */
static void setup_file_manager_screen(void)
{
    LOG_INFO("Setting up file manager screen");
    
    if (!app_ctx->screen.main_cont) {
        LOG_ERROR("main_cont is NULL in setup_file_manager_screen");
        return;
    }
    
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);
    
    /* 路径显示 */
    LOG_DEBUG("Creating path label: %s", app_ctx->current_path);
    lv_obj_t *path_label = CREATE_LABEL(app_ctx->screen.main_cont, app_ctx->current_path, 
                                        &lv_font_montserrat_12, LV_ALIGN_TOP_LEFT, 0, 0);
    if (path_label == NULL) {
        LOG_WARN("Failed to create path label");
    }

    /* 文件列表 */
    LOG_DEBUG("Creating file list");
    app_ctx->screen.list = lv_list_create(app_ctx->screen.main_cont);
    if (app_ctx->screen.list == NULL) {
        LOG_ERROR("Failed to create file list");
        return;
    }
    
    lv_obj_set_size(app_ctx->screen.list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->screen.list, 0, 0);

    /* 启动定时器 */
    LOG_DEBUG("Starting file manager timer");
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(file_manager_timer_cb, 100, app_ctx);
    
    if (app_ctx->app_timer == NULL) {
        LOG_WARN("Failed to create file manager timer");
    }
    
    LOG_DEBUG("Loading directory: %s", app_ctx->current_path);
    load_directory(app_ctx->current_path, app_ctx->screen.list);
    
    LOG_INFO("File manager screen setup complete");
}

/**
 * @brief 设置音频播放器界面
 */
static void setup_audio_player_screen(void)
{
    LOG_INFO("Setting up audio player screen");
    
    if (!app_ctx->screen.main_cont) {
        LOG_ERROR("main_cont is NULL in setup_audio_player_screen");
        return;
    }
    
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);

    /* 播放列表标签 */
    LOG_DEBUG("Creating playlist label");
    lv_obj_t *playlist_label = CREATE_LABEL(app_ctx->screen.main_cont, "Playlist:", 
                                            &lv_font_montserrat_14, LV_ALIGN_TOP_LEFT, 0, 0);
    if (playlist_label == NULL) {
        LOG_WARN("Failed to create playlist label");
    }

    /* 播放列表 */
    LOG_DEBUG("Creating playlist list");
    app_ctx->screen.list = lv_list_create(app_ctx->screen.main_cont);
    if (app_ctx->screen.list == NULL) {
        LOG_ERROR("Failed to create playlist list");
        return;
    }
    
    lv_obj_set_size(app_ctx->screen.list, LV_PCT(100), 180);
    lv_obj_set_style_border_width(app_ctx->screen.list, 1, 0);
    lv_obj_set_style_border_color(app_ctx->screen.list, 
                                  lv_palette_main(LV_PALETTE_GREY), 0);

    /* 当前播放信息 */
    LOG_DEBUG("Creating now playing label");
    app_ctx->now_playing_label = CREATE_LABEL(app_ctx->screen.main_cont, "Not playing",
                                              &lv_font_montserrat_14, LV_ALIGN_TOP_MID, 0, 0);
    if (app_ctx->now_playing_label == NULL) {
        LOG_WARN("Failed to create now playing label");
    } else {
        lv_obj_set_width(app_ctx->now_playing_label, LV_PCT(100));
        lv_obj_set_style_text_align(app_ctx->now_playing_label, LV_TEXT_ALIGN_CENTER, 0);
    }

    /* 控制区容器 */
    LOG_DEBUG("Creating control container");
    lv_obj_t *ctrl_cont = lv_obj_create(app_ctx->screen.main_cont);
    if (ctrl_cont == NULL) {
        LOG_ERROR("Failed to create control container");
        return;
    }
    
    lv_obj_set_size(ctrl_cont, LV_PCT(100), 120);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_bg_opa(ctrl_cont, LV_OPA_TRANSP, 0);

    /* 进度条区域 */
    LOG_DEBUG("Creating progress container");
    lv_obj_t *progress_cont = lv_obj_create(ctrl_cont);
    if (progress_cont == NULL) {
        LOG_WARN("Failed to create progress container");
    } else {
        lv_obj_set_size(progress_cont, LV_PCT(100), 40);
        lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_border_width(progress_cont, 0, 0);
        lv_obj_set_style_bg_opa(progress_cont, LV_OPA_TRANSP, 0);

        app_ctx->progress_bar = lv_bar_create(progress_cont);
        if (app_ctx->progress_bar == NULL) {
            LOG_WARN("Failed to create progress bar");
        } else {
            lv_obj_set_size(app_ctx->progress_bar, LV_PCT(80), 10);
            lv_bar_set_range(app_ctx->progress_bar, 0, 100);
        }
        
        app_ctx->time_label = CREATE_LABEL(progress_cont, "00:00/00:00",
                                           &lv_font_montserrat_12, LV_ALIGN_TOP_LEFT, 0, 0);
        if (app_ctx->time_label != NULL) {
            lv_obj_set_width(app_ctx->time_label, LV_PCT(18));
        }
    }

    /* 按钮区域 */
    LOG_DEBUG("Creating button container");
    lv_obj_t *btn_cont = lv_obj_create(ctrl_cont);
    if (btn_cont == NULL) {
        LOG_WARN("Failed to create button container");
    } else {
        lv_obj_set_size(btn_cont, LV_PCT(100), 60);
        lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, 
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_border_width(btn_cont, 0, 0);
        lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);

        /* 播放/暂停按钮 */
        app_ctx->play_btn = CREATE_BTN(btn_cont, 80, 50, on_play_click, NULL);
        if (app_ctx->play_btn != NULL) {
            lv_obj_t *play_label = lv_label_create(app_ctx->play_btn);
            if (play_label != NULL) {
                lv_label_set_text(play_label, LV_SYMBOL_PLAY);
                lv_obj_center(play_label);
            }
        }

        /* 停止按钮 */
        lv_obj_t *stop_btn = CREATE_BTN(btn_cont, 80, 50, on_stop_click, NULL);
        if (stop_btn != NULL) {
            lv_obj_t *stop_label = lv_label_create(stop_btn);
            if (stop_label != NULL) {
                lv_label_set_text(stop_label, LV_SYMBOL_STOP);
                lv_obj_center(stop_label);
            }
        }
    }

    /* 启动定时器 */
    LOG_DEBUG("Starting audio player timer");
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_player_timer_cb, 100, app_ctx);
    
    if (app_ctx->app_timer == NULL) {
        LOG_WARN("Failed to create audio player timer");
    }
    
    LOG_DEBUG("Loading audio files");
    load_audio_files("./", app_ctx->screen.list);
    
    LOG_INFO("Audio player screen setup complete");
}

/**
 * @brief 设置音频处理器界面
 */
static void setup_audio_processor_screen(void)
{
    LOG_INFO("Setting up audio processor screen");
    
    if (!app_ctx->screen.main_cont) {
        LOG_ERROR("main_cont is NULL in setup_audio_processor_screen");
        return;
    }
    
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_ROW);

    /* 效果器列表 */
    LOG_DEBUG("Creating effect list");
    lv_obj_t *effect_list = lv_obj_create(app_ctx->screen.main_cont);
    if (effect_list == NULL) {
        LOG_ERROR("Failed to create effect list");
        return;
    }
    
    lv_obj_set_size(effect_list, LV_PCT(30), LV_PCT(100));
    lv_obj_set_flex_flow(effect_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(effect_list, 0, 0);
    lv_obj_set_style_bg_opa(effect_list, LV_OPA_10, 0);

    lv_obj_t *effect_title = CREATE_LABEL(effect_list, "Effects", &lv_font_montserrat_14, 
                                          LV_ALIGN_TOP_LEFT, 0, 0);
    if (effect_title == NULL) {
        LOG_WARN("Failed to create effect title");
    }

    /* 初始化效果器 */
    app_ctx->effect_count = sizeof(effect_presets) / sizeof(effect_presets[0]);
    LOG_DEBUG("Initializing %d effects", app_ctx->effect_count);
    
    for (int i = 0; i < app_ctx->effect_count; i++) {
        effect_t *effect = &app_ctx->effects[i];
        effect->type = effect_presets[i].type;
        effect->enabled = 0;
        effect->param1 = effect_presets[i].default_param1;
        effect->param2 = effect_presets[i].default_param2;
        effect->param3 = effect_presets[i].default_param3;
        strcpy(effect->name, effect_presets[i].name);

        LOG_DEBUG("Creating effect button %d: %s", i, effect->name);

        /* 效果器按钮 */
        lv_obj_t *btn = CREATE_BTN(effect_list, LV_PCT(100), 40, 
                                   on_effect_click, (void *)(intptr_t)i);
        
        if (btn == NULL) {
            LOG_WARN("Failed to create effect button %d", i);
            continue;
        }
        
        lv_obj_t *label = lv_label_create(btn);
        if (label != NULL) {
            lv_label_set_text_fmt(label, "%s %s", effect->name, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
            lv_obj_center(label);
        }
        
        effect->btn = btn;
    }

    /* 参数调节区 */
    LOG_DEBUG("Creating parameter container");
    app_ctx->effect_cont = lv_obj_create(app_ctx->screen.main_cont);
    if (app_ctx->effect_cont == NULL) {
        LOG_WARN("Failed to create effect container");
    } else {
        lv_obj_set_size(app_ctx->effect_cont, LV_PCT(70), LV_PCT(100));
        lv_obj_set_flex_flow(app_ctx->effect_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_border_width(app_ctx->effect_cont, 0, 0);
        lv_obj_set_style_bg_opa(app_ctx->effect_cont, LV_OPA_10, 0);

        lv_obj_t *param_title = CREATE_LABEL(app_ctx->effect_cont, "Parameters", 
                                             &lv_font_montserrat_14, LV_ALIGN_TOP_LEFT, 0, 0);
        if (param_title == NULL) {
            LOG_WARN("Failed to create parameter title");
        }

        /* 创建参数滑块 */
        LOG_DEBUG("Creating parameter sliders");
        for (int i = 0; i < 3; i++) {
            lv_obj_t *slider_cont = lv_obj_create(app_ctx->effect_cont);
            if (slider_cont == NULL) {
                LOG_WARN("Failed to create slider container %d", i);
                continue;
            }
            
            lv_obj_set_size(slider_cont, LV_PCT(100), 60);
            lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_border_width(slider_cont, 0, 0);
            lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(slider_cont, 5, 0);

            lv_obj_t *label = CREATE_LABEL(slider_cont, effect_presets[0].param_names[i], 
                                           &lv_font_montserrat_12, LV_ALIGN_LEFT_MID, 0, 0);
            if (label == NULL) {
                LOG_WARN("Failed to create slider label %d", i);
            }

            lv_obj_t *slider = lv_slider_create(slider_cont);
            if (slider != NULL) {
                lv_obj_set_size(slider, 150, 10);
                lv_slider_set_range(slider, 0, 100);
                lv_slider_set_value(slider, 50, LV_ANIM_OFF);
                lv_obj_add_event_cb(slider, on_slider_change, LV_EVENT_VALUE_CHANGED, 
                                   (void *)(intptr_t)i);

                lv_obj_t *value_label = CREATE_LABEL(slider_cont, "50", 
                                                     &lv_font_montserrat_12, 
                                                     LV_ALIGN_LEFT_MID, 0, 0);
                if (value_label != NULL) {
                    lv_obj_set_user_data(slider, value_label);
                }
            }
        }
    }

    /* 启动定时器 */
    LOG_DEBUG("Starting audio processor timer");
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_processor_timer_cb, 10, app_ctx);
    
    if (app_ctx->app_timer == NULL) {
        LOG_WARN("Failed to create audio processor timer");
    }
    
    LOG_INFO("Audio processor screen setup complete");
}

/**
 * @brief 定时器回调函数
 */
static void file_manager_timer_cb(lv_timer_t *timer)
{
    static int counter = 0;
    counter++;
    
    /* 每100次输出一次（降低频率） */
    if (counter % 100 == 0) {
        LOG_DEBUG("File manager timer running...");
    }
}

static void audio_player_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx) {
        LOG_ERROR("audio_player_timer_cb: ctx is NULL");
        return;
    }
    
    if (!ctx->timer_running) {
        return;
    }
    
    if (!ctx->is_playing) return;

    static int progress = 0;
    progress = (progress + 1) % 101;
    
    if (ctx->progress_bar) {
        lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
    }

    int total = 180;
    int current = (progress * total) / 100;
    
    if (ctx->time_label) {
        lv_label_set_text_fmt(ctx->time_label, "%02d:%02d/03:00", 
                              current / 60, current % 60);
    }
}

static void audio_processor_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    if (!ctx) {
        LOG_ERROR("audio_processor_timer_cb: ctx is NULL");
        return;
    }
    
    if (!ctx->timer_running) return;

    static int counter = 0;
    counter++;
    
    /* 每100次输出一次效果处理日志 */
    if (counter % 100 == 0) {
        for (int i = 0; i < ctx->effect_count; i++) {
            if (ctx->effects[i].enabled) {
                LOG_DEBUG("Processing effect %d: %s", i, ctx->effects[i].name);
                break;
            }
        }
    }
}

/**
 * @brief 加载目录内容（文件管理器专用）
 */
static void load_directory(const char *path, lv_obj_t *list)
{
    LOG_INFO("Loading directory: %s", path);
    
    if (!path || !list) {
        LOG_ERROR("Invalid parameters in load_directory");
        return;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        LOG_ERROR("Cannot open directory: %s (errno: %d)", path, errno);
        show_notification("Cannot open directory", lv_palette_main(LV_PALETTE_RED));
        return;
    }

    LOG_DEBUG("Cleaning list");
    lv_obj_clean(list);
    app_ctx->file_count = 0;

    /* 添加上级目录 */
    if (strcmp(path, "./") != 0 && strcmp(path, "/") != 0) {
        LOG_DEBUG("Adding parent directory entry");
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_DIRECTORY, "..");
        if (btn != NULL) {
            lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
        }
    }

    struct dirent *entry;
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (stat(full_path, &st) == 0) {
            file_info_t *file = &app_ctx->files[app_ctx->file_count];
            strcpy(file->name, entry->d_name);
            strcpy(file->path, full_path);
            file->is_dir = S_ISDIR(st.st_mode);
            file->size = st.st_size;

            const char *icon = file->is_dir ? LV_SYMBOL_DIRECTORY : 
                              (strstr(entry->d_name, ".mp3") ? LV_SYMBOL_AUDIO : LV_SYMBOL_FILE);

            LOG_DEBUG("Adding file %d: %s (%s)", app_ctx->file_count, 
                      entry->d_name, file->is_dir ? "dir" : "file");

            lv_obj_t *btn = lv_list_add_btn(list, icon, entry->d_name);
            if (btn != NULL) {
                lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, 
                                   (void *)(intptr_t)app_ctx->file_count);
            }
            
            app_ctx->file_count++;
        } else {
            LOG_WARN("Failed to stat file: %s (errno: %d)", full_path, errno);
        }
    }

    closedir(dir);

    if (app_ctx->file_count == 0) {
        LOG_DEBUG("Directory is empty");
        lv_obj_t *label = lv_label_create(list);
        if (label != NULL) {
            lv_label_set_text(label, "Folder is empty");
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(label, LV_PCT(100));
        }
    }

    LOG_INFO("Directory loaded: %d items", app_ctx->file_count);
    show_notification("Directory loaded", lv_palette_main(LV_PALETTE_GREEN));
}

/**
 * @brief 加载音频文件（播放器专用）
 */
static void load_audio_files(const char *path, lv_obj_t *list)
{
    LOG_INFO("Loading audio files from: %s", path);
    
    if (!path || !list) {
        LOG_ERROR("Invalid parameters in load_audio_files");
        return;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        LOG_ERROR("Cannot open directory: %s (errno: %d)", path, errno);
        show_notification("Cannot open directory", lv_palette_main(LV_PALETTE_RED));
        return;
    }

    LOG_DEBUG("Cleaning list");
    lv_obj_clean(list);
    app_ctx->file_count = 0;

    struct dirent *entry;
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (stat(full_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".mp3") == 0 || 
                        strcasecmp(ext, ".wav") == 0 || 
                        strcasecmp(ext, ".flac") == 0)) {
                
                file_info_t *file = &app_ctx->files[app_ctx->file_count];
                strcpy(file->name, entry->d_name);
                strcpy(file->path, full_path);
                file->size = st.st_size;

                LOG_DEBUG("Found audio file %d: %s", app_ctx->file_count, entry->d_name);

                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, entry->d_name);
                if (btn != NULL) {
                    lv_obj_add_event_cb(btn, on_audio_file_click, LV_EVENT_CLICKED, 
                                       (void *)(intptr_t)app_ctx->file_count);
                }
                
                app_ctx->file_count++;
            }
        }
    }

    closedir(dir);

    if (app_ctx->file_count == 0) {
        LOG_DEBUG("No audio files found");
        lv_obj_t *label = lv_label_create(list);
        if (label != NULL) {
            lv_label_set_text(label, "No audio files found");
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(label, LV_PCT(100));
        }
    }

    LOG_INFO("Audio files loaded: %d items", app_ctx->file_count);
}

/**
 * @brief 事件处理函数
 */
static void on_app_click(lv_event_t *e)
{
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    LOG_INFO("App button clicked: %d", app_type);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle app click: not initialized");
        return;
    }
    
    create_app_screen(app_type);
}

static void on_back_click(lv_event_t *e)
{
    LOG_INFO("Back button clicked");
    (void)e;
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle back click: not initialized");
        return;
    }
    
    LOG_DEBUG("Cleaning up current app");
    cleanup_app();
    
    if (app_ctx->main_screen) {
        LOG_DEBUG("Loading main screen");
        lv_scr_load(app_ctx->main_screen);
    } else {
        LOG_ERROR("Main screen is NULL");
    }
}

static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    LOG_INFO("File clicked: index %d", file_index);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle file click: not initialized");
        return;
    }
    
    if (file_index == -1) {
        LOG_DEBUG("Navigating to parent directory");
        char *last_slash = strrchr(app_ctx->current_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (strlen(app_ctx->current_path) == 0) {
                strcpy(app_ctx->current_path, "./");
            }
            LOG_DEBUG("New path: %s", app_ctx->current_path);
        }
        load_directory(app_ctx->current_path, app_ctx->screen.list);
        return;
    }

    if (file_index < 0 || file_index >= app_ctx->file_count) {
        LOG_ERROR("Invalid file index: %d (max: %d)", file_index, app_ctx->file_count - 1);
        return;
    }

    file_info_t *file = &app_ctx->files[file_index];
    LOG_DEBUG("Selected file: %s (dir: %d)", file->name, file->is_dir);
    
    if (file->is_dir) {
        LOG_DEBUG("Entering directory: %s", file->name);
        strcpy(app_ctx->current_path, file->path);
        load_directory(app_ctx->current_path, app_ctx->screen.list);
    } else {
        LOG_DEBUG("Showing delete confirmation for: %s", file->name);
        static const char *btns[] = {"Confirm", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", 
                                          file->name, btns, true);
        if (mbox != NULL) {
            lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, 
                               (void *)(intptr_t)file_index);
            lv_obj_center(mbox);
        } else {
            LOG_ERROR("Failed to create message box");
        }
    }
}

static void on_audio_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    LOG_INFO("Audio file clicked: index %d", file_index);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle audio file click: not initialized");
        return;
    }
    
    if (file_index < 0 || file_index >= app_ctx->file_count) {
        LOG_ERROR("Invalid file index: %d (max: %d)", file_index, app_ctx->file_count - 1);
        return;
    }

    file_info_t *file = &app_ctx->files[file_index];
    LOG_DEBUG("Selected audio file: %s", file->name);

    char now_playing[128];
    snprintf(now_playing, sizeof(now_playing), "Playing: %s", file->name);
    
    if (app_ctx->now_playing_label) {
        lv_label_set_text(app_ctx->now_playing_label, now_playing);
    }
    
    LOG_INFO("%s", now_playing);
    show_notification(now_playing, lv_palette_main(LV_PALETTE_GREEN));

    app_ctx->current_track = file_index;
    app_ctx->is_playing = 1;

    if (app_ctx->play_btn) {
        lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
        if (play_label) {
            lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
        }
    }
    
    if (app_ctx->progress_bar) {
        lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    }
}

static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    LOG_INFO("Delete confirmation for file index %d", file_index);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle delete confirm: not initialized");
        return;
    }

    if (lv_msgbox_get_active_btn(mbox) == 0) {
        LOG_DEBUG("Delete confirmed, removing file: %s", app_ctx->files[file_index].path);
        
        if (remove(app_ctx->files[file_index].path) == 0) {
            LOG_INFO("File deleted successfully");
            load_directory(app_ctx->current_path, app_ctx->screen.list);
            show_notification("File deleted", lv_palette_main(LV_PALETTE_GREEN));
        } else {
            LOG_ERROR("Failed to delete file (errno: %d)", errno);
            show_notification("Delete failed", lv_palette_main(LV_PALETTE_RED));
        }
    } else {
        LOG_DEBUG("Delete cancelled");
    }

    lv_msgbox_close(mbox);
}

static void on_effect_click(lv_event_t *e)
{
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    LOG_INFO("Effect clicked: index %d", effect_index);
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle effect click: not initialized");
        return;
    }
    
    if (effect_index < 0 || effect_index >= app_ctx->effect_count) {
        LOG_ERROR("Invalid effect index: %d", effect_index);
        return;
    }
    
    effect_t *effect = &app_ctx->effects[effect_index];
    
    effect->enabled = !effect->enabled;
    LOG_DEBUG("Effect %s %s", effect->name, effect->enabled ? "enabled" : "disabled");

    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn) {
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        if (label) {
            lv_label_set_text_fmt(label, "%s %s", effect->name,
                                  effect->enabled ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
        }
    }

    show_notification(effect->enabled ? "Effect enabled" : "Effect disabled",
                     lv_palette_main(LV_PALETTE_BLUE));
}

static void on_play_click(lv_event_t *e)
{
    LOG_INFO("Play button clicked");
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle play click: not initialized");
        return;
    }
    
    if (app_ctx->current_track < 0 || app_ctx->current_track >= app_ctx->file_count) {
        LOG_WARN("No track selected");
        show_notification("No track selected", lv_palette_main(LV_PALETTE_RED));
        return;
    }

    app_ctx->is_playing = !app_ctx->is_playing;
    LOG_DEBUG("Play state changed to: %d", app_ctx->is_playing);

    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn) {
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        if (label) {
            lv_label_set_text(label, app_ctx->is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        }
    }

    show_notification(app_ctx->is_playing ? "Playing" : "Paused",
                     lv_palette_main(LV_PALETTE_BLUE));
}

static void on_stop_click(lv_event_t *e)
{
    LOG_INFO("Stop button clicked");
    (void)e;
    
    if (check_initialization() < 0) {
        LOG_ERROR("Cannot handle stop click: not initialized");
        return;
    }
    
    app_ctx->is_playing = 0;
    app_ctx->current_track = -1;

    if (app_ctx->progress_bar) {
        lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_ON);
    }
    
    if (app_ctx->time_label) {
        lv_label_set_text(app_ctx->time_label, "00:00/00:00");
    }
    
    if (app_ctx->now_playing_label) {
        lv_label_set_text(app_ctx->now_playing_label, "Not playing");
    }

    if (app_ctx->play_btn) {
        lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
        if (play_label) {
            lv_label_set_text(play_label, LV_SYMBOL_PLAY);
        }
    }

    LOG_DEBUG("Playback stopped");
    show_notification("Stopped", lv_palette_main(LV_PALETTE_BLUE));
}

static void on_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
    
    if (slider && value_label) {
        int32_t value = lv_slider_get_value(slider);
        LOG_DEBUG("Slider changed to: %d", (int)value);
        lv_label_set_text_fmt(value_label, "%d", (int)value);
    }
}

static void show_notification(const char *msg, lv_color_t color)
{
    LOG_DEBUG("Showing notification: %s", msg);
    
    if (!msg) {
        LOG_WARN("msg is NULL in show_notification");
        return;
    }
    
    lv_obj_t *notif = lv_label_create(lv_scr_act());
    if (notif == NULL) {
        LOG_WARN("Failed to create notification");
        return;
    }
    
    lv_label_set_text(notif, msg);
    lv_obj_set_style_text_font(notif, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notif, lv_color_white(), 0);
    lv_obj_set_style_bg_color(notif, color, 0);
    lv_obj_set_style_bg_opa(notif, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(notif, 10, 0);
    lv_obj_set_style_radius(notif, 5, 0);
    lv_obj_align(notif, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_t *timer = lv_timer_create(NULL, 2000, notif);
    if (timer != NULL) {
        lv_timer_set_repeat_count(timer, 1);
    }
}

static void cleanup_app(void)
{
    LOG_INFO("Cleaning up application");
    
    if (app_ctx) {
        if (app_ctx->app_timer) {
            LOG_DEBUG("Deleting app timer");
            app_ctx->timer_running = 0;
            lv_timer_del(app_ctx->app_timer);
            app_ctx->app_timer = NULL;
        }

        if (app_ctx->screen.screen) {
            LOG_DEBUG("Deleting app screen");
            lv_obj_del_async(app_ctx->screen.screen);
            app_ctx->screen.screen = NULL;
        }

        app_ctx->is_playing = 0;
        app_ctx->current_app = APP_NONE;
        
        LOG_DEBUG("Cleanup complete");
    }
}