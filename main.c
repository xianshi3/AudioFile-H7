/**
 * @file main.c
 * @brief 音频文件处理器 - 完整功能版本
 * @details 包含文件管理、音频播放、实时音频处理三大功能
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
#include <pthread.h>
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lv_drivers/sdl/sdl.h"

/*********************
 *      宏定义
 *********************/
#define MAX_PATH 256
#define MAX_FILES 128
#define MAX_EFFECTS 6
#define SCREEN_WIDTH 460
#define SCREEN_HEIGHT 460
#define TIMER_STACK_SIZE 16384    /* 16KB任务栈 */
#define AUDIO_BUFFER_SIZE 2048     /* 音频缓冲区大小 */

/* 颜色定义 */
#define COLOR_BG lv_color_hex(0xF5F5F5)
#define COLOR_CARD lv_color_hex(0xFFFFFF)
#define COLOR_PRIMARY lv_color_hex(0x2196F3)
#define COLOR_SECONDARY lv_color_hex(0x4CAF50)
#define COLOR_WARNING lv_color_hex(0xFF9800)
#define COLOR_DANGER lv_color_hex(0xF44336)
#define COLOR_TEXT_PRIMARY lv_color_hex(0x212121)
lv_color_t COLOR_TEXT_SECONDARY;

/**********************
 *      类型定义
 **********************/
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
    EFFECT_LOW_PASS,
    EFFECT_HIGH_PASS,
    EFFECT_BAND_PASS,
    EFFECT_DELAY,
    EFFECT_CHORUS,
    EFFECT_FLANGER
} effect_type_t;

/* 文件信息结构体 */
typedef struct {
    char name[64];
    char path[MAX_PATH];
    char display_name[64];
    int is_dir;
    int size;
    int is_audio;               /* 是否为音频文件 */
} file_info_t;

/* 效果器参数结构体 */
typedef struct {
    effect_type_t type;
    int enabled;
    int order;                  /* 处理顺序 1-6 */
    float params[5];            /* 效果器参数 */
    char name[20];
    lv_color_t color;
    lv_obj_t *card;
} effect_t;

/* 音频缓冲区结构体 */
typedef struct {
    int16_t buffer1[AUDIO_BUFFER_SIZE];
    int16_t buffer2[AUDIO_BUFFER_SIZE];
    volatile int active_buffer;
    volatile int buffer_ready;
    pthread_mutex_t mutex;
} audio_buffer_t;

/* 应用上下文结构体 */
typedef struct {
    /* 屏幕对象 */
    app_type_t current_app;
    lv_obj_t *main_screen;
    lv_obj_t *app_screen;
    lv_obj_t *nav_bar;
    lv_obj_t *content_area;
    lv_obj_t *title_label;
    
    /* 文件管理器相关 */
    file_info_t files[MAX_FILES];
    int file_count;
    char current_path[MAX_PATH];
    lv_obj_t *file_list;
    lv_obj_t *path_label;
    lv_obj_t *file_info_label;
    pthread_t file_manager_thread;
    int file_manager_running;
    
    /* 音频播放器相关 */
    int is_playing;
    int current_track;
    int track_count;
    lv_obj_t *play_btn;
    lv_obj_t *play_icon;
    lv_obj_t *progress_bar;
    lv_obj_t *time_label;
    lv_obj_t *track_list;
    lv_obj_t *track_info;
    pthread_t audio_player_thread;
    int audio_player_running;
    audio_buffer_t audio_buffer;
    
    /* 音频处理器相关 */
    effect_t effects[MAX_EFFECTS];
    int effect_count;
    lv_obj_t *effects_grid;
    lv_obj_t *effect_config_screen;
    int selected_effect;
    pthread_t audio_processor_thread;
    int processor_running;
    audio_buffer_t proc_buffer_in;
    audio_buffer_t proc_buffer_out;
    
    /* 定时器 */
    lv_timer_t *ui_timer;
    int timer_running;
} app_context_t;

/**********************
 *      静态变量
 **********************/
static app_context_t *app_ctx = NULL;
static lv_style_t style_card;
static lv_style_t style_btn;

/* 效果器名称映射 */
static const char* effect_names[] = {
    "None", "Reverb", "Echo", "Distortion", "Low Pass", 
    "High Pass", "Band Pass", "Delay", "Chorus", "Flanger"
};

/* 效果器图标映射 */
static const char* effect_icons[] = {
    LV_SYMBOL_CLOSE, LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_REFRESH, LV_SYMBOL_EDIT,
    LV_SYMBOL_DOWN, LV_SYMBOL_UP, LV_SYMBOL_SETTINGS,
    LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_SHUFFLE
};

/* 效果器颜色映射 */
static lv_color_t effect_colors[10];

/**********************
 *      静态函数声明
 **********************/
/* 初始化函数 */
static void hal_init(void);
static void init_styles(void);
static void create_main_screen(void);

/* 文件管理器函数 */
static void create_file_manager_screen(void);
static void* file_manager_thread_func(void *arg);
static void load_directory(const char *path);
static void update_file_list_display(void);
static void show_delete_dialog(int file_index);
static void on_file_click(lv_event_t *e);
static void on_delete_confirm(lv_event_t *e);
static void on_delete_cancel(lv_event_t *e);
static void on_navigate_up(lv_event_t *e);

/* 音频播放器函数 */
static void create_audio_player_screen(void);
static void* audio_player_thread_func(void *arg);
static void load_audio_files(const char *path);
static void update_playlist_display(void);
static void play_audio_file(int index);
static void stop_audio_playback(void);
static void on_audio_file_click(lv_event_t *e);
static void on_play_click(lv_event_t *e);
static void on_stop_click(lv_event_t *e);
static void on_prev_click(lv_event_t *e);
static void on_next_click(lv_event_t *e);

