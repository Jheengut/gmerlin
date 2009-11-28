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
#include <string.h>

#include <gmerlin/translation.h>

#include <gmerlin/recorder.h>
#include <recorder_private.h>
#include <language_table.h>

#include <gmerlin/log.h>
#define LOG_DOMAIN "recorder.audio"

void bg_recorder_create_audio(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;
  
  as->output_cnv = gavl_audio_converter_create();
  as->enc_cnv = gavl_audio_converter_create();

  bg_gavl_audio_options_init(&(as->opt));
  
  as->fc = bg_audio_filter_chain_create(&(as->opt), rec->plugin_reg);
  as->th = bg_player_thread_create(rec->tc);

  as->pd = gavl_peak_detector_create();
  
  }

void bg_recorder_destroy_audio(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;
  gavl_audio_converter_destroy(as->output_cnv);
  gavl_audio_converter_destroy(as->enc_cnv);
  bg_audio_filter_chain_destroy(as->fc);
  bg_player_thread_destroy(as->th);

  gavl_peak_detector_destroy(as->pd);
  
  }

static const bg_parameter_info_t parameters[] =
  {
    {
      .name = "do_audio",
      .long_name = TRS("Record audio"),
      .type = BG_PARAMETER_CHECKBUTTON,
      .val_default = { .val_i = 1 },
    },
    {
      .name      = "plugin",
      .long_name = TRS("Plugin"),
      .type      = BG_PARAMETER_MULTI_MENU,
      .flags     = BG_PARAMETER_PLUGIN,
    },
    {
      .name      = "language",
      .long_name = TRS("Language"),
      .type      = BG_PARAMETER_STRINGLIST,
      .val_default = { .val_str = "eng" },
      .multi_names = bg_language_codes,
      .multi_labels = bg_language_labels,
    },
    { },
  };

const bg_parameter_info_t *
bg_recorder_get_audio_parameters(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;
  if(!as->parameters)
    {
    as->parameters = bg_parameter_info_copy_array(parameters);
    
    bg_plugin_registry_set_parameter_info(rec->plugin_reg,
                                          BG_PLUGIN_RECORDER_AUDIO,
                                          BG_PLUGIN_RECORDER,
                                          &as->parameters[1]);
    }
  
  return as->parameters;
  }

void
bg_recorder_set_audio_parameter(void * data,
                                const char * name,
                                const bg_parameter_value_t * val)
  {
  bg_recorder_t * rec = data;
  bg_recorder_audio_stream_t * as = &rec->as;
  
  if(!name)
    return;
  
  //  if(name)
  //    fprintf(stderr, "bg_recorder_set_audio_parameter %s\n", name);

  if(!strcmp(name, "do_audio"))
    {
    if((rec->flags & FLAG_RUNNING) &&
       (!!(as->flags & STREAM_ACTIVE) != val->val_i))
      bg_recorder_stop(rec);
    
    if(val->val_i)
      as->flags |= STREAM_ACTIVE;
    else
      as->flags &= ~STREAM_ACTIVE;
    }
  else if(!strcmp(name, "language"))
    {
    strncpy(as->language, val->val_str, 3);
    }
  else if(!strcmp(name, "plugin"))
    {
    const bg_plugin_info_t * info;
    
    if(as->input_handle &&
       !strcmp(as->input_handle->info->name, val->val_str))
      return;
    
    if(rec->flags & FLAG_RUNNING)
      bg_recorder_stop(rec);

    if(as->input_handle)
      bg_plugin_unref(as->input_handle);
    
    info = bg_plugin_find_by_name(rec->plugin_reg, val->val_str);
    as->input_handle = bg_plugin_load(rec->plugin_reg, info);
    as->input_plugin = (bg_recorder_plugin_t*)(as->input_handle->plugin);
    }
  else if(as->input_handle && as->input_plugin->common.set_parameter)
    {
    as->input_plugin->common.set_parameter(as->input_handle->priv, name, val);
    }
  }

const bg_parameter_info_t *
bg_recorder_get_audio_filter_parameters(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;
  return bg_audio_filter_chain_get_parameters(as->fc);
  }

void
bg_recorder_set_audio_filter_parameter(void * data,
                                       const char * name,
                                       const bg_parameter_value_t * val)
  {
  int need_restart;
  
  bg_recorder_t * rec = data;
  bg_recorder_audio_stream_t * as = &rec->as;
  if(!name)
    {
    if(!(rec->flags & FLAG_RUNNING))
      bg_recorder_run(rec);
    return;
    }
  bg_audio_filter_chain_lock(as->fc);
  bg_audio_filter_chain_set_parameter(as->fc, name, val);
  
  if(bg_audio_filter_chain_need_rebuild(as->fc) ||
     bg_audio_filter_chain_need_restart(as->fc))
    need_restart = 1;
  else
    need_restart = 0;

  bg_audio_filter_chain_unlock(as->fc);

  if(need_restart)
    bg_recorder_stop(rec);
  
  }

