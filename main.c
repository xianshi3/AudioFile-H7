/**
 * @file main.c
 * @brief 音频文件处理器 - LVGL图形用户界面
 * @version 2.0
 * @date 2026-02-16
 */

/**
 * @defgroup Includes 头文件包含
 * @{
 */
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/sdl/sdl.h"
/** @} */

/**
 * @defgroup Macros 宏定义
 * @{
 */
#define MAX_PATH 256           /**< 最大路径长度 */
#define MAX_FILES 128          /**< 最大文件数量 */
#define MAX_EFFECTS 8          /**< 最大效果器数量 */

#define HEADER_HEIGHT 50       /**< 标题栏高度 */
#define BACK_BTN_SIZE 50       /**< 返回按钮大小 */
#define SCREEN_WIDTH 460       /**< 屏幕宽度 */
#define SCREEN_HEIGHT 460      /**< 屏幕高度 */
/** @} */

/**
 * @defgroup Enums 枚举类型定义
 * @{
 */

/**
 * @brief 应用类型枚举
 */
typedef enum {
    APP_NONE = 0,              /**< 无应用 */
    APP_FILE_MANAGER,          /**< 文件管理器 */
    APP_AUDIO_PLAYER,          /**< 音频播放器 */
    APP_AUDIO_PROCESSOR,       /**< 音频处理器 */
    APP_EFFECT_CONFIG,         /**< 效果器配置 */
    APP_DEVICE_INFO            /**< 设备信息 */
} app_type_t;

/**
 * @brief 效果器类型枚举
 */
typedef enum {
    EFFECT_NONE = 0,           /**< 无效果 */
    EFFECT_REVERB,             /**< 混响效果 */
    EFFECT_ECHO,               /**< 回声效果 */
    EFFECT_DISTORTION,         /**< 失真效果 */
    EFFECT_EQ,                 /**< 均衡器 */
    EFFECT_FILTER              /**< 滤波器 */
} effect_type_t;
/** @} */

/**
 * @defgroup Structs 结构体定义
 * @{
 */

/**
 * @brief 文件信息结构体
 */
typedef struct {
    char name[64];             /**< 文件名 */
    char path[MAX_PATH];       /**< 文件路径 */
    int is_dir;                /**< 是否为目录 */
    int size;                  /**< 文件大小 */
} file_info_t;

/**
 * @brief 效果器配置结构体
 */
typedef struct {
    const char *name;          /**< 效果器名称 */
    effect_type_t type;        /**< 效果器类型 */
    int default_param1;        /**< 默认参数1 */
    int default_param2;        /**< 默认参数2 */
    int default_param3;        /**< 默认参数3 */
    const char *param_names[3]; /**< 参数名称数组 */
    int param_min[3];          /**< 参数最小值 */
    int param_max[3];          /**< 参数最大值 */
} effect_config_t;

/**
 * @brief 效果器状态结构体
 */
typedef struct {
    effect_type_t type;        /**< 效果器类型 */
    int enabled;               /**< 是否启用 */
    int param1;                /**< 参数1值 */
    int param2;                /**< 参数2值 */
    int param3;                /**< 参数3值 */
    char name[32];             /**< 效果器名称 */
    lv_obj_t *btn;             /**< 效果器按钮对象 */
    lv_obj_t *status_indicator; /**< 状态指示器对象 */
} effect_t;

/**
 * @brief 滑块回调数据结构
 */
typedef struct {
    int effect_index;          /**< 效果器索引 */
    int param_index;           /**< 参数索引 */
    lv_obj_t *value_label;     /**< 数值标签对象 */
} param_slider_data_t;

/**
 * @brief 开关回调数据结构
 */
typedef struct {
    int effect_index;          /**< 效果器索引 */
    effect_t *effect;          /**< 效果器指针 */
} switch_data_t;

/**
 * @brief 屏幕组件结构体
 */
typedef struct {
    lv_obj_t *screen;          /**< 屏幕对象 */
    lv_obj_t *header;          /**< 标题栏对象 */
    lv_obj_t *back_btn;        /**< 返回按钮对象 */
    lv_obj_t *title;           /**< 标题对象 */
    lv_obj_t *main_cont;       /**< 主容器对象 */
    lv_obj_t *list;            /**< 列表对象 */
} screen_components_t;

/**
 * @brief 应用上下文结构体
 */
typedef struct {
    /** @name 屏幕相关 */
    ///@{
    lv_obj_t *main_screen;                 /**< 主屏幕对象 */
    app_type_t current_app;                 /**< 当前应用类型 */
    int current_effect_index;               /**< 当前效果器索引 */
    screen_components_t screen;              /**< 屏幕组件 */
    ///@}
    
    /** @name 文件相关 */
    ///@{
    file_info_t files[MAX_FILES];           /**< 文件信息数组 */
    int file_count;                          /**< 文件数量 */
    char current_path[MAX_PATH];             /**< 当前路径 */
    ///@}
    
    /** @name 播放器相关 */
    ///@{
    int is_playing;                          /**< 是否正在播放 */
    int current_track;                       /**< 当前音轨索引 */
    lv_obj_t *play_btn;                      /**< 播放按钮对象 */
    lv_obj_t *progress_bar;                  /**< 进度条对象 */
    lv_obj_t *time_label;                    /**< 时间标签对象 */
    lv_obj_t *now_playing_label;             /**< 当前播放标签对象 */
    ///@}
    
    /** @name 效果器相关 */
    ///@{
    effect_t effects[MAX_EFFECTS];           /**< 效果器数组 */
    int effect_count;                        /**< 效果器数量 */
    lv_obj_t *effect_cont;                   /**< 效果器容器对象 */
    lv_obj_t *param_cont;                    /**< 参数容器对象 */
    lv_obj_t *chain_label;                   /**< 效果链标签对象 */
    ///@}
    
    /** @name 定时器管理 */
    ///@{
    lv_timer_t *app_timer;                   /**< 应用定时器 */
    int timer_running;                        /**< 定时器是否运行 */
    ///@}
} app_context_t;
/** @} */

/**
 * @defgroup Constants 常量定义
 * @{
 */

