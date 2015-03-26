/*  DreamChess
**
**  DreamChess is the legal property of its developers, whose names are too
**  numerous to list here. Please refer to the COPYRIGHT file distributed
**  with this source distribution.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ui_sdlgl.h"
#include "git_rev.h"
#include <SDL_syswm.h>
#include <GL/glew.h>
#include "audio.h"
#include "options.h"
#include "theme.h"
#include "system_config.h"
#include "gamegui_dialogs.h"

static gg_dialog_style_t style_ingame, style_menu;
static int turn_counter_start=0;
static texture_t menu_title_tex;
static int game_in_stalemate;
static int white_in_check;
static int black_in_check;
static int white_in_checkmate;
static int black_in_checkmate;
static board_t board;
static int pgn_slot;
static int quit_to_menu=FALSE;
static int title_process_retval;
static int set_loading=FALSE;
static int dialog_promote_piece;
static int show_egg;
static int egg_req1=FALSE;
static int screen_width=640;
static int screen_height=480;
static int screen_fs=0;
static int screen_ms=0;
static int reflections=1;
static int mode_set_failed=0;
static int engine_error_shown;

static void music_callback(char *title, char *artist, char *album)
{
    DBG_LOG("now playing: %s - %s", artist, title);
}

int get_screen_width(void)
{
    return screen_width;
}

int get_screen_height(void)
{
    return screen_height;
}

static int menu_state;
enum {
    MENU_STATE_FADE_IN,
    MENU_STATE_IN_MENU,
    MENU_STATE_LOAD,
    MENU_STATE_FADE_OUT,
    MENU_STATE_RETURN
};

static void poll_move(void);

int get_egg_req(void)
{
    return egg_req1;
}

void set_show_egg( int set )
{
    show_egg=set;
}

int get_show_egg(void)
{
    return show_egg;
}

int get_dialog_promote_piece(void)
{
    return dialog_promote_piece;
}

void set_dialog_promote_piece( int piece )
{
    dialog_promote_piece=piece;
}

void set_title_process_retval( int ret )
{
    title_process_retval=ret;
}

void set_set_loading( int set )
{
    set_loading=set;
}

void set_quit_to_menu( int menu )
{
    quit_to_menu=menu;
}

void set_pgn_slot( int slot )
{
    pgn_slot=slot;
}

board_t *get_board(void)
{
    return &board;
}

config_t config;

config_t *get_config(void)
{
    return &config;
}

static int switch_to_menu=FALSE;
static int fading_out=FALSE;

void set_switch_to_menu(int sw)
{
    switch_to_menu=sw;
}

int get_fading_out(void)
{
    return fading_out;
}

void set_fading_out(int fade)
{
    fading_out=fade;
}
int get_game_stalemate(void)
{
    return game_in_stalemate;
}

int get_white_in_check(void)
{
    return white_in_check;
}

int get_black_in_check(void)
{
    return black_in_check;
}
int get_white_in_checkmate(void)
{
    return white_in_checkmate;
}

int get_black_in_checkmate(void)
{
    return black_in_checkmate;
}

gg_dialog_style_t *get_ingame_style(void)
{
    return &style_ingame;
}

gg_dialog_style_t *get_menu_style(void)
{
    return &style_menu;
}

int get_turn_counter(void)
{
    return turn_counter_start;
}

static void reset_turn_counter(void)
{
    turn_counter_start=SDL_GetTicks();
}

static int poll_event(gg_event_t *event)
{
    gg_event_t gg_event;
    SDL_Event sdl_event;

    audio_poll(1);

    while (SDL_PollEvent(&sdl_event))
    {

        if (sdl_event.type == SDL_QUIT)
            /* FIXME */
            exit(0);

        if ( (sdl_event.type == SDL_KEYDOWN && sdl_event.key.keysym.mod & KMOD_ALT &&
            sdl_event.key.keysym.sym == SDLK_RETURN) || (sdl_event.type == SDL_KEYDOWN &&
            sdl_event.key.keysym.sym == SDLK_F11) )
        {
            DBG_LOG( "toggled fullscreen" );
            toggle_fullscreen();
            continue;
        }

        gg_event = convert_event(&sdl_event);

        if (gg_event.type == GG_EVENT_KEY && gg_event.key == 0x06)
        {
            toggle_show_fps();
            continue;
        }

        *event = gg_event;
        return 1;
    }

    return 0;
}

