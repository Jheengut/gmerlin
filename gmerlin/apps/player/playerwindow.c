/*****************************************************************
 
  playerwindow.c
 
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
#include "gmerlin.h"
#include <utils.h>

#define DELAY_TIME 10

#define BACKGROUND_WINDOW GTK_LAYOUT(win->layout)->bin_window
#define MASK_WIDNOW       win->window->window

#define BACKGROUND_WIDGET win->layout
#define MASK_WIDGET       win->window

static void set_background(player_window_t * win)
  {
  GdkBitmap * mask;
  GdkPixmap * pixmap;
  int width, height;
  
  if(!win->background_pixbuf)
    return;
  
  width = gdk_pixbuf_get_width(win->background_pixbuf);
  height = gdk_pixbuf_get_height(win->background_pixbuf);

  gtk_widget_set_size_request(win->window, width, height);
  
  gdk_pixbuf_render_pixmap_and_mask(win->background_pixbuf,
                                    &pixmap, &mask, 0x80);
  
  if(pixmap && BACKGROUND_WINDOW)
    {
    gdk_window_set_back_pixmap(BACKGROUND_WINDOW, pixmap, FALSE);
    }
  if(mask && MASK_WIDNOW)
    {
    gdk_window_shape_combine_mask(MASK_WIDNOW, mask, 0, 0);
    }
  
  if(BACKGROUND_WINDOW)
    gdk_window_clear_area_e(BACKGROUND_WINDOW, 0, 0, width, height);
  }

void player_window_set_skin(player_window_t * win,
                            player_window_skin_t * s,
                            const char * directory)
  {
  int x, y;
  char * tmp_path;
  
  if(win->background_pixbuf)
    {
    g_object_unref(G_OBJECT(win->background_pixbuf));
    }

  tmp_path = bg_sprintf("%s/%s", directory, s->background);
  
  win->background_pixbuf = gdk_pixbuf_new_from_file(tmp_path, NULL);
  
  free(tmp_path);
  set_background(win);

  /* Apply the button skins */

  bg_gtk_button_set_skin(win->play_button, &(s->play_button), directory);
  bg_gtk_button_get_coords(win->play_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                  bg_gtk_button_get_widget(win->play_button),
                  x, y);

  bg_gtk_button_set_skin(win->stop_button, &(s->stop_button), directory);
  bg_gtk_button_get_coords(win->stop_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->stop_button),
                 x, y);

  bg_gtk_button_set_skin(win->pause_button, &(s->pause_button), directory);
  bg_gtk_button_get_coords(win->pause_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->pause_button),
                 x, y);
  
  bg_gtk_button_set_skin(win->next_button, &(s->next_button), directory);
  bg_gtk_button_get_coords(win->next_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->next_button),
                 x, y);
  
  bg_gtk_button_set_skin(win->prev_button, &(s->prev_button), directory);
  bg_gtk_button_get_coords(win->prev_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->prev_button),
                 x, y);

  bg_gtk_button_set_skin(win->close_button, &(s->close_button), directory);
  bg_gtk_button_get_coords(win->close_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->close_button),
                 x, y);

  bg_gtk_button_set_skin(win->menu_button, &(s->menu_button), directory);
  bg_gtk_button_get_coords(win->menu_button, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_button_get_widget(win->menu_button),
                 x, y);

  /* Apply slider skins */
  
  bg_gtk_slider_set_skin(win->seek_slider, &(s->seek_slider), directory);
  bg_gtk_slider_get_coords(win->seek_slider, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                 bg_gtk_slider_get_widget(win->seek_slider),
                 x, y);

  bg_gtk_slider_set_skin(win->volume_slider, &(s->volume_slider), directory);
  bg_gtk_slider_get_coords(win->volume_slider, &x, &y);
  gtk_layout_move(GTK_LAYOUT(win->layout),
                  bg_gtk_slider_get_widget(win->volume_slider),
                  x, y);

  /* Apply display skin */
  
  display_set_skin(win->display, &(s->display));
  display_get_coords(win->display, &x, &y);
  
  gtk_layout_move(GTK_LAYOUT(win->layout),
                  display_get_widget(win->display),
                  x, y);

  /* Update slider positions */

  //  fprintf(stderr, "Setting volume: %f\n", win->volume);
  bg_gtk_slider_set_pos(win->volume_slider,
                        (win->volume - BG_PLAYER_VOLUME_MIN)/
                        (-BG_PLAYER_VOLUME_MIN));
  
  }

/* Gtk Callbacks */

