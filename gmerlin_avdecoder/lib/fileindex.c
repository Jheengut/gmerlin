/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <md5.h>
#include <limits.h>

#include <dirent.h>
#include <ctype.h>

#define LOG_DOMAIN "fileindex"

#define INDEX_SIGNATURE "BGAVINDEX"

/* Version must be increased each time the fileformat
   changes */
#define INDEX_VERSION 2

static void dump_index(bgav_stream_t * s)
  {
  int i;
  for(i = 0; i < s->file_index->num_entries; i++)
    {
    bgav_dprintf("      K: %d, P: %"PRId64", T: %"PRId64" D: ",
                 !!(s->file_index->entries[i].flags & PACKET_FLAG_KEY),
                 s->file_index->entries[i].position,
                 s->file_index->entries[i].time);
    
    if(i < s->file_index->num_entries-1)
      bgav_dprintf("%"PRId64" posdiff: %"PRId64"\n",
                   s->file_index->entries[i+1].time-s->file_index->entries[i].time,
                   s->file_index->entries[i+1].position-s->file_index->entries[i].position
                   );
    else
      bgav_dprintf("%"PRId64"\n", s->duration-s->file_index->entries[i].time);
    }
  }

void bgav_file_index_dump(bgav_t * b)
  {
  int i, j;
  bgav_stream_t * s;
  if(!b->tt->tracks[0].has_file_index)
    {
    bgav_dprintf("No index available\n");
    return;
    }

  bgav_dprintf("Generated index table(s)\n");
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    bgav_dprintf(" Track %d\n", i+1);
    
    for(j = 0; j < b->tt->tracks[i].num_audio_streams; j++)
      {
      s = &b->tt->tracks[i].audio_streams[j];
      if(!s->file_index)
        continue;
      bgav_dprintf("   Audio stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->timescale,
                   s->first_timestamp);
      bgav_dprintf("   Duration: %ld\n", b->tt->tracks[i].audio_streams[j].duration);
      dump_index(&b->tt->tracks[i].audio_streams[j]);
      }
    for(j = 0; j < b->tt->tracks[i].num_video_streams; j++)
      {
      s = &b->tt->tracks[i].video_streams[j];
      if(!s->file_index)
        continue;
      bgav_dprintf("   Video stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->timescale,
                   s->first_timestamp);
      bgav_dprintf("   Duration: %ld\n", b->tt->tracks[i].video_streams[j].duration);
      dump_index(&b->tt->tracks[i].video_streams[j]);
      }
    for(j = 0; j < b->tt->tracks[i].num_subtitle_streams; j++)
      {
      s = &b->tt->tracks[i].subtitle_streams[j];
      if(!s->file_index)
        continue;
      bgav_dprintf("   Subtitle stream %d [ID: %08x, Timescale: %d, PTS offset: %"PRId64"]\n", j+1,
                   s->stream_id, s->timescale,
                   s->first_timestamp);
      bgav_dprintf("   Duration: %ld\n", b->tt->tracks[i].subtitle_streams[j].duration);
      dump_index(&b->tt->tracks[i].subtitle_streams[j]);
      }
    }
  }

