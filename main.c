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

/* 弹窗滑块数据结构 */
typedef struct {
    int effect_index;
    int param_index;
    lv_obj_t *value_label;
} param_slider_data_t;

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
    lv_obj_t *chain_label;  // 效果链显示标签
    
    /* 定时器管理 */
    lv_timer_t *app_timer;
    int timer_running;
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
static void on_play_click(lv_event_t *e);
static void on_stop_click(lv_event_t *e);
static void on_slider_change(lv_event_t *e);
static void show_notification(const char *msg, lv_color_t color);
static void cleanup_app(void);

/* 新添加的函数声明 */
static void create_parameter_dialog(int effect_index);
static void on_dialog_close(lv_event_t *e);
static void on_dialog_slider_change(lv_event_t *e);
static void on_effect_switch(lv_event_t *e);
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
    strcpy(app_ctx->current_path, "./");
    app_ctx->main_screen = NULL;

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
 * @brief 创建主屏幕
 */
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_scr_load(app_ctx->main_screen);
    
    /* 标题 */
    CREATE_LABEL(app_ctx->main_screen, "Audio File Processor", &lv_font_montserrat_16, 
                 LV_ALIGN_TOP_MID, 0, 10);

    /* 按钮容器 */
    lv_obj_t *btn_cont = lv_obj_create(app_ctx->main_screen);
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
        lv_obj_t *btn = CREATE_BTN(btn_cont, BUTTON_WIDTH, BUTTON_HEIGHT, 
                                   on_app_click, (void *)(intptr_t)app_btns[i].type);
        
        char btn_text[64];
        snprintf(btn_text, sizeof(btn_text), "%s %s", app_btns[i].symbol, app_btns[i].text);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, btn_text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);
    }
}

/**
 * @brief 创建应用屏幕（通用入口）
 */