static void realize_callback(GtkWidget * w, gpointer data)
  {
  
  player_window_t * win;
  
  win = (player_window_t*)data;
  
  set_background(win);
  }

static gboolean button_press_callback(GtkWidget * w, GdkEventButton * evt,
                                      gpointer data)
  {
  player_window_t * win;
  
  win = (player_window_t*)data;
  
  win->mouse_x = (int)(evt->x);
  win->mouse_y = (int)(evt->y);
  
  return TRUE;
  }


static gboolean motion_callback(GtkWidget * w, GdkEventMotion * evt,
                                gpointer data)
  {
  player_window_t * win;

  win = (player_window_t*)data;
  gtk_window_move(GTK_WINDOW(win->window),
                  (int)(evt->x_root) - win->mouse_x,
                  (int)(evt->y_root) - win->mouse_y);

  win->window_x = (int)(evt->x_root - evt->x);
  win->window_y = (int)(evt->y_root - evt->y);
  
  return TRUE;
  }

/* Gmerlin callbacks */

static void seek_change_callback(bg_gtk_slider_t * slider, float perc,
                                 void * data)
  {
  player_window_t * win = (player_window_t *)data;

  win->seek_active = 1;
  
  //  player_window_t * win = (player_window_t *)data;
  //  fprintf(stderr, "Seek change callback %f\n", perc);

  display_set_time(win->display, (gavl_time_t)(perc *
                                               (float)win->duration + 0.5));
  }

static void seek_release_callback(bg_gtk_slider_t * slider, float perc,
                                  void * data)
  {
  gavl_time_t time;
  player_window_t * win = (player_window_t *)data;

  time = (gavl_time_t)(perc * (double)win->duration);
  
  //  player_window_t * win = (player_window_t *)data;
  //  fprintf(stderr, "Seek release callback %f\n", perc);
  bg_player_seek(win->gmerlin->player, time);
  
  }

static void volume_change_callback(bg_gtk_slider_t * slider, float perc,
                                   void * data)
  {
  float volume;
  player_window_t * win = (player_window_t *)data;
  
  volume = BG_PLAYER_VOLUME_MIN - BG_PLAYER_VOLUME_MIN * perc;
  if(volume > 0.0)
    volume = 0.0;
  
  bg_player_set_volume(win->gmerlin->player, volume);
  win->volume = volume;
  }

static void gmerlin_button_callback(bg_gtk_button_t * b, void * data)
  {
  player_window_t * win = (player_window_t *)data;
  if(b == win->play_button)
    {
    // fprintf(stderr, "Play button clicked\n");

    gmerlin_play(win->gmerlin, BG_PLAY_FLAG_IGNORE_IF_PLAYING);
    
    }
  else if(b == win->pause_button)
    {
    gmerlin_pause(win->gmerlin);
    }
  else if(b == win->stop_button)
    {
    //    fprintf(stderr, "Stop button clicked\n");
    bg_player_stop(win->gmerlin->player);
    }
  else if(b == win->next_button)
    {
    //    fprintf(stderr, "Next button clicked\n");

    bg_media_tree_next(win->gmerlin->tree, 1, win->gmerlin->shuffle_mode);
    //    fprintf(stderr, "Handle: %p plugin %p\n", handle, handle->plugin);

    gmerlin_play(win->gmerlin, BG_PLAY_FLAG_IGNORE_IF_STOPPED);
    }
  else if(b == win->prev_button)
    {
    //    fprintf(stderr, "Prev button clicked\n");
    bg_media_tree_previous(win->gmerlin->tree, 1, win->gmerlin->shuffle_mode);

    gmerlin_play(win->gmerlin, BG_PLAY_FLAG_IGNORE_IF_STOPPED);
    }
  else if(b == win->close_button)
    {
    //    fprintf(stderr, "Close button clicked\n");
    gtk_main_quit();
    }
  }

