/**
 * @file main.c
 * @brief 音频文件处理器 - LVGL图形用户界面
 * @version 2.0
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

#define HEADER_HEIGHT 50
#define FOOTER_HEIGHT 85
#define BUTTON_WIDTH 220
#define BUTTON_HEIGHT 60
#define BACK_BTN_SIZE 50

#define CREATE_LABEL(parent, text, font, align, x, y) \
    ({ \
        lv_obj_t *label = lv_label_create(parent); \
        lv_label_set_text(label, text); \
        lv_obj_set_style_text_font(label, font, 0); \
        lv_obj_align(label, align, x, y); \
        label; \
    })

#define CREATE_BTN(parent, width, height, event_cb, user_data) \
    ({ \
        lv_obj_t *btn = lv_btn_create(parent); \
        lv_obj_set_size(btn, width, height); \
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, user_data); \
        btn; \
    })

/**********************
 *      类型定义
 **********************/
typedef enum {
    APP_NONE = 0,
    APP_FILE_MANAGER,
    APP_AUDIO_PLAYER,
    APP_AUDIO_PROCESSOR,
    APP_EFFECT_CONFIG
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
    int param_min[3];
    int param_max[3];
} effect_config_t;

typedef struct {
    effect_type_t type;
    int enabled;
    int param1;
    int param2;
    int param3;
    char name[32];
    lv_obj_t *btn;
    lv_obj_t *status_indicator;
} effect_t;

/* 滑块数据结构 */
typedef struct {
    int effect_index;
    int param_index;
    lv_obj_t *value_label;
} param_slider_data_t;

/* 开关事件数据结构 */
typedef struct {
    int effect_index;
    effect_t *effect;
} switch_data_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *header;
    lv_obj_t *back_btn;
    lv_obj_t *title;
    lv_obj_t *main_cont;
    lv_obj_t *list;
} screen_components_t;

typedef struct {
    /* 主屏幕 */
    lv_obj_t *main_screen;
    
    /* 当前状态 */
    app_type_t current_app;
    int current_effect_index;
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
    lv_obj_t *chain_label;
    
    /* 定时器管理 */
    lv_timer_t *app_timer;
    int timer_running;
} app_context_t;

/* 效果器预设配置 */
static const effect_config_t effect_presets[] = {
    {"Reverb", EFFECT_REVERB, 50, 0, 0, {"Mix", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Echo", EFFECT_ECHO, 30, 0, 0, {"Delay", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Distortion", EFFECT_DISTORTION, 70, 0, 0, {"Drive", "", ""}, {0, 0, 0}, {100, 0, 0}},
    {"Equalizer", EFFECT_EQ, 50, 50, 50, {"Low", "Mid", "High"}, {0, 0, 0}, {100, 100, 100}},
    {"Filter", EFFECT_FILTER, 1000, 0, 0, {"Freq", "", ""}, {20, 0, 0}, {20000, 0, 0}}
};

/**********************
 *      静态变量
 **********************/
static app_context_t *app_ctx = NULL;

/**********************
 *      静态函数声明
 **********************/
static void hal_init(void);
static void create_main_screen(void);
static void create_app_screen(app_type_t app_type);
static void setup_header(lv_obj_t *screen, const char *title);
static void setup_file_manager_screen(void);
static void setup_audio_player_screen(void);
static void setup_audio_processor_screen(void);
static void setup_effect_config_screen(int effect_index);
static void file_manager_timer_cb(lv_timer_t *timer);
static void audio_player_timer_cb(lv_timer_t *timer);
static void audio_processor_timer_cb(lv_timer_t *timer);
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
static void on_slider_change(lv_event_t *e);
static void on_config_slider_change(lv_event_t *e);
static void on_effect_enable_switch(lv_event_t *e);
static void show_notification(const char *msg, lv_color_t color);
static void cleanup_app(void);
static void update_effect_chain_display(void);

/**********************
 *      全局函数
 **********************/
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    lv_init();
    hal_init();

    app_ctx = (app_context_t *)calloc(1, sizeof(app_context_t));
    if (!app_ctx) {
        return -1;
    }
    strcpy(app_ctx->current_path, "./");
    app_ctx->main_screen = NULL;
    app_ctx->current_effect_index = -1;

    create_main_screen();

    while(1) {
        lv_timer_handler();
        usleep(5 * 1000);
    }

    return 0;
}

/**********************
 *      静态函数实现
 **********************/
static void hal_init(void)
{
    sdl_init();

    /* 显示缓冲区 */
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[SDL_HOR_RES * 100];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, SDL_HOR_RES * 100);

    /* 显示设备 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* 主题设置 */
    lv_theme_t *th = lv_theme_default_init(disp, 
        lv_palette_main(LV_PALETTE_BLUE), 
        lv_palette_main(LV_PALETTE_RED), 
        LV_THEME_DEFAULT_DARK, 
        LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

    /* 输入设备组 */
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    /* 鼠标输入 */
    static lv_indev_drv_t mouse_drv;
    lv_indev_drv_init(&mouse_drv);
    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = sdl_mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&mouse_drv);

    /* 键盘输入 */
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t *kb_indev = lv_indev_drv_register(&kb_drv);
    lv_indev_set_group(kb_indev, g);

    /* 编码器输入 */
    static lv_indev_drv_t enc_drv;
    lv_indev_drv_init(&enc_drv);
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = sdl_mousewheel_read;
    lv_indev_t *enc_indev = lv_indev_drv_register(&enc_drv);
    lv_indev_set_group(enc_indev, g);

    /* 鼠标光标 */
    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t *cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(cursor, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor);
}

