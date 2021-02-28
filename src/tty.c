// The MIT License (MIT)
//
// Copyright (c) 2015 Stefan Arentz - http://github.com/st3fan/ewm
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "chr.h"
#include "sdl.h"
#include "tty.h"mm

struct ewm_tty_t *ewm_tty_create(SDL_Renderer *renderer, SDL_Color color) {
   struct ewm_tty_t *tty = malloc(sizeof(struct ewm_tty_t));
   memset(tty, 0, sizeof(struct ewm_tty_t));
   tty->renderer = renderer;
   tty->chr = ewm_chr_create("rom/3410036.bin", EWM_CHR_ROM_TYPE_2716, renderer);

   tty->pixels = malloc(4 * EWM_ONE_TTY_COLUMNS * ewm_chr_width(tty->chr) * EWM_ONE_TTY_ROWS * ewm_chr_height(tty->chr));
   tty->surface = SDL_CreateRGBSurfaceWithFormatFrom(tty->pixels, EWM_ONE_TTY_COLUMNS * ewm_chr_width(tty->chr),
      EWM_ONE_TTY_ROWS * ewm_chr_height(tty->chr), 32, 4 * EWM_ONE_TTY_COLUMNS * ewm_chr_width(tty->chr),
         ewm_sdl_pixel_format(renderer));

   tty->screen_cursor_enabled = 1;
   tty->color = SDL_MapRGBA(tty->surface->format, color.r, color.g, color.b, color.a);
   ewm_tty_reset(tty);
   return tty;
}

void ewm_tty_destroy(struct ewm_tty_t *tty) {
   // TODO
}

#if 0
static inline void ewm_tty_render_character(struct ewm_tty_t *tty, int row, int column, uint8_t c) {
   // TODO Should we learn chr.c about the Apple1 character set instead of mapping it to the Apple ][+ one?
   c += 0x80;
   if (tty->chr->characters[c] != NULL) {
      SDL_Rect dst;
      dst.x = column * 7;
      dst.y = row * 8;
      dst.w = 7;
      dst.h = 8;
      SDL_SetTextureColorMod(tty->chr->characters[c], 0, 255, 0);
      SDL_RenderCopy(tty->renderer, tty->chr->characters[c], NULL, &dst);
   }
}
#endif

// Take one - get something on the screen. Very inefficient to do it char-by-char, but good baseline.
static inline void ewm_tty_render_character(struct ewm_tty_t *tty, int row, int column, uint8_t c) {
   c += 0x80; // TODO This should not be there really
   uint32_t *src = tty->chr->bitmaps[c];
   if (src != NULL) {
      uint32_t *dst = tty->pixels + ((40 * 7 * 8) * row) + (7 * column);
      for (int y = 0; y < 8; y++) {
         for (int x = 0; x < 7; x++) {
            *dst++ = *src++ ? tty->color : 0; // TODO Can be optimized by moving color into chr
         }
         dst += (40 * 7) - 7;
      }
   } else {
      uint32_t *dst = tty->pixels + ((40 * 7 * 8) * row) + (7 * column);
      for (int y = 0; y < 8; y++) {
         for (int x = 0; x < 7; x++) {
            *dst++ = 0;
         }
         dst += (40 * 7) - 7;
      }
   }
}

static void tty_scroll_up(struct ewm_tty_t *tty) {
   memmove(tty->screen_buffer, &tty->screen_buffer[EWM_ONE_TTY_COLUMNS], (EWM_ONE_TTY_ROWS-1) * EWM_ONE_TTY_COLUMNS);
   memset(&tty->screen_buffer[(EWM_ONE_TTY_ROWS-1) * EWM_ONE_TTY_COLUMNS], 0, EWM_ONE_TTY_COLUMNS);
}

void ewm_tty_write(struct ewm_tty_t *tty, uint8_t v) {
   if (v == '\r') {
      tty->screen_cursor_column = 0;
      tty->screen_cursor_row++;
      if (tty->screen_cursor_row == EWM_ONE_TTY_ROWS) {
         tty->screen_cursor_row = EWM_ONE_TTY_ROWS - 1; // TODO Scroll the screen up
         tty_scroll_up(tty);
      }
   } else {
      tty->screen_buffer[(tty->screen_cursor_row * EWM_ONE_TTY_COLUMNS) + tty->screen_cursor_column] = v;
      tty->screen_cursor_column++;
      if (tty->screen_cursor_column == EWM_ONE_TTY_COLUMNS) {
         tty->screen_cursor_column = 0;
         tty->screen_cursor_row++;
         if (tty->screen_cursor_row == EWM_ONE_TTY_ROWS) {
            tty->screen_cursor_row = EWM_ONE_TTY_ROWS - 1; // TODO Scroll the screen up
            tty_scroll_up(tty);
         }
      }
   }
   tty->screen_dirty = 1;
}

void ewm_tty_reset(struct ewm_tty_t *tty) {
   for (int row = 0; row < EWM_ONE_TTY_ROWS; row++) {
      for (int column = 0; column < EWM_ONE_TTY_COLUMNS; column++) {
         tty->screen_buffer[(row * EWM_ONE_TTY_COLUMNS) + column] = 0x00;
      }
   }

   tty->screen_cursor_row = 0;
   tty->screen_cursor_column = 0;
   tty->screen_dirty = true;
}

void ewm_tty_set_line(struct ewm_tty_t *tty, int v, char *line) {
   if (v < 24) {
      char buf[41];
      snprintf(buf, 40, "%-40s", line);
      memcpy(tty->screen_buffer + (v * 40), buf, 40);
   }
}

void ewm_tty_refresh(struct ewm_tty_t *tty, uint32_t phase, uint32_t fps) {
   for (int row = 0; row < 24; row++) {
      for (int column = 0; column < 40; column++) {
         ewm_tty_render_character(tty, row, column, tty->screen_buffer[(row * EWM_ONE_TTY_COLUMNS) + column]);
      }
   }

   if (fps != 0) {
      if ((phase % (fps / 4)) == 0) {
         tty->screen_cursor_blink = !tty->screen_cursor_blink;
      }
   }

   if (tty->screen_cursor_enabled) {
      if (tty->screen_cursor_blink) {
         ewm_tty_render_character(tty, tty->screen_cursor_row, tty->screen_cursor_column, EWM_ONE_TTY_CURSOR_ON);
      } else {
         ewm_tty_render_character(tty, tty->screen_cursor_row, tty->screen_cursor_column, EWM_ONE_TTY_CURSOR_OFF);
      }
   }
}