/** Implements ui_driver::menu */
static config_t *do_menu(int *pgn)
{
    title_process_retval=2;

    resize_window(screen_width, screen_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    engine_error_shown = 0;
    set_loading=FALSE;
    draw_credits(1);
    open_title_root_dialog();
    if (mode_set_failed) {
        gg_dialog_open(dialog_error_create(gg_dialog_get_active(), "Error: failed to set video mode; using defaults", NULL));
        mode_set_failed = 0;
    }

    set_fade_start(gg_system_get_ticks());
    set_show_egg(FALSE);

    DBG_LOG("entering title menu");

    while ( 1 )
    {
        Uint8 *keystate;
        gg_event_t event;

        keystate = SDL_GetKeyState(NULL);

        egg_req1=FALSE;

        gg_dialog_cleanup();

        /* Draw the menu.. */
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        draw_texture(&menu_title_tex, 0, 0, 640, 480, 1.0f, get_col(COL_WHITE));
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        text_draw_string_right(620, 20, "v" PACKAGE_VERSION " (" GIT_REV ")", 0.75f, get_col(COL_WHITE));

        /*if (get_show_egg())
            text_draw_string(560, 440, "Egg!", 1, get_col(COL_WHITE));*/

        switch(menu_state)
        {
        case MENU_STATE_FADE_IN:
            while (poll_event(&event));
            gg_dialog_render_all();

            if (!draw_fade(FADE_IN))
            {
                menu_state = MENU_STATE_IN_MENU;
                draw_credits(1);
            }
            break;

        case MENU_STATE_IN_MENU:

            if ( keystate[SDLK_UP] )
                egg_req1=TRUE;

            while (poll_event(&event))
                gg_dialog_input_current(event);

            if (title_process_retval == 1)
                return NULL;

            if (set_loading)
            {
                gg_widget_t *widget = gg_label_create("Loading, please wait...");
                widget = gg_dialog_create(widget, NULL, NULL, 0);
                gg_dialog_set_style(GG_DIALOG(widget), get_menu_style());
                gg_dialog_open(GG_DIALOG(widget));
                menu_state = MENU_STATE_LOAD;
            }
            else
                draw_credits(0);

            gg_dialog_render_all();
            break;

        case MENU_STATE_LOAD:
            {
                option_t *option = config_get_option("theme");
                struct theme_struct *theme = option->selected->data;

                load_theme(theme);
                sprintf(get_white_name(),"%s", theme->white_name);
                sprintf(get_black_name(),"%s", theme->black_name);
                reset_transition(TRUE);
            }

            *pgn = pgn_slot;
            if (pgn_slot >= 0)
                config = *get_config_save(pgn_slot);

            reset_3d(config.player[WHITE] == PLAYER_ENGINE);

            set_fade_start(gg_system_get_ticks());
            gg_dialog_close();

            menu_state = MENU_STATE_FADE_OUT;
            break;

        case MENU_STATE_FADE_OUT:
            while (poll_event(&event));
            if ((get_show_egg() && !draw_sonic_fade( FADE_OUT )) ||
                    (!get_show_egg() && !draw_fade( FADE_OUT )))
            {
                set_fade_start(gg_system_get_ticks());
                menu_state = MENU_STATE_RETURN;
                return &config;
            }
            break;

        case MENU_STATE_RETURN:
            while (poll_event(&event));
            gg_dialog_render_all();

            if (!draw_fade(FADE_IN))
                menu_state = MENU_STATE_IN_MENU;
            break;

        }

        /* Draw mouse cursor.. */
#ifndef __BEOS__
        draw_texture( get_menu_mouse_cursor(), get_mouse_x(), (479-get_mouse_y()-32), 32, 32, 1.0f,
                      get_col(COL_WHITE) );
#endif /* __BEOS __ */

        gl_swap();
    }
}
static void free_menu_tex(void)
{
    glDeleteTextures(1, &menu_title_tex.id);
    glDeleteTextures(1, &get_menu_mouse_cursor()->id);
    glDeleteTextures(1, &get_menu_border()->id);
    glDeleteTextures(1, &get_text_character(0)->id);
}

static void load_menu_tex(void)
{
    ch_datadir();
    /* For the menu.. */
    load_texture_png( &menu_title_tex, "menu_title.png" , 0, 1);
    /* New text stuff. */
    generate_text_chars();

    chdir("styles");
    chdir("default");
    load_border(get_menu_border(), "border.png");
#ifndef __BEOS__
    load_texture_png( get_menu_mouse_cursor(), "mouse_cursor.png", 1, 1 );
#endif /* __BEOS__ */    
}

static int set_video( int width, int height, int fullscreen, int ms )
{
    int video_flags;
    SDL_Surface *surface;

    video_flags = SDL_OPENGL;          /* Enable OpenGL in SDL */
    DBG_LOG("setting video mode to %ix%i; fullscreen %s; %ix multisampling",
            width, height, fullscreen ? "on" : "off", ms);
    if ( fullscreen )
        video_flags |= SDL_FULLSCREEN;

    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 1 );

    if (ms) {
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
    } else {
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
    }
    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, ms );

    int bpp = SDL_VideoModeOK(width, height, SCREEN_BPP, video_flags);

    if (!bpp)
    {
        DBG_ERROR("video mode not available: %ix%i; %i bpp; fullscreen %s; %ix multisampling",
            width, height, SCREEN_BPP, fullscreen ? "on" : "off", ms);
        return 1;
    }

    surface = SDL_SetVideoMode( width, height, bpp, video_flags );
    if ( !surface )
    {
        DBG_ERROR("failed to set video mode: %ix%i; %i bpp; fullscreen %s; %ix multisampling: %s",
            width, height, bpp, fullscreen ? "on" : "off", ms, SDL_GetError());
        exit(1);
    }

    return 0;
}

