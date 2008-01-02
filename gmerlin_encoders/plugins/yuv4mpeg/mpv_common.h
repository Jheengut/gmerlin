/*****************************************************************
 * gmerlin-encoders - encoder plugins for gmerlin
 *
 * Copyright (c) 2001 - 2008 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include "y4m_common.h"

/* Common defintions and routines for driving mpeg2enc */

typedef struct
  {
  int format;       /* -f */
  int bitrate;      /* -b */
  int video_buffer; /* -V */
  int bframes;      /* -R */

  int bitrate_mode; /* -cbr, -q ... */
  int quantization; /* -q */
  char * quant_matrix; /* -K */
  
  char * user_options;
  bg_subprocess_t * mpeg2enc;
  bg_y4m_common_t y4m;
  sigset_t oldset;
  } bg_mpv_common_t;

bg_parameter_info_t * bg_mpv_get_parameters();

/* Must pass a bg_mpv_common_t for data */
void bg_mpv_set_parameter(void * data, const char * name, const bg_parameter_value_t * val);

int bg_mpv_open(bg_mpv_common_t * com, const char * filename);

void bg_mpv_set_format(bg_mpv_common_t * com, const gavl_video_format_t * format);
void bg_mpv_get_format(bg_mpv_common_t * com, gavl_video_format_t * format);

int bg_mpv_start(bg_mpv_common_t * com);

int bg_mpv_write_video_frame(bg_mpv_common_t * com, gavl_video_frame_t * frame);

int bg_mpv_close(bg_mpv_common_t * com);


#if 0
char * bg_mpv_make_commandline(bg_mpv_common_t * com, const char * filename);


/* Adjust a video format into something, mpeg2enc will process without errors */

void bg_mpv_adjust_framerate(gavl_video_format_t * format);
void bg_mpv_adjust_interlacing(gavl_video_format_t * format,
                               int mpeg_format);

int bg_mpv_get_chroma_mode(bg_mpv_common_t * com);
#endif

const char * bg_mpv_get_extension(bg_mpv_common_t * com);