bgav_file_index_t * bgav_file_index_create()
  {
  bgav_file_index_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bgav_file_index_destroy(bgav_file_index_t * idx)
  {
  if(idx->entries)
    free(idx->entries);
  free(idx);
  }

void
bgav_file_index_append_packet(bgav_file_index_t * idx,
                              int64_t position,
                              int64_t time,
                              int flags)
  {
  if(idx->num_entries >= idx->entries_alloc)
    {
    idx->entries_alloc += 512;
    idx->entries = realloc(idx->entries,
                           idx->entries_alloc * sizeof(*idx->entries));
    }
  /* First frame is always a keyframe */
  if(!idx->num_entries)
    flags |= PACKET_FLAG_KEY;
    
  idx->entries[idx->num_entries].position = position;
  idx->entries[idx->num_entries].time     = time;
  idx->entries[idx->num_entries].flags    = flags;
  idx->num_entries++;
  }

/*
 * File I/O.
 *
 * Format:
 *
 * All multibyte numbers are big endian
 * (network byte order)
 *
 * 1. Signature "BGAVINDEX <version>\n"
 *    (Version is the INDEX_VERSION defined above)
 * 2. Filename terminated with \n
 * 3. File time (st_mtime returned by stat(2)) (64
 * 4. Number of tracks (32)
 * 5. Tracks consising of
 *    5.1 Number of streams (32)
 *    5.2 Stream entries consisting of
 *        5.2.2 Stream ID (32)
 *        5.2.3 Timescale (32)
 *        5.2.4 Timestamp offset (64)
 *        5.2.5 Duration (64)
 *        5.2.6 Number of entries (32)
 *        5.2.7 Index entries consiting of
 *              5.2.7.1 1 if frame is a keyframe, 0 else (8)
 *              5.2.7.2 position (64)
 *              5.2.7.3 time (64)
 */


int bgav_file_index_read_header(const char * filename,
                                bgav_input_context_t * input,
                                int * num_tracks)
  {
  int ret = 0;
  uint64_t file_time;
  char * line = (char *)0;
  int line_alloc = 0;
  uint32_t ntracks;
  struct stat stat_buf;
  int sig_len;
  sig_len = strlen(INDEX_SIGNATURE);
  
  if(!bgav_input_read_line(input, &line, &line_alloc, 0, NULL))
    goto fail;
  /* Check signature */
  if(strncmp(line, INDEX_SIGNATURE, sig_len))
    goto fail;
  /* Check version */
  if((strlen(line) < sig_len + 2) || !isdigit(*(line + sig_len + 1)))
    goto fail;
  if(atoi(line + sig_len + 1) != INDEX_VERSION)
    goto fail;
  
  if(!bgav_input_read_line(input, &line, &line_alloc, 0, NULL))
    goto fail;
  /* Check filename */
  if(strcmp(line, filename))
    goto fail;
  if(!bgav_input_read_64_be(input, &file_time))
    goto fail;
  
  /* Don't do this check if we have uuid's as names */
  
  if(filename[0] == '/')
    {
    if(stat(filename, &stat_buf))
      goto fail;
    if(file_time != stat_buf.st_mtime)
      goto fail;
    }
  if(!bgav_input_read_32_be(input, &ntracks))
    goto fail;
  
  *num_tracks = ntracks;
  ret = 1;
  fail:
  if(line)
    free(line);
  return ret;
  }

static void write_64(FILE * out, uint64_t i)
  {
  uint8_t buf[8];
  BGAV_64BE_2_PTR(i, buf);
  fwrite(buf, 8, 1, out);
  }

static void write_32(FILE * out, uint32_t i)
  {
  uint8_t buf[4];
  BGAV_32BE_2_PTR(i, buf);
  fwrite(buf, 4, 1, out);
  }
#if 0
static void write_8(FILE * out, uint8_t i)
  {
  fwrite(&i, 1, 1, out);
  }
#endif
void bgav_file_index_write_header(const char * filename,
                                  FILE * output,
                                  int num_tracks)
  {
  uint64_t file_time = 0;
  struct stat stat_buf;

  fprintf(output, "%s %d\n", INDEX_SIGNATURE, INDEX_VERSION);
  fprintf(output, "%s\n", filename);

  if(filename[0] == '/')
    {
    if(stat(filename, &stat_buf))
      return;
    file_time = stat_buf.st_mtime;
    }
  write_64(output, file_time);
  write_32(output, num_tracks);
  }

static bgav_file_index_t *
file_index_read_stream(bgav_input_context_t * input, bgav_stream_t * s)
  {
  int i;

  bgav_file_index_t * ret = calloc(1, sizeof(*ret));

  if(!bgav_input_read_32_be(input, (uint32_t*)&s->timescale))
    return NULL;
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->first_timestamp))
    return NULL;
  if(!bgav_input_read_64_be(input, (uint64_t*)&s->duration))
    return NULL;
  if(!bgav_input_read_32_be(input, &ret->num_entries))
    return NULL;
  
  ret->entries = calloc(ret->num_entries, sizeof(*ret->entries));

  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_32_be(input, &ret->entries[i].flags) ||
       !bgav_input_read_64_be(input, &ret->entries[i].position) ||
       !bgav_input_read_64_be(input, &ret->entries[i].time))
      return NULL;
    }
  return ret;
  }