/** @brief 效果器预设配置 */
static const effect_config_t effect_presets[] = {
    {"Reverb", EFFECT_REVERB, 50, 0, 0, {"Mix", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Echo", EFFECT_ECHO, 30, 0, 0, {"Delay", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Distortion", EFFECT_DISTORTION, 70, 0, 0, {"Drive", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Equalizer", EFFECT_EQ, 50, 50, 50, {"Low", "Mid", "High"}, {0, 0, 0}, {100, 100, 100}},
    {"Filter", EFFECT_FILTER, 1000, 0, 0, {"Freq", "", ""}, {20, 0, 0}, {20000, 0, 0}}
};
/** @} */

/**
 * @defgroup GlobalVariables 全局变量
 * @{
 */
static app_context_t *app_ctx = NULL;           /**< 应用上下文指针 */
static int screen_switch_in_progress = 0;       /**< 屏幕切换中标志 */
/** @} */

/**
 * @defgroup FunctionDeclarations 函数声明
 * @{
 */
static void hal_init(void);
static void create_main_screen(void);
static void create_app_screen(app_type_t app_type);
static void setup_header(lv_obj_t *screen, const char *title);
static void setup_file_manager_screen(void);
static void setup_audio_player_screen(void);
static void setup_audio_processor_screen(void);
static void setup_effect_config_screen(int effect_index);
static void setup_device_info_screen(void);
static void load_directory(const char *path, lv_obj_t *list);
static void load_audio_files(const char *path, lv_obj_t *list);
static void on_app_click(lv_event_t *e);
static void on_back_click(lv_event_t *e);
static void on_file_click(lv_event_t *e);
static void on_audio_file_click(lv_event_t *e);
static void on_delete_confirm(lv_event_t *e);
static void on_effect_click(lv_event_t *e);
static void on_effect_config_back(lv_event_t *e);
static void on_play_click(lv_event_t *e);
static void on_stop_click(lv_event_t *e);
static void on_config_slider_change(lv_event_t *e);
static void on_effect_enable_switch(lv_event_t *e);
static void show_notification(const char *msg, lv_color_t color);
static void update_effect_chain_display(void);
static void audio_player_timer_cb(lv_timer_t *timer);
static void audio_processor_timer_cb(lv_timer_t *timer);
static void on_prev_click(lv_event_t *e);
static void on_next_click(lv_event_t *e);
static void free_callback_data(lv_event_t *e);
/** @} */

/**
 * @brief 程序入口函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    lv_init();
    hal_init();

    app_ctx = (app_context_t *)calloc(1, sizeof(app_context_t));
    if (!app_ctx) return -1;
    
    strcpy(app_ctx->current_path, "./");
    app_ctx->current_effect_index = -1;

    /* 初始化效果器 */
    app_ctx->effect_count = sizeof(effect_presets) / sizeof(effect_presets[0]);
    for (int i = 0; i < 6; i++) {
        effect_t *e = &app_ctx->effects[i];
        if (i < app_ctx->effect_count) {
            e->type = effect_presets[i].type;
            e->param1 = effect_presets[i].default_param1;
            e->param2 = effect_presets[i].default_param2;
            e->param3 = effect_presets[i].default_param3;
            strcpy(e->name, effect_presets[i].name);
        } else {
            e->type = EFFECT_NONE;
            sprintf(e->name, "Slot %d", i + 1);
        }
        e->enabled = 0;
        e->btn = NULL;
        e->status_indicator = NULL;
    }

    create_main_screen();

    while(1) {
        lv_timer_handler();
        usleep(5 * 1000);
    }
    return 0;
}

/**
 * @defgroup Initialization 初始化函数
 * @{
 */

/**
 * @brief 硬件初始化
 */
static void hal_init(void)
{
    sdl_init();

    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[SDL_HOR_RES * 100];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, SDL_HOR_RES * 100);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    lv_disp_drv_register(&disp_drv);

    lv_theme_t *th = lv_theme_default_init(NULL, 
        lv_palette_main(LV_PALETTE_BLUE), 
        lv_palette_main(LV_PALETTE_RED), 
        LV_THEME_DEFAULT_DARK, 
        LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_disp_get_next(NULL), th);

    static lv_indev_drv_t mouse_drv;
    lv_indev_drv_init(&mouse_drv);
    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&mouse_drv);
}
/** @} */

/**
 * @defgroup MainScreen 主屏幕相关函数
 * @{
 */

