/**
 * @file main.c
 * @brief Audio File Processor - LVGL UI Interface
 */

/*********************
 *      INCLUDES
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
 *      DEFINES
 *********************/
#define MAX_PATH 256           /* Maximum path length */
#define MAX_FILES 128          /* Maximum number of files */
#define MAX_TIMERS 10          /* Maximum number of timers */
#define TIMER_STACK_SIZE 8192  /* Timer stack size */

/**********************
 *      TYPEDEFS
 **********************/
/* Application type enumeration */
typedef enum {
    APP_NONE = 0,              /* No application */
    APP_FILE_MANAGER,          /* File manager */
    APP_AUDIO_PLAYER,          /* Audio player */
    APP_AUDIO_PROCESSOR        /* Audio processor */
} app_type_t;

/* Effect type enumeration */
typedef enum {
    EFFECT_NONE = 0,           /* No effect */
    EFFECT_REVERB,             /* Reverb */
    EFFECT_ECHO,               /* Echo */
    EFFECT_DISTORTION,         /* Distortion */
    EFFECT_EQ,                 /* Equalizer */
    EFFECT_FILTER              /* Filter */
} effect_type_t;

/* File information structure */
typedef struct {
    char name[64];             /* File name */
    char path[MAX_PATH];       /* File path */
    int is_dir;                /* Is directory */
    int size;                  /* File size */
} file_info_t;

/* Effect structure */
typedef struct {
    effect_type_t type;        /* Effect type */
    int enabled;               /* Is enabled */
    int param1;                /* Parameter 1 */
    int param2;                /* Parameter 2 */
    int param3;                /* Parameter 3 */
    char name[32];             /* Effect name */
} effect_t;

/* Application context structure */
typedef struct {
    app_type_t current_app;    /* Current application */
    lv_obj_t *main_screen;     /* Main screen object */
    lv_obj_t *app_screen;      /* Application screen object */
    lv_obj_t *back_btn;        /* Back button */
    lv_obj_t *title_label;     /* Title label */
    
    /* File manager related */
    file_info_t files[MAX_FILES];  /* File array */
    int file_count;             /* Number of files */
    char current_path[MAX_PATH]; /* Current path */
    lv_obj_t *file_list;        /* File list object */
    
    /* Audio player related */
    int is_playing;             /* Is playing */
    int current_track;          /* Current track */
    lv_obj_t *play_btn;         /* Play button */
    lv_obj_t *progress_bar;     /* Progress bar */
    lv_obj_t *time_label;       /* Time label */
    
    /* Audio processor related */
    effect_t effects[8];        /* Effects array */
    int effect_count;           /* Number of effects */
    lv_obj_t *effect_cont;      /* Effect container */
    
    /* Timer management */
    lv_timer_t *app_timer;      /* Application timer */
    int timer_running;          /* Timer running flag */
} app_context_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static app_context_t *app_ctx = NULL;  /* Application context pointer */

