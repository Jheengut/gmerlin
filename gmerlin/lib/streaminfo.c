/*****************************************************************
 
  streaminfo.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <gavl/gavl.h>
#include <parameter.h>
#include <streaminfo.h>
#include <utils.h>

#define MY_FREE(ptr) \
  if(ptr) \
    { \
    free(ptr); \
    ptr = NULL; \
    }

#define CS(str) dst->str = bg_strdup(dst->str, src->str);

void bg_audio_info_copy(bg_audio_info_t * dst,
                        const bg_audio_info_t * src)
  {
  gavl_audio_format_copy(&(dst->format), &(src->format));
  
  CS(language);
  CS(description);
  }

void bg_video_info_copy(bg_video_info_t * dst,
                        const bg_video_info_t * src)
  {
  gavl_video_format_copy(&(dst->format), &(src->format));
  CS(description);
  }

void bg_audio_info_free(bg_audio_info_t * info)
  {
  MY_FREE(info->description);
  MY_FREE(info->language);
  }

void bg_video_info_free(bg_video_info_t * info)
  {
  MY_FREE(info->description);
  }

void bg_track_info_free(bg_track_info_t * info)
  {
  int i;

  if(info->audio_streams)
    {
    for(i = 0; i < info->num_audio_streams; i++)
      bg_audio_info_free(&(info->audio_streams[i]));
    MY_FREE(info->audio_streams);
    }

  if(info->video_streams)
    {
    for(i = 0; i < info->num_video_streams; i++)
      bg_video_info_free(&(info->video_streams[i]));
    MY_FREE(info->video_streams);
    }
  if(info->subpicture_streams)
    {
    for(i = 0; i < info->num_subpicture_streams; i++)
      MY_FREE(info->subpicture_streams[i].language);
    MY_FREE(info->subpicture_streams);
    }

  bg_metadata_free(&(info->metadata));
  
  MY_FREE(info->name);
  MY_FREE(info->description);
  MY_FREE(info->url);
  memset(info, 0, sizeof(*info));
  }

/*
 *  %p:    Artist
 *  %a:    Album
 *  %g:    Genre
 *  %t:    Track name
 *  %<d>n: Track number (d = number of digits, 1-9)
 *  %y:    Year
 *  %c:    Comment
 */

char * bg_create_track_name(const bg_track_info_t * info,
                            const char * format)
  {
  char * buf;
  const char * end;
  const char * f;
  char * ret = (char*)0;
  char track_format[5];
  f = format;

  while(*f != '\0')
    {
    end = f;
    while((*end != '%') && (*end != '\0'))
      end++;
    if(end != f)
      ret = bg_strncat(ret, f, end);

    if(*end == '%')
      {
      end++;

      /* %p:    Artist */
      if(*end == 'p')
        {
        end++;
        if(info->metadata.artist)
          {
          //          fprintf(stderr,  "Artist: %s ", info->metadata.artist);
          ret = bg_strcat(ret, info->metadata.artist);
          //          fprintf(stderr,  "%s\n", ret);
          }
        else
          {
          //          fprintf(stderr, "No Artist\n");
          goto fail;
          }
        }
      /* %a:    Album */
      else if(*end == 'a')
        {
        end++;
        if(info->metadata.album)
          ret = bg_strcat(ret, info->metadata.album);
        else
          goto fail;
        }
      /* %g:    Genre */
      else if(*end == 'g')
        {
        end++;
        if(info->metadata.genre)
          ret = bg_strcat(ret, info->metadata.genre);
        else
          goto fail;
        }
      /* %t:    Track name */
      else if(*end == 't')
        {
        end++;
        if(info->metadata.title)
          {
          //          fprintf(stderr, "Ret: %s\n", ret);
          ret = bg_strcat(ret, info->metadata.title);
          //          fprintf(stderr, "Title: %s\n", info->metadata.title);
          }
        else
          {
          //          fprintf(stderr, "No Title\n");
          goto fail;
          }
        }
      /* %c:    Comment */
      else if(*end == 'c')
        {
        end++;
        if(info->metadata.comment)
          ret = bg_strcat(ret, info->metadata.comment);
        else
          goto fail;
        }
      /* %y:    Year */
      else if(*end == 'y')
        {
        end++;
        if(info->metadata.date)
          {
          buf = bg_sprintf("%s", info->metadata.date);
          ret = bg_strcat(ret, buf);
          free(buf);
          }
        else
          goto fail;
        }
      /* %<d>n: Track number (d = number of digits, 1-9) */
      else if(isdigit(*end) && end[1] == 'n')
        {
        if(info->metadata.track)
          {
          track_format[0] = '%';
          track_format[1] = '0';
          track_format[2] = *end;
          track_format[3] = 'd';
          track_format[4] = '\0';
          
          buf = bg_sprintf(track_format, info->metadata.track);
          ret = bg_strcat(ret, buf);
          free(buf);
          end+=2;
          }
        else
          goto fail;
        }
      else
        {
        ret = bg_strcat(ret, "%");
        end++;
        }
      f = end;
      }
    }
  return ret;
  fail:
  if(ret)
    free(ret);
  return (char*)0;
  }

void bg_set_track_name_default(bg_track_info_t * info,
                               const char * location)
  {
  const char * start_pos;
  const char * end_pos;
  
  if(bg_string_is_url(location))
    {
    info->name = bg_strdup(info->name, location);
    return;
    }
  
  start_pos = strrchr(location, '/');
  if(start_pos)
    start_pos++;
  else
    start_pos = location;
  end_pos = strrchr(start_pos, '.');
  if(!end_pos)
    end_pos = &(start_pos[strlen(start_pos)]);
  info->name = bg_strndup(info->name, start_pos, end_pos);
  
  }
