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

static float zerodepth=1.0f;

static int fps_enabled = 0;
static int frames = 0;
static Uint32 fps_time = 0;
static float fps;

float get_fps(void)
{
    return fps;
}

void update_fps_time(void)
{
    fps_time=SDL_GetTicks();
}

void toggle_show_fps(void)
{
    fps_enabled = 1 - fps_enabled;
}

static struct
{
    int x;
    int y;
}
mouse_pos;

float get_zerodepth(void)
{
    return zerodepth;
}

void set_mouse_pos( int x, int y )
{
    mouse_pos.x=x;
    mouse_pos.y=y;
}

int get_true_mouse_x(void)
{
    return mouse_pos.x;
}

int get_true_mouse_y(void)
{
    return mouse_pos.y;
}

int get_mouse_x(void)
{
    return ((float)mouse_pos.x/(float)get_screen_width())*640;
}

int get_mouse_y(void)
{
    return ((float)mouse_pos.y/(float)get_screen_height())*480;
}

/** @brief Computes smallest power of two that's larger than the input value.
 *
 *  @param input Input value.
 *  @return Smallest power of two that's larger than input.
 */
int power_of_two(int input)
{
    int value = 1;

    while ( value < input )
    {
        value <<= 1;
    }
    return value;
}

void go_3d(int width, int height)
{
    glViewport( 0, 0, width, height );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluPerspective(45.0f, 640.0f/480.0f, 1.0f, 100.0f);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
}

/** @brief Sets the OpenGL rendering options. */
void init_gl(void)
{
    /* Enable smooth shading */
    glShadeModel( GL_SMOOTH );

    /* Set the background black */
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    /* Depth buffer setup */
    glClearDepth( 1.0f );

    /* Enables Depth Testing */
    glEnable( GL_DEPTH_TEST );

    /* The Type Of Depth Test To Do */
    glDepthFunc( GL_LEQUAL );

    /* Really Nice Perspective Calculations */
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    #ifndef __BEOS__
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glReadPixels(100, 100, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &zerodepth);
    if ( zerodepth != 1.0f )
        DBG_WARN( "z depth should be 1.0f, but we got %f", zerodepth );
    #endif /* __BEOS__ */    

    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

/** @brief Resizes the OpenGL window.
 *
 *  @param width Desired width in pixels.
 *  @param height Desired height in pixels.
 */
void resize_window( int width, int height )
{
    glViewport( 0, 0, width, height );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -1, 1);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
}

/** @brief Swaps the OpenGL buffer.
 *
 *  Also maintains the frames-per-second counter.
 */
void gl_swap(void)
{
    static Uint32 last = 0;
    Uint32 now;

    if (fps_enabled)
    {
        char fps_s[16];

        snprintf(fps_s, 16, "FPS: %.2f", fps);
        text_draw_string(10, 10, fps_s, 1, get_col(COL_RED));
    }

    SDL_GL_SwapBuffers();
    now = SDL_GetTicks();
    if (now - last < 1000 / FPS)
        SDL_Delay(1000 / FPS - (now - last));
    last = SDL_GetTicks();

    frames++;
    if (frames == 10)
    {
        fps = 10000 / (float) (now - fps_time);
        frames = 0;
        fps_time = now;
    }
}