/* Declare Chinese font */
LV_FONT_DECLARE(lv_font_simsun_16_cjk);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void hal_init(void);
static void create_main_screen(void);
static void create_file_manager_screen(void);
static void create_audio_player_screen(void);
static void create_audio_processor_screen(void);
static void file_manager_timer_cb(lv_timer_t *timer);
static void audio_player_timer_cb(lv_timer_t *timer);
static void audio_processor_timer_cb(lv_timer_t *timer);
static void load_directory(const char *path);
static void on_app_click(lv_event_t *e);
static void on_back_click(lv_event_t *e);
static void on_file_click(lv_event_t *e);
static void on_delete_confirm(lv_event_t *e);
static void on_effect_change(lv_event_t *e);
static void on_play_click(lv_event_t *e);
static void on_slider_change(lv_event_t *e);
static void free_timer_resources(void);
static void show_notification(const char *msg, lv_color_t color);
static void on_stop_click(lv_event_t *e);  /* Added separate handler for stop button */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/* Main function */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Initialize LVGL */
    lv_init();

    /* Initialize HAL */
    hal_init();

    /* Create application context */
    app_ctx = (app_context_t *)malloc(sizeof(app_context_t));
    memset(app_ctx, 0, sizeof(app_context_t));
    strcpy(app_ctx->current_path, "./");
    app_ctx->current_app = APP_NONE;

    /* Create main screen */
    create_main_screen();

    /* Main loop */
    while(1) {
        lv_timer_handler();
        usleep(5 * 1000);
    }

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Initialize Hardware Abstraction Layer */
static void hal_init(void)
{
    sdl_init();

    /* Create display buffer */
    static lv_disp_draw_buf_t disp_buf1;
    static lv_color_t buf1_1[SDL_HOR_RES * 100];
    lv_disp_draw_buf_init(&disp_buf1, buf1_1, NULL, SDL_HOR_RES * 100);

    /* Create display device */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf1;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* Set theme, use Chinese font as default */
    lv_theme_t *th = lv_theme_default_init(disp, 
        lv_palette_main(LV_PALETTE_BLUE), 
        lv_palette_main(LV_PALETTE_RED), 
        LV_THEME_DEFAULT_DARK, 
        &lv_font_simsun_16_cjk);  /* Use Chinese font */
    lv_disp_set_theme(disp, th);

    /* Create group for keyboard navigation */
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    /* Register mouse input device */
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = sdl_mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);

    /* Register keyboard input device */
    static lv_indev_drv_t indev_drv_2;
    lv_indev_drv_init(&indev_drv_2);
    indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv_2.read_cb = sdl_keyboard_read;
    lv_indev_t *kb_indev = lv_indev_drv_register(&indev_drv_2);
    lv_indev_set_group(kb_indev, g);

    /* Register encoder input device */
    static lv_indev_drv_t indev_drv_3;
    lv_indev_drv_init(&indev_drv_3);
    indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
    indev_drv_3.read_cb = sdl_mousewheel_read;
    lv_indev_t *enc_indev = lv_indev_drv_register(&indev_drv_3);
    lv_indev_set_group(enc_indev, g);

    /* Set mouse cursor */
    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t *cursor_obj = lv_img_create(lv_scr_act());
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor_obj);
}