/**
 * @brief 创建主屏幕
 */
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_scr_load(app_ctx->main_screen);
    
    lv_obj_set_style_bg_color(app_ctx->main_screen, lv_color_hex(0x1a1a1a), 0);
    
    lv_coord_t padding = 15;
    lv_coord_t card_width = (SCREEN_WIDTH - padding * 3) / 2;
    lv_coord_t card_height = 130;
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(app_ctx->main_screen);
    lv_label_set_text(title, "Audio Processor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(title, padding, 20);
    
    lv_obj_t *version = lv_label_create(app_ctx->main_screen);
    lv_label_set_text(version, "v2.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(version, SCREEN_WIDTH - padding - 50, 25);
    
    /* 功能卡片定义 */
    struct {
        const char *icon;      /**< 图标符号 */
        const char *title;     /**< 标题文字 */
        lv_color_t color;      /**< 图标颜色 */
        app_type_t type;       /**< 应用类型 */
    } cards[] = {
        {LV_SYMBOL_DIRECTORY, "File Manager", lv_color_hex(0x3498db), APP_FILE_MANAGER},
        {LV_SYMBOL_PLAY, "Audio Player", lv_color_hex(0x2ecc71), APP_AUDIO_PLAYER},
        {LV_SYMBOL_SETTINGS, "Audio FX", lv_color_hex(0xe74c3c), APP_AUDIO_PROCESSOR},
        {LV_SYMBOL_BELL, "Device Info", lv_color_hex(0xf39c12), APP_DEVICE_INFO}
    };
    
    for (int i = 0; i < 4; i++) {
        int x = padding + (i % 2) * (card_width + padding);
        int y = 70 + (i / 2) * (card_height + padding);
        
        lv_obj_t *card = lv_btn_create(app_ctx->main_screen);
        lv_obj_set_size(card, card_width, card_height);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_style_radius(card, 16, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_shadow_width(card, 8, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_add_event_cb(card, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)cards[i].type);

        /* 确保按钮接收事件 */
        lv_obj_clear_flag(card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_add_event_cb(card, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)cards[i].type);
        
        /* 图标 */
        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, cards[i].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(icon, cards[i].color, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 35);
        
        /* 标题 */
        lv_obj_t *label = lv_label_create(card);
        lv_label_set_text(label, cards[i].title);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -25);
    }
}
/** @} */

/**
 * @defgroup ScreenManagement 屏幕管理函数
 * @{
 */

/**
 * @brief 创建应用屏幕
 * @param app_type 应用类型
 */
static void create_app_screen(app_type_t app_type)
{
    /* 防止重复切换 */
    if (screen_switch_in_progress) return;
    screen_switch_in_progress = 1;
    
    /* 停止定时器 */
    if (app_ctx->app_timer) {
        app_ctx->timer_running = 0;
        lv_timer_del(app_ctx->app_timer);
        app_ctx->app_timer = NULL;
    }

    /* 删除旧的屏幕对象 - 这会自动释放所有子对象 */
    if (app_ctx->screen.screen) {
        lv_obj_del_async(app_ctx->screen.screen);
        app_ctx->screen.screen = NULL;
    }

    /* 重置相关指针，防止悬垂指针 */
    app_ctx->screen.list = NULL;
    app_ctx->play_btn = NULL;
    app_ctx->progress_bar = NULL;
    app_ctx->time_label = NULL;
    app_ctx->now_playing_label = NULL;
    app_ctx->chain_label = NULL;
    
    app_ctx->current_app = app_type;
    app_ctx->screen.screen = lv_obj_create(NULL);
    
    lv_obj_set_style_bg_color(app_ctx->screen.screen, lv_color_hex(0x1a1a1a), 0);
    
    const char *titles[] = {
        [APP_FILE_MANAGER] = "File Manager",
        [APP_AUDIO_PLAYER] = "Audio Player",
        [APP_AUDIO_PROCESSOR] = "Audio Processor",
        [APP_EFFECT_CONFIG] = "Effect Configuration",
        [APP_DEVICE_INFO] = "Device Information"
    };
    
    /* 设置标题栏 */
    setup_header(app_ctx->screen.screen, titles[app_type]);
    
    /* 主内容容器 */
    app_ctx->screen.main_cont = lv_obj_create(app_ctx->screen.screen);
    lv_obj_set_size(app_ctx->screen.main_cont, SCREEN_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT);
    lv_obj_set_pos(app_ctx->screen.main_cont, 0, HEADER_HEIGHT);
    lv_obj_set_style_border_width(app_ctx->screen.main_cont, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->screen.main_cont, lv_color_hex(0x1a1a1a), 0);
    
    /* 根据页面类型设置滚动条 */
    switch (app_type) {
        case APP_FILE_MANAGER:
        case APP_AUDIO_PLAYER:
            lv_obj_set_scrollbar_mode(app_ctx->screen.main_cont, LV_SCROLLBAR_MODE_AUTO);
            lv_obj_set_scroll_dir(app_ctx->screen.main_cont, LV_DIR_VER);
            break;
        case APP_AUDIO_PROCESSOR:
        case APP_EFFECT_CONFIG:
        case APP_DEVICE_INFO:
            lv_obj_set_scrollbar_mode(app_ctx->screen.main_cont, LV_SCROLLBAR_MODE_OFF);
            break;
        default:
            lv_obj_set_scrollbar_mode(app_ctx->screen.main_cont, LV_SCROLLBAR_MODE_OFF);
            break;
    }
    
    /* 创建具体界面 */
    switch (app_type) {
        case APP_FILE_MANAGER: setup_file_manager_screen(); break;
        case APP_AUDIO_PLAYER: setup_audio_player_screen(); break;
        case APP_AUDIO_PROCESSOR: setup_audio_processor_screen(); break;
        case APP_EFFECT_CONFIG: setup_effect_config_screen(app_ctx->current_effect_index); break;
        case APP_DEVICE_INFO: setup_device_info_screen(); break;
        default: break;
    }
    
    lv_scr_load(app_ctx->screen.screen);
    screen_switch_in_progress = 0;
}

/**
 * @brief 创建通用标题栏
 * @param screen 父屏幕对象
 * @param title 标题文字
 */
static void setup_header(lv_obj_t *screen, const char *title)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, BACK_BTN_SIZE, HEADER_HEIGHT - 10);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x34495e), 0);
    lv_obj_set_scrollbar_mode(back_btn, LV_SCROLLBAR_MODE_OFF);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    lv_obj_add_event_cb(back_btn, 
        app_ctx->current_app == APP_EFFECT_CONFIG ? on_effect_config_back : on_back_click, 
        LV_EVENT_CLICKED, NULL);

    /* 标题 */
    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_center(title_label);

    app_ctx->screen.header = header;
    app_ctx->screen.back_btn = back_btn;
}
/** @} */

/**
 * @defgroup DeviceInfo 设备信息页面
 * @{
 */

/**
 * @brief 设置设备信息页面
 */
