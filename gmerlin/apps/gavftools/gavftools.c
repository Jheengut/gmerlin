/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
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

#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "gavftools.h"

#define LOG_DOMAIN "gavftools"

bg_plugin_registry_t * plugin_reg;
bg_cfg_registry_t * cfg_reg;

char * gavftools_in_file  = NULL;
char * gavftools_out_file = NULL;

static int got_sigint = 0;
struct sigaction old_sigaction;

/* Quality parameters */

bg_cfg_section_t * aq_section;
bg_cfg_section_t * vq_section;

static const bg_parameter_info_t aq_params[] =
  {
    BG_GAVL_PARAM_CONVERSION_QUALITY,
    BG_GAVL_PARAM_RESAMPLE_MODE,
    BG_GAVL_PARAM_AUDIO_DITHER_MODE,
    { /* End */ },
  };

static const bg_parameter_info_t vq_params[] =
  {
    BG_GAVL_PARAM_CONVERSION_QUALITY,
    BG_GAVL_PARAM_DEINTERLACE,
    BG_GAVL_PARAM_SCALE_MODE,
    BG_GAVL_PARAM_RESAMPLE_CHROMA,
    BG_GAVL_PARAM_ALPHA,
    BG_GAVL_PARAM_BACKGROUND,
    { /* End */ },
  };

/* Conversion options */

bg_gavl_audio_options_t gavltools_aopt;
bg_gavl_video_options_t gavltools_vopt;

static void sigint_handler(int sig)
  {
  got_sigint = 1;
  sigaction(SIGINT, &old_sigaction, 0);
  bg_log(BG_LOG_INFO, LOG_DOMAIN, "Caught Ctrl-C");
  }

static void set_sigint_handler()
  {
  struct sigaction sa;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = sigint_handler;
  if (sigaction(SIGINT, &sa, &old_sigaction) == -1)
    {
    fprintf(stderr, "sigaction failed\n");
    }
  }

int gavftools_stop()
  {
  return got_sigint;
  }

void gavftools_init()
  {
  char * tmp_path;
  bg_cfg_section_t * cfg_section;
    
  cfg_reg = bg_cfg_registry_create();
  tmp_path =  bg_search_file_read("generic", "config.xml");
  bg_cfg_registry_load(cfg_reg, tmp_path);
  
  cfg_section = bg_cfg_registry_find_section(cfg_reg, "plugins");
  plugin_reg = bg_plugin_registry_create(cfg_section);

  bg_cfg_registry_save(cfg_reg, tmp_path);

  if(tmp_path)
    free(tmp_path);

  set_sigint_handler();

  /* Quality options */
  
  bg_gavl_audio_options_init(&gavltools_aopt);
  bg_gavl_video_options_init(&gavltools_vopt);

  aq_section = bg_cfg_section_create_from_parameters("aq", aq_params);
  vq_section = bg_cfg_section_create_from_parameters("vq", vq_params);
  
  }

static bg_cfg_section_t * ac_section = NULL;
static bg_cfg_section_t * vc_section = NULL;
static bg_cfg_section_t * oc_section = NULL;

static bg_cfg_section_t * iopt_section = NULL;
static bg_cfg_section_t * oopt_section = NULL;

static bg_parameter_info_t * ac_parameters = NULL;
static bg_parameter_info_t * vc_parameters = NULL;
static bg_parameter_info_t * oc_parameters = NULL;

static bg_stream_action_t * a_actions = NULL;
static int num_a_actions = 0;

static bg_stream_action_t * v_actions = NULL;
static int num_v_actions = 0;

static bg_stream_action_t * t_actions = NULL;
static int num_t_actions = 0;

static bg_stream_action_t * o_actions = NULL;
static int num_o_actions = 0;

void gavftools_cleanup(int save_regs)
  {
  bg_plugin_registry_destroy(plugin_reg);
  bg_cfg_registry_destroy(cfg_reg);

  if(ac_section)
    bg_cfg_section_destroy(ac_section);
  if(vc_section)
    bg_cfg_section_destroy(vc_section);
  if(oc_section)
    bg_cfg_section_destroy(oc_section);

  if(iopt_section)
    bg_cfg_section_destroy(iopt_section);
  if(oopt_section)
    bg_cfg_section_destroy(oopt_section);
  
  if(ac_parameters)
    bg_parameter_info_destroy_array(ac_parameters);
  if(vc_section)
    bg_parameter_info_destroy_array(vc_parameters);

  if(a_actions) free(a_actions);
  if(v_actions) free(v_actions);
  if(t_actions) free(t_actions);
  if(o_actions) free(o_actions);

  bg_gavl_audio_options_free(&gavltools_aopt);
  bg_gavl_video_options_free(&gavltools_vopt);

  if(aq_section)
    bg_cfg_section_destroy(aq_section);
  if(vq_section)
    bg_cfg_section_destroy(vq_section);
  
  
  xmlCleanupParser();
 
  }

const bg_parameter_info_t * gavftools_ac_params(void)
  {
  if(!ac_parameters)
    {
    ac_parameters =
      bg_plugin_registry_create_compressor_parameters(plugin_reg,
                                                      BG_PLUGIN_AUDIO_COMPRESSOR);
    
    }
  return ac_parameters;
  }

