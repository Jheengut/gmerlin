/*****************************************************************
 * gavl - a general purpose audio/video processing library
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
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

#include <stdlib.h>
#include <gavl/connectors.h>

struct gavl_audio_sink_s
  {
  gavl_audio_sink_get_func get_func;
  gavl_audio_sink_put_func put_func;
  void * priv;
  gavl_audio_format_t format;
  };

gavl_audio_sink_t *
gavl_audio_sink_create(gavl_audio_sink_get_func get_func,
                       gavl_audio_sink_put_func put_func,
                       void * priv,
                       const gavl_audio_format_t * format)
  {
  gavl_audio_sink_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->get_func = get_func;
  ret->put_func = put_func;
  ret->priv = priv;
  gavl_audio_format_copy(&ret->format, format);
  return ret;
  }

const gavl_audio_format_t *
gavl_audio_sink_get_format(gavl_audio_sink_t * s)
  {
  return &s->format;
  }

gavl_audio_frame_t *
gavl_audio_sink_get_frame(gavl_audio_sink_t * s)
  {
  if(s->get_func)
    return s->get_func(s->priv);
  else
    return NULL;
  }

gavl_sink_status_t
gavl_audio_sink_put_frame(gavl_audio_sink_t * s,
                          gavl_audio_frame_t * f)
  {
  return s->put_func(s->priv, f);
  }

void
gavl_audio_sink_destroy(gavl_audio_sink_t * s)
  {
  free(s);
  }