static void
file_index_write_stream(FILE * output,
                        bgav_file_index_t * idx, bgav_stream_t * s)
  {
  int i;
  
  write_32(output, s->stream_id);
  write_32(output, s->timescale);
  write_64(output, s->first_timestamp);
  write_64(output, s->duration);
  write_32(output, idx->num_entries);

  for(i = 0; i < idx->num_entries; i++)
    {
    write_32(output, idx->entries[i].flags);
    write_64(output, idx->entries[i].position);
    write_64(output, idx->entries[i].time);
    }
  }

static void update_duration(bgav_stream_t * s, gavl_time_t * duration)
  {
  gavl_time_t duration1;
  
  duration1 = gavl_time_unscale(s->timescale, s->duration);
  if((*duration == GAVL_TIME_UNDEFINED) || (duration1 > *duration))
    *duration = duration1;
  }

static void set_has_file_index(bgav_t * b)
  {
  int i, j;
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    b->tt->tracks[i].has_file_index = 1;
    b->tt->tracks[i].sample_accurate = 1;

    if(b->tt->tracks[i].duration == GAVL_TIME_UNDEFINED)
      {
      for(j= 0; j <b->tt->tracks[i].num_audio_streams; j++)
        update_duration(&b->tt->tracks[i].audio_streams[j], &b->tt->tracks[i].duration);
      for(j= 0; j <b->tt->tracks[i].num_video_streams; j++)
        update_duration(&b->tt->tracks[i].video_streams[j], &b->tt->tracks[i].duration);
      for(j= 0; j <b->tt->tracks[i].num_subtitle_streams; j++)
        update_duration(&b->tt->tracks[i].subtitle_streams[j], &b->tt->tracks[i].duration);
      }
    }
  b->demuxer->flags |= BGAV_DEMUXER_CAN_SEEK;
  }

int bgav_read_file_index(bgav_t * b)
  {
  int i, j;
  bgav_input_context_t * input = (bgav_input_context_t*)0;
  int num_tracks;
  uint32_t num_streams;
  int stream_id;
  char * filename;
  bgav_stream_t * s;
  /* Check if the input provided an index filename */
  if(!b->input->index_file || !b->input->filename)
    return 0;

  filename =
    bgav_search_file_read(&b->opt,
                          "indices", b->input->index_file);
  if(!filename)
    goto fail;
  
  input = bgav_input_create(&b->opt);
  if(!bgav_input_open(input, filename))
    goto fail;

  if(!bgav_file_index_read_header(b->input->filename,
                                  input, &num_tracks))
    goto fail;

  if(num_tracks != b->tt->num_tracks)
    goto fail;

  for(i = 0; i < num_tracks; i++)
    {
    if(!bgav_input_read_32_be(input, &num_streams))
      goto fail;
    
    for(j = 0; j < num_streams; j++)
      {
      if(!bgav_input_read_32_be(input, (uint32_t*)&stream_id))
        goto fail;
      s = bgav_track_find_stream_all(&b->tt->tracks[i], stream_id);
      if(!s)
        goto fail;
      s->file_index = file_index_read_stream(input, s);
      
      if(!s->file_index)
        {
        goto fail;
        }
      }
        
    }
  bgav_input_destroy(input);
  set_has_file_index(b);
  free(filename);
  return 1;
  fail:

  if(input)
    bgav_input_destroy(input);
  
  if(filename)
    free(filename);
  return 0;
  }