void * bg_recorder_audio_thread(void * data)
  {
  double peaks[2]; /* Doesn't work for > 2 channels!! */
  
  bg_recorder_t * rec = data;
  bg_recorder_audio_stream_t * as = &rec->as;

  bg_player_thread_wait_for_start(as->th);
  
  while(1)
    {
    if(!bg_player_thread_check(as->th))
      break;
    
    if(!as->in_func(as->in_data, as->pipe_frame, as->in_stream,
                    as->pipe_format.samples_per_frame))
      break; /* Should never happen */
    
    /* Peak detection */    
    gavl_peak_detector_update(as->pd, as->pipe_frame);
    gavl_peak_detector_get_peaks(as->pd, NULL, NULL, peaks);
    if(as->pipe_format.num_channels == 1)
      peaks[1] = peaks[0];
    bg_recorder_msg_audiolevel(rec, peaks, as->pipe_frame->valid_samples);
    gavl_peak_detector_reset(as->pd);   	

    /* Encoding */
    if(as->flags & STREAM_ENCODE_OPEN)
      {
      bg_recorder_update_time(rec,
                              gavl_time_unscale(as->pipe_format.samplerate,
                                                as->pipe_frame->timestamp +
                                                as->pipe_frame->valid_samples));
      
      if(as->do_convert_enc)
        {
        gavl_audio_convert(as->enc_cnv, as->pipe_frame, as->enc_frame);
        pthread_mutex_lock(&rec->enc_mutex);
        bg_encoder_write_audio_frame(rec->enc, as->enc_frame, as->enc_index);
        pthread_mutex_unlock(&rec->enc_mutex);
        }
      else
        {
        pthread_mutex_lock(&rec->enc_mutex);
        bg_encoder_write_audio_frame(rec->enc, as->pipe_frame, as->enc_index);
        pthread_mutex_unlock(&rec->enc_mutex);
        }
      }
    
    }
  return NULL;

  }

int bg_recorder_audio_init(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;

  /* Open input */
  if(!as->input_plugin->open(as->input_handle->priv, &as->input_format, NULL))
    {
    return 0;
    }

  as->flags |= STREAM_INPUT_OPEN;
  
  as->in_func   = as->input_plugin->read_audio;
  as->in_stream = 0;
  as->in_data   = as->input_handle->priv;
  
  /* Set up filter chain */

  bg_audio_filter_chain_connect_input(as->fc,
                                      as->in_func,
                                      as->in_data,
                                      as->in_stream);
  
  as->in_func = bg_audio_filter_chain_read;
  as->in_data = as->fc;
  as->in_stream = 0;
  
  bg_audio_filter_chain_init(as->fc, &as->input_format, &as->pipe_format);

  /* Set up peak detection */

  gavl_peak_detector_set_format(as->pd, &as->pipe_format);

  /* Create frame(s) */
  
  as->pipe_frame = gavl_audio_frame_create(&as->pipe_format);
  
  /* Set up output */

  if(as->flags & STREAM_ENCODE)
    {
    as->enc_index = bg_encoder_add_audio_stream(rec->enc, as->language,
                                                &as->pipe_format, 0);
    }
  
  return 1;
  }


void bg_recorder_audio_cleanup(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;

  if(as->flags & STREAM_INPUT_OPEN)
    as->input_plugin->close(as->input_handle->priv);

  if(as->pipe_frame)
    {
    gavl_audio_frame_destroy(as->pipe_frame);
    as->pipe_frame = NULL;
    }

  if(as->enc_frame)
    {
    gavl_audio_frame_destroy(as->enc_frame);
    as->enc_frame = NULL;
    }

  as->flags &= ~(STREAM_INPUT_OPEN | STREAM_ENCODE_OPEN);
  
  }

void bg_recorder_audio_finalize_encode(bg_recorder_t * rec)
  {
  bg_recorder_audio_stream_t * as = &rec->as;
  bg_encoder_get_audio_format(rec->enc, as->enc_index, &as->enc_format);

  as->do_convert_enc = gavl_audio_converter_init(as->enc_cnv, &as->pipe_format,
                                                 &as->enc_format);

  if(as->do_convert_enc)
    as->enc_frame = gavl_audio_frame_create(&as->enc_format);

  as->flags |= STREAM_ENCODE_OPEN;

  }