/* 音频处理器函数 */
static void create_audio_processor_screen(void);
static void* audio_processor_thread_func(void *arg);
static void create_effect_config_screen(int effect_idx);
static void update_effect_card(int idx);
static void process_audio_effects(int16_t *in, int16_t *out, int len);
static void on_effect_card_click(lv_event_t *e);
static void on_effect_toggle(lv_event_t *e);
static void on_effect_type_select(lv_event_t *e);
static void on_effect_param_change(lv_event_t *e);
static void on_effect_order_change(lv_event_t *e);

/* 通用函数 */
static void on_app_click(lv_event_t *e);
static void on_back_click(lv_event_t *e);
static void free_app_resources(void);
static void show_toast(const char *msg, lv_color_t color, uint32_t duration);
static void ui_timer_cb(lv_timer_t *timer);

/**********************
 *      全局函数
 **********************/
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* 初始化全局颜色变量 */
    COLOR_TEXT_SECONDARY = lv_color_hex(0x757575);

    /* 初始化效果器颜色映射 */
    effect_colors[0] = COLOR_TEXT_SECONDARY;
    effect_colors[1] = lv_color_hex(0x9C27B0);
    effect_colors[2] = lv_color_hex(0x00BCD4);
    effect_colors[3] = lv_color_hex(0xFF5722);
    effect_colors[4] = lv_color_hex(0x3F51B5);
    effect_colors[5] = lv_color_hex(0xE91E63);
    effect_colors[6] = lv_color_hex(0x009688);
    effect_colors[7] = lv_color_hex(0xFFC107);
    effect_colors[8] = lv_color_hex(0x8BC34A);
    effect_colors[9] = lv_color_hex(0x673AB7);

    /* 初始化LVGL */
    lv_init();
    hal_init();
    init_styles();

    /* 创建应用上下文 */
    app_ctx = (app_context_t *)calloc(1, sizeof(app_context_t));
    strcpy(app_ctx->current_path, ".");
    app_ctx->effect_count = MAX_EFFECTS;
    
    /* 初始化效果器 */
    effect_type_t default_types[] = {EFFECT_REVERB, EFFECT_ECHO, EFFECT_DISTORTION, 
                                     EFFECT_LOW_PASS, EFFECT_HIGH_PASS, EFFECT_DELAY};
    for (int i = 0; i < MAX_EFFECTS; i++) {
        app_ctx->effects[i].type = default_types[i];
        app_ctx->effects[i].enabled = (i < 2) ? 1 : 0;
        app_ctx->effects[i].order = i + 1;
        app_ctx->effects[i].params[0] = 50;
        app_ctx->effects[i].params[1] = 50;
        app_ctx->effects[i].params[2] = 50;
        app_ctx->effects[i].params[3] = 0;
        app_ctx->effects[i].params[4] = 0;
        strcpy(app_ctx->effects[i].name, effect_names[default_types[i]]);
        app_ctx->effects[i].color = effect_colors[default_types[i]];
    }

    /* 创建主屏幕 */
    create_main_screen();

    /* 创建UI定时器 */
    app_ctx->ui_timer = lv_timer_create(ui_timer_cb, 100, app_ctx);
    app_ctx->timer_running = 1;

    /* 主循环 */
    while(1) {
        lv_timer_handler();
        usleep(5 * 1000);
    }

    return 0;
}

/**********************
 *      初始化函数
 **********************/
static void hal_init(void)
{
    sdl_init();

    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[SDL_HOR_RES * 50];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, SDL_HOR_RES * 50);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    disp_drv.antialiasing = 1;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    lv_theme_t *th = lv_theme_default_init(disp, 
        COLOR_PRIMARY, COLOR_DANGER, 0, &lv_font_montserrat_14);
    lv_disp_set_theme(disp, th);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
}

static void init_styles(void)
{
    /* 卡片样式 */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0xE0E0E0));
    lv_style_set_radius(&style_card, 8);
    lv_style_set_shadow_width(&style_card, 4);
    lv_style_set_shadow_ofs_y(&style_card, 2);
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);
    lv_style_set_pad_all(&style_card, 8);
    
    /* 按钮样式 */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_btn, 4);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_pad_hor(&style_btn, 12);
    lv_style_set_pad_ver(&style_btn, 6);
}

/**********************
 *      主屏幕创建
 **********************/
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->main_screen, COLOR_BG, 0);
    lv_scr_load(app_ctx->main_screen);
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(app_ctx->main_screen);
    lv_label_set_text(title, "Audio File Processor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    /* 按钮容器 */
    lv_obj_t *btn_cont = lv_obj_create(app_ctx->main_screen);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_size(btn_cont, LV_PCT(90), LV_PCT(70));
    lv_obj_center(btn_cont);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_cont, 20, 0);
    
    /* 文件管理器按钮 */
    lv_obj_t *btn1 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn1, &style_btn, 0);
    lv_obj_set_size(btn1, 200, 60);
    lv_obj_add_event_cb(btn1, on_app_click, LV_EVENT_CLICKED, (void *)APP_FILE_MANAGER);
    
    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, LV_SYMBOL_DIRECTORY " File Manager");
    lv_obj_center(label1);
    
    /* 音频播放器按钮 */
    lv_obj_t *btn2 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn2, &style_btn, 0);
    lv_obj_set_size(btn2, 200, 60);
    lv_obj_add_event_cb(btn2, on_app_click, LV_EVENT_CLICKED, (void *)APP_AUDIO_PLAYER);
    
    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, LV_SYMBOL_PLAY " Audio Player");
    lv_obj_center(label2);
    
    /* 音频处理器按钮 */
    lv_obj_t *btn3 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn3, &style_btn, 0);
    lv_obj_set_size(btn3, 200, 60);
    lv_obj_add_event_cb(btn3, on_app_click, LV_EVENT_CLICKED, (void *)APP_AUDIO_PROCESSOR);
    
    lv_obj_t *label3 = lv_label_create(btn3);
    lv_label_set_text(label3, LV_SYMBOL_SETTINGS " Audio Processor");
    lv_obj_center(label3);
}