typedef struct
  {
  char * name;
  off_t size;
  time_t time;
  } index_file_t;

static void purge_cache(const char * filename,
                        int max_size, const bgav_options_t * opt)
  {
  int num_files;
  int files_alloc;
  index_file_t * files = (index_file_t *)0;
  int64_t total_size;
  int64_t max_total_size;
  time_t time_min;
  int index;
  char * directory, *pos;
  DIR * dir;
  struct dirent * res;
  struct stat st;
  int i;
  
  union
    {
    struct dirent d;
    char b[sizeof (struct dirent) + NAME_MAX];
    } u;

  directory = bgav_strdup(filename);
  pos = strrchr(directory, '/');
  if(!pos)
    {
    free(directory);
    return;
    }
  *pos = '\0';

  dir = opendir(directory);
  if(!dir)
    return;
  
  /* Get all index files with their sizes and mtimes */
  num_files = 0;
  files_alloc = 0;
  total_size = 0;
  while(!readdir_r(dir, &u.d, &res))
    {
    if(!res)
      break;
    
    if(res->d_type == DT_REG)
      {
      if(num_files + 1 > files_alloc)
        {
        files_alloc += 128;
        files = realloc(files, files_alloc * sizeof(*files));
        memset(files + num_files, 0,
               (files_alloc - num_files) * sizeof(*files));
        }
      files[num_files].name = bgav_sprintf("%s/%s", directory, res->d_name);

      if(!stat(files[num_files].name, &st))
        {
        files[num_files].time = st.st_mtime;
        files[num_files].size = st.st_size;
        }
      total_size += files[num_files].size;
      num_files++;
      }
    }
  closedir(dir);
  
  max_total_size = (int64_t)max_size * 1024 * 1024;

  while(total_size > max_total_size)
    {
    /* Look for the file to be deleted */
    time_min = 0;
    index = -1;
    for(i = 0; i < num_files; i++)
      {
      if(files[i].time &&
         ((files[i].time < time_min) || !time_min))
        {
        time_min = files[i].time;
        index = i;
        }
      }
    if(index == -1)
      break;
    bgav_log(opt, BGAV_LOG_INFO, LOG_DOMAIN,
             "Removing %s to keep maximum cache size", files[index].name);
    remove(files[index].name);
    files[index].time = 0;
    total_size -= files[index].size;
    }
  
  for(i = 0; i < num_files; i++)
    {
    if(files[i].name) free(files[i].name);
    }
  free(files);
  }

void bgav_write_file_index(bgav_t * b)
  {
  int i, j;
  FILE * output;
  char * filename;
  int num_streams;
  bgav_stream_t * s;
  /* Check if the input provided an index filename */
  if(!b->input->index_file || !b->input->filename)
    return;
  
  filename = 
    bgav_search_file_write(&b->opt,
                           "indices", b->input->index_file);
  
  output = fopen(filename, "w");
  
  bgav_file_index_write_header(b->input->filename,
                               output,
                               b->tt->num_tracks);
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    num_streams = 0;
    for(j = 0; j < b->tt->tracks[i].num_audio_streams; j++)
      {
      if(b->tt->tracks[i].audio_streams[j].file_index)
        num_streams++;
      }
    for(j = 0; j < b->tt->tracks[i].num_video_streams; j++)
      {
      if(b->tt->tracks[i].video_streams[j].file_index)
        num_streams++;
      }
    for(j = 0; j < b->tt->tracks[i].num_subtitle_streams; j++)
      {
      if(b->tt->tracks[i].subtitle_streams[j].file_index)
        num_streams++;
      }
    
    write_32(output, num_streams);
    
    for(j = 0; j < b->tt->tracks[i].num_audio_streams; j++)
      {
      s = &b->tt->tracks[i].audio_streams[j];
      if(s->file_index)
        file_index_write_stream(output, s->file_index, s);
      }
    for(j = 0; j < b->tt->tracks[i].num_video_streams; j++)
      {
      s = &b->tt->tracks[i].video_streams[j];
      if(s->file_index)
        file_index_write_stream(output, s->file_index, s);
      }
    for(j = 0; j < b->tt->tracks[i].num_subtitle_streams; j++)
      {
      s = &b->tt->tracks[i].subtitle_streams[j];
      if(s->file_index)
        file_index_write_stream(output, s->file_index, s);
      }
    }
  fclose(output);
  
  if(b->opt.cache_size > 0)
    purge_cache(filename, b->opt.cache_size, &b->opt);
  
  free(filename);

  }