static void setup_device_info_screen(void)
{
    lv_obj_t *cont = app_ctx->screen.main_cont;
    
    /* 清空容器 */
    lv_obj_clean(cont);
    
    /* 统一使用440宽度 */
    const lv_coord_t content_width = 440;
    const lv_coord_t padding = 10;
    
    lv_coord_t current_y = 20;
    
    /* 标题图标 */
    lv_obj_t *icon_title = lv_label_create(cont);
    lv_label_set_text(icon_title, LV_SYMBOL_SETTINGS " System Information");
    lv_obj_set_style_text_font(icon_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(icon_title, lv_color_hex(0x3498db), 0);
    lv_obj_set_pos(icon_title, padding, current_y);
    lv_obj_set_size(icon_title, content_width, 30);
    current_y += 40;
    
    /* 设备信息卡片 */
    lv_obj_t *info_card = lv_obj_create(cont);
    lv_obj_set_size(info_card, content_width, 290);
    lv_obj_set_pos(info_card, padding, current_y);
    lv_obj_set_style_border_width(info_card, 1, 0);
    lv_obj_set_style_border_color(info_card, lv_color_hex(0x34495e), 0);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_radius(info_card, 8, 0);
    lv_obj_set_style_pad_all(info_card, 15, 0);
    
    /* 计算卡片内的布局 */
    lv_coord_t label_x = 10;
    lv_coord_t value_x = 120;
    lv_coord_t line_y_step = 35;
    
    /* 设备名称 */
    lv_obj_t *device_label = lv_label_create(info_card);
    lv_label_set_text(device_label, "Device:");
    lv_obj_set_style_text_font(device_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(device_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(device_label, label_x, 10);
    lv_obj_set_size(device_label, 100, 20);
    
    lv_obj_t *device_value = lv_label_create(info_card);
    lv_label_set_text(device_value, "STM32H743VIT6");
    lv_obj_set_style_text_font(device_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(device_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(device_value, value_x, 10);
    lv_obj_set_size(device_value, 300, 20);
    
    /* 内核 */
    lv_obj_t *core_label = lv_label_create(info_card);
    lv_label_set_text(core_label, "Core:");
    lv_obj_set_style_text_font(core_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(core_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(core_label, label_x, 10 + line_y_step);
    lv_obj_set_size(core_label, 100, 20);
    
    lv_obj_t *core_value = lv_label_create(info_card);
    lv_label_set_text(core_value, "Cortex-M7 @ 480MHz");
    lv_obj_set_style_text_font(core_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(core_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(core_value, value_x, 10 + line_y_step);
    lv_obj_set_size(core_value, 300, 20);
    
    /* 闪存 */
    lv_obj_t *flash_label = lv_label_create(info_card);
    lv_label_set_text(flash_label, "Flash:");
    lv_obj_set_style_text_font(flash_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flash_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(flash_label, label_x, 10 + line_y_step * 2);
    lv_obj_set_size(flash_label, 100, 20);
    
    lv_obj_t *flash_value = lv_label_create(info_card);
    lv_label_set_text(flash_value, "2MB");
    lv_obj_set_style_text_font(flash_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flash_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(flash_value, value_x, 10 + line_y_step * 2);
    lv_obj_set_size(flash_value, 300, 20);
    
    /* RAM */
    lv_obj_t *ram_label = lv_label_create(info_card);
    lv_label_set_text(ram_label, "RAM:");
    lv_obj_set_style_text_font(ram_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ram_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(ram_label, label_x, 10 + line_y_step * 3);
    lv_obj_set_size(ram_label, 100, 20);
    
    lv_obj_t *ram_value = lv_label_create(info_card);
    lv_label_set_text(ram_value, "1MB");
    lv_obj_set_style_text_font(ram_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ram_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(ram_value, value_x, 10 + line_y_step * 3);
    lv_obj_set_size(ram_value, 300, 20);
    
    /* 屏幕 */
    lv_obj_t *screen_label = lv_label_create(info_card);
    lv_label_set_text(screen_label, "Display:");
    lv_obj_set_style_text_font(screen_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(screen_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(screen_label, label_x, 10 + line_y_step * 4);
    lv_obj_set_size(screen_label, 100, 20);
    
    lv_obj_t *screen_value = lv_label_create(info_card);
    lv_label_set_text(screen_value, "460x460 RGB LCD");
    lv_obj_set_style_text_font(screen_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(screen_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(screen_value, value_x, 10 + line_y_step * 4);
    lv_obj_set_size(screen_value, 300, 20);
    
    /* 分隔线 */
    lv_obj_t *line1 = lv_obj_create(info_card);
    lv_obj_set_size(line1, content_width - 30, 1);
    lv_obj_set_pos(line1, 15, 10 + line_y_step * 5);
    lv_obj_set_style_border_width(line1, 0, 0);
    lv_obj_set_style_bg_color(line1, lv_color_hex(0x34495e), 0);
    
    /* 连接状态 */
    lv_obj_t *status_title = lv_label_create(info_card);
    lv_label_set_text(status_title, "Connection Status:");
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_title, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(status_title, label_x, 30 + line_y_step * 5);
    lv_obj_set_size(status_title, 150, 20);
    
    lv_obj_t *status_value = lv_label_create(info_card);
    lv_label_set_text(status_value, "Connected");
    lv_obj_set_style_text_font(status_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_value, lv_color_hex(0x2ecc71), 0);
    lv_obj_set_pos(status_value, value_x + 80, 30 + line_y_step * 5);
    lv_obj_set_size(status_value, 120, 20);
    
    /* 波特率 */
    lv_obj_t *baud_label = lv_label_create(info_card);
    lv_label_set_text(baud_label, "Baud Rate:");
    lv_obj_set_style_text_font(baud_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(baud_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(baud_label, label_x, 30 + line_y_step * 6);
    lv_obj_set_size(baud_label, 100, 20);
    
    lv_obj_t *baud_value = lv_label_create(info_card);
    lv_label_set_text(baud_value, "115200 bps");
    lv_obj_set_style_text_font(baud_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(baud_value, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(baud_value, value_x, 30 + line_y_step * 6);
    lv_obj_set_size(baud_value, 200, 20);
    
    /* 底部提示 */
    lv_obj_t *footer = lv_label_create(cont);
    lv_label_set_text(footer, "STM32H743VIT6 • System Ready");
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(footer, padding, 400);
    lv_obj_set_size(footer, content_width, 20);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
}
/** @} */

/**
 * @defgroup FileManager 文件管理器页面
 * @{
 */

/**
 * @brief 设置文件管理器页面
 */
static void setup_file_manager_screen(void)
{
    lv_obj_t *cont = app_ctx->screen.main_cont;
    
    /* 清空容器 */
    lv_obj_clean(cont);
    
    lv_coord_t content_width = 400;
    
    /* 路径显示 - 使用完整宽度 */
    lv_obj_t *path_label = lv_label_create(cont);
    lv_label_set_text(path_label, app_ctx->current_path);
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(path_label, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(path_label, 10, 5);
    lv_obj_set_size(path_label, content_width, 20);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    /* 文件列表 - 使用完整宽度 */
    app_ctx->screen.list = lv_list_create(cont);
    lv_obj_set_size(app_ctx->screen.list, content_width, SCREEN_HEIGHT - HEADER_HEIGHT - 35);
    lv_obj_set_pos(app_ctx->screen.list, 10, 30);
    lv_obj_set_style_bg_color(app_ctx->screen.list, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_border_color(app_ctx->screen.list, lv_color_hex(0x34495e), 0);
    lv_obj_set_scrollbar_mode(app_ctx->screen.list, LV_SCROLLBAR_MODE_AUTO);
    
    load_directory(app_ctx->current_path, app_ctx->screen.list);
}
/** @} */

/**
 * @defgroup AudioPlayer 音频播放器页面
 * @{
 */

/**
 * @brief 设置音频播放器页面
 */
static void setup_audio_player_screen(void)
{
    lv_obj_t *cont = app_ctx->screen.main_cont;
    
    /* 清空容器 */
    lv_obj_clean(cont);
    
    /* 基础尺寸定义 */
    const lv_coord_t screen_width = 460;
    const lv_coord_t screen_height = 460;
    const lv_coord_t header_height = 50;
    const lv_coord_t padding = (screen_width - 425) / 2;
    
    /* 内容区域 */
    const lv_coord_t content_width = 400;
    const lv_coord_t start_x = padding;
    
    /* 按钮尺寸 */
    const lv_coord_t btn_main_size = 75;
    const lv_coord_t btn_normal_size = 60;
    
    /* 区域高度 */
    const lv_coord_t progress_area_height = 50;
    const lv_coord_t control_area_height = 85;
    
    /* 字体大小 */
    const lv_font_t *font_large = &lv_font_montserrat_24;
    const lv_font_t *font_medium = &lv_font_montserrat_16;
    const lv_font_t *font_small = &lv_font_montserrat_14;
    const lv_font_t *font_icon_large = &lv_font_montserrat_36;
    const lv_font_t *font_icon_normal = &lv_font_montserrat_28;
    
    /* 间距 */
    const lv_coord_t spacing_small = 10;
    const lv_coord_t spacing_medium = 25;
    const lv_coord_t spacing_large = 20;
    
    /* 计算Y坐标 */
    lv_coord_t current_y = 20;
    
    const lv_coord_t y_song = current_y;
    current_y += 35 + spacing_medium;
    
    const lv_coord_t y_progress = current_y;
    current_y += progress_area_height + spacing_large;
    
    const lv_coord_t y_controls = current_y;
    current_y += control_area_height + spacing_large;
    
    const lv_coord_t y_list_title = current_y;
    current_y += 25 + spacing_small;
    
    const lv_coord_t y_list = current_y;
    const lv_coord_t list_height = screen_height - header_height - y_list - 10;
    
    /* 确保内容容器没有滚动条 */
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    
    /* 歌名区域 */
    app_ctx->now_playing_label = lv_label_create(cont);
    lv_label_set_text(app_ctx->now_playing_label, "Not playing");
    lv_obj_set_style_text_font(app_ctx->now_playing_label, font_large, 0);
    lv_obj_set_style_text_color(app_ctx->now_playing_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_align(app_ctx->now_playing_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(app_ctx->now_playing_label, content_width);
    lv_obj_set_pos(app_ctx->now_playing_label, start_x, y_song);
    lv_label_set_long_mode(app_ctx->now_playing_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_height(app_ctx->now_playing_label, 35);
    
    /* 进度条区域 */
    app_ctx->time_label = lv_label_create(cont);
    lv_label_set_text(app_ctx->time_label, "00:00");
    lv_obj_set_style_text_font(app_ctx->time_label, font_small, 0);
    lv_obj_set_style_text_color(app_ctx->time_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(app_ctx->time_label, start_x, y_progress + 16);
    
    lv_obj_t *total_time = lv_label_create(cont);
    lv_label_set_text(total_time, "03:00");
    lv_obj_set_style_text_font(total_time, font_small, 0);
    lv_obj_set_style_text_color(total_time, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(total_time, start_x + content_width - 45, y_progress + 16);
    
    app_ctx->progress_bar = lv_bar_create(cont);
    lv_obj_set_size(app_ctx->progress_bar, content_width - 80, 6);
    lv_obj_set_pos(app_ctx->progress_bar, start_x + 40, y_progress + 18);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    lv_obj_set_style_radius(app_ctx->progress_bar, 3, 0);
    lv_obj_set_style_bg_color(app_ctx->progress_bar, lv_color_hex(0x34495e), LV_PART_MAIN);
    lv_obj_set_style_bg_color(app_ctx->progress_bar, lv_color_hex(0x3498db), LV_PART_INDICATOR);
    lv_obj_set_scrollbar_mode(app_ctx->progress_bar, LV_SCROLLBAR_MODE_OFF);
    
    /* 控制按钮区域 */
    const lv_coord_t total_btn_width = btn_normal_size * 3 + btn_main_size;
    const lv_coord_t btn_spacing = (content_width - total_btn_width) / 5;
    const lv_coord_t btn_y_offset = y_controls + (control_area_height - btn_main_size) / 2;
    
    /* 上一首按钮 */
    lv_obj_t *prev_btn = lv_btn_create(cont);
    lv_obj_set_size(prev_btn, btn_normal_size, btn_normal_size);
    lv_obj_set_pos(prev_btn, start_x + btn_spacing, btn_y_offset + (btn_main_size - btn_normal_size) / 2);
    lv_obj_set_style_radius(prev_btn, btn_normal_size / 2, 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x34495e), 0);
    lv_obj_set_scrollbar_mode(prev_btn, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(prev_btn, on_prev_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prev_label, font_icon_normal, 0);
    lv_obj_set_style_text_color(prev_label, lv_color_white(), 0);
    lv_obj_center(prev_label);
    
    /* 播放/暂停按钮 */
    app_ctx->play_btn = lv_btn_create(cont);
    lv_obj_set_size(app_ctx->play_btn, btn_main_size, btn_main_size);
    lv_obj_set_pos(app_ctx->play_btn, start_x + btn_spacing * 2 + btn_normal_size, btn_y_offset);
    lv_obj_set_style_radius(app_ctx->play_btn, btn_main_size / 2, 0);
    lv_obj_set_style_bg_color(app_ctx->play_btn, lv_color_hex(0x3498db), 0);
    lv_obj_set_scrollbar_mode(app_ctx->play_btn, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(app_ctx->play_btn, on_play_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_label, font_icon_large, 0);
    lv_obj_set_style_text_color(play_label, lv_color_white(), 0);
    lv_obj_center(play_label);
    
    /* 下一首按钮 */
    lv_obj_t *next_btn = lv_btn_create(cont);
    lv_obj_set_size(next_btn, btn_normal_size, btn_normal_size);
    lv_obj_set_pos(next_btn, start_x + btn_spacing * 3 + btn_normal_size + btn_main_size, 
                   btn_y_offset + (btn_main_size - btn_normal_size) / 2);
    lv_obj_set_style_radius(next_btn, btn_normal_size / 2, 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x34495e), 0);
    lv_obj_set_scrollbar_mode(next_btn, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(next_btn, on_next_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(next_label, font_icon_normal, 0);
    lv_obj_set_style_text_color(next_label, lv_color_white(), 0);
    lv_obj_center(next_label);
    
    /* 停止按钮 */
    lv_obj_t *stop_btn = lv_btn_create(cont);
    lv_obj_set_size(stop_btn, btn_normal_size, btn_normal_size);
    lv_obj_set_pos(stop_btn, start_x + btn_spacing * 4 + btn_normal_size * 2 + btn_main_size, 
                   btn_y_offset + (btn_main_size - btn_normal_size) / 2);
    lv_obj_set_style_radius(stop_btn, btn_normal_size / 2, 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xe74c3c), 0);
    lv_obj_set_scrollbar_mode(stop_btn, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(stop_btn, on_stop_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(stop_label, font_icon_normal, 0);
    lv_obj_set_style_text_color(stop_label, lv_color_white(), 0);
    lv_obj_center(stop_label);
    
    /* 播放列表区域 */
    lv_obj_t *list_title = lv_label_create(cont);
    lv_label_set_text(list_title, "Playlist");
    lv_obj_set_style_text_font(list_title, font_medium, 0);
    lv_obj_set_style_text_color(list_title, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(list_title, start_x, y_list_title);
    
    app_ctx->screen.list = lv_list_create(cont);
    lv_obj_set_size(app_ctx->screen.list, content_width, list_height);
    lv_obj_set_pos(app_ctx->screen.list, start_x, y_list);
    lv_obj_set_style_bg_color(app_ctx->screen.list, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_border_color(app_ctx->screen.list, lv_color_hex(0x34495e), 0);
    lv_obj_set_style_radius(app_ctx->screen.list, 8, 0);
    lv_obj_set_scrollbar_mode(app_ctx->screen.list, LV_SCROLLBAR_MODE_AUTO);
    
    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_player_timer_cb, 100, app_ctx);
    
    load_audio_files("./", app_ctx->screen.list);
}
/** @} */

/**
 * @defgroup AudioProcessor 音频处理器页面
 * @{
 */

/**
 * @brief 设置音频处理器页面
 */
static void setup_audio_processor_screen(void)
{
    lv_obj_t *cont = app_ctx->screen.main_cont;
    
    /* 清空容器 */
    lv_obj_clean(cont);
    
    /* 计算网格尺寸 */
    lv_coord_t padding = 10;
    lv_coord_t card_width = (440 - padding * 4) / 3;
    lv_coord_t card_height = 85;
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Audio Effects");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x3498db), 0);
    lv_obj_set_pos(title, 10, 10);
    
    /* 创建5个效果器框 */
    for (int i = 0; i < 5; i++) {
        effect_t *effect = &app_ctx->effects[i];
        
        int row = i / 3;
        int col = i % 3;
        
        int x;
        if (row == 1) {
            int total_width = card_width * 2 + padding;
            int start_x = (440 - total_width) / 2;
            x = start_x + col * (card_width + padding);
        } else {
            x = 10 + col * (card_width + padding);
        }
        
        int y = 45 + row * (card_height + padding);
        
        /* 效果器卡片 */
        lv_obj_t *card = lv_btn_create(cont);
        lv_obj_set_size(card, card_width, card_height);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_shadow_width(card, 4, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_ofs_y(card, 2, 0);
        lv_obj_add_event_cb(card, on_effect_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        
        effect->btn = card;
        
        /* 效果器图标 */
        const char *icon;
        switch (i) {
            case 0: icon = LV_SYMBOL_VOLUME_MAX; break;
            case 1: icon = LV_SYMBOL_LOOP; break;
            case 2: icon = LV_SYMBOL_EDIT; break;
            case 3: icon = LV_SYMBOL_SETTINGS; break;
            case 4: icon = LV_SYMBOL_WARNING; break;
            default: icon = LV_SYMBOL_AUDIO; break;
        }
        
        lv_obj_t *icon_label = lv_label_create(card);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x3498db), 0);
        lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 10);
        
        lv_obj_t *name_label = lv_label_create(card);
        lv_label_set_text(name_label, effect->name);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
        lv_obj_align(name_label, LV_ALIGN_BOTTOM_MID, 0, -8);
        
        /* 状态指示器 */
        lv_obj_t *indicator = lv_obj_create(card);
        lv_obj_set_size(indicator, 10, 10);
        lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(indicator, 0, 0);
        lv_obj_set_style_bg_color(indicator, effect->enabled ? lv_color_hex(0x2ecc71) : lv_color_hex(0xe74c3c), 0);
        lv_obj_align(indicator, LV_ALIGN_TOP_RIGHT, -8, 8);
        
        effect->status_indicator = indicator;
    }
    
    /* 效果链显示区域 */
    lv_obj_t *chain_bg = lv_obj_create(cont);
    lv_obj_set_size(chain_bg, 410, 70);
    lv_obj_set_pos(chain_bg, 10, 270);
    lv_obj_set_style_border_width(chain_bg, 1, 0);
    lv_obj_set_style_border_color(chain_bg, lv_color_hex(0x34495e), 0);
    lv_obj_set_style_bg_color(chain_bg, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_radius(chain_bg, 8, 0);
    lv_obj_set_style_pad_all(chain_bg, 10, 0);
    
    lv_obj_t *chain_title = lv_label_create(chain_bg);
    lv_label_set_text(chain_title, "Active Effect Chain:");
    lv_obj_set_style_text_font(chain_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chain_title, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(chain_title, 10, 5);
    
    app_ctx->chain_label = lv_label_create(chain_bg);
    lv_obj_set_style_text_font(app_ctx->chain_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app_ctx->chain_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_size(app_ctx->chain_label, 420, 30);
    lv_obj_set_pos(app_ctx->chain_label, 10, 25);
    lv_label_set_long_mode(app_ctx->chain_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    /* 底部提示信息 */
    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "Tap any effect to configure");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    /* 初始化效果链显示 */
    update_effect_chain_display();
    
    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_processor_timer_cb, 1000, app_ctx);
}
/** @} */

/**
 * @defgroup EffectConfig 效果器配置页面
 * @{
 */

/**
 * @brief 设置效果器配置页面
 * @param effect_index 效果器索引
 */
static void setup_effect_config_screen(int effect_index)
{
    if (effect_index < 0 || effect_index >= 6) return;
    
    effect_t *effect = &app_ctx->effects[effect_index];
    lv_obj_t *cont = app_ctx->screen.main_cont;
    
    const effect_config_t *config = NULL;
    for (size_t i = 0; i < sizeof(effect_presets)/sizeof(effect_presets[0]); i++) {
        if (effect_presets[i].type == effect->type) {
            config = &effect_presets[i];
            break;
        }
    }
    
    lv_coord_t current_y = 10;
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text_fmt(title, "%s Settings", effect->name);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 10, current_y);
    lv_obj_set_size(title, 440, 30);
    current_y += 40;
    
    /* 启用开关 */
    lv_obj_t *switch_label = lv_label_create(cont);
    lv_label_set_text(switch_label, "Enable Effect:");
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(switch_label, lv_color_white(), 0);
    lv_obj_set_pos(switch_label, 10, current_y);
    current_y += 30;
    
    lv_obj_t *enable_switch = lv_switch_create(cont);
    lv_obj_set_size(enable_switch, 60, 25);
    lv_obj_set_pos(enable_switch, 130, current_y - 22);
    if (effect->enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }
    
    switch_data_t *switch_data = (switch_data_t *)malloc(sizeof(switch_data_t));
    if (switch_data) {
        switch_data->effect_index = effect_index;
        switch_data->effect = effect;
        lv_obj_add_event_cb(enable_switch, on_effect_enable_switch, LV_EVENT_VALUE_CHANGED, switch_data);
        lv_obj_add_event_cb(enable_switch, free_callback_data, LV_EVENT_DELETE, switch_data);
    }
    
    if (!config) return;
    
    /* 参数滑块 */
    int param_values[3] = {effect->param1, effect->param2, effect->param3};
    
    for (int i = 0; i < 3; i++) {
        if (strlen(config->param_names[i]) == 0) continue;
        
        current_y += 10;
        
        lv_obj_t *name = lv_label_create(cont);
        lv_label_set_text(name, config->param_names[i]);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0x888888), 0);
        lv_obj_set_pos(name, 10, current_y);
        
        lv_obj_t *value = lv_label_create(cont);
        lv_label_set_text_fmt(value, "%d", param_values[i]);
        lv_obj_set_style_text_font(value, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0x3498db), 0);
        lv_obj_set_pos(value, 380, current_y);
        current_y += 25;
        
        lv_obj_t *slider = lv_slider_create(cont);
        lv_obj_set_size(slider, 350, 8);
        lv_obj_set_pos(slider, 10, current_y);
        lv_slider_set_range(slider, config->param_min[i], config->param_max[i]);
        lv_slider_set_value(slider, param_values[i], LV_ANIM_OFF);
        current_y += 25;
        
        param_slider_data_t *slider_data = (param_slider_data_t *)malloc(sizeof(param_slider_data_t));
        if (slider_data) {
            slider_data->effect_index = effect_index;
            slider_data->param_index = i;
            slider_data->value_label = value;
            lv_obj_add_event_cb(slider, on_config_slider_change, LV_EVENT_VALUE_CHANGED, slider_data);
            lv_obj_add_event_cb(slider, free_callback_data, LV_EVENT_DELETE, slider_data);
        }
    }
}
/** @} */

/**
 * @defgroup EventHandlers 事件处理函数
 * @{
 */

/**
 * @brief 上一首按钮点击事件
 * @param e 事件对象
 */
static void on_prev_click(lv_event_t *e)
{
    if (app_ctx->file_count == 0) {
        show_notification("No tracks", lv_color_hex(0xe74c3c));
        return;
    }
    
    app_ctx->current_track--;
    if (app_ctx->current_track < 0) {
        app_ctx->current_track = app_ctx->file_count - 1;
    }
    
    file_info_t *file = &app_ctx->files[app_ctx->current_track];
    lv_label_set_text(app_ctx->now_playing_label, file->name);
    
    app_ctx->is_playing = 1;
    
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    if (play_label) lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    
    show_notification("Previous", lv_color_hex(0x3498db));
}

/**
 * @brief 下一首按钮点击事件
 * @param e 事件对象
 */
static void on_next_click(lv_event_t *e)
{
    if (app_ctx->file_count == 0) {
        show_notification("No tracks", lv_color_hex(0xe74c3c));
        return;
    }
    
    app_ctx->current_track++;
    if (app_ctx->current_track >= app_ctx->file_count) {
        app_ctx->current_track = 0;
    }
    
    file_info_t *file = &app_ctx->files[app_ctx->current_track];
    lv_label_set_text(app_ctx->now_playing_label, file->name);
    
    app_ctx->is_playing = 1;
    
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    if (play_label) lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    
    show_notification("Next", lv_color_hex(0x3498db));
}

/**
 * @brief 应用卡片点击事件
 * @param e 事件对象
 */
static void on_app_click(lv_event_t *e)
{
    if (screen_switch_in_progress) return;
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    create_app_screen(app_type);
}

/**
 * @brief 返回按钮点击事件
 * @param e 事件对象
 */
static void on_back_click(lv_event_t *e) 
{ 
    create_main_screen(); 
}

/**
 * @brief 效果器配置返回按钮点击事件
 * @param e 事件对象
 */
static void on_effect_config_back(lv_event_t *e) 
{ 
    create_app_screen(APP_AUDIO_PROCESSOR); 
}

/**
 * @brief 效果器卡片点击事件
 * @param e 事件对象
 */
static void on_effect_click(lv_event_t *e)
{
    if (screen_switch_in_progress) return;
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    app_ctx->current_effect_index = effect_index;
    create_app_screen(APP_EFFECT_CONFIG);
}

/**
 * @brief 文件点击事件
 * @param e 事件对象
 */
static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (file_index == -1) {
        char *last_slash = strrchr(app_ctx->current_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (strlen(app_ctx->current_path) == 0) strcpy(app_ctx->current_path, "./");
        }
        load_directory(app_ctx->current_path, app_ctx->screen.list);
        return;
    }

    file_info_t *file = &app_ctx->files[file_index];
    
    if (file->is_dir) {
        strcpy(app_ctx->current_path, file->path);
        load_directory(app_ctx->current_path, app_ctx->screen.list);
    } else {
        static const char *btns[] = {"Delete", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", 
                                          "Delete this file?", btns, true);
        lv_obj_set_style_bg_color(mbox, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_text_color(mbox, lv_color_white(), 0);
        lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)file_index);
        lv_obj_center(mbox);
    }
}

/**
 * @brief 音频文件点击事件
 * @param e 事件对象
 */
static void on_audio_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    if (file_index < 0 || file_index >= app_ctx->file_count) return;
    
    file_info_t *file = &app_ctx->files[file_index];
    lv_label_set_text(app_ctx->now_playing_label, file->name);
    
    app_ctx->current_track = file_index;
    app_ctx->is_playing = 1;
    
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    if (play_label) lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
}

/**
 * @brief 删除确认事件
 * @param e 事件对象
 */
static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);

    if (lv_msgbox_get_active_btn(mbox) == 0 && remove(app_ctx->files[file_index].path) == 0) {
        load_directory(app_ctx->current_path, app_ctx->screen.list);
        show_notification("File deleted", lv_color_hex(0x2ecc71));
    }
    lv_msgbox_close(mbox);
}

/**
 * @brief 播放/暂停按钮点击事件
 * @param e 事件对象
 */
static void on_play_click(lv_event_t *e)
{
    if (app_ctx->current_track < 0) {
        show_notification("No track selected", lv_color_hex(0xe74c3c));
        return;
    }
    
    app_ctx->is_playing = !app_ctx->is_playing;
    
    lv_obj_t *label = lv_obj_get_child(app_ctx->play_btn, 0);
    if (label) lv_label_set_text(label, app_ctx->is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

/**
 * @brief 停止按钮点击事件
 * @param e 事件对象
 */
static void on_stop_click(lv_event_t *e)
{
    app_ctx->is_playing = 0;
    app_ctx->current_track = -1;
    
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(app_ctx->time_label, "00:00");
    lv_label_set_text(app_ctx->now_playing_label, "Not playing");
    
    lv_obj_t *label = lv_obj_get_child(app_ctx->play_btn, 0);
    if (label) lv_label_set_text(label, LV_SYMBOL_PLAY);
}

/**
 * @brief 配置页面滑块值改变事件
 * @param e 事件对象
 */
static void on_config_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    param_slider_data_t *data = (param_slider_data_t *)lv_event_get_user_data(e);
    
    if (!data) return;
    
    int32_t value = lv_slider_get_value(slider);
    
    if (data->value_label && lv_obj_is_valid(data->value_label)) {
        lv_label_set_text_fmt(data->value_label, "%d", (int)value);
    }
    
    if (data->effect_index >= 0 && data->effect_index < 6) {
        effect_t *effect = &app_ctx->effects[data->effect_index];
        switch (data->param_index) {
            case 0: effect->param1 = value; break;
            case 1: effect->param2 = value; break;
            case 2: effect->param3 = value; break;
        }
    }
}

/**
 * @brief 效果器启用开关事件
 * @param e 事件对象
 */
static void on_effect_enable_switch(lv_event_t *e)
{
    lv_obj_t *switch_btn = lv_event_get_current_target(e);
    switch_data_t *data = (switch_data_t *)lv_event_get_user_data(e);
    
    if (!data || !data->effect) return;
    
    effect_t *effect = data->effect;
    effect->enabled = (lv_obj_get_state(switch_btn) & LV_STATE_CHECKED) != 0;
    
    if (app_ctx->current_app == APP_AUDIO_PROCESSOR) {
        if (effect->status_indicator && lv_obj_is_valid(effect->status_indicator)) {
            if (effect->enabled) {
                lv_obj_set_style_bg_color(effect->status_indicator, lv_color_hex(0x2ecc71), 0);
            } else {
                lv_obj_set_style_bg_color(effect->status_indicator, lv_color_hex(0xe74c3c), 0);
            }
        }
        update_effect_chain_display();
    }
    
    show_notification(effect->enabled ? "Effect enabled" : "Effect disabled",
                     lv_color_hex(0x3498db));
}
/** @} */

/**
 * @defgroup FileOperations 文件操作函数
 * @{
 */

/**
 * @brief 加载目录内容
 * @param path 目录路径
 * @param list 列表对象
 */
static void load_directory(const char *path, lv_obj_t *list)
{
    DIR *dir = opendir(path);
    if (!dir) {
        show_notification("Cannot open directory", lv_color_hex(0xe74c3c));
        return;
    }

    lv_obj_clean(list);
    app_ctx->file_count = 0;

    if (strcmp(path, "./") != 0 && strcmp(path, "/") != 0) {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_DIRECTORY, "..");
        lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
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

            const char *icon = file->is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
            lv_obj_t *btn = lv_list_add_btn(list, icon, entry->d_name);
            lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)app_ctx->file_count);
            
            app_ctx->file_count++;
        }
    }
    closedir(dir);
}

/**
 * @brief 加载音频文件
 * @param path 目录路径
 * @param list 列表对象
 */
static void load_audio_files(const char *path, lv_obj_t *list)
{
    DIR *dir = opendir(path);
    if (!dir) {
        show_notification("Cannot open directory", lv_color_hex(0xe74c3c));
        return;
    }

    lv_obj_clean(list);
    app_ctx->file_count = 0;

    struct dirent *entry;
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (stat(full_path, &st) != 0 || S_ISDIR(st.st_mode)) continue;
        
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || (strcasecmp(ext, ".mp3") != 0 && strcasecmp(ext, ".wav") != 0 && 
                     strcasecmp(ext, ".flac") != 0)) continue;
        
        file_info_t *file = &app_ctx->files[app_ctx->file_count];
        strcpy(file->name, entry->d_name);
        strcpy(file->path, full_path);
        file->size = st.st_size;

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, entry->d_name);
        lv_obj_add_event_cb(btn, on_audio_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)app_ctx->file_count);
        
        app_ctx->file_count++;
    }
    closedir(dir);
    
    if (app_ctx->file_count == 0) {
        lv_obj_t *empty_label = lv_label_create(list);
        lv_label_set_text(empty_label, "No audio files found");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x888888), 0);
        lv_obj_set_width(empty_label, lv_obj_get_width(list));
        lv_obj_center(empty_label);
    }
}
/** @} */

/**
 * @defgroup HelperFunctions 辅助函数
 * @{
 */

/**
 * @brief 更新效果链显示
 */
static void update_effect_chain_display(void)
{
    if (!app_ctx->chain_label || !lv_obj_is_valid(app_ctx->chain_label)) {
        return;
    }
    
    static char chain_text[128];
    chain_text[0] = '\0';
    int enabled_count = 0;
    int pos = 0;
    
    for (int i = 0; i < 5; i++) {
        if (app_ctx->effects[i].enabled) {
            if (enabled_count > 0) {
                strcpy(&chain_text[pos], " → ");
                pos += 3;
            }
            int len = strlen(app_ctx->effects[i].name);
            strcpy(&chain_text[pos], app_ctx->effects[i].name);
            pos += len;
            enabled_count++;
        }
    }
    
    if (enabled_count == 0) {
        strcpy(chain_text, "No active effects");
        lv_obj_set_style_text_color(app_ctx->chain_label, lv_color_hex(0x888888), 0);
    } else {
        lv_obj_set_style_text_color(app_ctx->chain_label, lv_color_hex(0x2ecc71), 0);
    }
    
    lv_label_set_text(app_ctx->chain_label, chain_text);
}

/**
 * @brief 显示通知
 * @param msg 消息内容
 * @param color 背景颜色
 */
static void show_notification(const char *msg, lv_color_t color)
{
    lv_obj_t *notif = lv_label_create(lv_scr_act());
    lv_label_set_text(notif, msg);
    lv_obj_set_style_text_font(notif, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(notif, lv_color_white(), 0);
    lv_obj_set_style_bg_color(notif, color, 0);
    lv_obj_set_style_bg_opa(notif, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(notif, 10, 0);
    lv_obj_set_style_radius(notif, 5, 0);
    lv_obj_align(notif, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_timer_t *timer = lv_timer_create(NULL, 1500, notif);
    lv_timer_set_repeat_count(timer, 1);
}

/**
 * @brief 释放回调数据
 * @param e 事件对象
 */
static void free_callback_data(lv_event_t *e)
{
    void *data = lv_event_get_user_data(e);
    if (data) {
        free(data);
    }
}
/** @} */

/**
 * @defgroup TimerCallbacks 定时器回调函数
 * @{
 */

/**
 * @brief 音频播放器定时器回调
 * @param timer 定时器对象
 */
static void audio_player_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    if (!ctx || !ctx->timer_running || !ctx->is_playing) return;

    static int progress = 0;
    progress = (progress + 1) % 101;
    lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
    
    int total = 180;
    int current = (progress * total) / 100;
    
    char time_str[16];
    sprintf(time_str, "%02d:%02d", current / 60, current % 60);
    lv_label_set_text(ctx->time_label, time_str);
    
    if (progress == 100) {
        ctx->is_playing = 0;
        lv_obj_t *play_label = lv_obj_get_child(ctx->play_btn, 0);
        if (play_label) lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    }
}

/**
 * @brief 音频处理器定时器回调
 * @param timer 定时器对象
 */
static void audio_processor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
}
/** @} */