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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "player.h"
#include "playerprivate.h"

static void wait_notify(bg_player_t * p)
  {
  pthread_mutex_lock(&p->stop_mutex);

  pthread_mutex_lock(&(p->waiting_plugin_threads_mutex));
  p->waiting_plugin_threads++;

  if(p->waiting_plugin_threads == p->total_plugin_threads)
    pthread_cond_broadcast(&(p->stop_cond));
  
  pthread_mutex_unlock(&p->stop_mutex);
  
  pthread_mutex_unlock(&(p->waiting_plugin_threads_mutex));
  }

static void wait_unnotify(bg_player_t * p)
  {
  pthread_mutex_lock(&(p->waiting_plugin_threads_mutex));
  p->waiting_plugin_threads--;
  pthread_mutex_unlock(&(p->waiting_plugin_threads_mutex));
  }

int bg_player_keep_going(bg_player_t * p, void (*ping_func)(void*), void * data)
  {
  struct timespec timeout;
  struct timeval now;
  int state, old_state;
  old_state = bg_player_get_state(p);
  switch(old_state)
    {
    case BG_PLAYER_STATE_STOPPED:
    case BG_PLAYER_STATE_CHANGING:
      return 0;
    case BG_PLAYER_STATE_PLAYING:
    case BG_PLAYER_STATE_FINISHING:
      break;
    case BG_PLAYER_STATE_STARTING:
    case BG_PLAYER_STATE_PAUSED:
    case BG_PLAYER_STATE_SEEKING:
    case BG_PLAYER_STATE_BUFFERING:

      /* Suspend the thread until restart */

      pthread_mutex_lock(&(p->start_mutex));

      /* If we are the last thread to stop, tell the player
         to continue */


      if(!ping_func)
        {
        wait_notify(p);
        pthread_cond_wait(&(p->start_cond), &(p->start_mutex));
        }
      else
        {
        if(old_state == BG_PLAYER_STATE_PAUSED)
          ping_func(data);
        wait_notify(p);
        while(1)
          {
          gettimeofday(&now, NULL);
          timeout.tv_sec = now.tv_sec;
          /* 10 milliseconds */
          timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
          while(timeout.tv_nsec >= 1000000000)
            {
            timeout.tv_nsec -= 1000000000;
            timeout.tv_sec++;
            }
          if(!pthread_cond_timedwait(&(p->start_cond), &(p->start_mutex), &timeout))
            break;
          
          state = bg_player_get_state(p);
          if(state == BG_PLAYER_STATE_PAUSED)
            ping_func(data);
          }
        }
      
      wait_unnotify(p);
      
      pthread_mutex_unlock(&(p->start_mutex));

      
      if(old_state == BG_PLAYER_STATE_PAUSED)
        {
        state = bg_player_get_state(p);
        if((state == BG_PLAYER_STATE_STOPPED) ||
           (state == BG_PLAYER_STATE_CHANGING))
          return 0;
        }

      break;
    }
  return 1;
  }

bg_player_t * bg_player_create(bg_plugin_registry_t * plugin_reg)
  {
  bg_player_t * ret;
    
  ret = calloc(1, sizeof(*ret));

  /* Create message queues */

  ret->command_queue  = bg_msg_queue_create();
  
  ret->message_queues = bg_msg_queue_list_create();
  
  ret->visualizer = bg_visualizer_create(plugin_reg);
  
  /* Create contexts */

  bg_player_audio_create(ret, plugin_reg);
  bg_player_video_create(ret, plugin_reg);
  bg_player_subtitle_create(ret);
  
  bg_player_input_create(ret);
  bg_player_oa_create(ret);
  bg_player_ov_create(ret);


  pthread_mutex_init(&(ret->state_mutex), (pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->start_mutex), (pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->stop_mutex),  (pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->waiting_plugin_threads_mutex),
                     (pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->mute_mutex), (pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->config_mutex), (pthread_mutexattr_t *)0);


  pthread_cond_init (&(ret->start_cond),  (pthread_condattr_t *)0);
  pthread_cond_init (&(ret->stop_cond),   (pthread_condattr_t *)0);

  /* Subtitles are off by default */
  ret->current_subtitle_stream = -1;
  //  ret->current_subtitle_stream = 5;
  ret->state = BG_PLAYER_STATE_INIT;
  
  return ret;
  }