/*
 *  Top level packets contain complete frames of one elemtary stream
 */

static void flush_stream_simple(bgav_stream_t * s)
  {
  bgav_packet_t * p;
  while(bgav_demuxer_peek_packet_read(s->demuxer, s, 0))
    {
    p = bgav_demuxer_get_packet_read(s->demuxer, s);

    /* This will omit B-frames */
    if(p->pts >= s->duration)
      {
      bgav_file_index_append_packet(s->file_index,
                                    p->position, p->pts, p->flags);
      s->duration = p->pts + p->duration;
      }
    bgav_demuxer_done_packet_read(s->demuxer, p);
    }
  }

static void flush_stream_pts(bgav_stream_t * s, int force)
  {
  bgav_packet_t * p;
  while(bgav_demuxer_peek_packet_read(s->demuxer, s, force))
    {
    p = bgav_demuxer_get_packet_read(s->demuxer, s);
    /* We don't output B-Frames (recognizable by non-mobotone PTSes) */
    if((p->pts != BGAV_TIMESTAMP_UNDEFINED) &&
       (p->pts >= s->out_time))
      {
      bgav_file_index_append_packet(s->file_index,
                                    p->position, p->pts, p->flags);
      s->out_time = p->pts;
      if(p->pts > s->duration)
        s->duration = p->pts;
      }
    bgav_demuxer_done_packet_read(s->demuxer, p);
    }
  }

static int build_file_index_simple(bgav_t * b)
  {
  int j;
  int64_t old_position;
  old_position = b->input->position;
  
  while(1)
    {
    if(!bgav_demuxer_next_packet(b->demuxer))
      return 1;
    
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      flush_stream_simple(&b->tt->cur->audio_streams[j]);
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      flush_stream_simple(&b->tt->cur->video_streams[j]);
    for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
      flush_stream_simple(&b->tt->cur->subtitle_streams[j]);
    }
  
  bgav_input_seek(b->input, old_position, SEEK_SET);
  return 1;
  }

/*
 *  Top level packets contain frames of one elemtary stream
 *  framed at codec level
 */

static int flush_stream_mpeg_audio(bgav_stream_t * s)
  {
  if(!s->data.audio.decoder->decoder->parse)
    return 0;
  s->data.audio.decoder->decoder->parse(s);
  return 1;
  }

static int flush_stream_mpeg_video(bgav_stream_t * s, int flush)
  {
  if(!s->data.video.decoder->decoder->parse)
    return 0;
  s->data.video.decoder->decoder->parse(s, flush);
  return 1;
  }

static int flush_stream_mpeg_subtitle(bgav_stream_t * s)
  {
  return 0;
  }