static void create_app_screen(app_type_t app_type)
{
    cleanup_app();
    
    app_ctx->current_app = app_type;
    app_ctx->screen.screen = lv_obj_create(NULL);
    
    const char *titles[] = {
        [APP_FILE_MANAGER] = "File Manager",
        [APP_AUDIO_PLAYER] = "Audio Player",
        [APP_AUDIO_PROCESSOR] = "Audio Processor"
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
                                    on_back_click, NULL);
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
 * @brief 设置音频播放器界面
 */
static void setup_audio_player_screen(void)
{
    lv_obj_set_flex_flow(app_ctx->screen.main_cont, LV_FLEX_FLOW_COLUMN);

    /* 播放列表标签 */
    CREATE_LABEL(app_ctx->screen.main_cont, "Playlist:", 
                 &lv_font_montserrat_14, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 播放列表 */
    app_ctx->screen.list = lv_list_create(app_ctx->screen.main_cont);
    lv_obj_set_size(app_ctx->screen.list, LV_PCT(100), 180);
    lv_obj_set_style_border_width(app_ctx->screen.list, 1, 0);
    lv_obj_set_style_border_color(app_ctx->screen.list, 
                                  lv_palette_main(LV_PALETTE_GREY), 0);

    /* 当前播放信息 */
    app_ctx->now_playing_label = CREATE_LABEL(app_ctx->screen.main_cont, "Not playing",
                                              &lv_font_montserrat_14, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_width(app_ctx->now_playing_label, LV_PCT(100));
    lv_obj_set_style_text_align(app_ctx->now_playing_label, LV_TEXT_ALIGN_CENTER, 0);

    /* 控制区容器 */
    lv_obj_t *ctrl_cont = lv_obj_create(app_ctx->screen.main_cont);
    lv_obj_set_size(ctrl_cont, LV_PCT(100), 120);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_bg_opa(ctrl_cont, LV_OPA_TRANSP, 0);

    /* 进度条区域 */
    lv_obj_t *progress_cont = lv_obj_create(ctrl_cont);
    lv_obj_set_size(progress_cont, LV_PCT(100), 40);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_border_width(progress_cont, 0, 0);
    lv_obj_set_style_bg_opa(progress_cont, LV_OPA_TRANSP, 0);

    app_ctx->progress_bar = lv_bar_create(progress_cont);
    lv_obj_set_size(app_ctx->progress_bar, LV_PCT(80), 10);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    
    app_ctx->time_label = CREATE_LABEL(progress_cont, "00:00/00:00",
                                       &lv_font_montserrat_12, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(app_ctx->time_label, LV_PCT(18));

    /* 按钮区域 */
    lv_obj_t *btn_cont = lv_obj_create(ctrl_cont);
    lv_obj_set_size(btn_cont, LV_PCT(100), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);

    /* 播放/暂停按钮 */
    app_ctx->play_btn = CREATE_BTN(btn_cont, 80, 50, on_play_click, NULL);
    lv_obj_t *play_label = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_label);

    /* 停止按钮 */
    lv_obj_t *stop_btn = CREATE_BTN(btn_cont, 80, 50, on_stop_click, NULL);
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP);
    lv_obj_center(stop_label);

    /* 启动定时器 */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_player_timer_cb, 100, app_ctx);
    
    load_audio_files("./", app_ctx->screen.list);
}

/**
 * @brief 设置音频处理器界面 - 弹出窗口参数调节版本
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
        lv_obj_set_user_data(btn, status_indicator);
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
 * @brief 创建参数调节弹窗
 */
static void create_parameter_dialog(int effect_index)
{
    effect_t *effect = &app_ctx->effects[effect_index];
    
    /* 创建弹窗 */
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, 380, 400);
    lv_obj_center(dialog);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(dialog, 15, 0);
    lv_obj_set_style_shadow_width(dialog, 20, 0);
    lv_obj_set_style_bg_color(dialog, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_pad_all(dialog, 15, 0);
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text_fmt(title, "Configure: %s", effect->name);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* 关闭按钮 */
    lv_obj_t *close_btn = lv_btn_create(dialog);
    lv_obj_set_size(close_btn, 30, 30);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(close_btn, on_dialog_close, LV_EVENT_CLICKED, dialog);
    
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);

    /* 获取效果器参数配置 */
    int param_count = 0;
    const effect_config_t *config = NULL;
    
    for (size_t i = 0; i < sizeof(effect_presets) / sizeof(effect_presets[0]); i++) {
        if (effect_presets[i].type == effect->type) {
            config = &effect_presets[i];
            for (int j = 0; j < 3; j++) {
                if (strlen(effect_presets[i].param_names[j]) > 0) {
                    param_count++;
                }
            }
            break;
        }
    }

    /* 参数容器 */
    lv_obj_t *params_cont = lv_obj_create(dialog);
    lv_obj_set_size(params_cont, LV_PCT(100), 260);
    lv_obj_align(params_cont, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_border_width(params_cont, 0, 0);
    lv_obj_set_style_bg_opa(params_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(params_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(params_cont, 15, 0);

    /* 创建参数滑块 */
    int param_values[3] = {effect->param1, effect->param2, effect->param3};
    
    for (int i = 0; i < param_count; i++) {
        lv_obj_t *param_cont = lv_obj_create(params_cont);
        lv_obj_set_size(param_cont, LV_PCT(100), 70);
        lv_obj_set_style_border_width(param_cont, 1, 0);
        lv_obj_set_style_border_color(param_cont, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_radius(param_cont, 8, 0);
        lv_obj_set_style_pad_all(param_cont, 8, 0);

        /* 参数名 */
        lv_obj_t *name_label = lv_label_create(param_cont);
        lv_label_set_text(name_label, config->param_names[i]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 0, 0);

        /* 参数值 */
        lv_obj_t *value_label = lv_label_create(param_cont);
        lv_label_set_text_fmt(value_label, "%d", param_values[i]);
        lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
        lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, 0);

        /* 滑块 */
        lv_obj_t *slider = lv_slider_create(param_cont);
        lv_obj_set_size(slider, LV_PCT(100), 10);
        lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, param_values[i], LV_ANIM_OFF);
        
        /* 存储参数信息 */
        param_slider_data_t *slider_data = (param_slider_data_t *)malloc(sizeof(param_slider_data_t));
        slider_data->effect_index = effect_index;
        slider_data->param_index = i;
        slider_data->value_label = value_label;
        
        lv_obj_add_event_cb(slider, on_dialog_slider_change, LV_EVENT_VALUE_CHANGED, slider_data);
    }

    /* 启用/禁用开关 */
    lv_obj_t *switch_cont = lv_obj_create(dialog);
    lv_obj_set_size(switch_cont, LV_PCT(100), 50);
    lv_obj_align(switch_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_set_style_bg_opa(switch_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *switch_label = lv_label_create(switch_cont);
    lv_label_set_text(switch_label, "Enable Effect");
    lv_obj_set_style_text_font(switch_label, &lv_font_montserrat_14, 0);

    lv_obj_t *switch_btn = lv_switch_create(switch_cont);
    lv_obj_set_size(switch_btn, 50, 25);
    if (effect->enabled) {
        lv_obj_add_state(switch_btn, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(switch_btn, on_effect_switch, LV_EVENT_VALUE_CHANGED, 
                       (void *)(intptr_t)effect_index);
}

/**
 * @brief 效果器点击事件处理 - 弹出参数调节窗口
 */
static void on_effect_click(lv_event_t *e)
{
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    create_parameter_dialog(effect_index);
}

/**
 * @brief 弹窗关闭事件
 */
static void on_dialog_close(lv_event_t *e)
{
    lv_obj_t *dialog = (lv_obj_t *)lv_event_get_user_data(e);
    
    /* 更新效果链显示 */
    update_effect_chain_display();
    
    lv_obj_del_async(dialog);
}

/**
 * @brief 弹窗内滑块值改变事件
 */
static void on_dialog_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    param_slider_data_t *data = (param_slider_data_t *)lv_event_get_user_data(e);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(data->value_label, "%d", (int)value);
    
    /* 更新效果器参数 */
    effect_t *effect = &app_ctx->effects[data->effect_index];
    switch (data->param_index) {
        case 0: effect->param1 = value; break;
        case 1: effect->param2 = value; break;
        case 2: effect->param3 = value; break;
    }
}

/**
 * @brief 效果器开关事件
 */
static void on_effect_switch(lv_event_t *e)
{
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *switch_btn = lv_event_get_current_target(e);
    
    effect_t *effect = &app_ctx->effects[effect_index];
    effect->enabled = (lv_obj_get_state(switch_btn) & LV_STATE_CHECKED) != 0;
    
    /* 更新效果器框的状态指示器 */
    lv_obj_t *status_indicator = (lv_obj_t *)lv_obj_get_user_data(effect->btn);
    if (effect->enabled) {
        lv_obj_set_style_bg_color(status_indicator, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_obj_set_style_bg_color(status_indicator, lv_palette_main(LV_PALETTE_RED), 0);
    }
    
    /* 更新效果链显示 */
    update_effect_chain_display();
}

/**
 * @brief 更新效果链显示
 */
static void update_effect_chain_display(void)
{
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
    
    /* 更新显示 */
    if (app_ctx->chain_label) {
        lv_label_set_text(app_ctx->chain_label, chain_text);
    }
}

/**
 * @brief 定时器回调函数
 */
static void file_manager_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    /* 文件系统监控逻辑可以在这里添加 */
}

static void audio_player_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx || !ctx->timer_running || !ctx->is_playing) return;

    static int progress = 0;
    progress = (progress + 1) % 101;
    lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);

    int total = 180;
    int current = (progress * total) / 100;
    lv_label_set_text_fmt(ctx->time_label, "%02d:%02d/03:00", 
                          current / 60, current % 60);
}

static void audio_processor_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    if (!ctx || !ctx->timer_running) return;

    static int buffer_index = 0;
    buffer_index = !buffer_index;

    /* 音频处理逻辑 */
    for (int i = 0; i < ctx->effect_count; i++) {
        if (ctx->effects[i].enabled) {
            /* 根据效果器类型处理音频 */
        }
    }
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
 * @brief 加载音频文件（播放器专用）
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

                lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, entry->d_name);
                lv_obj_add_event_cb(btn, on_audio_file_click, LV_EVENT_CLICKED, 
                                   (void *)(intptr_t)app_ctx->file_count);
                
                app_ctx->file_count++;
            }
        }
    }

    closedir(dir);

    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, "No audio files found");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }
}

/**
 * @brief 事件处理函数
 */
static void on_app_click(lv_event_t *e)
{
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    create_app_screen(app_type);
}

static void on_back_click(lv_event_t *e)
{
    (void)e;
    cleanup_app();
    
    if (app_ctx->main_screen) {
        lv_scr_load(app_ctx->main_screen);
    }
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

static void on_audio_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    file_info_t *file = &app_ctx->files[file_index];

    char now_playing[128];
    snprintf(now_playing, sizeof(now_playing), "Playing: %s", file->name);
    lv_label_set_text(app_ctx->now_playing_label, now_playing);
    
    show_notification(now_playing, lv_palette_main(LV_PALETTE_GREEN));

    app_ctx->current_track = file_index;
    app_ctx->is_playing = 1;

    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
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

    show_notification(app_ctx->is_playing ? "Playing" : "Paused",
                     lv_palette_main(LV_PALETTE_BLUE));
}

static void on_stop_click(lv_event_t *e)
{
    (void)e;
    app_ctx->is_playing = 0;
    app_ctx->current_track = -1;

    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_ON);
    lv_label_set_text(app_ctx->time_label, "00:00/00:00");
    lv_label_set_text(app_ctx->now_playing_label, "Not playing");

    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);

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