/**********************
 *      文件管理器
 **********************/
static void create_file_manager_screen(void)
{
    /* 释放之前的资源 */
    free_app_resources();
    
    app_ctx->current_app = APP_FILE_MANAGER;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_scr_load(app_ctx->app_screen);
    
    /* 导航栏 - 固定高度，不使用滚动 */
    app_ctx->nav_bar = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(app_ctx->nav_bar, LV_PCT(100), 50);
    lv_obj_set_pos(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_border_width(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->nav_bar, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(app_ctx->nav_bar, LV_OPA_10, 0);
    lv_obj_clear_flag(app_ctx->nav_bar, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动 */
    
    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(app_ctx->nav_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_set_style_bg_color(back_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    
    /* 标题 */
    app_ctx->title_label = lv_label_create(app_ctx->nav_bar);
    lv_label_set_text(app_ctx->title_label, "File Manager");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);
    
    /* 当前路径显示 - 固定位置 */
    app_ctx->path_label = lv_label_create(app_ctx->app_screen);
    lv_label_set_text_fmt(app_ctx->path_label, "Path: %s", app_ctx->current_path);
    lv_obj_set_style_text_font(app_ctx->path_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(app_ctx->path_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(app_ctx->path_label, 10, 55);
    lv_obj_set_size(app_ctx->path_label, 440, 20);
    lv_obj_set_style_text_line_space(app_ctx->path_label, 0, 0);
    lv_label_set_long_mode(app_ctx->path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);  /* 长路径滚动 */
    
    /* 文件列表容器 - 可滚动区域 */
    lv_obj_t *list_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(list_cont, 440, 350);
    lv_obj_set_pos(list_cont, 10, 80);
    lv_obj_set_style_border_width(list_cont, 1, 0);
    lv_obj_set_style_border_color(list_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_color(list_cont, COLOR_CARD, 0);
    lv_obj_set_style_radius(list_cont, 4, 0);
    lv_obj_set_style_pad_all(list_cont, 5, 0);
    
    /* 文件列表 */
    app_ctx->file_list = lv_list_create(list_cont);
    lv_obj_set_size(app_ctx->file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->file_list, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->file_list, COLOR_CARD, 0);
    
    /* 启动文件管理器线程 */
    app_ctx->file_manager_running = 1;
    pthread_create(&app_ctx->file_manager_thread, NULL, file_manager_thread_func, NULL);
}

static void* file_manager_thread_func(void *arg)
{
    (void)arg;
    
    while (app_ctx->file_manager_running) {
        /* 线程可以在这里执行文件监控等后台任务 */
        usleep(100000);  /* 100ms */
    }
    
    return NULL;
}

static void load_directory(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    
    dir = opendir(path);
    if (!dir) {
        show_toast("Cannot open directory", COLOR_DANGER, 2000);
        return;
    }
    
    /* 清空文件列表 */
    app_ctx->file_count = 0;
    
    /* 读取目录内容 */
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            strcpy(app_ctx->files[app_ctx->file_count].name, entry->d_name);
            strcpy(app_ctx->files[app_ctx->file_count].path, full_path);
            app_ctx->files[app_ctx->file_count].is_dir = S_ISDIR(st.st_mode);
            app_ctx->files[app_ctx->file_count].size = st.st_size;
            
            /* 检查是否为音频文件 */
            const char *ext = strrchr(entry->d_name, '.');
            app_ctx->files[app_ctx->file_count].is_audio = 
                (ext && (strcasecmp(ext, ".mp3") == 0 || 
                        strcasecmp(ext, ".wav") == 0 || 
                        strcasecmp(ext, ".flac") == 0));
            
            /* 格式化显示名称 */
            if (app_ctx->files[app_ctx->file_count].is_dir) {
                snprintf(app_ctx->files[app_ctx->file_count].display_name, 
                        sizeof(app_ctx->files[app_ctx->file_count].display_name),
                        "📁 %s", entry->d_name);
            } else {
                char size_str[16];
                if (st.st_size < 1024) {
                    snprintf(size_str, sizeof(size_str), "%ld B", st.st_size);
                } else if (st.st_size < 1024 * 1024) {
                    snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size / 1024.0);
                } else {
                    snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size / (1024.0 * 1024.0));
                }
                
                snprintf(app_ctx->files[app_ctx->file_count].display_name, 
                        sizeof(app_ctx->files[app_ctx->file_count].display_name),
                        "%s %s [%s]", 
                        app_ctx->files[app_ctx->file_count].is_audio ? "🎵" : "📄",
                        entry->d_name, size_str);
            }
            
            app_ctx->file_count++;
        }
    }
    
    closedir(dir);
    
    /* 更新UI */
    update_file_list_display();
}

static void update_file_list_display(void)
{
    if (!app_ctx->file_list) return;
    
    /* 清空列表 */
    lv_obj_clean(app_ctx->file_list);
    
    /* 添加上级目录选项（如果不是根目录） */
    if (strcmp(app_ctx->current_path, ".") != 0 && 
        strcmp(app_ctx->current_path, "/") != 0 &&
        strcmp(app_ctx->current_path, "") != 0) {
        lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, LV_SYMBOL_UP, ".. (Parent Directory)");
        lv_obj_add_event_cb(btn, on_navigate_up, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xF5F5F5), 0);
    }
    
    /* 添加文件列表 */
    for (int i = 0; i < app_ctx->file_count; i++) {
        const char *icon = app_ctx->files[i].is_dir ? LV_SYMBOL_DIRECTORY : 
                          (app_ctx->files[i].is_audio ? LV_SYMBOL_AUDIO : LV_SYMBOL_FILE);
        
        lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, icon, 
                                        app_ctx->files[i].display_name);
        lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        
        /* 为目录设置不同背景色 */
        if (app_ctx->files[i].is_dir) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xE3F2FD), 0);
        }
    }
    
    /* 空文件夹提示 */
    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(app_ctx->file_list);
        lv_label_set_text(label, "Folder is empty");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }
    
    /* 更新路径显示 */
    lv_label_set_text_fmt(app_ctx->path_label, "Path: %s", app_ctx->current_path);
}