static int build_file_index_mpeg(bgav_t * b)
  {
  int j;
  int64_t old_position;
  old_position = b->input->position;
  
  while(1)
    {
    /* Process one packet */
    if(!bgav_demuxer_next_packet(b->demuxer))
      return 1;

    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      if(!flush_stream_mpeg_audio(&b->tt->cur->audio_streams[j]))
        return 0;
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      if(!flush_stream_mpeg_video(&b->tt->cur->video_streams[j], 0))
        return 0;
      }
    for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
      {
      if(!b->tt->cur->subtitle_streams[j].data.subtitle.subreader)
        {
        if(!flush_stream_mpeg_subtitle(&b->tt->cur->subtitle_streams[j]))
          return 0;
        }
      }
    if(b->opt.index_callback)
      b->opt.index_callback(b->opt.index_callback_data,
                            (float)b->input->position / 
                            (float)b->input->total_bytes);
    }

  for(j = 0; j < b->tt->cur->num_video_streams; j++)
    {
    if(!flush_stream_mpeg_video(&b->tt->cur->video_streams[j], 1))
      return 0;
    }

  
  bgav_input_seek(b->input, old_position, SEEK_SET);
  return 1;
  }

static int build_file_index_mixed(bgav_t * b)
  {
  int j, eof = 0;
  int64_t old_position;
  old_position = b->input->position;
  
  while(!eof)
    {
    /* Process one packet */
    if(!bgav_demuxer_next_packet(b->demuxer))
      eof = 1;
    
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      switch(b->tt->cur->audio_streams[j].index_mode)
        {
        case INDEX_MODE_MPEG:
          if(!flush_stream_mpeg_audio(&b->tt->cur->audio_streams[j]))
            return 0;
          break;
        case INDEX_MODE_SIMPLE:
          flush_stream_simple(&b->tt->cur->audio_streams[j]);
          break;
        case INDEX_MODE_PTS:
          flush_stream_pts(&b->tt->cur->audio_streams[j], 0);
          break;
        }
      
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      switch(b->tt->cur->video_streams[j].index_mode)
        {
        case INDEX_MODE_MPEG:
          if(!flush_stream_mpeg_video(&b->tt->cur->video_streams[j], 0))
            return 0;
          break;
        case INDEX_MODE_SIMPLE:
          flush_stream_simple(&b->tt->cur->video_streams[j]);
          break;
        case INDEX_MODE_PTS:
          flush_stream_pts(&b->tt->cur->video_streams[j], 0);
          break;
        }
      }
    for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
      {
      if(!b->tt->cur->subtitle_streams[j].data.subtitle.subreader)
        {
        switch(b->tt->cur->subtitle_streams[j].index_mode)
          {
          case INDEX_MODE_MPEG:
            if(!flush_stream_mpeg_subtitle(&b->tt->cur->subtitle_streams[j]))
              return 0;
            break;
          case INDEX_MODE_SIMPLE:
            flush_stream_simple(&b->tt->cur->subtitle_streams[j]);
            break;
          case INDEX_MODE_PTS:
            flush_stream_pts(&b->tt->cur->subtitle_streams[j], 0);
            break;
          }
        }
      }
    
    if(b->opt.index_callback)
      b->opt.index_callback(b->opt.index_callback_data,
                            (float)b->input->position / 
                            (float)b->input->total_bytes);
    }

  for(j = 0; j < b->tt->cur->num_audio_streams; j++)
    {
    switch(b->tt->cur->audio_streams[j].index_mode)
      {
      case INDEX_MODE_PTS:
        flush_stream_pts(&b->tt->cur->audio_streams[j], 1);
        break;
      }
      
    }
  for(j = 0; j < b->tt->cur->num_video_streams; j++)
    {
    switch(b->tt->cur->video_streams[j].index_mode)
      {
      case INDEX_MODE_PTS:
        flush_stream_pts(&b->tt->cur->video_streams[j], 1);
        break;
      case INDEX_MODE_MPEG:
        if(!flush_stream_mpeg_video(&b->tt->cur->video_streams[j], 1))
          return 0;
      }
    }
  for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
    {
    if(!b->tt->cur->subtitle_streams[j].data.subtitle.subreader)
      {
      switch(b->tt->cur->subtitle_streams[j].index_mode)
        {
        case INDEX_MODE_PTS:
          flush_stream_pts(&b->tt->cur->subtitle_streams[j], 1);
          break;
        }
      }
    }
  
  bgav_input_seek(b->input, old_position, SEEK_SET);
  return 1;
  }