/* Create main screen */
static void create_main_screen(void)
{
    app_ctx->main_screen = lv_obj_create(NULL);
    lv_scr_load(app_ctx->main_screen);
    
    /* Title */
    lv_obj_t *title = lv_label_create(app_ctx->main_screen);
    lv_label_set_text(title, "Audio File Processor");
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* Create application button container */
    lv_obj_t *btn_cont = lv_obj_create(app_ctx->main_screen);
    lv_obj_set_size(btn_cont, LV_PCT(90), LV_PCT(70));
    lv_obj_center(btn_cont);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_cont, 20, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);

    /* File manager application button */
    lv_obj_t *btn1 = lv_btn_create(btn_cont);
    lv_obj_set_size(btn1, 220, 60);
    lv_obj_add_event_cb(btn1, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)APP_FILE_MANAGER);
    
    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, LV_SYMBOL_DIRECTORY " File Manager");
    lv_obj_set_style_text_font(label1, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(label1);

    /* Audio player application button */
    lv_obj_t *btn2 = lv_btn_create(btn_cont);
    lv_obj_set_size(btn2, 220, 60);
    lv_obj_add_event_cb(btn2, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)APP_AUDIO_PLAYER);
    
    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, LV_SYMBOL_PLAY " Audio Player");
    lv_obj_set_style_text_font(label2, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(label2);

    /* Audio processor application button */
    lv_obj_t *btn3 = lv_btn_create(btn_cont);
    lv_obj_set_size(btn3, 220, 60);
    lv_obj_add_event_cb(btn3, on_app_click, LV_EVENT_CLICKED, (void *)(intptr_t)APP_AUDIO_PROCESSOR);
    
    lv_obj_t *label3 = lv_label_create(btn3);
    lv_label_set_text(label3, LV_SYMBOL_SETTINGS " Audio Processor");
    lv_obj_set_style_text_font(label3, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(label3);
}

/* Create file manager interface */
static void create_file_manager_screen(void)
{
    /* Release previous timer resources */
    free_timer_resources();
    
    /* Create new screen */
    app_ctx->app_screen = lv_obj_create(NULL);
    
    /* Title bar */
    lv_obj_t *header = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);

    /* Back button */
    app_ctx->back_btn = lv_btn_create(header);
    lv_obj_set_size(app_ctx->back_btn, 50, 40);
    lv_obj_align(app_ctx->back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(app_ctx->back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(app_ctx->back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(back_label);

    /* Title */
    app_ctx->title_label = lv_label_create(header);
    lv_label_set_text(app_ctx->title_label, "File Manager");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);

    /* Current path display */
    lv_obj_t *path_label = lv_label_create(header);
    lv_label_set_text_fmt(path_label, "Path: %s", app_ctx->current_path);
    lv_obj_set_style_text_font(path_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(path_label, LV_ALIGN_RIGHT_MID, -5, 0);

    /* File list container */
    lv_obj_t *list_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(list_cont, LV_PCT(100), LV_PCT(85));
    lv_obj_align(list_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(list_cont, 5, 0);
    lv_obj_set_style_border_width(list_cont, 0, 0);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);

    /* Create file list */
    app_ctx->file_list = lv_list_create(list_cont);
    lv_obj_set_size(app_ctx->file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(app_ctx->file_list, 0, 0);

    /* Start file manager timer */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(file_manager_timer_cb, 100, app_ctx);
    
    /* Load current directory */
    load_directory(app_ctx->current_path);
}

/* Create audio player interface */
static void create_audio_player_screen(void)
{
    free_timer_resources();
    
    app_ctx->app_screen = lv_obj_create(NULL);
    
    /* Title bar */
    lv_obj_t *header = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);

    /* Back button */
    app_ctx->back_btn = lv_btn_create(header);
    lv_obj_set_size(app_ctx->back_btn, 50, 40);
    lv_obj_align(app_ctx->back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(app_ctx->back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(app_ctx->back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(back_label);

    /* Title */
    app_ctx->title_label = lv_label_create(header);
    lv_label_set_text(app_ctx->title_label, "Audio Player");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);

    /* Main content area */
    lv_obj_t *main_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(85));
    lv_obj_align(main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);

    /* Playlist */
    lv_obj_t *list_label = lv_label_create(main_cont);
    lv_label_set_text(list_label, "Playlist:");
    lv_obj_set_style_text_font(list_label, &lv_font_simsun_16_cjk, 0);
    
    app_ctx->file_list = lv_list_create(main_cont);
    lv_obj_set_size(app_ctx->file_list, LV_PCT(100), 180);
    lv_obj_set_style_border_width(app_ctx->file_list, 1, 0);
    lv_obj_set_style_border_color(app_ctx->file_list, lv_palette_main(LV_PALETTE_GREY), 0);

    /* Now playing info */
    lv_obj_t *now_playing = lv_label_create(main_cont);
    lv_label_set_text(now_playing, "Not playing");
    lv_obj_set_style_text_font(now_playing, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_style_text_align(now_playing, LV_TEXT_ALIGN_CENTER, 0);

    /* Playback control area */
    lv_obj_t *control_cont = lv_obj_create(main_cont);
    lv_obj_set_size(control_cont, LV_PCT(100), 120);
    lv_obj_set_flex_flow(control_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(control_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(control_cont, 0, 0);
    lv_obj_set_style_bg_opa(control_cont, LV_OPA_TRANSP, 0);

    /* Progress bar and time */
    lv_obj_t *progress_cont = lv_obj_create(control_cont);
    lv_obj_set_size(progress_cont, LV_PCT(100), 40);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_border_width(progress_cont, 0, 0);
    lv_obj_set_style_bg_opa(progress_cont, LV_OPA_TRANSP, 0);
    
    app_ctx->progress_bar = lv_bar_create(progress_cont);
    lv_obj_set_size(app_ctx->progress_bar, LV_PCT(80), 10);
    lv_bar_set_range(app_ctx->progress_bar, 0, 100);
    
    app_ctx->time_label = lv_label_create(progress_cont);
    lv_label_set_text(app_ctx->time_label, "00:00/03:00");
    lv_obj_set_style_text_font(app_ctx->time_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(app_ctx->time_label, LV_PCT(18));

    /* Button container */
    lv_obj_t *btn_cont = lv_obj_create(control_cont);
    lv_obj_set_size(btn_cont, LV_PCT(100), 60);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);

    /* Play/Pause button */
    app_ctx->play_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(app_ctx->play_btn, 80, 50);
    lv_obj_add_event_cb(app_ctx->play_btn, on_play_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(app_ctx->play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_label);

    /* Stop button - use separate handler */
    lv_obj_t *stop_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(stop_btn, 80, 50);
    lv_obj_add_event_cb(stop_btn, on_stop_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP);
    lv_obj_center(stop_label);

    /* Start audio player timer */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_player_timer_cb, 100, app_ctx);
    
    /* Load audio file list */
    load_directory("./");
}

/* Create audio processor interface */
static void create_audio_processor_screen(void)
{
    free_timer_resources();
    
    app_ctx->app_screen = lv_obj_create(NULL);
    
    /* Title bar */
    lv_obj_t *header = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(header, LV_PCT(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);

    /* Back button */
    app_ctx->back_btn = lv_btn_create(header);
    lv_obj_set_size(app_ctx->back_btn, 50, 40);
    lv_obj_align(app_ctx->back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(app_ctx->back_btn, on_back_click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(app_ctx->back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(back_label);

    /* Title */
    app_ctx->title_label = lv_label_create(header);
    lv_label_set_text(app_ctx->title_label, "Audio Processor");
    lv_obj_set_style_text_font(app_ctx->title_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(app_ctx->title_label, LV_ALIGN_CENTER, 0, 0);

    /* Main content area */
    lv_obj_t *main_cont = lv_obj_create(app_ctx->app_screen);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(85));
    lv_obj_align(main_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0);

    /* Effect list */
    lv_obj_t *effect_list = lv_obj_create(main_cont);
    lv_obj_set_size(effect_list, LV_PCT(30), LV_PCT(100));
    lv_obj_set_flex_flow(effect_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(effect_list, 0, 0);
    lv_obj_set_style_bg_opa(effect_list, LV_OPA_10, 0);
    
    lv_obj_t *effect_title = lv_label_create(effect_list);
    lv_label_set_text(effect_title, "Effects");
    lv_obj_set_style_text_font(effect_title, &lv_font_simsun_16_cjk, 0);

    /* Initialize effects */
    app_ctx->effect_count = 5;
    strcpy(app_ctx->effects[0].name, "Reverb");
    app_ctx->effects[0].type = EFFECT_REVERB;
    app_ctx->effects[0].enabled = 0;
    app_ctx->effects[0].param1 = 50;
    
    strcpy(app_ctx->effects[1].name, "Echo");
    app_ctx->effects[1].type = EFFECT_ECHO;
    app_ctx->effects[1].enabled = 0;
    app_ctx->effects[1].param1 = 30;
    
    strcpy(app_ctx->effects[2].name, "Distortion");
    app_ctx->effects[2].type = EFFECT_DISTORTION;
    app_ctx->effects[2].enabled = 0;
    app_ctx->effects[2].param1 = 70;
    
    strcpy(app_ctx->effects[3].name, "Equalizer");
    app_ctx->effects[3].type = EFFECT_EQ;
    app_ctx->effects[3].enabled = 0;
    app_ctx->effects[3].param1 = 50;
    app_ctx->effects[3].param2 = 50;
    app_ctx->effects[3].param3 = 50;
    
    strcpy(app_ctx->effects[4].name, "Filter");
    app_ctx->effects[4].type = EFFECT_FILTER;
    app_ctx->effects[4].enabled = 0;
    app_ctx->effects[4].param1 = 1000;

    /* Create effect buttons */
    for (int i = 0; i < app_ctx->effect_count; i++) {
        lv_obj_t *btn = lv_btn_create(effect_list);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        lv_obj_add_event_cb(btn, on_effect_change, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text_fmt(label, "%s %s", 
            app_ctx->effects[i].name,
            app_ctx->effects[i].enabled ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(label);
    }

    /* Parameter adjustment area */
    app_ctx->effect_cont = lv_obj_create(main_cont);
    lv_obj_set_size(app_ctx->effect_cont, LV_PCT(70), LV_PCT(100));
    lv_obj_set_flex_flow(app_ctx->effect_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(app_ctx->effect_cont, 0, 0);
    lv_obj_set_style_bg_opa(app_ctx->effect_cont, LV_OPA_10, 0);
    
    lv_obj_t *param_title = lv_label_create(app_ctx->effect_cont);
    lv_label_set_text(param_title, "Parameters");
    lv_obj_set_style_text_font(param_title, &lv_font_simsun_16_cjk, 0);
    
    /* Add parameter sliders */
    const char *param_names[] = {"Param 1", "Param 2", "Param 3"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *slider_cont = lv_obj_create(app_ctx->effect_cont);
        lv_obj_set_size(slider_cont, LV_PCT(100), 50);
        lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_border_width(slider_cont, 0, 0);
        lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
        
        lv_obj_t *label = lv_label_create(slider_cont);
        lv_label_set_text(label, param_names[i]);
        lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_width(label, 60);
        
        lv_obj_t *slider = lv_slider_create(slider_cont);
        lv_obj_set_size(slider, 150, 10);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider, on_slider_change, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
        
        lv_obj_t *value_label = lv_label_create(slider_cont);
        lv_label_set_text(value_label, "50");
        lv_obj_set_style_text_font(value_label, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_user_data(slider, value_label);
    }

    /* Start audio processor timer */
    app_ctx->timer_running = 1;
    app_ctx->app_timer = lv_timer_create(audio_processor_timer_cb, 10, app_ctx);
}

/* File manager timer callback */
static void file_manager_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx || !ctx->timer_running) {
        return;
    }
    
    /* File system monitoring logic can be added here */
}

/* Audio player timer callback */
static void audio_player_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx || !ctx->timer_running) {
        return;
    }
    
    /* Simulate audio playback progress update */
    if (ctx->is_playing) {
        static int progress = 0;
        progress = (progress + 1) % 101;
        lv_bar_set_value(ctx->progress_bar, progress, LV_ANIM_ON);
        
        int total = 180; /* Assume total duration 3 minutes */
        int current = (progress * total) / 100;
        lv_label_set_text_fmt(ctx->time_label, "%02d:%02d/03:00", 
            current / 60, current % 60);
    }
}

/* Audio processor timer callback */
static void audio_processor_timer_cb(lv_timer_t *timer)
{
    app_context_t *ctx = (app_context_t *)timer->user_data;
    
    if (!ctx || !ctx->timer_running) {
        return;
    }
    
    /* Simulate audio processing */
    static int buffer_index = 0;
    buffer_index = !buffer_index;
    
    /* Process audio based on enabled effects */
    for (int i = 0; i < ctx->effect_count; i++) {
        if (ctx->effects[i].enabled) {
            switch (ctx->effects[i].type) {
                case EFFECT_REVERB:
                    /* Reverb effect processing */
                    break;
                case EFFECT_ECHO:
                    /* Echo effect processing */
                    break;
                case EFFECT_DISTORTION:
                    /* Distortion effect processing */
                    break;
                case EFFECT_EQ:
                    /* Equalizer processing */
                    break;
                case EFFECT_FILTER:
                    /* Filter processing */
                    break;
                default:
                    break;
            }
        }
    }
}

/* Load directory contents */
static void load_directory(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    
    if (!app_ctx->file_list) {
        return;
    }
    
    dir = opendir(path);
    if (dir == NULL) {
        show_notification("Cannot open directory", lv_palette_main(LV_PALETTE_RED));
        return;
    }
    
    /* Clear file list */
    app_ctx->file_count = 0;
    
    if (app_ctx->file_list != NULL) {
        lv_obj_clean(app_ctx->file_list);
    }
    
    /* Add parent directory option */
    if (strcmp(path, "./") != 0 && strcmp(path, "/") != 0) {
        lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, LV_SYMBOL_DIRECTORY, "..");
        lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    }
    
    /* Read directory contents */
    while ((entry = readdir(dir)) != NULL && app_ctx->file_count < MAX_FILES) {
        /* Skip current directory */
        if (strcmp(entry->d_name, ".") == 0) {
            continue;
        }
        
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            strcpy(app_ctx->files[app_ctx->file_count].name, entry->d_name);
            strcpy(app_ctx->files[app_ctx->file_count].path, full_path);
            app_ctx->files[app_ctx->file_count].is_dir = S_ISDIR(st.st_mode);
            app_ctx->files[app_ctx->file_count].size = st.st_size;
            
            /* Select icon */
            const char *icon;
            if (app_ctx->files[app_ctx->file_count].is_dir) {
                icon = LV_SYMBOL_DIRECTORY;
            } else {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && (strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".wav") == 0)) {
                    icon = LV_SYMBOL_AUDIO;
                } else {
                    icon = LV_SYMBOL_FILE;
                }
            }
            
            lv_obj_t *btn = lv_list_add_btn(app_ctx->file_list, icon, entry->d_name);
            lv_obj_add_event_cb(btn, on_file_click, LV_EVENT_CLICKED, 
                (void *)(intptr_t)app_ctx->file_count);
            
            app_ctx->file_count++;
        }
    }
    
    closedir(dir);
    
    /* Empty folder提示 */
    if (app_ctx->file_count == 0) {
        lv_obj_t *label = lv_label_create(app_ctx->file_list);
        lv_label_set_text(label, "Folder is empty");
        lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, LV_PCT(100));
    }
    
    show_notification("Directory loaded", lv_palette_main(LV_PALETTE_GREEN));
}

/* Application click event handler */
static void on_app_click(lv_event_t *e)
{
    app_type_t app_type = (app_type_t)(intptr_t)lv_event_get_user_data(e);
    
    app_ctx->current_app = app_type;
    
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
    
    if (app_ctx->app_screen) {
        lv_scr_load(app_ctx->app_screen);
    }
}

/* Back button click event handler */
static void on_back_click(lv_event_t *e)
{
    /* Stop all timers first */
    if (app_ctx->app_timer != NULL) {
        app_ctx->timer_running = 0;
        lv_timer_del(app_ctx->app_timer);
        app_ctx->app_timer = NULL;
    }
    
    /* Reset state */
    app_ctx->is_playing = 0;
    app_ctx->current_app = APP_NONE;
    
    /* Load main screen */
    if (app_ctx->main_screen) {
        lv_scr_load(app_ctx->main_screen);
    }
    
    /* Delete app screen after delay to avoid issues */
    if (app_ctx->app_screen) {
        lv_obj_del_async(app_ctx->app_screen);
        app_ctx->app_screen = NULL;
    }
}

/* File click event handler */
static void on_file_click(lv_event_t *e)
{
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    /* Handle parent directory */
    if (file_index == -1) {
        char *last_slash = strrchr(app_ctx->current_path, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
            if (strlen(app_ctx->current_path) == 0) {
                strcpy(app_ctx->current_path, "./");
            }
        }
        load_directory(app_ctx->current_path);
        return;
    }
    
    file_info_t *file = &app_ctx->files[file_index];
    
    if (file->is_dir) {
        /* Enter directory */
        strcpy(app_ctx->current_path, file->path);
        load_directory(app_ctx->current_path);
    } else {
        /* Show delete confirmation dialog */
        static const char *btns[] = {"Confirm", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Delete", 
            file->name, btns, true);
        lv_obj_add_event_cb(mbox, on_delete_confirm, LV_EVENT_VALUE_CHANGED, 
            (void *)(intptr_t)file_index);
        lv_obj_center(mbox);
    }
}

/* Delete confirmation handler */
static void on_delete_confirm(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    int file_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (lv_msgbox_get_active_btn(mbox) == 0) {  /* Confirm button */
        file_info_t *file = &app_ctx->files[file_index];
        
        if (remove(file->path) == 0) {
            load_directory(app_ctx->current_path);
            show_notification("File deleted successfully", lv_palette_main(LV_PALETTE_GREEN));
        } else {
            show_notification("Failed to delete file", lv_palette_main(LV_PALETTE_RED));
        }
    }
    
    lv_msgbox_close(mbox);
}

/* Effect toggle handler */
static void on_effect_change(lv_event_t *e)
{
    int effect_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    app_ctx->effects[effect_index].enabled = 
        !app_ctx->effects[effect_index].enabled;
    
    /* Update button text - using LVGL symbols */
    lv_obj_t *btn = lv_event_get_current_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text_fmt(label, "%s %s", 
        app_ctx->effects[effect_index].name,
        app_ctx->effects[effect_index].enabled ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    
    show_notification(app_ctx->effects[effect_index].enabled ? 
        "Effect enabled" : "Effect disabled", lv_palette_main(LV_PALETTE_BLUE));
}

/* Play button click handler */
static void on_play_click(lv_event_t *e)
{
    app_ctx->is_playing = !app_ctx->is_playing;
    
    lv_obj_t *btn = lv_event_get_current_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text(label, app_ctx->is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    
    show_notification(app_ctx->is_playing ? "Playing" : "Paused", 
        lv_palette_main(LV_PALETTE_BLUE));
}

/* Stop button click handler - separate function */
static void on_stop_click(lv_event_t *e)
{
    app_ctx->is_playing = 0;
    
    /* Reset progress */
    lv_bar_set_value(app_ctx->progress_bar, 0, LV_ANIM_ON);
    lv_label_set_text(app_ctx->time_label, "00:00/03:00");
    
    /* Update play button text */
    lv_obj_t *play_label = lv_obj_get_child(app_ctx->play_btn, 0);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    
    show_notification("Stopped", lv_palette_main(LV_PALETTE_BLUE));
}

/* Slider value change handler */
static void on_slider_change(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target(e);
    lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(value_label, "%d", (int)value);
}

/* Show notification message */
static void show_notification(const char *msg, lv_color_t color)
{
    lv_obj_t *notification = lv_label_create(lv_scr_act());
    lv_label_set_text(notification, msg);
    lv_obj_set_style_text_font(notification, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_style_text_color(notification, color, 0);
    lv_obj_set_style_bg_color(notification, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(notification, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(notification, 10, 0);
    lv_obj_align(notification, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    /* Auto disappear after 2 seconds */
    lv_timer_t *timer = lv_timer_create(NULL, 2000, notification);
    lv_timer_set_repeat_count(timer, 1);
}

/* Free timer resources */
static void free_timer_resources(void)
{
    if (app_ctx->app_timer != NULL) {
        app_ctx->timer_running = 0;
        lv_timer_del(app_ctx->app_timer);
        app_ctx->app_timer = NULL;
    }
    
    /* Clean other resources - use async delete to avoid issues */
    if (app_ctx->app_screen != NULL) {
        lv_obj_del_async(app_ctx->app_screen);
        app_ctx->app_screen = NULL;
    }
}