static void show_delete_dialog(int file_index)
{
    static const char *btns[] = {"Delete", "Cancel", ""};
    
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", 
        app_ctx->files[file_index].name, btns, true);
    lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, 
        (void *)(intptr_t)file_index);
    lv_obj_add_event_cb(mbox, on_delete_cancel, LV_EVENT_VALUE_CHANGED, 
        (void *)(intptr_t)file_index);
    lv_obj_center(mbox);
    
    /* 设置消息框样式 */
    lv_obj_set_style_bg_color(mbox, COLOR_CARD, 0);
    lv_obj_set_style_radius(mbox, 8, 0);
}

static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (file_index < 0 || file_index >= app_ctx->file_count) return;
    
    file_info_t *file = &app_ctx->files[file_index];
    
    if (file->is_dir) {
        /* 进入目录 */
        strcpy(app_ctx->current_path, file->path);
        load_directory(app_ctx->current_path);
    } else {
        /* 显示文件信息并询问是否删除 */
        show_delete_dialog(file_index);
    }
}

static void on_navigate_up(lv_event_t *e)
{
    char *last_slash = strrchr(app_ctx->current_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (strlen(app_ctx->current_path) == 0) {
            strcpy(app_ctx->current_path, ".");
        }
    } else if (strcmp(app_ctx->current_path, ".") != 0) {
        strcpy(app_ctx->current_path, ".");
    }
    
    load_directory(app_ctx->current_path);
}

static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (lv_msgbox_get_active_btn(mbox) == 0) {  /* Delete按钮 */
        file_info_t *file = &app_ctx->files[file_index];
        
        if (remove(file->path) == 0) {
            /* 重新加载目录 */
            load_directory(app_ctx->current_path);
            show_toast("File deleted successfully", COLOR_SECONDARY, 1500);
        } else {
            show_toast("Failed to delete file", COLOR_DANGER, 1500);
        }
    }
    
    lv_msgbox_close(mbox);
}