static int resize(int width, int height, int fullscreen, int ms)
{
    /* We assume we lose the GL context here */
    free_menu_tex();
    deinit_fbo();

    if (!set_video(width, height, fullscreen, ms))
    {
        init_gl();
        init_fbo();
        load_menu_tex();
        resize_window(width, height);
        screen_width=width;
        screen_height=height;
        screen_fs=fullscreen;
        screen_ms=ms;
        return 0;
    }

    init_fbo();
    load_menu_tex();
    return 1;
}

/** Implements ui_driver::init. */
static int init_gui( int width, int height, int fullscreen, int ms)
{
    int i;
    int err;

    screen_width=width;
    screen_height=height;
    screen_fs=fullscreen;
    screen_ms=ms;

    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE ) < 0 )
    {
        DBG_ERROR("SDL initialization failed: %s", SDL_GetError());
        exit(1);
    }
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    SDL_EnableUNICODE(1);

    ch_datadir();

#ifdef __WIN32__
    {
        HMODULE handle = GetModuleHandle(NULL);
        HICON ico = LoadIcon(handle, "icon");
        if (ico) {
            SDL_SysWMinfo wminfo;
            SDL_VERSION(&wminfo.version);
            if (SDL_GetWMInfo(&wminfo))
                SetClassLongPtr(wminfo.window, GCLP_HICON, (ULONG_PTR)ico);
        }
    }
#endif

    if (set_video( screen_width, screen_height, fullscreen, ms ))
    {
        mode_set_failed = 1;
        return 1;
    }

    init_gl();
    err = glewInit();
    if (err != GLEW_OK)
    {
        DBG_ERROR("failed to initialize GLEW: %s", glewGetErrorString(err));
        exit(1);
    }

    if (!glewIsSupported("GL_EXT_framebuffer_object"))
    {
        DBG_ERROR("OpenGL extension GL_EXT_framebuffer_object not supported");
        exit(1);
    }

    resize_window(width, height);
    init_fbo();

    load_menu_tex();

#ifndef __BEOS__
    SDL_ShowCursor(SDL_DISABLE);
#endif /* __BEOS__ */

    SDL_WM_SetCaption( "DreamChess", NULL );

    gg_system_init(get_gg_driver_sdlgl());
    style_ingame.fade_col = gg_colour(0.0f, 0.0f, 0.0f, 0.5f);
    style_ingame.hor_pad = 20;
    style_ingame.vert_pad = 10;

    for (i = 0; i < 9; i++)
        style_ingame.border.image[i] = &get_border()[i];

    style_menu.textured = 1;
    style_menu.fade_col = gg_colour(0.0f, 0.0f, 0.0f, 0.0f);
    style_menu.hor_pad = 20;
    style_menu.vert_pad = 10;

    for (i = 0; i < 9; i++)
        style_menu.border.image[i] = &get_menu_border()[i];

    ch_datadir();
    audio_init();

    /* Make virtual keyboard table? */
    populate_key_table();

    update_fps_time();

    /* Register music callback */
    audio_set_music_callback(music_callback);

    return 0;
}