static void handle_message(player_window_t * win,
                           bg_msg_t * msg)
  {
  int id;
  int arg_i_1;
  int arg_i_2;
  float arg_f_1;
  char * arg_str_1;
  id = bg_msg_get_id(msg);

  gavl_time_t time;

  switch(id)
    {
    case BG_PLAYER_MSG_TIME_CHANGED:
      if(!win->seek_active)
        {
        time = bg_msg_get_arg_time(msg, 0);
        display_set_time(win->display, time);
        if(win->duration != GAVL_TIME_UNDEFINED)
          bg_gtk_slider_set_pos(win->seek_slider,
                                (float)(time) / (float)(win->duration));
        }
      break;
    case BG_PLAYER_MSG_VOLUME_CHANGED:
      arg_f_1 = bg_msg_get_arg_float(msg, 0);
      bg_gtk_slider_set_pos(win->volume_slider,
                            (arg_f_1 - BG_PLAYER_VOLUME_MIN) /
                            (- BG_PLAYER_VOLUME_MIN));
      break;
    case BG_PLAYER_MSG_TRACK_CHANGED:
      //      fprintf(stderr, "Got BG_PLAYER_MSG_TRACK_CHANGED\n");
      arg_i_1 = bg_msg_get_arg_int(msg, 0);
      gmerlin_check_next_track(win->gmerlin, arg_i_1);
      

      break;
    case BG_PLAYER_MSG_STATE_CHANGED:
      win->gmerlin->player_state = bg_msg_get_arg_int(msg, 0);
      
      switch(win->gmerlin->player_state)
        {
        case BG_PLAYER_STATE_PAUSED:
          display_set_state(win->display, win->gmerlin->player_state, NULL);
          break;
        case BG_PLAYER_STATE_SEEKING:
          display_set_state(win->display, win->gmerlin->player_state, NULL);
          win->seek_active = 0;
          break;
        case BG_PLAYER_STATE_ERROR:
          arg_str_1 = bg_msg_get_arg_string(msg, 1);
          //          fprintf(stderr, "State Error %s\n", arg_str_1);
          display_set_state(win->display, win->gmerlin->player_state, arg_str_1);
          arg_i_2 = bg_msg_get_arg_int(msg, 2);

          switch(arg_i_2)
            {
            case BG_PLAYER_ERROR_TRACK:
              break;
            }
          break;
        case BG_PLAYER_STATE_BUFFERING:
          arg_f_1 = bg_msg_get_arg_float(msg, 1);
          display_set_state(win->display, win->gmerlin->player_state, &arg_f_1);
          break;
        case BG_PLAYER_STATE_PLAYING:
          display_set_state(win->display, win->gmerlin->player_state, NULL);
          bg_media_tree_mark_error(win->gmerlin->tree, 0);
          break;
        case BG_PLAYER_STATE_STOPPED:
          bg_gtk_slider_set_state(win->seek_slider,
                                  BG_GTK_SLIDER_INACTIVE);
          //          fprintf(stderr, "Player is now stopped\n");
          display_set_state(win->display, win->gmerlin->player_state, NULL);
          break;
        case BG_PLAYER_STATE_CHANGING:
          arg_i_2 = bg_msg_get_arg_int(msg, 1);

          //          fprintf(stderr, "Changing 2 %d\n", arg_i_2);
          
          display_set_state(win->display, win->gmerlin->player_state, NULL);
          if(arg_i_2)
            gmerlin_next_track(win->gmerlin);
          break;
        }
      break;
    case BG_PLAYER_MSG_TRACK_NAME:
      arg_str_1 = bg_msg_get_arg_string(msg, 0);
      display_set_track_name(win->display, arg_str_1);
      //      fprintf(stderr, "BG_PLAYER_MSG_TRACK_NAME %s\n", arg_str_1);
      free(arg_str_1);
      break;
    case BG_PLAYER_MSG_TRACK_NUM_STREAMS:
      break;
    case BG_PLAYER_MSG_TRACK_DURATION:
      win->duration = bg_msg_get_arg_time(msg, 0);

      arg_i_2 = bg_msg_get_arg_int(msg, 1);

      //      fprintf(stderr, "Duration: %f %d\n", gavl_time_to_seconds(win->duration),
      //              arg_i_2);

      if(arg_i_2)
        bg_gtk_slider_set_state(win->seek_slider,
                                BG_GTK_SLIDER_ACTIVE);
      else if(win->duration != GAVL_TIME_UNDEFINED)
        bg_gtk_slider_set_state(win->seek_slider,
                                BG_GTK_SLIDER_INACTIVE);
      else
        bg_gtk_slider_set_state(win->seek_slider,
                                BG_GTK_SLIDER_HIDDEN);
      break;
    case BG_PLAYER_MSG_AUDIO_STREAM:
      break;
    case BG_PLAYER_MSG_VIDEO_STREAM:
      break;
    case BG_PLAYER_MSG_AUDIO_DESCRIPTION:
      break;
    case BG_PLAYER_MSG_VIDEO_DESCRIPTION:
      break;
    case BG_PLAYER_MSG_SUBPICTURE_DESCRIPTION:
      break;
    case BG_PLAYER_MSG_STREAM_DESCRIPTION:
      break;
    }
  
  }