/**
 * @brief 创建主屏幕 - 优化版本
 * 屏幕分辨率：460x460
 * 布局：顶部标题、中部功能卡片、底部状态栏
 */
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_scr_load(app_ctx->main_screen);
    
    /* 设置基础背景色 - 深色简约风格 */
    lv_obj_set_style_bg_color(app_ctx->main_screen, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(app_ctx->main_screen, LV_OPA_COVER, 0);
    
    /* 获取屏幕尺寸 */
    lv_coord_t screen_w = lv_obj_get_width(lv_scr_act());
    lv_coord_t screen_h = lv_obj_get_height(lv_scr_act());
    
    /* 计算各区域尺寸 - 固定值，避免过度计算 */
    lv_coord_t padding = 15;
    lv_coord_t header_height = 50;
    lv_coord_t footer_height = 40;
    lv_coord_t card_area_height = screen_h - header_height - footer_height - padding * 2;
    lv_coord_t card_width = (screen_w - padding * 3) / 2;
    lv_coord_t card_height = (card_area_height - padding) / 2;
    
    /* ==================== 顶部标题区域 ==================== */
    lv_obj_t *header = lv_obj_create(app_ctx->main_screen);
    lv_obj_set_size(header, LV_PCT(100), header_height);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    
    /* 主标题 */
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Audio Processor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, padding, 0);
    
    /* 版本号 */
    lv_obj_t *version = lv_label_create(header);
    lv_label_set_text(version, "v2.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(0x888888), 0);
    lv_obj_align(version, LV_ALIGN_RIGHT_MID, -padding, 0);
    
    /* ==================== 功能卡片区域 ==================== */
    /* 创建卡片容器 - 使用绝对定位，避免flex布局带来的性能问题 */
    lv_obj_t *card_container = lv_obj_create(app_ctx->main_screen);
    lv_obj_set_size(card_container, screen_w - padding * 2, card_area_height);
    lv_obj_set_pos(card_container, padding, header_height + padding);
    lv_obj_set_style_border_width(card_container, 0, 0);
    lv_obj_set_style_bg_opa(card_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(card_container, 0, 0);
    lv_obj_set_scrollbar_mode(card_container, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
    
    /* 功能卡片定义 */
    struct {
        const char *icon;
        const char *title;
        const char *desc;
        lv_color_t color;
        app_type_t type;
        int col;
        int row;
    } cards[] = {
        {LV_SYMBOL_DIRECTORY, "Files", "Browse", lv_color_hex(0x3498db), APP_FILE_MANAGER, 0, 0},
        {LV_SYMBOL_PLAY, "Player", "Listen", lv_color_hex(0x2ecc71), APP_AUDIO_PLAYER, 1, 0},
        {LV_SYMBOL_SETTINGS, "Effects", "Process", lv_color_hex(0xe74c3c), APP_AUDIO_PROCESSOR, 0, 1},
        {LV_SYMBOL_AUDIO, "EQ", "Adjust", lv_color_hex(0xf39c12), APP_AUDIO_PROCESSOR, 1, 1}
    };
    
    /* 创建功能卡片 - 使用绝对定位，避免flex布局 */
    for (int i = 0; i < 4; i++) {
        /* 计算卡片位置 */
        lv_coord_t card_x = cards[i].col * (card_width + padding);
        lv_coord_t card_y = cards[i].row * (card_height + padding);
        
        /* 卡片容器 */
        lv_obj_t *card = lv_obj_create(card_container);
        lv_obj_set_size(card, card_width, card_height);
        lv_obj_set_pos(card, card_x, card_y);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(card, 16, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x222222), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(card, 8, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_ofs_y(card, 4, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
        
        /* 设置点击事件 */
        lv_obj_add_event_cb(card, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)cards[i].type);
        
        /* 点击效果 - 使用更简单的方式 */
        lv_obj_set_style_bg_color(card, cards[i].color, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_STATE_PRESSED);
        
        /* 图标容器 - 简化，去除不必要的嵌套 */
        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, cards[i].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(icon, cards[i].color, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 15);
        
        /* 标题 */
        lv_obj_t *card_title = lv_label_create(card);
        lv_label_set_text(card_title, cards[i].title);
        lv_obj_set_style_text_font(card_title, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(card_title, lv_color_hex(0xffffff), 0);
        lv_obj_align(card_title, LV_ALIGN_BOTTOM_LEFT, 10, -25);
        
        /* 描述 */
        lv_obj_t *card_desc = lv_label_create(card);
        lv_label_set_text(card_desc, cards[i].desc);
        lv_obj_set_style_text_font(card_desc, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(card_desc, lv_color_hex(0x888888), 0);
        lv_obj_align(card_desc, LV_ALIGN_BOTTOM_LEFT, 10, -8);
    }
    
    /* ==================== 底部状态栏 ==================== */
    lv_obj_t *footer = lv_obj_create(app_ctx->main_screen);
    lv_obj_set_size(footer, LV_PCT(100), footer_height);
    lv_obj_set_pos(footer, 0, screen_h - footer_height);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    
    /* 左侧状态信息 */
    lv_obj_t *status = lv_label_create(footer);
    lv_label_set_text(status, "STM32H743");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0x888888), 0);
    lv_obj_align(status, LV_ALIGN_LEFT_MID, padding, 0);
    
    /* 右侧系统信息 */
    lv_obj_t *sys_info = lv_label_create(footer);
    lv_label_set_text(sys_info, "460x460");
    lv_obj_set_style_text_font(sys_info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sys_info, lv_color_hex(0x888888), 0);
    lv_obj_align(sys_info, LV_ALIGN_RIGHT_MID, -padding, 0);
    
    /* 装饰线 */
    lv_obj_t *line = lv_obj_create(app_ctx->main_screen);
    lv_obj_set_size(line, screen_w - padding * 2, 2);
    lv_obj_set_pos(line, padding, screen_h - footer_height - 2);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_50, 0);
    lv_obj_set_style_radius(line, 0, 0);
}

/**
 * @brief 点击动画定时器回调 - 简化版本
 */
static void card_animation_cb(lv_timer_t *timer)
{
    lv_obj_t *card = (lv_obj_t *)timer->user_data;
    if (card && lv_obj_is_valid(card)) {
        /* 恢复原始大小 */
        lv_coord_t w = lv_obj_get_width(card) + 5;
        lv_coord_t h = lv_obj_get_height(card) + 5;
        lv_obj_set_size(card, w, h);
    }
    lv_timer_del(timer);
}


/**
 * @brief 创建应用屏幕（通用入口）
 */
static void create_app_screen(app_type_t app_type)
{
    /* 先停止定时器，但不清除屏幕 */
    if (app_ctx->app_timer) {
        app_ctx->timer_running = 0;
        lv_timer_del(app_ctx->app_timer);
        app_ctx->app_timer = NULL;
    }

    /* 延迟删除旧的屏幕对象 */
    if (app_ctx->screen.screen) {
        lv_obj_del_async(app_ctx->screen.screen);
        app_ctx->screen.screen = NULL;
    }
    
    app_ctx->current_app = app_type;
    
    /* 创建新屏幕 */
    app_ctx->screen.screen = lv_obj_create(NULL);
    
    const char *titles[] = {
        [APP_FILE_MANAGER] = "File Manager",
        [APP_AUDIO_PLAYER] = "Audio Player",
        [APP_AUDIO_PROCESSOR] = "Audio Processor",
        [APP_EFFECT_CONFIG] = "Effect Configuration"
    };
    
    setup_header(app_ctx->screen.screen, titles[app_type]);
    
    /* 主内容容器 */
    app_ctx->screen.main_cont = lv_obj_create(app_ctx->screen.screen);
    lv_obj_set_size(app_ctx->screen.main_cont, LV_PCT(100), LV_PCT(85));
    lv_obj_align(app_ctx->screen.main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(app_ctx->screen.main_cont, 10, 0);
    lv_obj_set_style_border_width(app_ctx->screen.main_cont, 0, 0);
    lv_obj_set_style_bg_opa(app_ctx->screen.main_cont, LV_OPA_TRANSP, 0);

    /* 根据应用类型创建具体界面 */
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
        case APP_EFFECT_CONFIG:
            setup_effect_config_screen(app_ctx->current_effect_index);
            break;
        default:
            break;
    }

    lv_scr_load(app_ctx->screen.screen);
}

/**
 * @brief 创建通用标题栏
 */
static void setup_header(lv_obj_t *screen, const char *title)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    /* 返回按钮 */
    lv_obj_t *back_btn = CREATE_BTN(header, BACK_BTN_SIZE, HEADER_HEIGHT - 10, 
                                    app_ctx->current_app == APP_EFFECT_CONFIG ? 
                                    on_effect_config_back : on_back_click, NULL);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);

    /* 标题 */
    CREATE_LABEL(header, title, &lv_font_montserrat_16, LV_ALIGN_CENTER, 0, 0);

    /* 保存到上下文 */
    app_ctx->screen.header = header;
    app_ctx->screen.back_btn = back_btn;
}

