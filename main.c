/**
 * @file main.c
 * @brief 音频文件处理器 - 修复布局和英文版本
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
#define AUDIO_BUFFER_SIZE 2048
#define NAV_BAR_HEIGHT 50
#define CONTENT_START_Y NAV_BAR_HEIGHT
#define CONTENT_HEIGHT (SCREEN_HEIGHT - NAV_BAR_HEIGHT - 10)

/* 颜色定义 */
#define COLOR_BG lv_color_hex(0xF5F5F5)
#define COLOR_CARD lv_color_hex(0xFFFFFF)
#define COLOR_PRIMARY lv_color_hex(0x2196F3)
#define COLOR_PRIMARY_DARK lv_color_hex(0x1976D2)
#define COLOR_SECONDARY lv_color_hex(0x4CAF50)
#define COLOR_ACCENT lv_color_hex(0xFF4081)
#define COLOR_WARNING lv_color_hex(0xFF9800)
#define COLOR_DANGER lv_color_hex(0xF44336)
#define COLOR_TEXT_PRIMARY lv_color_hex(0x212121)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x757575)
#define COLOR_DIVIDER lv_color_hex(0xE0E0E0)

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
    char display_name[128];
    int is_dir;
    int size;
    int is_audio;
} file_info_t;

/* 效果器参数结构体 */
typedef struct {
    effect_type_t type;
    int enabled;
    int order;
    float params[5];
    char name[20];
    lv_color_t color;
    lv_obj_t *card;
} effect_t;

/* 应用上下文结构体 */
typedef struct {
    app_type_t current_app;
    lv_obj_t *main_screen;
    lv_obj_t *app_screen;
    lv_obj_t *effect_config_screen;
    
    /* 文件管理器相关 */
    file_info_t files[MAX_FILES];
    int file_count;
    char current_path[MAX_PATH];
    lv_obj_t *file_list;
    lv_obj_t *path_label;
    
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
    
    /* 音频处理器相关 */
    effect_t effects[MAX_EFFECTS];
    int effect_count;
    int selected_effect;
    
    /* 定时器 */
    lv_timer_t *ui_timer;
} app_context_t;

/**********************
 *      静态变量
 **********************/
static app_context_t *app_ctx = NULL;
static lv_style_t style_card;
static lv_style_t style_btn;
static lv_style_t style_btn_small;
static lv_style_t style_scrollbar;
static lv_style_t style_nav_btn;
static lv_style_t style_title;

/* 效果器名称映射 - 英文 */
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
static void hal_init(void);
static void init_styles(void);
static void create_main_screen(void);

/* 文件管理器函数 */
static void create_file_manager_screen(void);
static void load_all_files(const char *path);
static void update_file_list_display(void);
static void show_delete_dialog(int file_index);
static void on_file_click(lv_event_t *e);
static void on_delete_confirm(lv_event_t *e);
static void on_delete_cancel(lv_event_t *e);
static void on_navigate_up(lv_event_t *e);

/* 音频播放器函数 */
static void create_audio_player_screen(void);
static void load_audio_files(const char *path);
static void play_audio_file(int index);
static void stop_audio_playback(void);
static void on_audio_file_click(lv_event_t *e);
static void on_play_click(lv_event_t *e);
static void on_stop_click(lv_event_t *e);
static void on_prev_click(lv_event_t *e);
static void on_next_click(lv_event_t *e);

/* 音频处理器函数 */
static void create_audio_processor_screen(void);
static void create_effect_config_screen(int effect_idx);
static void update_effect_card(int idx);
static void on_effect_card_click(lv_event_t *e);
static void on_effect_toggle(lv_event_t *e);
static void on_effect_type_select(lv_event_t *e);
static void on_effect_param_change(lv_event_t *e);
static void on_effect_order_change(lv_event_t *e);