gboolean idle_callback(gpointer data)
  {
  bg_msg_t * msg;
  
  player_window_t * w = (player_window_t *)data;

  while((msg = bg_msg_queue_try_lock_read(w->msg_queue)))
    {
    handle_message(w, msg);
    bg_msg_queue_unlock_read(w->msg_queue);
    }
  
  /* Handle remote control */

  while((msg = bg_remote_server_get_msg(w->gmerlin->remote)))
    {
    //    fprintf(stderr, "Got remote message %d\n", bg_msg_get_id(msg));
    gmerlin_handle_remote(w->gmerlin, msg);

    }
  return TRUE;
  }

player_window_t * player_window_create(gmerlin_t * g)
  {
  player_window_t * ret;

  ret = calloc(1, sizeof(*ret));
  ret->gmerlin = g;

  ret->tooltips = gtk_tooltips_new();
  
  g_object_ref (G_OBJECT (ret->tooltips));
  gtk_object_sink (GTK_OBJECT (ret->tooltips));
  
  ret->msg_queue = bg_msg_queue_create();

  bg_player_add_message_queue(g->player,
                              ret->msg_queue);

  g_timeout_add(DELAY_TIME, idle_callback, (gpointer)ret);
  
  /* Create objects */
  
  ret->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(ret->window), FALSE);
  
  ret->layout = gtk_layout_new((GtkAdjustment*)0,
                               (GtkAdjustment*)0);
  
  /* Set attributes */

  gtk_widget_set_events(ret->layout,
                        GDK_BUTTON1_MOTION_MASK|
                        GDK_BUTTON2_MOTION_MASK|
                        GDK_BUTTON3_MOTION_MASK|
                        GDK_BUTTON_PRESS_MASK);
  /* Set Callbacks */

  g_signal_connect(G_OBJECT(ret->window), "realize",
                   G_CALLBACK(realize_callback), (gpointer*)ret);
  g_signal_connect(G_OBJECT(ret->layout), "realize",
                   G_CALLBACK(realize_callback), (gpointer*)ret);

  g_signal_connect(G_OBJECT(ret->layout), "motion-notify-event",
                   G_CALLBACK(motion_callback), (gpointer*)ret);

  g_signal_connect(G_OBJECT(ret->layout), "button-press-event",
                   G_CALLBACK(button_press_callback), (gpointer*)ret);

  /* Create child objects */

  ret->play_button = bg_gtk_button_create();
  ret->stop_button = bg_gtk_button_create();
  ret->next_button = bg_gtk_button_create();
  ret->prev_button = bg_gtk_button_create();
  ret->pause_button = bg_gtk_button_create();
  ret->menu_button = bg_gtk_button_create();
  ret->close_button = bg_gtk_button_create();

  ret->seek_slider = bg_gtk_slider_create();
  ret->volume_slider = bg_gtk_slider_create();
  
  ret->main_menu = main_menu_create(g);

  ret->display = display_create(g, ret->tooltips);
  
  /* Set callbacks */

  bg_gtk_slider_set_change_callback(ret->seek_slider,
                                    seek_change_callback, ret);
  
  bg_gtk_slider_set_release_callback(ret->seek_slider,
                                     seek_release_callback, ret);

  bg_gtk_slider_set_change_callback(ret->volume_slider,
                                    volume_change_callback, ret);
  
  bg_gtk_button_set_callback(ret->play_button, gmerlin_button_callback, ret);
  bg_gtk_button_set_callback(ret->stop_button, gmerlin_button_callback, ret);
  bg_gtk_button_set_callback(ret->pause_button, gmerlin_button_callback, ret);
  bg_gtk_button_set_callback(ret->next_button, gmerlin_button_callback, ret);
  bg_gtk_button_set_callback(ret->prev_button, gmerlin_button_callback, ret);
  bg_gtk_button_set_callback(ret->close_button, gmerlin_button_callback, ret);

  bg_gtk_button_set_menu(ret->menu_button,
                         main_menu_get_widget(ret->main_menu));

  /* Set tooltips */
  
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->play_button),
                       "Play", "Play");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->stop_button),
                       "Stop", "Stop");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->pause_button),
                       "Pause", "Pause");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->next_button),
                       "Next track", "Next track");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->prev_button),
                       "Previous track", "Previous track");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->menu_button),
                       "Main menu", "Main menu");
  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_button_get_widget(ret->close_button),
                       "Quit program", "Quit program");

  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_slider_get_slider_widget(ret->volume_slider),
                       "Volume", "Volume");

  gtk_tooltips_set_tip(ret->tooltips, bg_gtk_slider_get_slider_widget(ret->seek_slider),
                       "Seek", "Seek");
  
  /* Pack Objects */

  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->play_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->stop_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->pause_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->next_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->prev_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->close_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_button_get_widget(ret->menu_button),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_slider_get_widget(ret->seek_slider),
                 0, 0);
  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 bg_gtk_slider_get_widget(ret->volume_slider),
                 0, 0);

  gtk_layout_put(GTK_LAYOUT(ret->layout),
                 display_get_widget(ret->display),
                 0, 0);
  
  gtk_widget_show(ret->layout);
  gtk_container_add(GTK_CONTAINER(ret->window), ret->layout);
    
  return ret;
  }