static int bgav_build_file_index_parseall(bgav_t * b)
  {
  int i, j;
  int ret = 0;
  bgav_stream_t * s;
  
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    bgav_select_track(b, i);
    b->demuxer->flags |= BGAV_DEMUXER_BUILD_INDEX;
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      b->tt->cur->audio_streams[j].file_index = bgav_file_index_create();
      bgav_set_audio_stream(b, j, BGAV_STREAM_PARSE);
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      b->tt->cur->video_streams[j].file_index = bgav_file_index_create();
      bgav_set_video_stream(b, j, BGAV_STREAM_PARSE);
      }
    for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
      {
      if(!b->tt->cur->subtitle_streams[j].data.subtitle.subreader)
        {
        b->tt->cur->subtitle_streams[j].file_index = bgav_file_index_create();
        bgav_set_subtitle_stream(b, j, BGAV_STREAM_PARSE);
        }
      }

    bgav_start(b);
    
    switch(b->demuxer->index_mode)
      {
      case INDEX_MODE_SIMPLE:
        build_file_index_simple(b);
        ret = 1;
        break;
      case INDEX_MODE_MIXED:
        build_file_index_mixed(b);
        ret = 1;
        break;
      case INDEX_MODE_MPEG:
        ret = build_file_index_mpeg(b);

        break;
      }
    b->demuxer->flags &= ~BGAV_DEMUXER_BUILD_INDEX;

    /* Change input timescales to the output timescales and
       correct timestamp offsets */
    
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      s = &b->tt->cur->audio_streams[j];
      if(s->timescale && (s->first_timestamp != BGAV_TIMESTAMP_UNDEFINED))
        {
        s->first_timestamp =
          gavl_time_rescale(s->timescale, s->data.audio.format.samplerate,
                            s->first_timestamp);
        }
      else
        s->first_timestamp = 0;
    
      s->timescale = s->data.audio.format.samplerate;
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      s = &b->tt->cur->video_streams[j];
      if(s->timescale && (s->first_timestamp != BGAV_TIMESTAMP_UNDEFINED))
        {
        s->first_timestamp =
          gavl_time_rescale(s->timescale, s->data.video.format.timescale,
                            s->first_timestamp);
        }
      else
        s->first_timestamp = 0;
    
      s->timescale = s->data.video.format.timescale;
      }

    /* Switch off streams again */
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      bgav_set_audio_stream(b, j, BGAV_STREAM_MUTE);
      }
    for(j = 0; j < b->tt->cur->num_video_streams; j++)
      {
      bgav_set_video_stream(b, j, BGAV_STREAM_MUTE);
      }
    for(j = 0; j < b->tt->cur->num_subtitle_streams; j++)
      {
      if(!b->tt->cur->subtitle_streams[j].data.subtitle.subreader)
        {
        bgav_set_subtitle_stream(b, j, BGAV_STREAM_MUTE);
        }
      }
    }
  return ret;
  }