/**
 * @brief 设置文件管理器界面
 */
static void setup_file_manager_screen(void)
{
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);
    
    /* 路径显示 */
    CREATE_LABEL(app_ctx->screen.main_cont, app_ctx->current_path, 
                 &lv_font_montserrat_12, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 文件列表 */
    app_ctx->screen.list = lv_list_create(app_ctx->screen.main_cont);
    lv_obj_set_size(app_ctx->screen.list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->screen.list, 0, 0);

    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(file_manager_timer_cb, 100, app_ctx);
    
    load_directory(app_ctx->current_path, app_ctx->screen.list);
}

/**
 * @brief 设置音频播放器界面 - 优化版本
 * 屏幕分辨率：460x460
 * 布局优化：播放列表、当前播放信息、控制按钮合理分布
 */
static void setup_audio_player_screen(void)
{
    /* 获取屏幕实际尺寸 */
    lv_coord_t screen_h = lv_obj_get_height(lv_scr_act());
    lv_coord_t screen_w = lv_obj_get_width(lv_scr_act());
    
    /* 计算各区域高度 */
    lv_coord_t header_height = HEADER_HEIGHT;
    lv_coord_t content_height = screen_h - header_height - 20;
    lv_coord_t playlist_height = content_height * 0.4;  // 播放列表占40%
    lv_coord_t now_playing_height = 40;                 // 当前播放信息高度
    lv_coord_t progress_height = 40;                    // 进度条区域高度
    lv_coord_t control_height = 80;                     // 控制按钮区域高度
    
    /* 主容器设置为垂直布局 */
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(app_ctx->screen.main_cont, 10, 0);
    lv_obj_set_style_pad_row(app_ctx->screen.main_cont, 8, 0);
    lv_obj_set_style_bg_opa(app_ctx->screen.main_cont, LV_OPA_TRANSP, 0);

    /* ==================== 播放列表区域 ==================== */
    /* 播放列表标题和统计 */
    lv_obj_t *playlist_header = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(playlist_header, LV_PCT(100), 30);
    lv_obj_set_style_border_width(playlist_header, 0, 0);
    lv_obj_set_style_bg_opa(playlist_header, LV_OPA_10, 0);
    lv_obj_set_style_radius(playlist_header, 8, 0);
    lv_obj_set_style_pad_all(playlist_header, 5, 0);

    lv_obj_t *playlist_title = lv_label_create(playlist_header);
    lv_label_set_text(playlist_title, LV_SYMBOL_AUDIO " Playlist");
    lv_obj_set_style_text_font(playlist_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(playlist_title, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(playlist_title, LV_ALIGN_LEFT_MID, 5, 0);

    /* 播放列表 */
    app_ctx->screen.list = lv_list_create(app_ctx->screen.main_cont);
    lv_obj_set_size(app_ctx->screen.list, LV_PCT(100), playlist_height);
    lv_obj_set_style_border_width(app_ctx->screen.list, 1, 0);
    lv_obj_set_style_border_color(app_ctx->screen.list, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(app_ctx->screen.list, 8, 0);
    lv_obj_set_style_bg_opa(app_ctx->screen.list, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(app_ctx->screen.list, 5, 0);

    /* ==================== 当前播放信息区域 ==================== */
    lv_obj_t *now_playing_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(now_playing_cont, LV_PCT(100), now_playing_height);
    lv_obj_set_style_border_width(now_playing_cont, 1, 0);
    lv_obj_set_style_border_color(now_playing_cont, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(now_playing_cont, 8, 0);
    lv_obj_set_style_bg_opa(now_playing_cont, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(now_playing_cont, 5, 0);

    /* 现在播放图标 */
    lv_obj_t *playing_icon = lv_label_create(now_playing_cont);
    lv_label_set_text(playing_icon, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(playing_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(playing_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(playing_icon, LV_ALIGN_LEFT_MID, 5, 0);

    /* 当前播放歌曲名 */
    app_ctx->now_playing_label = lv_label_create(now_playing_cont);
    lv_label_set_text(app_ctx->now_playing_label, "Not playing");
    lv_obj_set_style_text_font(app_ctx->now_playing_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(app_ctx->now_playing_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_width(app_ctx->now_playing_label, LV_PCT(80));
    lv_obj_align(app_ctx->now_playing_label, LV_ALIGN_LEFT_MID, 30, 0);

    /* ==================== 进度条区域 ==================== */
    lv_obj_t *progress_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(progress_cont, LV_PCT(100), progress_height);
    lv_obj_set_style_border_width(progress_cont, 0, 0);
    lv_obj_set_style_bg_opa(progress_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_cont, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(progress_cont, 5, 0);

    /* 进度条 */
    app_ctx->progress_bar = lv_bar_create(progress_cont);
    lv_obj_set_size(app_ctx->progress_bar, LV_PCT(70), 8);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    lv_obj_set_style_radius(app_ctx->progress_bar, 5, 0);
    lv_obj_set_style_bg_color(app_ctx->progress_bar, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(app_ctx->progress_bar, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    
    /* 时间显示 */
    app_ctx->time_label = lv_label_create(progress_cont);
    lv_label_set_text(app_ctx->time_label, "00:00/03:00");
    lv_obj_set_style_text_font(app_ctx->time_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(app_ctx->time_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_width(app_ctx->time_label, LV_PCT(25));

    /* ==================== 控制按钮区域 ==================== */
    lv_obj_t *control_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(control_cont, LV_PCT(100), control_height);
    lv_obj_set_style_border_width(control_cont, 1, 0);
    lv_obj_set_style_border_color(control_cont, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(control_cont, 8, 0);
    lv_obj_set_style_bg_opa(control_cont, LV_OPA_10, 0);
    lv_obj_set_flex_flow(control_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(control_cont, LV_FLEX_ALIGN_SPACE_EVENLY, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(control_cont, 5, 0);

    /* 上一首按钮 */
    lv_obj_t *prev_btn = lv_btn_create(control_cont);
    lv_obj_set_size(prev_btn, 70, 60);
    lv_obj_set_style_radius(prev_btn, 8, 0);
    lv_obj_set_style_bg_color(prev_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(prev_btn, NULL, LV_EVENT_CLICKED, NULL); // TODO: 添加上一首功能
    
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_20, 0);
    lv_obj_center(prev_label);

    /* 播放/暂停按钮 - 主按钮，更大一些 */
    app_ctx->play_btn = lv_btn_create(control_cont);
    lv_obj_set_size(app_ctx->play_btn, 80, 70);
    lv_obj_set_style_radius(app_ctx->play_btn, 10, 0);
    lv_obj_set_style_bg_color(app_ctx->play_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_opa(app_ctx->play_btn, LV_OPA_80, 0);
    lv_obj_add_event_cb(app_ctx->play_btn, on_play_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_24, 0);
    lv_obj_center(play_label);

    /* 停止按钮 */
    lv_obj_t *stop_btn = lv_btn_create(control_cont);
    lv_obj_set_size(stop_btn, 70, 60);
    lv_obj_set_style_radius(stop_btn, 8, 0);
    lv_obj_set_style_bg_color(stop_btn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(stop_btn, on_stop_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(stop_label, &lv_font_montserrat_20, 0);
    lv_obj_center(stop_label);

    /* 下一首按钮 */
    lv_obj_t *next_btn = lv_btn_create(control_cont);
    lv_obj_set_size(next_btn, 70, 60);
    lv_obj_set_style_radius(next_btn, 8, 0);
    lv_obj_set_style_bg_color(next_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(next_btn, NULL, LV_EVENT_CLICKED, NULL); // TODO: 添加下一首功能
    
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_20, 0);
    lv_obj_center(next_label);

    /* 底部装饰信息 */
    lv_obj_t *footer_label = lv_label_create(app_ctx->screen.main_cont);
    lv_label_set_text(footer_label, LV_SYMBOL_VOLUME_MAX " Volume Control");
    lv_obj_set_style_text_font(footer_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(footer_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(footer_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_player_timer_cb, 100, app_ctx);
    
    /* 加载音频文件 */
    load_audio_files("./", app_ctx->screen.list);
}

/**
 * @brief 设置音频处理器主界面
 */
static void setup_audio_processor_screen(void)
{
    /* 获取屏幕实际尺寸 */
    lv_coord_t screen_h = lv_obj_get_height(lv_scr_act());
    lv_coord_t screen_w = lv_obj_get_width(lv_scr_act());
    
    /* 计算自适应尺寸 */
    lv_coord_t header_height = HEADER_HEIGHT;
    lv_coord_t content_height = screen_h - header_height - 10;
    lv_coord_t effect_height = content_height - 60;
    lv_coord_t chain_height = 50;
    
    /* 计算网格尺寸 */
    lv_coord_t grid_padding = 10;
    lv_coord_t grid_spacing = 8;
    lv_coord_t grid_width = screen_w - (grid_padding * 2) - (grid_spacing * 2);
    lv_coord_t card_width = grid_width / 3;
    lv_coord_t card_height = (effect_height - grid_padding * 2 - grid_spacing) / 2;

    /* 主容器设置 */
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(app_ctx->screen.main_cont, grid_padding, 0);
    lv_obj_set_style_pad_row(app_ctx->screen.main_cont, 5, 0);
    lv_obj_set_style_bg_opa(app_ctx->screen.main_cont, LV_OPA_TRANSP, 0);

    /* ==================== 效果器网格区域 ==================== */
    lv_obj_t *effect_grid_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(effect_grid_cont, LV_PCT(100), effect_height);
    lv_obj_set_style_border_width(effect_grid_cont, 0, 0);
    lv_obj_set_style_bg_opa(effect_grid_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(effect_grid_cont, 0, 0);
    
    /* 创建网格布局 */
    lv_obj_set_flex_flow(effect_grid_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(effect_grid_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                          LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_row(effect_grid_cont, grid_spacing, 0);
    lv_obj_set_style_pad_column(effect_grid_cont, grid_spacing, 0);

    /* 初始化效果器 */
    app_ctx->effect_count = sizeof(effect_presets) / sizeof(effect_presets[0]);
    
    /* 创建6个效果器框 */
    for (int i = 0; i < 6; i++) {
        effect_t *effect;
        
        if (i < app_ctx->effect_count) {
            effect = &app_ctx->effects[i];
            effect->type = effect_presets[i].type;
            effect->enabled = 0;
            effect->param1 = effect_presets[i].default_param1;
            effect->param2 = effect_presets[i].default_param2;
            effect->param3 = effect_presets[i].default_param3;
            strcpy(effect->name, effect_presets[i].name);
        } else {
            effect = &app_ctx->effects[i];
            effect->type = EFFECT_NONE;
            effect->enabled = 0;
            effect->param1 = 0;
            effect->param2 = 0;
            effect->param3 = 0;
            sprintf(effect->name, "Slot %d", i + 1);
        }

        /* 效果器卡片容器 */
        lv_obj_t *card = lv_obj_create(effect_grid_cont);
        lv_obj_set_size(card, card_width, card_height);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_shadow_width(card, 5, 0);
        lv_obj_set_style_shadow_ofs_y(card, 3, 0);
        lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_20, 0);
        lv_obj_set_style_bg_color(card, lv_palette_darken(LV_PALETTE_GREY, 3), 0);

        /* 效果器按钮 */
        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_add_event_cb(btn, on_effect_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_10, 0);

        /* 按钮内容垂直居中 */
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, 
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(btn, 5, 0);

        /* 序号标签 */
        lv_obj_t *index_label = lv_label_create(btn);
        lv_label_set_text_fmt(index_label, "[%d]", i + 1);
        lv_obj_set_style_text_font(index_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(index_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_align(index_label, LV_ALIGN_TOP_LEFT, 5, 5);

        /* 效果器图标 */
        lv_obj_t *icon = lv_label_create(btn);
        const char *icon_text;
        switch (i) {
            case 0: icon_text = LV_SYMBOL_VOLUME_MAX; break;
            case 1: icon_text = LV_SYMBOL_LOOP; break;
            case 2: icon_text = LV_SYMBOL_EDIT; break;
            case 3: icon_text = LV_SYMBOL_SETTINGS; break;
            case 4: icon_text = LV_SYMBOL_WARNING; break;
            default: icon_text = LV_SYMBOL_IMAGE; break;
        }
        lv_label_set_text(icon, icon_text);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, lv_palette_main(LV_PALETTE_BLUE), 0);

        /* 效果器名称 */
        lv_obj_t *name_label = lv_label_create(btn);
        lv_label_set_text(name_label, effect->name);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(name_label, lv_color_white(), 0);

        /* 启用状态指示器 */
        lv_obj_t *status_indicator = lv_obj_create(btn);
        lv_obj_set_size(status_indicator, 10, 10);
        lv_obj_set_style_radius(status_indicator, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(status_indicator, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_border_width(status_indicator, 0, 0);
        lv_obj_align(status_indicator, LV_ALIGN_TOP_RIGHT, -5, 5);
        
        effect->btn = btn;
        effect->status_indicator = status_indicator;
    }

    /* ==================== 效果链显示区域 ==================== */
    lv_obj_t *chain_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(chain_cont, LV_PCT(100), chain_height);
    lv_obj_set_style_border_width(chain_cont, 1, 0);
    lv_obj_set_style_border_color(chain_cont, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(chain_cont, LV_OPA_20, 0);
    lv_obj_set_style_radius(chain_cont, 8, 0);
    lv_obj_set_style_pad_all(chain_cont, 5, 0);

    lv_obj_t *chain_label = lv_label_create(chain_cont);
    lv_label_set_text(chain_label, "Effect Chain: ");
    lv_obj_set_style_text_font(chain_label, &lv_font_montserrat_12, 0);
    lv_obj_align(chain_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *chain_status = lv_label_create(chain_cont);
    lv_label_set_text(chain_status, "None");
    lv_obj_set_style_text_font(chain_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chain_status, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(chain_status, LV_ALIGN_RIGHT_MID, 0, 0);

    app_ctx->chain_label = chain_status;

    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_processor_timer_cb, 10, app_ctx);
}

/**
 * @brief 设置效果器配置页面 - 完整修复版本
 */
static void setup_effect_config_screen(int effect_index)
{
    if (effect_index < 0 || effect_index >= 6) return;
    
    effect_t *effect = &app_ctx->effects[effect_index];
    
    /* 获取屏幕实际尺寸 */
    lv_coord_t screen_h = lv_obj_get_height(lv_scr_act());
    lv_coord_t header_height = HEADER_HEIGHT;
    lv_coord_t content_height = screen_h - header_height - 20;
    
    /* 获取效果器配置 */
    const effect_config_t *config = NULL;
    for (size_t i = 0; i < sizeof(effect_presets) / sizeof(effect_presets[0]); i++) {
        if (effect_presets[i].type == effect->type) {
            config = &effect_presets[i];
            break;
        }
    }
    
    if (!config && effect_index >= app_ctx->effect_count) {
        /* 空白槽位，显示简单配置 */
        lv_obj_t *label = lv_label_create(app_ctx->screen.main_cont);
        lv_label_set_text(label, "Empty Effect Slot\n\nSelect effect type:");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        return;
    }
    
    /* 主容器设置为垂直布局 */
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(app_ctx->screen.main_cont, 10, 0);
    lv_obj_set_style_pad_row(app_ctx->screen.main_cont, 10, 0);
    
    /* 效果器标题和开关 */
    lv_obj_t *header_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(header_cont, LV_PCT(100), 60);
    lv_obj_set_style_border_width(header_cont, 1, 0);
    lv_obj_set_style_border_color(header_cont, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(header_cont, 8, 0);
    lv_obj_set_style_pad_all(header_cont, 10, 0);
    lv_obj_set_style_bg_opa(header_cont, LV_OPA_20, 0);
    
    lv_obj_t *title_label = lv_label_create(header_cont);
    lv_label_set_text_fmt(title_label, "%s Settings", effect->name);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 0, 0);
    
    lv_obj_t *enable_switch = lv_switch_create(header_cont);
    lv_obj_set_size(enable_switch, 60, 30);
    lv_obj_align(enable_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    if (effect->enabled) {
        lv_obj_add_state(enable_switch, LV_STATE_CHECKED);
    }
    
    /* 创建开关事件回调的数据结构 */
    switch_data_t *switch_data = (switch_data_t *)malloc(sizeof(switch_data_t));
    if (switch_data) {
        switch_data->effect_index = effect_index;
        switch_data->effect = effect;
        lv_obj_add_event_cb(enable_switch, on_effect_enable_switch, LV_EVENT_VALUE_CHANGED, switch_data);
    }
    
    /* 统计有效参数数量 */
    int param_count = 0;
    for (int i = 0; i < 3; i++) {
        if (strlen(config->param_names[i]) > 0) {
            param_count++;
        }
    }
    
    /* 计算参数容器的高度 */
    lv_coord_t params_height = content_height - 80;
    
    /* 参数容器 */
    lv_obj_t *params_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(params_cont, LV_PCT(100), params_height);
    lv_obj_set_style_border_width(params_cont, 0, 0);
    lv_obj_set_style_bg_opa(params_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(params_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(params_cont, 10, 0);
    lv_obj_set_style_pad_all(params_cont, 5, 0);
    
    /* 计算每个参数项的高度 */
    lv_coord_t param_item_height = (params_height - (param_count - 1) * 10) / param_count;
    if (param_item_height > 80) param_item_height = 80;
    if (param_item_height < 60) param_item_height = 60;  // 设置最小高度
    
    /* 创建参数滑块 */
    int param_values[3] = {effect->param1, effect->param2, effect->param3};
    
    for (int i = 0; i < param_count; i++) {
        lv_obj_t *param_cont = lv_obj_create(params_cont);
        lv_obj_set_size(param_cont, LV_PCT(100), param_item_height);
        lv_obj_set_style_border_width(param_cont, 1, 0);
        lv_obj_set_style_border_color(param_cont, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_radius(param_cont, 8, 0);
        lv_obj_set_style_pad_all(param_cont, 8, 0);
        lv_obj_set_style_bg_opa(param_cont, LV_OPA_10, 0);
        
        /* 参数名和数值显示 */
        lv_obj_t *name_value_cont = lv_obj_create(param_cont);
        lv_obj_set_size(name_value_cont, LV_PCT(100), 20);
        lv_obj_set_style_border_width(name_value_cont, 0, 0);
        lv_obj_set_style_bg_opa(name_value_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(name_value_cont, 0, 0);
        
        lv_obj_t *name_label = lv_label_create(name_value_cont);
        lv_label_set_text(name_label, config->param_names[i]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(name_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 0, 0);
        
        lv_obj_t *value_label = lv_label_create(name_value_cont);
        lv_label_set_text_fmt(value_label, "%d", param_values[i]);
        lv_obj_set_style_text_font(value_label, &lv_font_montserrat_12, 0);
        lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, 0, 0);
        
        /* 滑块 */
        lv_obj_t *slider = lv_slider_create(param_cont);
        lv_obj_set_size(slider, LV_PCT(100), 8);
        lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_slider_set_range(slider, config->param_min[i], config->param_max[i]);
        lv_slider_set_value(slider, param_values[i], LV_ANIM_OFF);
        
        /* 存储参数信息用于回调 */
        param_slider_data_t *slider_data = (param_slider_data_t *)malloc(sizeof(param_slider_data_t));
        if (slider_data) {
            slider_data->effect_index = effect_index;
            slider_data->param_index = i;
            slider_data->value_label = value_label;
            lv_obj_add_event_cb(slider, on_config_slider_change, LV_EVENT_VALUE_CHANGED, slider_data);
        }
    }
    
    /* 提示信息 */
    lv_obj_t *info_label = lv_label_create(app_ctx->screen.main_cont);
    lv_label_set_text(info_label, "Audio data processed in slot order");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -5);
}

/**
 * @brief 效果器点击事件 - 打开配置页面
 */
static void on_effect_click(lv_event_t *e)
{
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    app_ctx->current_effect_index = effect_index;
    create_app_screen(APP_EFFECT_CONFIG);
}

/**
 * @brief 从配置页面返回主处理器页面
 */
static void on_effect_config_back(lv_event_t *e)
{
    (void)e;
    create_app_screen(APP_AUDIO_PROCESSOR);
}

/**
 * @brief 配置页面滑块值改变事件
 */
static void on_config_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    param_slider_data_t *data = (param_slider_data_t *)lv_event_get_user_data(e);
    
    if (!data) return;
    
    int32_t value = lv_slider_get_value(slider);
    
    /* 更新值标签 */
    if (data->value_label && lv_obj_is_valid(data->value_label)) {
        lv_label_set_text_fmt(data->value_label, "%d", (int)value);
    }
    
    /* 更新效果器参数 */
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
 * @brief 效果器启用开关事件 - 修复版本
 */
static void on_effect_enable_switch(lv_event_t *e)
{
    lv_obj_t *switch_btn = lv_event_get_current_target(e);
    switch_data_t *data = (switch_data_t *)lv_event_get_user_data(e);
    
    if (!data || !data->effect) return;
    
    effect_t *effect = data->effect;
    effect->enabled = (lv_obj_get_state(switch_btn) & LV_STATE_CHECKED) != 0;
    
    /* 只在主界面存在时更新状态指示器 */
    if (app_ctx->current_app == APP_AUDIO_PROCESSOR) {
        /* 检查状态指示器是否有效 */
        if (effect->status_indicator && lv_obj_is_valid(effect->status_indicator)) {
            if (effect->enabled) {
                lv_obj_set_style_bg_color(effect->status_indicator, lv_palette_main(LV_PALETTE_GREEN), 0);
            } else {
                lv_obj_set_style_bg_color(effect->status_indicator, lv_palette_main(LV_PALETTE_RED), 0);
            }
        }
        /* 更新效果链显示 */
        update_effect_chain_display();
    }
    
    show_notification(effect->enabled ? "Effect enabled" : "Effect disabled",
                     lv_palette_main(LV_PALETTE_BLUE));
}

/**
 * @brief 更新效果链显示 - 添加对象有效性检查
 */
static void update_effect_chain_display(void)
{
    if (!app_ctx->chain_label || !lv_obj_is_valid(app_ctx->chain_label)) {
        return;
    }
    
    char chain_text[128] = "";
    int enabled_count = 0;
    
    for (int i = 0; i < 6; i++) {
        if (app_ctx->effects[i].enabled) {
            if (enabled_count > 0) {
                strcat(chain_text, " → ");
            }
            strcat(chain_text, app_ctx->effects[i].name);
            enabled_count++;
        }
    }
    
    if (enabled_count == 0) {
        strcpy(chain_text, "None");
    }
    
    lv_label_set_text(app_ctx->chain_label, chain_text);
}

/**
 * @brief 定时器回调函数
 */
static void file_manager_timer_cb(lv_timer_t *timer)
{
    (void)timer;
}

/**
 * @brief 定时器回调函数 - 更新进度条和时间显示
 */
static void audio_player_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx || !ctx->timer_running || !ctx->is_playing) return;

    static int progress = 0;
    progress = (progress + 1) % 101;
    lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);

    int total = 180; // 假设歌曲总时长为3分钟
    int current = (progress * total) / 100;
    lv_label_set_text_fmt(ctx->time_label, "%02d:%02d/%02d:%02d", 
                          current / 60, current % 60,
                          total / 60, total % 60);
}

static void audio_processor_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    if (!ctx || !ctx->timer_running) return;
}

/**
 * @brief 加载目录内容（文件管理器专用）
 */
static void load_directory(const char *path, lv_obj_t *list)
{
    DIR *dir = opendir(path);
    if (!dir) {
        show_notification("Cannot open directory", lv_palette_main(LV_PALETTE_RED));
        return;
    }

    lv_obj_clean(list);
    app_ctx->file_count = 0;

    /* 添加上级目录 */
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

            const char *icon = file->is_dir ? LV_SYMBOL_DIRECTORY : 
                              (strstr(entry->d_name, ".mp3") ? LV_SYMBOL_AUDIO : LV_SYMBOL_FILE);

            lv_obj_t *btn = lv_list_add_btn(list, icon, entry->d_name);
            lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, 
                               (void *)(intptr_t)app_ctx->file_count);
            
            app_ctx->file_count++;
        }
    }

    closedir(dir);

    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, "Folder is empty");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }

    show_notification("Directory loaded", lv_palette_main(LV_PALETTE_GREEN));
}

/**
 * @brief 加载音频文件（播放器专用）- 只修改歌曲名字颜色
 */
static void load_audio_files(const char *path, lv_obj_t *list)
{
    DIR *dir = opendir(path);
    if (!dir) {
        show_notification("Cannot open directory", lv_palette_main(LV_PALETTE_RED));
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

        if (stat(full_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".mp3") == 0 || 
                        strcasecmp(ext, ".wav") == 0 || 
                        strcasecmp(ext, ".flac") == 0)) {
                
                file_info_t *file = &app_ctx->files[app_ctx->file_count];
                strcpy(file->name, entry->d_name);
                strcpy(file->path, full_path);
                file->size = st.st_size;

                /* 创建自定义列表项 */
                lv_obj_t *btn = lv_btn_create(list);
                lv_obj_set_size(btn, LV_PCT(100), 45);
                lv_obj_set_style_border_width(btn, 0, 0);
                lv_obj_set_style_radius(btn, 5, 0);
                lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_40, 0);
                lv_obj_set_style_pad_all(btn, 5, 0);
                lv_obj_add_event_cb(btn, on_audio_file_click, LV_EVENT_CLICKED, 
                                   (void *)(intptr_t)app_ctx->file_count);

                /* 图标 */
                lv_obj_t *icon = lv_label_create(btn);
                lv_label_set_text(icon, LV_SYMBOL_AUDIO);
                lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(icon, lv_palette_main(LV_PALETTE_BLUE), 0);
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);

                /* 文件名 - 修改这里：将颜色改为白色 */
                lv_obj_t *name_label = lv_label_create(btn);
                char display_name[32];
                if (strlen(entry->d_name) > 25) {
                    strncpy(display_name, entry->d_name, 22);
                    strcpy(display_name + 22, "...");
                } else {
                    strcpy(display_name, entry->d_name);
                }
                lv_label_set_text(name_label, display_name);
                lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, 0);
                /* 修改这行：将颜色改为白色 */
                lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
                lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 35, 0);

                /* 文件大小 - 保持不变 */
                lv_obj_t *size_label = lv_label_create(btn);
                char size_str[16];
                if (file->size < 1024) {
                    sprintf(size_str, "%dB", file->size);
                } else if (file->size < 1024 * 1024) {
                    sprintf(size_str, "%.1fKB", file->size / 1024.0);
                } else {
                    sprintf(size_str, "%.1fMB", file->size / (1024.0 * 1024.0));
                }
                lv_label_set_text(size_label, size_str);
                lv_obj_set_style_text_font(size_label, &lv_font_montserrat_10, 0);
                lv_obj_set_style_text_color(size_label, lv_palette_main(LV_PALETTE_GREY), 0);
                lv_obj_align(size_label, LV_ALIGN_RIGHT_MID, -10, 0);
                
                app_ctx->file_count++;
            }
        }
    }

    closedir(dir);

    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, "No audio files found");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_center(label);
    } else {
        /* 文件计数信息 */
        char count_str[32];
        sprintf(count_str, "%d files loaded", app_ctx->file_count);
        show_notification(count_str, lv_palette_main(LV_PALETTE_GREEN));
    }
}

/**
 * @brief 应用点击事件 - 简化版本
 */
static void on_app_click(lv_event_t *e)
{
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    
    /* 简单的点击反馈 - 只改变颜色，不做复杂动画 */
    lv_obj_t *card = lv_event_get_current_target(e);
    
    /* 直接切换页面，不等待动画 */
    create_app_screen(app_type);
}

static void on_back_click(lv_event_t *e)
{
    (void)e;
    create_main_screen();
}

static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (file_index == -1) {
        char *last_slash = strrchr(app_ctx->current_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (strlen(app_ctx->current_path) == 0) {
                strcpy(app_ctx->current_path, "./");
            }
        }
        load_directory(app_ctx->current_path, app_ctx->screen.list);
        return;
    }

    file_info_t *file = &app_ctx->files[file_index];
    
    if (file->is_dir) {
        strcpy(app_ctx->current_path, file->path);
        load_directory(app_ctx->current_path, app_ctx->screen.list);
    } else {
        static const char *btns[] = {"Confirm", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", 
                                          file->name, btns, true);
        lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, 
                           (void *)(intptr_t)file_index);
        lv_obj_center(mbox);
    }
}

/**
 * @brief 音频文件点击事件 - 更新显示
 */
static void on_audio_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    if (file_index < 0 || file_index >= app_ctx->file_count) return;
    
    file_info_t *file = &app_ctx->files[file_index];

    /* 更新当前播放显示 */
    char now_playing[128];
    snprintf(now_playing, sizeof(now_playing), "%s", file->name);
    lv_label_set_text(app_ctx->now_playing_label, now_playing);
    
    /* 显示通知 */
    show_notification("Playing: " LV_SYMBOL_PLAY, lv_palette_main(LV_PALETTE_GREEN));

    /* 更新播放状态 */
    app_ctx->current_track = file_index;
    app_ctx->is_playing = 1;

    /* 更新播放按钮图标 */
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    
    /* 重置进度条 */
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(app_ctx->time_label, "00:00/03:00");
}

static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);

    if (lv_msgbox_get_active_btn(mbox) == 0) {
        if (remove(app_ctx->files[file_index].path) == 0) {
            load_directory(app_ctx->current_path, app_ctx->screen.list);
            show_notification("File deleted", lv_palette_main(LV_PALETTE_GREEN));
        } else {
            show_notification("Delete failed", lv_palette_main(LV_PALETTE_RED));
        }
    }

    lv_msgbox_close(mbox);
}

/**
 * @brief 播放/暂停按钮点击事件
 */
static void on_play_click(lv_event_t *e)
{
    if (app_ctx->current_track < 0 || app_ctx->current_track >= app_ctx->file_count) {
        show_notification("No track selected", lv_palette_main(LV_PALETTE_RED));
        return;
    }

    app_ctx->is_playing = !app_ctx->is_playing;

    lv_obj_t *btn = lv_event_get_current_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text(label, app_ctx->is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    /* 更新播放图标颜色 */
    if (app_ctx->is_playing) {
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    }

    show_notification(app_ctx->is_playing ? "Playing" : "Paused",
                     lv_palette_main(LV_PALETTE_BLUE));
}
/**
 * @brief 停止按钮点击事件
 */
static void on_stop_click(lv_event_t *e)
{
    (void)e;
    app_ctx->is_playing = 0;
    
    /* 重置播放状态 */
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_ON);
    lv_label_set_text(app_ctx->time_label, "00:00/03:00");
    lv_label_set_text(app_ctx->now_playing_label, "Not playing");

    /* 更新播放按钮 */
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_bg_color(app_ctx->play_btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    show_notification("Stopped", lv_palette_main(LV_PALETTE_BLUE));
}

static void on_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(value_label, "%d", (int)value);
}

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

    lv_timer_t *timer = lv_timer_create(NULL, 2000, notif);
    lv_timer_set_repeat_count(timer, 1);
}

static void cleanup_app(void)
{
    if (app_ctx->app_timer) {
        app_ctx->timer_running = 0;
        lv_timer_del(app_ctx->app_timer);
        app_ctx->app_timer = NULL;
    }

    if (app_ctx->screen.screen) {
        lv_obj_del_async(app_ctx->screen.screen);
        app_ctx->screen.screen = NULL;
    }

    app_ctx->is_playing = 0;
    app_ctx->current_app = APP_NONE;
}