const bg_parameter_info_t * gavftools_vc_params(void)
  {
  if(!vc_parameters)
    {
    vc_parameters =
      bg_plugin_registry_create_compressor_parameters(plugin_reg,
                                                      BG_PLUGIN_VIDEO_COMPRESSOR);
    
    }
  return vc_parameters;

  }

const bg_parameter_info_t * gavftools_oc_params(void)
  {
  if(!oc_parameters)
    {
    oc_parameters =
      bg_plugin_registry_create_compressor_parameters(plugin_reg,
                                                      BG_PLUGIN_OVERLAY_COMPRESSOR);
    
    }
  return oc_parameters;

  }

bg_cfg_section_t * gavftools_ac_section(void)
  {
  if(!ac_section)
    {
    ac_section =
      bg_cfg_section_create_from_parameters("ac",
                                            gavftools_ac_params());
    }
  return ac_section;
  }

bg_cfg_section_t * gavftools_vc_section(void)
  {
  if(!vc_section)
    {
    vc_section =
      bg_cfg_section_create_from_parameters("vc",
                                            gavftools_vc_params());
    }
  return vc_section;
  }

bg_cfg_section_t * gavftools_oc_section(void)
  {
  if(!oc_section)
    {
    oc_section =
      bg_cfg_section_create_from_parameters("oc",
                                            gavftools_oc_params());
    }
  return oc_section;
  }

bg_cfg_section_t * gavftools_iopt_section(void)
  {
  if(!iopt_section)
    {
    iopt_section =
      bg_cfg_section_create_from_parameters("iopt",
                                            bg_plug_get_input_parameters());
    }
  return iopt_section;
  }

bg_cfg_section_t * gavftools_oopt_section(void)
  {
  if(!oopt_section)
    {
    oopt_section =
      bg_cfg_section_create_from_parameters("oopt",
                                            bg_plug_get_output_parameters());
    }
  return oopt_section;
  }