void player_window_show(player_window_t * win)
  {
  gtk_window_move(GTK_WINDOW(win->window),
                  win->window_x,
                  win->window_y);
  gtk_widget_show(win->window);
  }

void player_window_destroy(player_window_t * win)
  {
  /* Fetch parameters */
  
  bg_cfg_section_get(win->gmerlin->display_section,
                     display_get_parameters(win->display),
                     display_get_parameter, (void*)(win->display));

  bg_msg_queue_destroy(win->msg_queue);

  bg_gtk_slider_destroy(win->seek_slider);
  bg_gtk_slider_destroy(win->volume_slider);

  main_menu_destroy(win->main_menu);
  g_object_unref(win->tooltips);
  
  free(win);
  }

void player_window_set_tooltips(player_window_t * win, int enable)
  {
  if(enable)
    gtk_tooltips_enable(win->tooltips);
  else
    gtk_tooltips_disable(win->tooltips);
  }


void player_window_skin_load(player_window_skin_t * s,
                             xmlDocPtr doc, xmlNodePtr node)
  {
  xmlNodePtr child;
  char * tmp_string;
  child = node->children;
  while(child)
    {
    if(!child->name)
      {
      child = child->next;
      continue;
      }
    else if(!strcmp(child->name, "BACKGROUND"))
      {
      tmp_string = xmlNodeListGetString(doc, child->children, 1);
      s->background = bg_strdup(s->background, tmp_string);
      xmlFree(tmp_string);
      }
    else if(!strcmp(child->name, "DISPLAY"))
      display_skin_load(&(s->display), doc, child);
    else if(!strcmp(child->name, "PLAYBUTTON"))
      bg_gtk_button_skin_load(&(s->play_button), doc, child);
    else if(!strcmp(child->name, "PAUSEBUTTON"))
      bg_gtk_button_skin_load(&(s->pause_button), doc, child);
    else if(!strcmp(child->name, "NEXTBUTTON"))
      bg_gtk_button_skin_load(&(s->next_button), doc, child);
    else if(!strcmp(child->name, "PREVBUTTON"))
      bg_gtk_button_skin_load(&(s->prev_button), doc, child);
    else if(!strcmp(child->name, "STOPBUTTON"))
      bg_gtk_button_skin_load(&(s->stop_button), doc, child);
    else if(!strcmp(child->name, "MENUBUTTON"))
      bg_gtk_button_skin_load(&(s->menu_button), doc, child);
    else if(!strcmp(child->name, "CLOSEBUTTON"))
      bg_gtk_button_skin_load(&(s->close_button), doc, child);
    else if(!strcmp(child->name, "SEEKSLIDER"))
      bg_gtk_slider_skin_load(&(s->seek_slider), doc, child);
    else if(!strcmp(child->name, "VOLUMESLIDER"))
      bg_gtk_slider_skin_load(&(s->volume_slider), doc, child);
    else if(!strcmp(child->name, "DISPLAY"))
      display_skin_load(&(s->display), doc, child);
    child = child->next;
    }
  }

void player_window_skin_destroy(player_window_skin_t * s)
  {
  if(s->background)
    free(s->background);
  bg_gtk_button_skin_free(&(s->play_button));
  bg_gtk_button_skin_free(&(s->stop_button));
  bg_gtk_button_skin_free(&(s->pause_button));
  bg_gtk_button_skin_free(&(s->next_button));
  bg_gtk_button_skin_free(&(s->prev_button));
  bg_gtk_button_skin_free(&(s->close_button));
  bg_gtk_button_skin_free(&(s->menu_button));
  bg_gtk_slider_skin_free(&(s->seek_slider));
  bg_gtk_slider_skin_free(&(s->volume_slider));
  
  }
