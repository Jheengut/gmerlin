/*
 *  Really simple audio player
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pluginregistry.h>
#include <utils.h>

#if 0
static void dump_format(gavl_audio_format_t * format)
  {
  fprintf(stderr, "Channels: %d\nSamplerate: %d\nSamples per frame: %d\nSample format: %s\nInterleave Mode: %s\nChannel Setup: %s\n",
          format->num_channels,
          format->samplerate,
          format->samples_per_frame,
          gavl_sample_format_to_string(format->sample_format),
          gavl_interleave_mode_to_string(format->interleave_mode),
          gavl_channel_setup_to_string(format->channel_setup));
  
  }

#endif

int main(int argc, char ** argv)
  {
  bg_track_info_t * info;
  char * tmp_path;
  const bg_plugin_info_t * plugin_info;
  bg_cfg_section_t * cfg_section;
  int do_convert;
  gavl_audio_options_t opt;
  bg_plugin_registry_t * plugin_reg;
  bg_cfg_registry_t    * cfg_reg;
  
  bg_plugin_handle_t * input_handle = NULL;
  bg_plugin_handle_t * output_handle = NULL;
  
  /* Plugins */
  
  bg_input_plugin_t * input_plugin = NULL;
  bg_oa_plugin_t    * output_plugin = NULL;
    
  /* Output format */
  
  gavl_audio_format_t audio_format;

  /* Frames */
  
  gavl_audio_frame_t * input_frame;
  gavl_audio_frame_t * output_frame = (gavl_audio_frame_t*)0;

  /* Converter */

  gavl_audio_converter_t * audio_converter;
  
  if(argc == 1)
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);

  /* Create registries */

  cfg_reg = bg_cfg_registry_create();
  tmp_path =  bg_search_file_read("generic", "config.xml");
  bg_cfg_registry_load(cfg_reg, tmp_path);
  if(tmp_path)
    free(tmp_path);

  cfg_section = bg_cfg_registry_find_section(cfg_reg, "plugins");
  plugin_reg = bg_plugin_registry_create(cfg_section);

  /* Load input plugin */

  plugin_info = bg_plugin_find_by_filename(plugin_reg, argv[1], BG_PLUGIN_INPUT);
  if(!plugin_info)
    {
    fprintf(stderr, "Input plugin not found\n");
    return -1;
    }
  input_handle = bg_plugin_load(plugin_reg, plugin_info);
  input_plugin = (bg_input_plugin_t*)(input_handle->plugin);

  /* Load output plugin */
  
  plugin_info = bg_plugin_registry_get_default(plugin_reg, BG_PLUGIN_OUTPUT_AUDIO);

  if(!plugin_info)
    {
    fprintf(stderr, "Output plugin not found\n");
    return -1;
    }
  output_handle = bg_plugin_load(plugin_reg, plugin_info);
  output_plugin = (bg_oa_plugin_t*)(output_handle->plugin);
    
  /* Open the input */
  
  if(!input_plugin->open(input_handle->priv, argv[1]))
    {
    fprintf(stderr, "Cannot open %s\n", argv[1]);
    return -1;
    }

  /* Select the first track */
  
  if(input_plugin->set_track)
    input_plugin->set_track(input_handle->priv, 0);

  info = input_plugin->get_track_info(input_handle->priv, 0);
  
  if(!info->num_audio_streams)
    {
    fprintf(stderr, "File %s has no audio\n", argv[1]);
    return -1;
    }

  /* Select first stream */

  input_plugin->set_audio_stream(input_handle->priv, 0,
                                 BG_STREAM_ACTION_DECODE);
  
  /* Start playback */
  if(input_plugin->start)
    input_plugin->start(input_handle->priv);
  
  /* Get audio format */

  gavl_audio_format_copy(&audio_format, &(info->audio_streams[0].format));
  
  /* Initialize output plugin */
  
  output_plugin->open(output_handle->priv, &audio_format);

  /* Initialize audio converter */

  audio_converter = gavl_audio_converter_create();

  gavl_audio_default_options(&opt);

  fprintf(stderr, "gavl_audio_converter_init...");
  
  do_convert = gavl_audio_converter_init(audio_converter,
                                         &opt,
                                         &(info->audio_streams[0].format),
                                         &audio_format);
  fprintf(stderr, "done\n");
  
  /* Dump formats */

  /*
  
  fprintf(stderr, "Input format:\n");
  dump_format(&(info->audio_streams[0].format));
  fprintf(stderr, "Output format:\n");
  dump_format(&audio_format);
  
  */
  
  /* Allocate frames */

  info->audio_streams[0].format.samples_per_frame = audio_format.samples_per_frame;
  
  input_frame = gavl_audio_frame_create(&(info->audio_streams[0].format));

  if(do_convert)
    output_frame = gavl_audio_frame_create(&audio_format);
  
  /* Playback until we are done */

  
  while(1)
    {
    //    fprintf(stderr, "Read audio %d\n", audio_format.samples_per_frame);
    if(!input_plugin->read_audio_samples(input_handle->priv,
                                         input_frame, 0,
                                         audio_format.samples_per_frame))
      break;
    
    if(do_convert)
      {
      gavl_audio_convert(audio_converter, input_frame, output_frame);
      output_plugin->write_frame(output_handle->priv, output_frame);
      }
    else
      output_plugin->write_frame(output_handle->priv, input_frame);
    }
  
  /* Clean up */

  gavl_audio_converter_destroy(audio_converter);
  gavl_audio_frame_destroy(input_frame);
  if(output_frame)
    gavl_audio_frame_destroy(output_frame);

  if(input_plugin->stop)
    input_plugin->stop(input_handle->priv);

  input_plugin->close(input_handle->priv);
  output_plugin->close(output_handle->priv);

  bg_plugin_unref(input_handle);
  bg_plugin_unref(output_handle);
    
  bg_plugin_registry_destroy(plugin_reg);
  bg_cfg_registry_destroy(cfg_reg);
  
  
  return 0;
  }