void
gavftools_opt_ac(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -ac requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(gavftools_ac_section(),
                               NULL,
                               NULL,
                               gavftools_ac_params(),
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }


void
gavftools_opt_vc(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -vc requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(gavftools_vc_section(),
                               NULL,
                               NULL,
                               gavftools_vc_params(),
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

void
gavftools_opt_oc(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -oc requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(gavftools_oc_section(),
                               NULL,
                               NULL,
                               gavftools_oc_params(),
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void set_aq_parameter(void * data, const char * name,
                             const bg_parameter_value_t * val)
  {
  bg_gavl_audio_set_parameter(&gavltools_aopt, name, val);
  }

static void set_vq_parameter(void * data, const char * name,
                             const bg_parameter_value_t * val)
  {
  bg_gavl_video_set_parameter(&gavltools_vopt, name, val);
  }


void
gavftools_opt_aq(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -aq requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(aq_section,
                               set_aq_parameter,
                               NULL,
                               aq_params,
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

void
gavftools_opt_vq(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -vq requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(vq_section,
                               set_vq_parameter,
                               NULL,
                               vq_params,
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }


void
gavftools_opt_iopt(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -iopt requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(gavftools_iopt_section(),
                               NULL,
                               NULL,
                               bg_plug_get_input_parameters(),
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

void
gavftools_opt_oopt(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -oopt requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(gavftools_oopt_section(),
                               NULL,
                               NULL,
                               bg_plug_get_output_parameters(),
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }



void
gavftools_block_sigpipe(void)
  {
  signal(SIGPIPE, SIG_IGN);
#if 0
  sigset_t newset;
  /* Block SIGPIPE */
  sigemptyset(&newset);
  sigaddset(&newset, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &newset, NULL);
#endif
  }

/* Stream actions */

static bg_stream_action_t * stream_actions_from_arg(const char * arg, int * num_p)
  {
  int i;
  char ** actions_c;
  bg_stream_action_t * ret;
  int num;

  actions_c = bg_strbreak(arg, ',');
  num = 0;
  while(actions_c[num])
    num++;

  ret = calloc(num, sizeof(*ret));

  for(i = 0; i < num; i++)
    {
    if(*(actions_c[i]) == 'd')
      ret[i] = BG_STREAM_ACTION_DECODE;
    else if(*(actions_c[i]) == 'm')
      ret[i] = BG_STREAM_ACTION_OFF;
    else if(*(actions_c[i]) == 'c')
      ret[i] = BG_STREAM_ACTION_READRAW;
    }
  *num_p = num;
  bg_strbreak_free(actions_c);
  return ret;
  }

void
gavftools_opt_as(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -as requires an argument\n");
    exit(-1);
    }

  a_actions = stream_actions_from_arg((*_argv)[arg], &num_a_actions);
  bg_cmdline_remove_arg(argc, _argv, arg);
  
  }

void
gavftools_opt_vs(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -vs requires an argument\n");
    exit(-1);
    }
  v_actions = stream_actions_from_arg((*_argv)[arg], &num_v_actions);
  bg_cmdline_remove_arg(argc, _argv, arg);
  
  }

void
gavftools_opt_os(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -os requires an argument\n");
    exit(-1);
    }
  o_actions = stream_actions_from_arg((*_argv)[arg], &num_o_actions);
  bg_cmdline_remove_arg(argc, _argv, arg);

  }

void
gavftools_opt_ts(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -ts requires an argument\n");
    exit(-1);
    }
  t_actions = stream_actions_from_arg((*_argv)[arg], &num_t_actions);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

void gavftools_opt_v(void * data, int * argc, char *** _argv, int arg)
  {
  int val, verbose = 0;

  if(arg >= *argc)
    {
    fprintf(stderr, "Option -v requires an argument\n");
    exit(-1);
    }
  val = atoi((*_argv)[arg]);  
  
  if(val > 0)
    verbose |= BG_LOG_ERROR;
  if(val > 1)
    verbose |= BG_LOG_WARNING;
  if(val > 2)
    verbose |= BG_LOG_INFO;
  if(val > 3)
    verbose |= BG_LOG_DEBUG;
  bg_log_set_verbose(verbose);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }


bg_stream_action_t *
gavftools_get_stream_actions(int num, gavf_stream_type_t type)
  {
  int i;
  int num_actions;
  bg_stream_action_t * actions;
  bg_stream_action_t * ret;
  bg_stream_action_t def_action = BG_STREAM_ACTION_OFF;
  
  if(!num)
    return NULL;
  
  switch(type)
    {
    case GAVF_STREAM_AUDIO:
      actions = a_actions;
      num_actions = num_a_actions;
      def_action = BG_STREAM_ACTION_READRAW;
      break;
    case GAVF_STREAM_VIDEO:
      actions = v_actions;
      num_actions = num_v_actions;
      def_action = BG_STREAM_ACTION_READRAW;
      break;
    case GAVF_STREAM_TEXT:
      actions = t_actions;
      num_actions = num_t_actions;
      break;
    case GAVF_STREAM_OVERLAY:
      actions = o_actions;
      num_actions = num_o_actions;
      break;
    default:
      return NULL;
    }

  if(!num_actions)
    {
    ret = calloc(num, sizeof(*ret));
    for(i = 0; i < num; i++)
      ret[i] = def_action;
    return ret;
    }
  
  ret = calloc(num, sizeof(*ret));
  
  i = num;
  if(i > num_actions)
    i = num_actions;

  memcpy(ret, actions, i * sizeof(*actions));

  if(num > num_actions)
    {
    for(i = num_actions; i < num; i++)
      ret[i] = BG_STREAM_ACTION_OFF;
    }
  return ret;
  }

void
gavftools_set_compresspor_options(bg_cmdline_arg_t * global_options)
  {
  int i = 0;

  while(global_options[i].arg)
    {
    if(!strcmp(global_options[i].arg, "-ac"))
      global_options[i].parameters = gavftools_ac_params();
    if(!strcmp(global_options[i].arg, "-vc"))
      global_options[i].parameters = gavftools_vc_params();
    if(!strcmp(global_options[i].arg, "-oc"))
      global_options[i].parameters = gavftools_oc_params();
    i++;
    }
  }


bg_plug_t * gavftools_create_in_plug()
  {
  bg_plug_t * in_plug;
  in_plug = bg_plug_create_reader(plugin_reg);
  bg_cfg_section_apply(gavftools_iopt_section(),
                       bg_plug_get_input_parameters(),
                       bg_plug_set_parameter,
                       in_plug);
  return in_plug;
  }

bg_plug_t * gavftools_create_out_plug()
  {
  bg_plug_t * out_plug;
  out_plug = bg_plug_create_writer(plugin_reg);
  bg_cfg_section_apply(gavftools_oopt_section(),
                       bg_plug_get_output_parameters(),
                       bg_plug_set_parameter,
                       out_plug);
  return out_plug;
  }

void gavftools_set_cmdline_parameters(bg_cmdline_arg_t * args)
  {
  bg_cmdline_arg_set_parameters(args, "-iopt",
                                bg_plug_get_input_parameters());
  bg_cmdline_arg_set_parameters(args, "-oopt",
                                bg_plug_get_output_parameters());
  bg_cmdline_arg_set_parameters(args, "-aq", aq_params);
  bg_cmdline_arg_set_parameters(args, "-vq", aq_params);
  }

static void update_metadata(void * priv, const gavl_metadata_t * m)
  {
  gavf_update_metadata(priv, m);
  }

int gavftools_open_out_plug_from_in_plug(bg_plug_t * out_plug,
                                         bg_plug_t * in_plug)
  {
  if(!bg_plug_open_location(out_plug, gavftools_out_file,
                            bg_plug_get_metadata(in_plug),
                            bg_plug_get_chapter_list(in_plug)))
    return 0;
  
  gavf_options_set_metadata_callback(gavf_get_options(bg_plug_get_gavf(in_plug)),
                                     update_metadata, bg_plug_get_gavf(out_plug));

  return 1;
  }