/** Implements ui_driver::update. */
static void update(board_t *b, move_t *move)
{
    board = *b;

    if ( move != NULL )
        start_piece_move( move->source, move->destination );
    if ( board.state == BOARD_STALEMATE )
    {
        game_in_stalemate=TRUE;
    }
    else
        game_in_stalemate=FALSE;

    black_in_check=FALSE;
    white_in_check=FALSE;
    if ( board.state == BOARD_CHECK )
    {
        if (IS_WHITE(board.turn))
            white_in_check=TRUE;
        else
            black_in_check=TRUE;
    }
    black_in_checkmate=FALSE;
    white_in_checkmate=FALSE;
    if ( board.state == BOARD_CHECKMATE )
    {
        if (IS_WHITE(board.turn))
            white_in_checkmate=TRUE;
        else
            black_in_checkmate=TRUE;
    }

    /* FIXME */
    while ( get_piece_moving_done() == 0 )
        poll_move();
}

/** Implements ui_driver::show_result. */
static void show_result(result_t *res)
{
    gg_dialog_open(dialog_victory_create(res));
}
/** Implements ui_driver::init. */
static int sdlgl_init(int width, int height, int fullscreen, int ms)
{
    return init_gui(width,height,fullscreen,ms);
}

/** Implements ui_driver::exit. */
static int sdlgl_exit(void)
{
    audio_set_music_callback(NULL);

    gg_system_exit();
    free_menu_tex();
    deinit_fbo();

    SDL_Quit();
    return 0;
}

/** Implements ui_driver::show_message. */
static void show_message (char *msg)
{
    gg_dialog_open(dialog_message_create(msg));
}

/** Implements ui_driver::poll. */
static void poll_move(void)
{
    static int source = -1, dest = -1, needprom = 0;
    /* board_t *board = history->play->board; */
    move_t move;
    int input;

    audio_poll(0);
    draw_scene(&board, reflections);

    if (switch_to_menu == TRUE)
    {
        quit_to_menu = 0;
        switch_to_menu=FALSE;
        fading_out=FALSE;
        needprom = 0;
        source = -1;
        dest = -1;
        unload_theme();
        game_quit();
        return;
    }
    else if (quit_to_menu)
    {
        DBG_LOG("returning to title menu");
        set_fading_out(TRUE);
        reset_transition(FALSE);
        set_fade_start(gg_system_get_ticks());
        quit_to_menu=FALSE;
    }
    input = get_move();
    /* FIXME */
    if (!engine_error_shown && game_get_engine_error())
    {
        gg_dialog_open(dialog_engine_error_create());
        engine_error_shown = 1;
        return;
    }

    if (!game_want_move())
    {
        source = -1;
        dest = -1;
        needprom = 0;
    }

    if (source == -1)
    {
        source = input;
        /* Only allow piece of current player to be moved. */
        if ((source >= 0) && ((PIECE(board.square[source]) == NONE) || (COLOUR(board.square[source]) != board.turn)))
            source = -1;
        return;
    }

    if (dest == -1)
    {
        dest = input;
        if (dest >= 0)
        {
            /* Destination square must not contain piece of current player. */
            if ((PIECE(board.square[dest]) != NONE) && (COLOUR(board.square[dest]) == board.turn))
            {
                dest = -1;
                /* We use currently selected square as source. */
                source = input;
            }
            else
                select_piece(-1);
        }
        return;
    }

    if (needprom == 1)
    {
        if (dialog_promote_piece != NONE)
            needprom = 2;
        return;
    }

    if ((needprom  == 0) && (((board.square[source] == WHITE_PAWN) && (dest >= 56)) ||
                             ((board.square[source] == BLACK_PAWN) && (dest <= 7))))
    {
        gg_dialog_open(dialog_promote_create(COLOUR(board.square[source])));
        needprom = 1;
        return;
    }

    move.source = source;
    move.destination = dest;
    if (needprom == 2)
        move.promotion_piece = dialog_promote_piece;
    else
        move.promotion_piece = NONE;
    needprom = 0;

    /* start_piece_move( source, dest ); */

    source = -1;
    dest = -1;
    game_make_move(&move, 1);
    reset_turn_counter();
}

/** SDL + OpenGL driver. */
ui_driver_t ui_sdlgl =
    {
        "sdlgl",
        sdlgl_init,
        resize,
        sdlgl_exit,
        do_menu,
        update,
        poll_move,
        show_message,
        show_result
    };