/* 通用函数 */
static void create_nav_bar(lv_obj_t *parent, const char *title);
static lv_obj_t* create_content_area(lv_obj_t *parent, int height);
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

    /* 初始化效果器颜色映射 */
    effect_colors[0] = COLOR_TEXT_SECONDARY;
    effect_colors[1] = lv_color_hex(0x9C27B0);  /* 紫色 */
    effect_colors[2] = lv_color_hex(0x00BCD4);  /* 青色 */
    effect_colors[3] = lv_color_hex(0xFF5722);  /* 橙色 */
    effect_colors[4] = lv_color_hex(0x3F51B5);  /* 靛蓝 */
    effect_colors[5] = lv_color_hex(0xE91E63);  /* 粉色 */
    effect_colors[6] = lv_color_hex(0x009688);  /* 蓝绿 */
    effect_colors[7] = lv_color_hex(0xFFC107);  /* 琥珀 */
    effect_colors[8] = lv_color_hex(0x8BC34A);  /* 浅绿 */
    effect_colors[9] = lv_color_hex(0x673AB7);  /* 深紫 */

    lv_init();
    hal_init();
    init_styles();

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
        strcpy(app_ctx->effects[i].name, effect_names[default_types[i]]);
        app_ctx->effects[i].color = effect_colors[default_types[i]];
    }

    create_main_screen();

    app_ctx->ui_timer = lv_timer_create(ui_timer_cb, 100, app_ctx);

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
    
    /* 注册显示驱动并获取disp指针 */
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* 设置默认主题 - 使用上面注册的disp */
    lv_theme_t *th = lv_theme_default_init(disp, 
        COLOR_PRIMARY, COLOR_ACCENT, 0, &lv_font_montserrat_14);
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
    lv_style_set_border_color(&style_card, COLOR_DIVIDER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_shadow_width(&style_card, 4);
    lv_style_set_shadow_ofs_y(&style_card, 2);
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);
    lv_style_set_pad_all(&style_card, 8);
    
    /* 主按钮样式 */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, COLOR_PRIMARY);
    lv_style_set_bg_grad_color(&style_btn, COLOR_PRIMARY_DARK);
    lv_style_set_bg_grad_dir(&style_btn, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_radius(&style_btn, 4);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_pad_hor(&style_btn, 12);
    lv_style_set_pad_ver(&style_btn, 6);
    lv_style_set_shadow_width(&style_btn, 2);
    lv_style_set_shadow_ofs_y(&style_btn, 1);
    
    /* 小按钮样式 */
    lv_style_init(&style_btn_small);
    lv_style_set_bg_color(&style_btn_small, COLOR_CARD);
    lv_style_set_bg_opa(&style_btn_small, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_small, 4);
    lv_style_set_text_color(&style_btn_small, COLOR_TEXT_PRIMARY);
    lv_style_set_pad_hor(&style_btn_small, 8);
    lv_style_set_pad_ver(&style_btn_small, 4);
    lv_style_set_border_width(&style_btn_small, 1);
    lv_style_set_border_color(&style_btn_small, COLOR_DIVIDER);
    
    /* 导航按钮样式 */
    lv_style_init(&style_nav_btn);
    lv_style_set_bg_color(&style_nav_btn, COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_nav_btn, LV_OPA_20);
    lv_style_set_radius(&style_nav_btn, 20);
    lv_style_set_text_color(&style_nav_btn, lv_color_white());
    lv_style_set_shadow_width(&style_nav_btn, 2);
    lv_style_set_shadow_ofs_y(&style_nav_btn, 1);
    
    /* 标题样式 */
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_title, COLOR_TEXT_PRIMARY);
    
    /* 滚动条样式 */
    lv_style_init(&style_scrollbar);
    lv_style_set_bg_color(&style_scrollbar, COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_scrollbar, LV_OPA_40);
    lv_style_set_width(&style_scrollbar, 4);
    lv_style_set_radius(&style_scrollbar, 2);
}

/**********************
 *      通用UI组件函数
 **********************/

/* 创建导航栏 - 返回按钮左对齐，标题居中 */
static void create_nav_bar(lv_obj_t *parent, const char *title)
{
    /* 导航栏容器 */
    lv_obj_t *nav_bar = lv_obj_create(parent);
    lv_obj_set_size(nav_bar, LV_PCT(100), NAV_BAR_HEIGHT);
    lv_obj_set_pos(nav_bar, 0, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_bg_color(nav_bar, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_10, 0);
    lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(nav_bar, LV_SCROLLBAR_MODE_OFF);
    
    /* 返回按钮 - 左对齐 */
    lv_obj_t *back_btn = lv_btn_create(nav_bar);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_add_style(back_btn, &style_nav_btn, 0);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(back_btn, LV_SCROLLBAR_MODE_OFF);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_20, 0);
    lv_obj_center(back_icon);
    
    /* 标题 - 绝对居中 */
    lv_obj_t *title_label = lv_label_create(nav_bar);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, &style_title, 0);
    lv_obj_center(title_label);
    
    /* 装饰线 */
    lv_obj_t *line = lv_obj_create(nav_bar);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_set_pos(line, 0, NAV_BAR_HEIGHT - 1);
    lv_obj_set_style_bg_color(line, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

/* 创建内容区域 - 可滚动 */
static lv_obj_t* create_content_area(lv_obj_t *parent, int height)
{
    lv_obj_t *content = lv_obj_create(parent);
    lv_obj_set_size(content, SCREEN_WIDTH, height);
    lv_obj_set_pos(content, 0, CONTENT_START_Y);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    
    /* 启用滚动 */
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_add_style(content, &style_scrollbar, LV_PART_SCROLLBAR);
    
    return content;
}

/**********************
 *      主屏幕创建
 **********************/
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->main_screen, COLOR_BG, 0);
    lv_obj_clear_flag(app_ctx->main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(app_ctx->main_screen, LV_SCROLLBAR_MODE_OFF);
    lv_scr_load(app_ctx->main_screen);
    
    /* 标题 */
    lv_obj_t *title = lv_label_create(app_ctx->main_screen);
    lv_label_set_text(title, "Audio File Processor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    /* 按钮容器 */
    lv_obj_t *btn_cont = lv_obj_create(app_ctx->main_screen);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_size(btn_cont, 400, 300);
    lv_obj_center(btn_cont);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_cont, 20, 0);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 文件管理器按钮 */
    lv_obj_t *btn1 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn1, &style_btn, 0);
    lv_obj_set_size(btn1, 220, 60);
    lv_obj_add_event_cb(btn1, on_app_click, LV_EVENT_CLICKED, (void *)APP_FILE_MANAGER);
    
    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, LV_SYMBOL_DIRECTORY " File Manager");
    lv_obj_center(label1);
    
    /* 音频播放器按钮 */
    lv_obj_t *btn2 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn2, &style_btn, 0);
    lv_obj_set_size(btn2, 220, 60);
    lv_obj_add_event_cb(btn2, on_app_click, LV_EVENT_CLICKED, (void *)APP_AUDIO_PLAYER);
    
    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, LV_SYMBOL_PLAY " Audio Player");
    lv_obj_center(label2);
    
    /* 音频处理器按钮 */
    lv_obj_t *btn3 = lv_btn_create(btn_cont);
    lv_obj_add_style(btn3, &style_btn, 0);
    lv_obj_set_size(btn3, 220, 60);
    lv_obj_add_event_cb(btn3, on_app_click, LV_EVENT_CLICKED, (void *)APP_AUDIO_PROCESSOR);
    
    lv_obj_t *label3 = lv_label_create(btn3);
    lv_label_set_text(label3, LV_SYMBOL_SETTINGS " Audio Processor");
    lv_obj_center(label3);
}