void bg_player_destroy(bg_player_t * player)
  {
    
  bg_player_input_destroy(player);
  bg_player_oa_destroy(player);
  bg_player_ov_destroy(player);
  bg_player_audio_destroy(player);
  bg_player_video_destroy(player);
  bg_player_subtitle_destroy(player);

  bg_visualizer_destroy(player->visualizer);
  
  bg_msg_queue_destroy(player->command_queue);
 
  bg_msg_queue_list_destroy(player->message_queues);
  
  pthread_mutex_destroy(&(player->state_mutex));
  pthread_mutex_destroy(&(player->config_mutex));
  
  free(player);
  }

void bg_player_add_message_queue(bg_player_t * player,
                                 bg_msg_queue_t * message_queue)
  {
  bg_msg_queue_list_add(player->message_queues, message_queue);
  }

void bg_player_delete_message_queue(bg_player_t * player,
                                    bg_msg_queue_t * message_queue)
  {
  bg_msg_queue_list_remove(player->message_queues, message_queue);
  }


int  bg_player_get_state(bg_player_t * player)
  {
  int ret;
  pthread_mutex_lock(&(player->state_mutex));
  ret = player->state;
  pthread_mutex_unlock(&(player->state_mutex));
  return ret;
  }

struct state_struct
  {
  int state;
  float percentage;
  int want_new;
  };

static void msg_state(bg_msg_t * msg,
                      const void * data)
  {
  struct state_struct * s;
  s = (struct state_struct *)data;
  

  bg_msg_set_id(msg, BG_PLAYER_MSG_STATE_CHANGED);
  bg_msg_set_arg_int(msg, 0, s->state);

  if(s->state == BG_PLAYER_STATE_BUFFERING)
    {
    bg_msg_set_arg_float(msg, 1, s->percentage);
    }
  else if(s->state == BG_PLAYER_STATE_CHANGING)
    {
    bg_msg_set_arg_int(msg, 1, s->want_new);
    }
  }

void bg_player_set_state(bg_player_t * player, int state,
                         const void * arg1, const void * arg2)
  {
  struct state_struct s;
  pthread_mutex_lock(&(player->state_mutex));
  player->state = state;
  pthread_mutex_unlock(&(player->state_mutex));

  /* Broadcast this message */

  
  //  memset(&state, 0, sizeof(state));
    
  s.state = state;

  if(state == BG_PLAYER_STATE_BUFFERING)
    s.percentage = *((const float*)arg1);
  else if(state == BG_PLAYER_STATE_CHANGING)
    s.want_new = *((const int*)arg1);
  
  bg_msg_queue_list_send(player->message_queues,
                         msg_state,
                         &s);
  }

const bg_parameter_info_t *
bg_player_get_visualization_parameters(bg_player_t *  player)
  {
  return bg_visualizer_get_parameters(player->visualizer);
  }

void
bg_player_set_visualization_parameter(void*data,
                                      const char * name,
                                      const bg_parameter_value_t*val)
  {
  bg_player_t * p;
  int do_init;

  p = (bg_player_t*)data;
  do_init = (bg_player_get_state(p) == BG_PLAYER_STATE_INIT);
  
  bg_visualizer_set_parameter(p->visualizer, name, val);
  
  if(!do_init)
    {
    if(bg_visualizer_need_restart(p->visualizer))
      {
      bg_player_interrupt(p);
      bg_player_interrupt_resume(p);
      }
    }
  }

void
bg_player_set_visualization_plugin_parameter(void*data,
                                             const char * name,
                                             const bg_parameter_value_t*val)
  {
  bg_player_t * p;
  p = (bg_player_t*)data;
  bg_visualizer_set_vis_parameter(p->visualizer, name, val);
  }

void
bg_player_set_visualization(bg_player_t * p,
                            int enable)
  {
  int was_enabled;
  int do_init;
  do_init = (bg_player_get_state(p) == BG_PLAYER_STATE_INIT);

  pthread_mutex_lock(&p->config_mutex);
  was_enabled = p->visualizer_enabled;
  p->visualizer_enabled = enable;
  pthread_mutex_unlock(&p->config_mutex);

  if((was_enabled != enable) && !do_init)
    {
    bg_player_interrupt(p);
    bg_player_interrupt_resume(p);
    }
  }

void
bg_player_set_visualization_plugin(bg_player_t * p, const bg_plugin_info_t * plugin_info)
  {
  int do_init;
  do_init = (bg_player_get_state(p) == BG_PLAYER_STATE_INIT);
  
  bg_visualizer_set_vis_plugin(p->visualizer, plugin_info);
  
  if(!do_init)
    {
    if(bg_visualizer_need_restart(p->visualizer))
      {
      bg_player_interrupt(p);
      bg_player_interrupt_resume(p);
      }
    }

  }
