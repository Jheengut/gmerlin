/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
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

#include <config.h>

#include <sys/types.h> /* stat() */
#include <sys/stat.h>  /* stat() */
#include <unistd.h>    /* stat() */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <gmerlin/utils.h>
#include <gmerlin/pluginregistry.h>
#include <gmerlin/subprocess.h>

#include <gmerlin/log.h>
#define LOG_DOMAIN "thumbnails"

static int thumbnail_up_to_date(const char * gml,
                                bg_plugin_registry_t * plugin_reg,
                                const char * thumbnail_file,
                                gavl_video_frame_t ** frame,
                                gavl_video_format_t * format)
  {
  struct stat st;
  int i;
  bg_metadata_t metadata;
  int64_t mtime;
  int ret = 0;
  memset(&metadata, 0, sizeof(metadata));

  if(stat(gml, &st))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot stat %s: %s",
           gml, strerror(errno));
    return 0;
    }
  
  *frame = bg_plugin_registry_load_image(plugin_reg,
                                         thumbnail_file,
                                         format,
                                         &metadata);
  
  i = 0;
  if(metadata.ext)
    {
    while(metadata.ext[i].key)
      {
      if(!strcmp(metadata.ext[i].key, "Thumb::MTime"))
        {
        mtime = strtoll(metadata.ext[i].value, (char**)0, 10);
        if(mtime == st.st_mtime)
          ret = 1;
        break;
        }
      }
    }
  bg_metadata_free(&metadata);
  return ret;
  }

static void make_fail_thumbnail(const char * gml)
  {
  
  }
                                

int bg_get_thumbnail(const char * gml,
                     bg_plugin_registry_t * plugin_reg,
                     char ** thumbnail_filename_ret,
                     gavl_video_frame_t ** frame_ret,
                     gavl_video_format_t * format_ret)
  {
  bg_subprocess_t * sp;
  char hash[33];
  char * home_dir;
  
  char * thumb_filename_normal = NULL;
  char * thumb_filename_fail = NULL;

  char * thumbs_dir_normal = NULL;
  char * thumbs_dir_fail = NULL;
  char * command;
  
  int ret = 0;
  gavl_video_frame_t * frame = NULL;
  gavl_video_format_t format;
  
  /* Get and create directories */
  home_dir = getenv("HOME");
  if(!home_dir)
    return 0;
  
  thumbs_dir_normal = bg_sprintf("%s/.thumbnails/normal",       home_dir);
  thumbs_dir_fail   = bg_sprintf("%s/.thumbnails/fail/gmerlin", home_dir);
  
  if(!bg_ensure_directory(thumbs_dir_normal) ||
     !bg_ensure_directory(thumbs_dir_fail))
    goto done;
  
  bg_get_filename_hash(gml, hash);

  thumb_filename_normal = bg_sprintf("%s/%s.png", thumbs_dir_normal, hash);
  
  if(access(thumb_filename_normal, R_OK)) /* Thumbnail file not present */
    {
    /* Check if there is a failed thumbnail */
    thumb_filename_fail = bg_sprintf("%s/%s.png", thumbs_dir_fail, hash);
    if(!access(thumb_filename_fail, R_OK))
      {
      if(thumbnail_up_to_date(gml, plugin_reg, thumb_filename_fail,
                              &frame, &format))
        {
        gavl_video_frame_destroy(frame);
        frame = NULL;
        goto done;
        }
      else /* Failed thumbnail is *not* up to date, remove it */
        {
        remove(thumb_filename_fail);
        gavl_video_frame_destroy(frame);
        frame = NULL;
        }
      }
    /* else: Regenerate */
    }
  else /* Thumbnail file present */
    {
    /* Check if the thumbnail is recent */
    if(thumbnail_up_to_date(gml, plugin_reg, thumb_filename_normal,
                            &frame, &format))
      {
      if(thumbnail_filename_ret)
        {
        *thumbnail_filename_ret = thumb_filename_normal;
        thumb_filename_normal = NULL;
        }
      if(frame_ret)
        {
        *frame_ret = frame;
        frame = NULL;
        }
      if(format_ret)
        gavl_video_format_copy(format_ret, &format);
      ret = 1;
      goto done;
      }
    else
      {
      remove(thumb_filename_normal);
      gavl_video_frame_destroy(frame);
      frame = NULL;
      }
    /* else: Regenerate */
    }

  /* Regenerate */
  command = bg_sprintf("gmerlin-video-thumbnailer '%s' '%s'", gml, thumb_filename_normal);
  sp = bg_subprocess_create(command, 0, 0, 0);
  bg_subprocess_close(sp);
  free(command);
  
  if(!access(thumb_filename_normal, R_OK)) /* Thumbnail generation succeeded */
    {
    if(thumbnail_filename_ret)
      {
      *thumbnail_filename_ret = thumb_filename_normal;
      thumb_filename_normal = NULL;
      }
    if(frame_ret)
      {
      *frame_ret = frame;
      frame = NULL;
      }
    if(format_ret)
      gavl_video_format_copy(format_ret, &format);
    ret = 1;
    goto done;
    }
  else /* Thumbnail generation failed */
    {
    make_fail_thumbnail(gml);
    }
  
  done:
  
  free(thumbs_dir_normal);
  free(thumbs_dir_fail);

  if(thumb_filename_normal)
    free(thumb_filename_normal);
  if(thumb_filename_fail)
    free(thumb_filename_fail);
  if(frame)
    gavl_video_frame_destroy(frame);
  
  return ret;
  }