/**********************
 *      音频处理器 - 修复布局版本
 **********************/
static void create_audio_processor_screen(void)
{
    free_app_resources();
    
    app_ctx->current_app = APP_AUDIO_PROCESSOR;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_obj_clear_flag(app_ctx->app_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(app_ctx->app_screen, LV_SCROLLBAR_MODE_OFF);
    lv_scr_load(app_ctx->app_screen);
    
    /* 1. 创建导航栏 */
    create_nav_bar(app_ctx->app_screen, "Audio Processor");
    
    /* 2. 创建可滚动的内容区域 */
    lv_obj_t *content = create_content_area(app_ctx->app_screen, CONTENT_HEIGHT);
    
    /* 3. 效果器网格标题 - 英文 */
    lv_obj_t *grid_label = lv_label_create(content);
    lv_label_set_text(grid_label, "Effect Chain");
    lv_obj_set_style_text_font(grid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(grid_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_pos(grid_label, 10, 5);
    
    /* 创建效果器网格容器 - 使用表格布局确保对齐 */
    lv_obj_t *grid_cont = lv_obj_create(content);
    lv_obj_set_size(grid_cont, 430, 240);
    lv_obj_set_pos(grid_cont, 10, 30);
    lv_obj_set_style_border_width(grid_cont, 0, 0);
    lv_obj_set_style_bg_opa(grid_cont, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(grid_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 使用网格布局创建2行3列，修复对齐问题 */
    lv_obj_set_layout(grid_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid_cont, 10, 0);
    lv_obj_set_style_pad_column(grid_cont, 5, 0);
    
    /* 创建6个效果器卡片 - 使用精确尺寸确保不重叠 */
    for (int i = 0; i < MAX_EFFECTS; i++) {
        lv_obj_t *card = lv_obj_create(grid_cont);
        lv_obj_add_style(card, &style_card, 0);
        
        /* 计算卡片宽度：总宽度430，减去左右边距20，减去列间距10，除以3 = 133.33，取133 */
        lv_obj_set_size(card, 133, 105);
        lv_obj_set_style_pad_all(card, 5, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        
        /* 序号 - 左上角 */
        lv_obj_t *num = lv_label_create(card);
        lv_label_set_text_fmt(num, "#%d", i + 1);
        lv_obj_set_style_text_font(num, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(num, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_pos(num, 3, 3);
        
        /* 效果器图标 - 居中 */
        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, effect_icons[app_ctx->effects[i].type]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(icon, app_ctx->effects[i].color, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -10);
        
        /* 效果器名称 - 底部 */
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, app_ctx->effects[i].name);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(name, lv_pct(100));
        lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -15);
        
        /* 状态指示 - 右上角 */
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
        lv_obj_set_pos(status, 100, 3);
        
        app_ctx->effects[i].card = card;
        lv_obj_add_event_cb(card, on_effect_card_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    
    /* 4. 分隔线 */
    lv_obj_t *line1 = lv_obj_create(content);
    lv_obj_set_size(line1, 420, 1);
    lv_obj_set_pos(line1, 10, 280);
    lv_obj_set_style_bg_color(line1, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(line1, 0, 0);
    lv_obj_clear_flag(line1, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 5. 状态信息区域 - 英文 */
    lv_obj_t *status_label = lv_label_create(content);
    lv_label_set_text(status_label, "System Status");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_pos(status_label, 10, 295);
    
    /* 创建状态卡片 */
    lv_obj_t *status_card = lv_obj_create(content);
    lv_obj_set_size(status_card, 430, 80);
    lv_obj_set_pos(status_card, 10, 320);
    lv_obj_set_style_bg_color(status_card, COLOR_CARD, 0);
    lv_obj_set_style_radius(status_card, 4, 0);
    lv_obj_set_style_pad_all(status_card, 10, 0);
    lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 处理顺序提示 - 英文 */
    lv_obj_t *order_icon = lv_label_create(status_card);
    lv_label_set_text(order_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_pos(order_icon, 10, 5);
    
    lv_obj_t *order_text = lv_label_create(status_card);
    lv_label_set_text(order_text, "Processing order: 1 → 2 → 3 → 4 → 5 → 6");
    lv_obj_set_style_text_font(order_text, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(order_text, 35, 5);
    
    /* ADC状态 - 英文 */
    lv_obj_t *adc_icon = lv_label_create(status_card);
    lv_label_set_text(adc_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_pos(adc_icon, 10, 30);
    
    lv_obj_t *adc_text = lv_label_create(status_card);
    lv_label_set_text(adc_text, "ADC: Ready | Double buffer mode");
    lv_obj_set_style_text_font(adc_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(adc_text, COLOR_SECONDARY, 0);
    lv_obj_set_pos(adc_text, 35, 30);
    
    /* I2S状态 - 英文 */
    lv_obj_t *i2s_icon = lv_label_create(status_card);
    lv_label_set_text(i2s_icon, LV_SYMBOL_WIFI);
    lv_obj_set_pos(i2s_icon, 10, 55);
    
    lv_obj_t *i2s_text = lv_label_create(status_card);
    lv_label_set_text(i2s_text, "I2S: Running | 44.1kHz 16bit");
    lv_obj_set_style_text_font(i2s_text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(i2s_text, COLOR_SECONDARY, 0);
    lv_obj_set_pos(i2s_text, 35, 55);
    
    /* 添加一些底部空间确保滚动正常 */
    lv_obj_t *bottom_space = lv_obj_create(content);
    lv_obj_set_size(bottom_space, 430, 20);
    lv_obj_set_pos(bottom_space, 10, 410);
    lv_obj_set_style_bg_opa(bottom_space, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_space, 0, 0);
    lv_obj_clear_flag(bottom_space, LV_OBJ_FLAG_SCROLLABLE);
}

/* 效果器配置界面 - 修复布局，全部英文 */
static void create_effect_config_screen(int effect_idx)
{
    app_ctx->selected_effect = effect_idx;
    effect_t *effect = &app_ctx->effects[effect_idx];
    
    app_ctx->effect_config_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->effect_config_screen, COLOR_BG, 0);
    lv_obj_clear_flag(app_ctx->effect_config_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(app_ctx->effect_config_screen, LV_SCROLLBAR_MODE_OFF);
    lv_scr_load(app_ctx->effect_config_screen);
    
    /* 1. 创建导航栏 - 英文标题 */
    char title[64];
    snprintf(title, sizeof(title), "%s Settings", effect->name);
    create_nav_bar(app_ctx->effect_config_screen, title);
    
    /* 2. 创建可滚动的内容区域 */
    lv_obj_t *content = create_content_area(app_ctx->effect_config_screen, CONTENT_HEIGHT);
    
    /* 3. 效果器图标和开关 - 英文 */
    lv_obj_t *header = lv_obj_create(content);
    lv_obj_set_size(header, 430, 70);
    lv_obj_set_pos(header, 10, 10);
    lv_obj_set_style_bg_color(header, COLOR_CARD, 0);
    lv_obj_set_style_radius(header, 8, 0);
    lv_obj_set_style_pad_all(header, 10, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 大图标 */
    lv_obj_t *big_icon = lv_label_create(header);
    lv_label_set_text(big_icon, effect_icons[effect->type]);
    lv_obj_set_style_text_font(big_icon, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(big_icon, effect->color, 0);
    lv_obj_set_pos(big_icon, 15, 5);
    
    /* 效果器名称 */
    lv_obj_t *name_label = lv_label_create(header);
    lv_label_set_text(name_label, effect->name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(name_label, 80, 15);
    
    /* 启用开关 */
    lv_obj_t *toggle = lv_switch_create(header);
    lv_obj_set_size(toggle, 60, 30);
    lv_obj_set_pos(toggle, 340, 15);
    lv_obj_add_state(toggle, effect->enabled ? LV_STATE_CHECKED : 0);
    lv_obj_add_event_cb(toggle, on_effect_toggle, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)effect_idx);
    
    /* 开关状态文字 - 英文 */
    lv_obj_t *toggle_label = lv_label_create(header);
    lv_label_set_text(toggle_label, effect->enabled ? "Enabled" : "Disabled");
    lv_obj_set_style_text_font(toggle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(toggle_label, effect->enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(toggle_label, 350, 45);
    
    /* 4. 效果类型选择 - 英文 */
    lv_obj_t *type_section = lv_obj_create(content);
    lv_obj_set_size(type_section, 430, 100);
    lv_obj_set_pos(type_section, 10, 90);
    lv_obj_set_style_bg_color(type_section, COLOR_CARD, 0);
    lv_obj_set_style_radius(type_section, 8, 0);
    lv_obj_set_style_pad_all(type_section, 10, 0);
    lv_obj_clear_flag(type_section, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *type_title = lv_label_create(type_section);
    lv_label_set_text(type_title, "Effect Type");
    lv_obj_set_style_text_font(type_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(type_title, 10, 5);
    
    /* 类型按钮容器 - 使用flex布局 */
    lv_obj_t *type_cont = lv_obj_create(type_section);
    lv_obj_set_size(type_cont, 410, 50);
    lv_obj_set_pos(type_cont, 10, 30);
    lv_obj_set_style_border_width(type_cont, 0, 0);
    lv_obj_set_style_bg_opa(type_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(type_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(type_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(type_cont, 5, 0);
    lv_obj_set_style_pad_column(type_cont, 5, 0);
    lv_obj_clear_flag(type_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 创建类型选择按钮 */
    for (int i = 1; i < 10; i++) {
        lv_obj_t *btn = lv_btn_create(type_cont);
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 30);
        lv_obj_set_style_bg_color(btn, 
            effect->type == i ? effect_colors[i] : COLOR_CARD, 0);
        lv_obj_set_style_radius(btn, 15, 0);
        lv_obj_set_style_pad_hor(btn, 12, 0);
        lv_obj_add_event_cb(btn, on_effect_type_select, LV_EVENT_CLICKED, 
            (void *)(intptr_t)((effect_idx << 16) | i));
        
        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, effect_names[i]);
        lv_obj_set_style_text_color(btn_label, 
            effect->type == i ? lv_color_white() : COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(btn_label);
    }
    
    /* 5. 参数调节区域 - 英文 */
    lv_obj_t *param_section = lv_obj_create(content);
    lv_obj_set_size(param_section, 430, 150);
    lv_obj_set_pos(param_section, 10, 200);
    lv_obj_set_style_bg_color(param_section, COLOR_CARD, 0);
    lv_obj_set_style_radius(param_section, 8, 0);
    lv_obj_set_style_pad_all(param_section, 10, 0);
    lv_obj_clear_flag(param_section, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *param_title = lv_label_create(param_section);
    lv_label_set_text(param_title, "Parameters");
    lv_obj_set_style_text_font(param_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(param_title, 10, 5);
    
    /* 创建参数滑块 - 英文参数名 */
    const char *param_names[] = {"Intensity", "Mix", "Feedback", "Threshold", "Ratio"};
    for (int p = 0; p < 3; p++) {
        lv_obj_t *slider_cont = lv_obj_create(param_section);
        lv_obj_set_size(slider_cont, 410, 35);
        lv_obj_set_pos(slider_cont, 10, 35 + p * 40);
        lv_obj_set_style_border_width(slider_cont, 0, 0);
        lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(slider_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(slider_cont, LV_OBJ_FLAG_SCROLLABLE);
        
        /* 参数名称 */
        lv_obj_t *name = lv_label_create(slider_cont);
        lv_label_set_text(name, param_names[p]);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
        
        /* 滑块 */
        lv_obj_t *slider = lv_slider_create(slider_cont);
        lv_obj_set_size(slider, 200, 8);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, (int)effect->params[p], LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, on_effect_param_change, LV_EVENT_VALUE_CHANGED, 
            (void *)(intptr_t)((effect_idx << 16) | (p << 8) | p));
        
        /* 数值显示 */
        lv_obj_t *value = lv_label_create(slider_cont);
        lv_label_set_text_fmt(value, "%d%%", (int)effect->params[p]);
        lv_obj_set_style_text_font(value, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(value, COLOR_PRIMARY, 0);
        lv_obj_set_user_data(slider, value);
    }
    
    /* 6. 处理顺序 - 英文 */
    lv_obj_t *order_section = lv_obj_create(content);
    lv_obj_set_size(order_section, 430, 80);
    lv_obj_set_pos(order_section, 10, 360);
    lv_obj_set_style_bg_color(order_section, COLOR_CARD, 0);
    lv_obj_set_style_radius(order_section, 8, 0);
    lv_obj_set_style_pad_all(order_section, 10, 0);
    lv_obj_clear_flag(order_section, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *order_title = lv_label_create(order_section);
    lv_label_set_text(order_title, "Processing Order");
    lv_obj_set_style_text_font(order_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(order_title, 10, 5);
    
    lv_obj_t *order_roller = lv_roller_create(order_section);
    lv_roller_set_options(order_roller, "1\n2\n3\n4\n5\n6", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(order_roller, 80, 60);
    lv_obj_set_pos(order_roller, 10, 25);
    lv_roller_set_selected(order_roller, effect->order - 1, LV_ANIM_OFF);
    lv_obj_add_event_cb(order_roller, on_effect_order_change, LV_EVENT_VALUE_CHANGED, 
        (void *)(intptr_t)effect_idx);
    
    lv_obj_t *order_hint = lv_label_create(order_section);
    lv_label_set_text(order_hint, "Lower number = higher priority");
    lv_obj_set_style_text_font(order_hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(order_hint, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(order_hint, 100, 35);
    
    /* 底部空间 */
    lv_obj_t *bottom_space = lv_obj_create(content);
    lv_obj_set_size(bottom_space, 430, 20);
    lv_obj_set_pos(bottom_space, 10, 450);
    lv_obj_set_style_bg_opa(bottom_space, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_space, 0, 0);
    lv_obj_clear_flag(bottom_space, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_effect_card(int idx)
{
    if (!app_ctx->effects[idx].card) return;
    
    effect_t *effect = &app_ctx->effects[idx];
    lv_obj_t *card = effect->card;
    
    /* 更新图标 (第2个子对象) */
    lv_obj_t *icon = lv_obj_get_child(card, 1);
    if (icon) {
        lv_label_set_text(icon, effect_icons[effect->type]);
        lv_obj_set_style_text_color(icon, effect->color, 0);
    }
    
    /* 更新名称 (第3个子对象) */
    lv_obj_t *name = lv_obj_get_child(card, 2);
    if (name) {
        lv_label_set_text(name, effect->name);
    }
    
    /* 更新状态 (第4个子对象) */
    lv_obj_t *status = lv_obj_get_child(card, 3);
    if (status) {
        lv_label_set_text(status, effect->enabled ? "ON" : "OFF");
        lv_obj_set_style_text_color(status, 
            effect->enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_bg_color(status, 
            effect->enabled ? lv_color_lighten(COLOR_SECONDARY, LV_OPA_30) : COLOR_CARD, 0);
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
    
    /* 更新开关旁边的文字 - 英文 */
    lv_obj_t *parent = lv_obj_get_parent(sw);
    lv_obj_t *label = lv_obj_get_child(parent, 4);  /* 开关状态文字 */
    if (label) {
        lv_label_set_text(label, app_ctx->effects[effect_idx].enabled ? "Enabled" : "Disabled");
        lv_obj_set_style_text_color(label, 
            app_ctx->effects[effect_idx].enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 0);
    }
    
    show_toast(app_ctx->effects[effect_idx].enabled ? "Effect enabled" : "Effect disabled", 
               app_ctx->effects[effect_idx].enabled ? COLOR_SECONDARY : COLOR_TEXT_SECONDARY, 1000);
}

static void on_effect_type_select(lv_event_t *e)
{
    uint32_t data = (uint32_t)(intptr_t)lv_event_get_user_data(e);
    int effect_idx = data >> 16;
    int type = data & 0xFFFF;
    
    app_ctx->effects[effect_idx].type = type;
    strcpy(app_ctx->effects[effect_idx].name, effect_names[type]);
    app_ctx->effects[effect_idx].color = effect_colors[type];
    
    lv_obj_del_async(app_ctx->effect_config_screen);
    create_effect_config_screen(effect_idx);
    
    show_toast("Effect type changed", COLOR_PRIMARY, 1000);
}

static void on_effect_param_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(value_label, "%d%%", (int)value);
    
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
 *      文件管理器页面 - 英文版
 **********************/
static void create_file_manager_screen(void)
{
    free_app_resources();
    
    app_ctx->current_app = APP_FILE_MANAGER;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_obj_clear_flag(app_ctx->app_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(app_ctx->app_screen, LV_SCROLLBAR_MODE_OFF);
    lv_scr_load(app_ctx->app_screen);
    
    /* 创建导航栏 - 英文 */
    create_nav_bar(app_ctx->app_screen, "File Manager");
    
    /* 路径标签 */
    app_ctx->path_label = lv_label_create(app_ctx->app_screen);
    lv_label_set_text_fmt(app_ctx->path_label, "Current path: %s", app_ctx->current_path);
    lv_obj_set_style_text_font(app_ctx->path_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(app_ctx->path_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_pos(app_ctx->path_label, 10, NAV_BAR_HEIGHT + 5);
    lv_obj_set_size(app_ctx->path_label, 440, 20);
    lv_label_set_long_mode(app_ctx->path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    /* 可滚动的文件列表区域 */
    lv_obj_t *content = create_content_area(app_ctx->app_screen, CONTENT_HEIGHT - 30);
    lv_obj_set_pos(content, 0, NAV_BAR_HEIGHT + 30);
    
    /* 文件列表容器 */
    lv_obj_t *list_cont = lv_obj_create(content);
    lv_obj_set_size(list_cont, 440, 800);
    lv_obj_set_pos(list_cont, 0, 0);
    lv_obj_set_style_border_width(list_cont, 1, 0);
    lv_obj_set_style_border_color(list_cont, COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_color(list_cont, COLOR_CARD, 0);
    lv_obj_set_style_radius(list_cont, 8, 0);
    lv_obj_set_style_pad_all(list_cont, 5, 0);
    lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 文件列表 */
    app_ctx->file_list = lv_list_create(list_cont);
    lv_obj_set_size(app_ctx->file_list, 430, 780);
    lv_obj_set_style_border_width(app_ctx->file_list, 0, 0);
    lv_obj_set_style_bg_color(app_ctx->file_list, COLOR_CARD, 0);
    lv_obj_clear_flag(app_ctx->file_list, LV_OBJ_FLAG_SCROLLABLE);
    
    load_all_files(app_ctx->current_path);
}

/* 文件管理器相关函数 - 英文提示 */
static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (file_index < 0 || file_index >= app_ctx->file_count) return;
    
    file_info_t *file = &app_ctx->files[file_index];
    
    if (file->is_dir) {
        strcpy(app_ctx->current_path, file->path);
        load_all_files(app_ctx->current_path);
    } else {
        static const char *btns[] = {"Delete", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", file->name, btns, true);
        lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, 
            (void *)(intptr_t)file_index);
        lv_obj_add_event_cb(mbox, on_delete_cancel, LV_EVENT_VALUE_CHANGED, 
            (void *)(intptr_t)file_index);
        lv_obj_center(mbox);
    }
}

static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        if (remove(app_ctx->files[file_index].path) == 0) {
            load_all_files(app_ctx->current_path);
            show_toast("File deleted successfully", COLOR_SECONDARY, 1500);
        } else {
            show_toast("File deletion failed", COLOR_DANGER, 1500);
        }
    }
    
    lv_msgbox_close(mbox);
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
    
    load_all_files(app_ctx->current_path);
}

static void update_file_list_display(void)
{
    if (!app_ctx->file_list) return;
    
    lv_obj_clean(app_ctx->file_list);
    
    if (strcmp(app_ctx->current_path, ".") != 0 && 
        strcmp(app_ctx->current_path, "/") != 0) {
        lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, LV_SYMBOL_UP, ".. (Parent directory)");
        lv_obj_add_event_cb(btn, on_navigate_up, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xF5F5F5), 0);
    }
    
    for (int i = 0; i < app_ctx->file_count; i++) {
        const char *icon = app_ctx->files[i].is_dir ? LV_SYMBOL_DIRECTORY : 
                          (app_ctx->files[i].is_audio ? LV_SYMBOL_AUDIO : LV_SYMBOL_FILE);
        
        lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, icon, 
                                        app_ctx->files[i].display_name);
        lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        
        if (app_ctx->files[i].is_dir) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xE3F2FD), 0);
        }
    }
    
    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(app_ctx->file_list);
        lv_label_set_text(label, "Folder is empty");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }
    
    lv_label_set_text_fmt(app_ctx->path_label, "Current path: %s", app_ctx->current_path);
}

static void load_all_files(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    
    dir = opendir(path);
    if (!dir) {
        show_toast("Cannot open directory", COLOR_DANGER, 2000);
        return;
    }
    
    app_ctx->file_count = 0;
    
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            strcpy(app_ctx->files[app_ctx->file_count].name, entry->d_name);
            strcpy(app_ctx->files[app_ctx->file_count].path, full_path);
            app_ctx->files[app_ctx->file_count].is_dir = S_ISDIR(st.st_mode);
            app_ctx->files[app_ctx->file_count].size = st.st_size;
            
            const char *ext = strrchr(entry->d_name, '.');
            app_ctx->files[app_ctx->file_count].is_audio = 
                (ext && (strcasecmp(ext, ".mp3") == 0 || 
                        strcasecmp(ext, ".wav") == 0 || 
                        strcasecmp(ext, ".flac") == 0));
            
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
    update_file_list_display();
}

/**********************
 *      音频播放器页面 - 英文版
 **********************/
static void create_audio_player_screen(void)
{
    free_app_resources();
    
    app_ctx->current_app = APP_AUDIO_PLAYER;
    app_ctx->app_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(app_ctx->app_screen, COLOR_BG, 0);
    lv_obj_clear_flag(app_ctx->app_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(app_ctx->app_screen, LV_SCROLLBAR_MODE_OFF);
    lv_scr_load(app_ctx->app_screen);
    
    /* 创建导航栏 - 英文 */
    create_nav_bar(app_ctx->app_screen, "Audio Player");
    
    /* 可滚动的内容区域 */
    lv_obj_t *content = create_content_area(app_ctx->app_screen, CONTENT_HEIGHT);
    
    /* 当前播放信息卡片 */
    lv_obj_t *info_card = lv_obj_create(content);
    lv_obj_set_size(info_card, 430, 80);
    lv_obj_set_pos(info_card, 10, 10);
    lv_obj_set_style_bg_color(info_card, COLOR_CARD, 0);
    lv_obj_set_style_radius(info_card, 8, 0);
    lv_obj_set_style_pad_all(info_card, 10, 0);
    
    lv_obj_t *play_icon = lv_label_create(info_card);
    lv_label_set_text(play_icon, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(play_icon, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(play_icon, COLOR_PRIMARY, 0);
    lv_obj_set_pos(play_icon, 15, 15);
    
    app_ctx->track_info = lv_label_create(info_card);
    lv_label_set_text(app_ctx->track_info, "No track selected");
    lv_obj_set_style_text_font(app_ctx->track_info, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(app_ctx->track_info, 70, 20);
    lv_obj_set_size(app_ctx->track_info, 330, 25);
    lv_label_set_long_mode(app_ctx->track_info, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    /* 进度条和时间 */
    app_ctx->progress_bar = lv_bar_create(info_card);
    lv_obj_set_size(app_ctx->progress_bar, 250, 6);
    lv_obj_set_pos(app_ctx->progress_bar, 70, 50);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    
    app_ctx->time_label = lv_label_create(info_card);
    lv_label_set_text(app_ctx->time_label, "00:00 / 03:00");
    lv_obj_set_style_text_font(app_ctx->time_label, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(app_ctx->time_label, 330, 45);
    
    /* 控制按钮 */
    lv_obj_t *ctrl_card = lv_obj_create(content);
    lv_obj_set_size(ctrl_card, 430, 70);
    lv_obj_set_pos(ctrl_card, 10, 100);
    lv_obj_set_style_bg_color(ctrl_card, COLOR_CARD, 0);
    lv_obj_set_style_radius(ctrl_card, 8, 0);
    lv_obj_set_flex_flow(ctrl_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_card, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(ctrl_card, 10, 0);
    
    /* 上一首 */
    lv_obj_t *prev_btn = lv_btn_create(ctrl_card);
    lv_obj_set_size(prev_btn, 45, 45);
    lv_obj_add_style(prev_btn, &style_btn_small, 0);
    lv_obj_add_event_cb(prev_btn, on_prev_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *prev_icon = lv_label_create(prev_btn);
    lv_label_set_text(prev_icon, LV_SYMBOL_PREV);
    lv_obj_center(prev_icon);
    
    /* 播放/暂停 */
    app_ctx->play_btn = lv_btn_create(ctrl_card);
    lv_obj_set_size(app_ctx->play_btn, 55, 55);
    lv_obj_add_style(app_ctx->play_btn, &style_btn, 0);
    lv_obj_add_event_cb(app_ctx->play_btn, on_play_click, LV_EVENT_CLICKED, NULL);
    
    app_ctx->play_icon = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PLAY);
    lv_obj_center(app_ctx->play_icon);
    
    /* 下一首 */
    lv_obj_t *next_btn = lv_btn_create(ctrl_card);
    lv_obj_set_size(next_btn, 45, 45);
    lv_obj_add_style(next_btn, &style_btn_small, 0);
    lv_obj_add_event_cb(next_btn, on_next_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *next_icon = lv_label_create(next_btn);
    lv_label_set_text(next_icon, LV_SYMBOL_NEXT);
    lv_obj_center(next_icon);
    
    /* 停止 */
    lv_obj_t *stop_btn = lv_btn_create(ctrl_card);
    lv_obj_set_size(stop_btn, 45, 45);
    lv_obj_add_style(stop_btn, &style_btn_small, 0);
    lv_obj_add_event_cb(stop_btn, on_stop_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *stop_icon = lv_label_create(stop_btn);
    lv_label_set_text(stop_icon, LV_SYMBOL_STOP);
    lv_obj_center(stop_icon);
    
    /* 播放列表标题 - 英文 */
    lv_obj_t *list_title = lv_label_create(content);
    lv_label_set_text(list_title, "Playlist");
    lv_obj_set_style_text_font(list_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(list_title, 10, 190);
    
    /* 播放列表容器 */
    lv_obj_t *list_cont = lv_obj_create(content);
    lv_obj_set_size(list_cont, 430, 200);
    lv_obj_set_pos(list_cont, 10, 220);
    lv_obj_set_style_border_width(list_cont, 1, 0);
    lv_obj_set_style_border_color(list_cont, COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_color(list_cont, COLOR_CARD, 0);
    lv_obj_set_style_radius(list_cont, 8, 0);
    lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
    
    /* 播放列表 */
    app_ctx->track_list = lv_list_create(list_cont);
    lv_obj_set_size(app_ctx->track_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->track_list, 0, 0);
    lv_obj_clear_flag(app_ctx->track_list, LV_OBJ_FLAG_SCROLLABLE);
    
    load_audio_files(".");
}

/* 音频播放器相关函数 - 英文提示 */
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
    
    lv_obj_clean(app_ctx->track_list);
    app_ctx->track_count = 0;
    
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
    
    char info[128];
    snprintf(info, sizeof(info), "Now playing: %s", file->name);
    lv_label_set_text(app_ctx->track_info, info);
    
    app_ctx->is_playing = 1;
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PAUSE);
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    
    show_toast(file->name, COLOR_SECONDARY, 1500);
}

static void stop_audio_playback(void)
{
    app_ctx->is_playing = 0;
    lv_label_set_text(app_ctx->play_icon, LV_SYMBOL_PLAY);
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(app_ctx->time_label, "00:00 / 03:00");
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
        lv_obj_del_async(app_ctx->effect_config_screen);
        app_ctx->effect_config_screen = NULL;
        lv_scr_load(app_ctx->app_screen);
    } else if (app_ctx->app_screen) {
        free_app_resources();
        app_ctx->current_app = APP_NONE;
        lv_scr_load(app_ctx->main_screen);
    }
}

static void free_app_resources(void)
{
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

static void on_delete_cancel(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    lv_msgbox_close(mbox);
}

static void show_toast(const char *msg, lv_color_t color, uint32_t duration)
{
    lv_obj_t *toast = lv_label_create(lv_scr_act());
    lv_label_set_text(toast, msg);
    lv_obj_set_style_text_color(toast, lv_color_white(), 0);
    lv_obj_set_style_bg_color(toast, color, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(toast, 10, 0);
    lv_obj_set_style_radius(toast, 20, 0);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_timer_t *timer = lv_timer_create(NULL, duration, toast);
    lv_timer_set_repeat_count(timer, 1);
}