static void on_delete_cancel(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

/**********************
 *      音频播放器
 **********************/
static void create_audio_player_screen(void)
{
    free_app_resources();
    
    app_ctx->current_app = APP_AUDIO_PLAYER;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_scr_load(app_ctx->app_screen);
    
    /* 导航栏 - 固定高度 */
    app_ctx->nav_bar = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(app_ctx->nav_bar, LV_PCT(100), 50);
    lv_obj_set_pos(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_border_width(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->nav_bar, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(app_ctx->nav_bar, LV_OPA_10, 0);
    lv_obj_clear_flag(app_ctx->nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(app_ctx->nav_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_set_style_bg_color(back_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    
    /* 标题 */
    app_ctx->title_label = lv_label_create(app_ctx->nav_bar);
    lv_label_set_text(app_ctx->title_label, "Audio Player");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);
    
    /* 当前播放信息 - 固定位置 */
    app_ctx->track_info = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(app_ctx->track_info, "No track selected");
    lv_obj_set_style_text_font(app_ctx->track_info, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(app_ctx->track_info, 10, 60);
    lv_obj_set_size(app_ctx->track_info, 440, 25);
    lv_label_set_long_mode(app_ctx->track_info, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    /* 进度条和时间 - 固定位置 */
    app_ctx->progress_bar = lv_bar_create(app_ctx->app_screen);
    lv_obj_set_size(app_ctx->progress_bar, 300, 6);
    lv_obj_set_pos(app_ctx->progress_bar, 10, 90);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    
    app_ctx->time_label = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(app_ctx->time_label, "00:00 / 00:00");
    lv_obj_set_style_text_font(app_ctx->time_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(app_ctx->time_label, 320, 85);
    
    /* 控制按钮 - 固定位置 */
    lv_obj_t *ctrl_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_remove_style_all(ctrl_cont);
    lv_obj_set_size(ctrl_cont, 440, 60);
    lv_obj_set_pos(ctrl_cont, 10, 110);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_cont, 15, 0);
    
    /* 上一首 */
    lv_obj_t *prev_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_size(prev_btn, 45, 45);
    lv_obj_set_style_bg_color(prev_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(prev_btn, 4, 0);
    lv_obj_add_event_cb(prev_btn, on_prev_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *prev_icon = lv_label_create(prev_btn);
    lv_label_set_text(prev_icon, LV_SYMBOL_PREV);
    lv_obj_center(prev_icon);
    
    /* 播放/暂停 */
    app_ctx->play_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_size(app_ctx->play_btn, 55, 55);
    lv_obj_set_style_bg_color(app_ctx->play_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(app_ctx->play_btn, 4, 0);
    lv_obj_add_event_cb(app_ctx->play_btn, on_play_click, LV_EVENT_CLICKED, NULL);
    
    app_ctx->play_icon = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(app_ctx->play_icon, lv_color_white(), 0);
    lv_obj_center(app_ctx->play_icon);
    
    /* 下一首 */
    lv_obj_t *next_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_size(next_btn, 45, 45);
    lv_obj_set_style_bg_color(next_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(next_btn, 4, 0);
    lv_obj_add_event_cb(next_btn, on_next_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *next_icon = lv_label_create(next_btn);
    lv_label_set_text(next_icon, LV_SYMBOL_NEXT);
    lv_obj_center(next_icon);
    
    /* 停止 */
    lv_obj_t *stop_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_size(stop_btn, 45, 45);
    lv_obj_set_style_bg_color(stop_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(stop_btn, 4, 0);
    lv_obj_add_event_cb(stop_btn, on_stop_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *stop_icon = lv_label_create(stop_btn);
    lv_label_set_text(stop_icon, LV_SYMBOL_STOP);
    lv_obj_center(stop_icon);
    
    /* 播放列表标题 */
    lv_obj_t *list_title = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(list_title, "Playlist");
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(list_title, 10, 180);
    
    /* 播放列表容器 */
    lv_obj_t *list_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(list_cont, 440, 200);
    lv_obj_set_pos(list_cont, 10, 200);
    lv_obj_set_style_border_width(list_cont, 1, 0);
    lv_obj_set_style_border_color(list_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_color(list_cont, COLOR_CARD, 0);
    lv_obj_set_style_radius(list_cont, 4, 0);
    
    /* 播放列表 */
    app_ctx->track_list = lv_list_create(list_cont);
    lv_obj_set_size(app_ctx->track_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->track_list, 0, 0);
    
    /* 加载音频文件 */
    load_audio_files(".");
    
    /* 启动音频播放器线程 */
    app_ctx->audio_player_running = 1;
    pthread_create(&app_ctx->audio_player_thread, NULL, audio_player_thread_func, NULL);
}

static void* audio_player_thread_func(void *arg)
{
    (void)arg;
    
    while (app_ctx->audio_player_running) {
        if (app_ctx->is_playing) {
            /* 模拟音频播放 - 实际应用中这里会调用I2S驱动 */
            static int progress = 0;
            progress = (progress + 1) % 101;
            
            /* 更新进度条（通过UI定时器） */
            app_ctx->progress_bar->user_data = (void *)(intptr_t)progress;
            
            usleep(50000);  /* 50ms */
        } else {
            usleep(100000);
        }
    }
    
    return NULL;
}

static void load_audio_files(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    
    dir = opendir(path);
    if (!dir) {
        show_toast("Cannot open directory", COLOR_DANGER, 2000);
        return;
    }
    
    /* 清空播放列表 */
    lv_obj_clean(app_ctx->track_list);
    app_ctx->track_count = 0;
    
    /* 读取目录内容 */
    while ((entry = readdir(dir)) != NULL && app_ctx->track_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (stat(full_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".mp3") == 0 || 
                        strcasecmp(ext, ".wav") == 0 || 
                        strcasecmp(ext, ".flac") == 0)) {
                
                strcpy(app_ctx->files[app_ctx->track_count].name, entry->d_name);
                strcpy(app_ctx->files[app_ctx->track_count].path, full_path);
                app_ctx->files[app_ctx->track_count].size = st.st_size;
                
                /* 格式化显示名称 */
                char size_str[16];
                if (st.st_size < 1024 * 1024) {
                    snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size / 1024.0);
                } else {
                    snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size / (1024.0 * 1024.0));
                }
                
                snprintf(app_ctx->files[app_ctx->track_count].display_name, 
                        sizeof(app_ctx->files[app_ctx->track_count].display_name),
                        "🎵 %s [%s]", entry->d_name, size_str);
                
                lv_obj_t *btn = lv_list_add_btn(app_ctx->track_list, LV_SYMBOL_AUDIO, 
                                                app_ctx->files[app_ctx->track_count].display_name);
                lv_obj_add_event_cb(btn, on_audio_file_click, LV_EVENT_CLICKED, 
                    (void *)(intptr_t)app_ctx->track_count);
                
                app_ctx->track_count++;
            }
        }
    }
    
    closedir(dir);
    
    if (app_ctx->track_count == 0) {
        lv_obj_t *label = lv_label_create(app_ctx->track_list);
        lv_label_set_text(label, "No audio files found");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }
}

static void play_audio_file(int index)
{
    if (index < 0 || index >= app_ctx->track_count) return;
    
    app_ctx->current_track = index;
    file_info_t *file = &app_ctx->files[index];
    
    /* 更新显示 */
    char info[128];
    snprintf(info, sizeof(info), "Playing: %s", file->name);
    lv_label_set_text(app_ctx->track_info, info);
    
    app_ctx->is_playing = 1;
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PAUSE);
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    
    /* 这里应该调用I2S驱动播放音频 */
    char toast_msg[128];
    snprintf(toast_msg, sizeof(toast_msg), "Playing: %s", file->name);
    show_toast(toast_msg, COLOR_SECONDARY, 1500);
}

static void stop_audio_playback(void)
{
    app_ctx->is_playing = 0;
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PLAY);
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(app_ctx->time_label, "00:00 / 00:00");
    lv_label_set_text(app_ctx->track_info, "No track selected");
    app_ctx->current_track = -1;
}

static void on_audio_file_click(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    play_audio_file(index);
}

static void on_play_click(lv_event_t *e)
{
    if (app_ctx->current_track < 0) {
        if (app_ctx->track_count > 0) {
            play_audio_file(0);
        } else {
            show_toast("No audio files", COLOR_WARNING, 1500);
        }
        return;
    }
    
    app_ctx->is_playing = !app_ctx->is_playing;
    lv_label_set_text(app_ctx->play_icon, 
        app_ctx->is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void on_stop_click(lv_event_t *e)
{
    stop_audio_playback();
}

static void on_prev_click(lv_event_t *e)
{
    if (app_ctx->track_count == 0) return;
    
    int new_track = app_ctx->current_track - 1;
    if (new_track < 0) new_track = app_ctx->track_count - 1;
    play_audio_file(new_track);
}

static void on_next_click(lv_event_t *e)
{
    if (app_ctx->track_count == 0) return;
    
    int new_track = app_ctx->current_track + 1;
    if (new_track >= app_ctx->track_count) new_track = 0;
    play_audio_file(new_track);
}

/**********************
 *      音频处理器
 **********************/
static void create_audio_processor_screen(void)
{
    free_app_resources();
    
    app_ctx->current_app = APP_AUDIO_PROCESSOR;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_scr_load(app_ctx->app_screen);
    
    /* 导航栏 - 固定高度 */
    app_ctx->nav_bar = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(app_ctx->nav_bar, LV_PCT(100), 50);
    lv_obj_set_pos(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_border_width(app_ctx->nav_bar, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->nav_bar, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(app_ctx->nav_bar, LV_OPA_10, 0);
    lv_obj_clear_flag(app_ctx->nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(app_ctx->nav_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_set_style_bg_color(back_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    
    /* 标题 */
    app_ctx->title_label = lv_label_create(app_ctx->nav_bar);
    lv_label_set_text(app_ctx->title_label, "Audio Processor");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);
    
    /* 效果器网格容器 - 固定位置 */
    lv_obj_t *grid_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(grid_cont, 440, 220);
    lv_obj_set_pos(grid_cont, 10, 60);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, 0);
    
    /* 使用flex布局创建2x3网格 */
    lv_obj_set_flex_flow(grid_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    
    /* 创建6个效果器卡片 */
    for (int i = 0; i < MAX_EFFECTS; i++) {
        lv_obj_t *card = lv_obj_create(grid_cont);
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_size(card, 130, 100);
        lv_obj_set_style_pad_all(card, 5, 0);
        
        /* 序号 */
        lv_obj_t *num = lv_label_create(card);
        lv_label_set_text_fmt(num, "#%d", i + 1);
        lv_obj_set_style_text_font(num, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(num, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_pos(num, 5, 5);
        
        /* 效果器图标 */
        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, effect_icons[app_ctx->effects[i].type]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, app_ctx->effects[i].color, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 15);
        
        /* 效果器名称 */
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, app_ctx->effects[i].name);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
        lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -15);
        
        /* 状态指示 */
        lv_obj_t *status = lv_label_create(card);
        lv_label_set_text(status, app_ctx->effects[i].enabled ? "ON" : "OFF");
        lv_obj_set_style_text_font(status, &lv_font_montserrat_8, 0);
        lv_obj_set_style_text_color(status, 
            app_ctx->effects[i].enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_bg_color(status, 
            app_ctx->effects[i].enabled ? lv_color_lighten(COLOR_SECONDARY, LV_OPA_30) : COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(status, LV_OPA_20, 0);
        lv_obj_set_style_pad_all(status, 2, 0);
        lv_obj_set_style_radius(status, 8, 0);
        lv_obj_align(status, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
        
        app_ctx->effects[i].card = card;
        lv_obj_add_event_cb(card, on_effect_card_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    }
    
    /* 处理顺序提示 */
    lv_obj_t *hint = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(hint, "Processing order: 1 → 2 → 3 → 4 → 5 → 6");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(hint, 10, 290);
    
    /* ADC状态指示 */
    lv_obj_t *adc_status = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(adc_status, "ADC: Ready | Buffer: Double");
    lv_obj_set_style_text_font(adc_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(adc_status, COLOR_SECONDARY, 0);
    lv_obj_set_pos(adc_status, 10, 310);
    
    /* I2S状态指示 */
    lv_obj_t *i2s_status = lv_label_create(app_ctx->app_screen);
    lv_label_set_text(i2s_status, "I2S: Ready");
    lv_obj_set_style_text_font(i2s_status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(i2s_status, COLOR_SECONDARY, 0);
    lv_obj_set_pos(i2s_status, 10, 330);
    
    /* 启动音频处理器线程 */
    app_ctx->processor_running = 1;
    pthread_mutex_init(&app_ctx->proc_buffer_in.mutex, NULL);
    pthread_mutex_init(&app_ctx->proc_buffer_out.mutex, NULL);
    pthread_create(&app_ctx->audio_processor_thread, NULL, audio_processor_thread_func, NULL);
}

static void* audio_processor_thread_func(void *arg)
{
    (void)arg;
    
    int16_t input_buffer[AUDIO_BUFFER_SIZE];
    int16_t output_buffer[AUDIO_BUFFER_SIZE];
    
    while (app_ctx->processor_running) {
        /* 模拟ADC读取双缓冲区 */
        pthread_mutex_lock(&app_ctx->proc_buffer_in.mutex);
        
        /* 读取ADC数据到缓冲区 */
        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            input_buffer[i] = rand() % 1000;  /* 模拟音频数据 */
        }
        
        /* 处理音频效果 - 按顺序处理 */
        process_audio_effects(input_buffer, output_buffer, AUDIO_BUFFER_SIZE);
        
        /* 发送到I2S */
        pthread_mutex_lock(&app_ctx->proc_buffer_out.mutex);
        memcpy(app_ctx->proc_buffer_out.buffer1, output_buffer, sizeof(output_buffer));
        app_ctx->proc_buffer_out.buffer_ready = 1;
        pthread_mutex_unlock(&app_ctx->proc_buffer_out.mutex);
        
        pthread_mutex_unlock(&app_ctx->proc_buffer_in.mutex);
        
        usleep(10000);  /* 10ms - 模拟音频采样率 */
    }
    
    return NULL;
}

static void process_audio_effects(int16_t *in, int16_t *out, int len)
{
    /* 先拷贝输入到输出 */
    memcpy(out, in, len * sizeof(int16_t));
    
    /* 按顺序处理启用的效果器 */
    for (int order = 1; order <= MAX_EFFECTS; order++) {
        for (int i = 0; i < MAX_EFFECTS; i++) {
            if (app_ctx->effects[i].enabled && app_ctx->effects[i].order == order) {
                /* 根据效果器类型处理音频 */
                switch (app_ctx->effects[i].type) {
                    case EFFECT_REVERB:
                        /* 混响效果 */
                        for (int j = 0; j < len; j++) {
                            out[j] = out[j] + (int16_t)(out[j] * app_ctx->effects[i].params[0] / 200.0);
                        }
                        break;
                        
                    case EFFECT_ECHO:
                        /* 回声效果 */
                        for (int j = 100; j < len; j++) {
                            out[j] = out[j] + (int16_t)(out[j-100] * app_ctx->effects[i].params[1] / 100.0);
                        }
                        break;
                        
                    case EFFECT_DISTORTION:
                        /* 失真效果 */
                        for (int j = 0; j < len; j++) {
                            if (out[j] > 1000) out[j] = 1000;
                            if (out[j] < -1000) out[j] = -1000;
                        }
                        break;
                        
                    case EFFECT_LOW_PASS:
                        /* 低通滤波 */
                        for (int j = 1; j < len; j++) {
                            out[j] = (out[j] + out[j-1]) / 2;
                        }
                        break;
                        
                    case EFFECT_HIGH_PASS:
                        /* 高通滤波 */
                        for (int j = 1; j < len; j++) {
                            out[j] = out[j] - out[j-1];
                        }
                        break;
                        
                    default:
                        break;
                }
            }
        }
    }
}

static void create_effect_config_screen(int effect_idx)
{
    app_ctx->selected_effect = effect_idx;
    effect_t *effect = &app_ctx->effects[effect_idx];
    
    app_ctx->effect_config_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->effect_config_screen, COLOR_BG, 0);
    lv_scr_load(app_ctx->effect_config_screen);
    
    /* 导航栏 */
    lv_obj_t *nav_bar = lv_obj_create(app_ctx->effect_config_screen);
    lv_obj_set_size(nav_bar, LV_PCT(100), 50);
    lv_obj_set_pos(nav_bar, 0, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_bg_color(nav_bar, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_10, 0);
    lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(nav_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_set_style_bg_color(back_btn, COLOR_CARD, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(nav_bar);
    lv_label_set_text_fmt(title, "%s Settings", effect->name);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    /* 启用/禁用开关 */
    lv_obj_t *toggle = lv_switch_create(nav_bar);
    lv_obj_set_size(toggle, 50, 25);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_state(toggle, effect->enabled ? LV_STATE_CHECKED : 0);
    lv_obj_add_event_cb(toggle, on_effect_toggle, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)effect_idx);
    
    /* 内容区域 */
    lv_obj_t *content = lv_obj_create(app_ctx->effect_config_screen);
    lv_obj_set_size(content, 440, 350);
    lv_obj_set_pos(content, 10, 60);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    
    /* 效果类型选择 */
    lv_obj_t *type_label = lv_label_create(content);
    lv_label_set_text(type_label, "Effect Type");
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(type_label, 0, 0);
    
    lv_obj_t *type_cont = lv_obj_create(content);
    lv_obj_set_size(type_cont, 440, 80);
    lv_obj_set_pos(type_cont, 0, 25);
    lv_obj_set_style_border_width(type_cont, 0, 0);
    lv_obj_set_style_bg_opa(type_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(type_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(type_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(type_cont, 5, 0);
    lv_obj_set_style_pad_column(type_cont, 5, 0);
    
    /* 创建类型选择按钮 */
    for (int i = 1; i < 10; i++) {  /* 跳过EFFECT_NONE */
        lv_obj_t *btn = lv_btn_create(type_cont);
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 30);
        lv_obj_set_style_bg_color(btn, 
            effect->type == i ? effect_colors[i] : COLOR_CARD, 0);
        lv_obj_set_style_radius(btn, 15, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_add_event_cb(btn, on_effect_type_select, LV_EVENT_CLICKED, 
            (void *)(intptr_t)((effect_idx << 16) | i));
        
        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, effect_names[i]);
        lv_obj_set_style_text_color(btn_label, 
            effect->type == i ? lv_color_white() : COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(btn_label);
    }
    
    /* 处理顺序 */
    lv_obj_t *order_label = lv_label_create(content);
    lv_label_set_text(order_label, "Processing Order");
    lv_obj_set_style_text_font(order_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(order_label, 0, 115);
    
    lv_obj_t *order_roller = lv_roller_create(content);
    lv_roller_set_options(order_roller, "1\n2\n3\n4\n5\n6", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(order_roller, 80, 80);
    lv_obj_set_pos(order_roller, 0, 140);
    lv_roller_set_selected(order_roller, effect->order - 1, LV_ANIM_OFF);
    lv_obj_add_event_cb(order_roller, on_effect_order_change, LV_EVENT_VALUE_CHANGED, 
        (void *)(intptr_t)effect_idx);
    
    /* 参数调节 */
    lv_obj_t *param_label = lv_label_create(content);
    lv_label_set_text(param_label, "Parameters");
    lv_obj_set_style_text_font(param_label, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(param_label, 100, 115);
    
    /* 参数滑块容器 */
    lv_obj_t *param_cont = lv_obj_create(content);
    lv_obj_set_size(param_cont, 320, 180);
    lv_obj_set_pos(param_cont, 100, 140);
    lv_obj_set_style_border_width(param_cont, 0, 0);
    lv_obj_set_style_bg_opa(param_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(param_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(param_cont, 10, 0);
    
    /* 根据效果器类型创建参数滑块 */
    for (int p = 0; p < 3; p++) {
        lv_obj_t *slider_cont = lv_obj_create(param_cont);
        lv_obj_set_size(slider_cont, LV_PCT(100), 40);
        lv_obj_set_style_border_width(slider_cont, 0, 0);
        lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(slider_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        char param_name[20];
        snprintf(param_name, sizeof(param_name), "Param %d", p + 1);
        
        lv_obj_t *name = lv_label_create(slider_cont);
        lv_label_set_text(name, param_name);
        
        lv_obj_t *slider = lv_slider_create(slider_cont);
        lv_obj_set_size(slider, 150, 8);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, (int)effect->params[p], LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, on_effect_param_change, LV_EVENT_VALUE_CHANGED, 
            (void *)(intptr_t)((effect_idx << 16) | (p << 8) | p));
        
        lv_obj_t *value = lv_label_create(slider_cont);
        lv_label_set_text_fmt(value, "%d", (int)effect->params[p]);
        lv_obj_set_user_data(slider, value);
    }
}

static void update_effect_card(int idx)
{
    if (!app_ctx->effects[idx].card) return;
    
    effect_t *effect = &app_ctx->effects[idx];
    lv_obj_t *card = effect->card;
    
    /* 更新图标 */
    lv_obj_t *icon = lv_obj_get_child(card, 1);
    if (icon) {
        lv_label_set_text(icon, effect_icons[effect->type]);
        lv_obj_set_style_text_color(icon, effect->color, 0);
    }
    
    /* 更新名称 */
    lv_obj_t *name = lv_obj_get_child(card, 2);
    if (name) {
        lv_label_set_text(name, effect->name);
    }
    
    /* 更新状态 */
    lv_obj_t *status = lv_obj_get_child(card, 3);
    if (status) {
        lv_label_set_text(status, effect->enabled ? "ON" : "OFF");
        lv_obj_set_style_text_color(status, 
            effect->enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 0);
    }
}

static void on_effect_card_click(lv_event_t *e)
{
    int effect_idx = (int)(intptr_t)lv_event_get_user_data(e);
    create_effect_config_screen(effect_idx);
}

static void on_effect_toggle(lv_event_t *e)
{
    int effect_idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    
    app_ctx->effects[effect_idx].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    update_effect_card(effect_idx);
    
    show_toast(app_ctx->effects[effect_idx].enabled ? "Effect enabled" : "Effect disabled", 
               COLOR_SECONDARY, 1000);
}

static void on_effect_type_select(lv_event_t *e)
{
    uint32_t data = (uint32_t)(intptr_t)lv_event_get_user_data(e);
    int effect_idx = data >> 16;
    int type = data & 0xFFFF;
    
    app_ctx->effects[effect_idx].type = type;
    strcpy(app_ctx->effects[effect_idx].name, effect_names[type]);
    app_ctx->effects[effect_idx].color = effect_colors[type];
    
    /* 重新创建配置界面 */
    lv_obj_del_async(app_ctx->effect_config_screen);
    create_effect_config_screen(effect_idx);
    
    show_toast("Effect type changed", COLOR_PRIMARY, 1000);
}

static void on_effect_param_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(value_label, "%d", (int)value);
    
    uint32_t data = (uint32_t)(intptr_t)lv_event_get_user_data(e);
    int effect_idx = data >> 16;
    int param_idx = (data >> 8) & 0xFF;
    
    app_ctx->effects[effect_idx].params[param_idx] = value;
}

static void on_effect_order_change(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    int effect_idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    char buf[4];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    int order = atoi(buf);
    
    app_ctx->effects[effect_idx].order = order;
    
    show_toast("Processing order updated", COLOR_PRIMARY, 1000);
}

/**********************
 *      通用函数
 **********************/
static void on_app_click(lv_event_t *e)
{
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    
    switch (app_type) {
        case APP_FILE_MANAGER:
            create_file_manager_screen();
            break;
        case APP_AUDIO_PLAYER:
            create_audio_player_screen();
            break;
        case APP_AUDIO_PROCESSOR:
            create_audio_processor_screen();
            break;
        default:
            return;
    }
}

static void on_back_click(lv_event_t *e)
{
    if (app_ctx->effect_config_screen) {
        /* 从效果器配置界面返回 */
        lv_obj_del_async(app_ctx->effect_config_screen);
        app_ctx->effect_config_screen = NULL;
        lv_scr_load(app_ctx->app_screen);
    } else {
        /* 从应用返回主界面 */
        free_app_resources();
        app_ctx->current_app = APP_NONE;
        lv_scr_load(app_ctx->main_screen);
    }
}

static void free_app_resources(void)
{
    /* 停止所有线程 */
    app_ctx->file_manager_running = 0;
    app_ctx->audio_player_running = 0;
    app_ctx->processor_running = 0;
    
    if (app_ctx->file_manager_thread) {
        pthread_join(app_ctx->file_manager_thread, NULL);
        app_ctx->file_manager_thread = 0;
    }
    
    if (app_ctx->audio_player_thread) {
        pthread_join(app_ctx->audio_player_thread, NULL);
        app_ctx->audio_player_thread = 0;
    }
    
    if (app_ctx->audio_processor_thread) {
        pthread_join(app_ctx->audio_processor_thread, NULL);
        app_ctx->audio_processor_thread = 0;
    }
    
    /* 销毁互斥锁 */
    pthread_mutex_destroy(&app_ctx->proc_buffer_in.mutex);
    pthread_mutex_destroy(&app_ctx->proc_buffer_out.mutex);
    
    /* 删除应用屏幕 */
    if (app_ctx->app_screen) {
        lv_obj_del_async(app_ctx->app_screen);
        app_ctx->app_screen = NULL;
    }
    
    if (app_ctx->effect_config_screen) {
        lv_obj_del_async(app_ctx->effect_config_screen);
        app_ctx->effect_config_screen = NULL;
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    /* 更新音频播放器进度条 */
    if (ctx->current_app == APP_AUDIO_PLAYER && ctx->is_playing) {
        static int progress = 0;
        progress = (progress + 1) % 101;
        lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
        
        int total = 180;
        int current = (progress * total) / 100;
        lv_label_set_text_fmt(ctx->time_label, "%02d:%02d / 03:00", 
            current / 60, current % 60);
    }
}

static void show_toast(const char *msg, lv_color_t color, uint32_t duration)
{
    lv_obj_t *toast = lv_label_create(lv_scr_act());
    lv_label_set_text(toast, msg);
    lv_obj_set_style_text_color(toast, lv_color_white(), 0);
    lv_obj_set_style_bg_color(toast, color, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(toast, 10, 0);
    lv_obj_set_style_radius(toast, 4, 0);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_timer_t *timer = lv_timer_create(NULL, duration, toast);
    lv_timer_set_repeat_count(timer, 1);
}