static int bgav_build_file_index_si_parse(bgav_t * b)
  {
  int i, j;
  for(i = 0; i < b->tt->num_tracks; i++)
    {
    b->demuxer->flags |= BGAV_DEMUXER_BUILD_INDEX;
    for(j = 0; j < b->tt->cur->num_audio_streams; j++)
      {
      if(!b->tt->cur->audio_streams[j].index_mode)
        continue;

      bgav_select_track(b, i);
      b->tt->cur->audio_streams[j].file_index = bgav_file_index_create();
      bgav_set_audio_stream(b, j, BGAV_STREAM_PARSE);
      bgav_start(b);
      b->demuxer->request_stream = &b->tt->cur->audio_streams[j];
      b->tt->cur->audio_streams[j].duration = 0; 

      if(b->tt->cur->audio_streams[j].index_mode == INDEX_MODE_MPEG)
        {
        while(bgav_demuxer_next_packet(b->demuxer))
          {
          if(!flush_stream_mpeg_audio(&b->tt->cur->audio_streams[j]))
            {
            b->demuxer->flags &= ~BGAV_DEMUXER_BUILD_INDEX;
            return 0;
            }
          b->demuxer->request_stream = &b->tt->cur->audio_streams[j];
          }
        }
      else if(b->tt->cur->audio_streams[j].index_mode == INDEX_MODE_SIMPLE)
        {
        while(1)
          {
          if(!bgav_demuxer_next_packet(b->demuxer))
            break;
          flush_stream_simple(&b->tt->cur->audio_streams[j]);
          b->demuxer->request_stream = &b->tt->cur->audio_streams[j];
          }
        }
      bgav_set_audio_stream(b, j, BGAV_STREAM_MUTE);
      }
    b->demuxer->flags &= ~BGAV_DEMUXER_BUILD_INDEX;
    }
  return 1;
  }

int bgav_build_file_index(bgav_t * b, gavl_time_t * time_needed)
  {
  int ret;
  
  gavl_timer_t * timer = gavl_timer_create();

  gavl_timer_start(timer);
  
  switch(b->demuxer->index_mode)
    {
    case INDEX_MODE_MPEG:
    case INDEX_MODE_SIMPLE:
    case INDEX_MODE_MIXED:
      ret = bgav_build_file_index_parseall(b);
      break;
    case INDEX_MODE_SI_PARSE:
      ret = bgav_build_file_index_si_parse(b);
      break;
    default:
      ret = 0;
      break; 
    }
  if(!ret)
    {
    bgav_log(&b->opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Building file index failed");
    gavl_timer_destroy(timer);
    return 0;
    }
  *time_needed = gavl_timer_get(timer);
  bgav_log(&b->opt, BGAV_LOG_INFO, LOG_DOMAIN,
           "Built file index in %.2f seconds",
           gavl_time_to_seconds(*time_needed));
  gavl_timer_destroy(timer);
  set_has_file_index(b);
  return 1;
  }

int bgav_demuxer_next_packet_fileindex(bgav_demuxer_context_t * ctx)
  {
  bgav_stream_t * s = ctx->request_stream;
  int new_pos;

  /* Check for EOS */
  if(s->index_position >= s->file_index->num_entries)
    return 0;
  
  /* Seek to right position */
  if(s->file_index->entries[s->index_position].position !=
     ctx->input->position)
    bgav_input_seek(ctx->input,
                    s->file_index->entries[s->index_position].position,
                    SEEK_SET);

  /* Advance next index position until we have a new file position */
  new_pos = s->index_position + 1;
  
  while((new_pos < s->file_index->num_entries) &&
        (s->file_index->entries[s->index_position].position ==
         s->file_index->entries[new_pos].position))
    {
    new_pos++;
    }
  s->index_position = new_pos;

  /* Tell the demuxer where to stop */

  if(new_pos >= s->file_index->num_entries)
    ctx->next_packet_pos = 0x7FFFFFFFFFFFFFFFLL;
  else 
    ctx->next_packet_pos = s->file_index->entries[new_pos].position;

  if(!ctx->demuxer->next_packet(ctx))
    return 0;
  
  return 1;
  }


int64_t bgav_audio_start_time(bgav_t * bgav, int stream)
  {
  return bgav->tt->cur->audio_streams[stream].first_timestamp;
  }

int64_t bgav_video_start_time(bgav_t * bgav, int stream)
  {
  return bgav->tt->cur->video_streams[stream].first_timestamp;
  }
