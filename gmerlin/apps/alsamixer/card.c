#include <utils.h>
#include "alsamixer.h"

int alsa_mixer_control_read(alsa_mixer_control_t * c)
  {
  if(snd_hctl_elem_read(c->hctl, c->val))
    {
    fprintf(stderr, "snd_hctl_elem_read Failed\n");
    return 0;
    }
  return 1;
  }


int alsa_mixer_control_write(alsa_mixer_control_t * c)
  {
  if(snd_hctl_elem_write(c->hctl, c->val))
    {
    fprintf(stderr, "snd_hctl_elem_write Failed\n");
    return 0;
    }
  return 1;
  }

static alsa_mixer_group_t * get_group(alsa_card_t * c, const char * label)
  {
  int i;
  for(i = 0; i < c->num_groups; i++)
    {
    if(!strcmp(c->groups[i].label, label))
      {
      return &(c->groups[i]);
      }
    }
  c->num_groups++;
  c->groups = realloc(c->groups, c->num_groups * sizeof(*(c->groups)));
  memset(c->groups + (c->num_groups-1), 0, sizeof(*(c->groups)));
  c->groups[c->num_groups-1].label =
    bg_strdup(c->groups[c->num_groups-1].label, label);
  return &(c->groups[c->num_groups-1]);
  }

static alsa_mixer_control_t * create_control(snd_hctl_elem_t * hctl_elem)
  {
  alsa_mixer_control_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->hctl = hctl_elem;
  snd_ctl_elem_value_malloc(&(ret->val));
  alsa_mixer_control_read(ret);
  return ret;
  }

alsa_card_t * alsa_card_create(int index)
  {
  char name[32];
  alsa_card_t * card;
  int err;
  snd_hctl_elem_t * hctl_elem;
  snd_ctl_t * ctl;
  const char * element_name;
  snd_ctl_elem_info_t * info;
  snd_ctl_elem_id_t * id;
  snd_ctl_card_info_t * card_info;
  
  char ** strings;
  int num_strings;
  char * label;
  char * tmp_label;
  int done;
  // int is_bass     = 0;
  //  int is_treble   = 0;
  //  int is_tone_switch;
  

  int is_switch   = 0;
  int is_volume   = 0;
  int is_playback = 0;
  int is_capture  = 0;
  int elem_index;
  int i;
  alsa_mixer_group_t * group;
  
  sprintf(name, "hw:%d", index);
  
  card = calloc(1, sizeof(*card));
  
  if((err = snd_hctl_open(&(card->hctl), name, 0)))
    {
    fprintf(stderr, "snd_hctl_open failed\n");
    goto fail;
    }
  if((err = snd_hctl_load(card->hctl)) < 0)
    {
    fprintf(stderr, "snd_hctl_load failed\n");
    goto fail;
    }

  ctl = snd_hctl_ctl(card->hctl);
  
  if(snd_ctl_card_info_malloc(&(card_info)))
    {
    fprintf(stderr, "snd_ctl_card_info_malloc failed\n");
    goto fail;
    }
  if(snd_ctl_card_info(ctl, card_info))
    {
    fprintf(stderr, "snd_ctl_card_info failed\n");
    goto fail;
    }
  card->name = bg_strdup(card->name,
                         snd_ctl_card_info_get_mixername(card_info));
  
  hctl_elem = snd_hctl_first_elem(card->hctl);

  while(hctl_elem)
    {
    done = 0;
    snd_ctl_elem_info_malloc(&info);

    snd_ctl_elem_id_malloc(&id);
    snd_hctl_elem_get_id(hctl_elem, id);
    //    dump_ctl_elem_id(id);
    element_name = snd_ctl_elem_id_get_name(id);
    elem_index = snd_ctl_elem_id_get_index(id);

    /* Special case: Tone control */
    
    if(!strcmp(element_name, "Tone Control - Switch"))
      {
      group = get_group(card, "Tone");
      group->tone_switch = create_control(hctl_elem);
      done = 1;
      }
    else if(!strcmp(element_name, "Tone Control - Bass"))
      {
      group = get_group(card, "Tone");
      group->tone_bass = create_control(hctl_elem);
      done = 1;
      }
    else if(!strcmp(element_name, "Tone Control - Treble"))
      {
      group = get_group(card, "Tone");
      group->tone_treble = create_control(hctl_elem);
      done = 1;
      }

    if(done)
      {
      snd_ctl_elem_id_free(id);
      hctl_elem = snd_hctl_elem_next(hctl_elem);
      continue;
      }
    
    strings = bg_strbreak(element_name, ' ');
    num_strings = 0;
    while(strings[num_strings])
      num_strings++;

    is_switch   = 0;
    is_volume   = 0;
    is_playback = 0;
    is_capture  = 0;
    
    if(num_strings >= 2)
      {
      if(!strcmp(strings[num_strings-1], "Switch"))
        {
        is_switch = 1;
        num_strings--;
        }
      else if(!strcmp(strings[num_strings-1], "Volume"))
        {
        is_volume = 1;
        num_strings--;
        }
      
      if(!strcmp(strings[num_strings-1], "Playback"))
        {
        is_playback = 1;
        num_strings--;
        }
      else if(!strcmp(strings[num_strings-1], "Capture"))
        {
        is_capture = 1;
        num_strings--;
        }
      }

    if(num_strings)
      {
      label = (char*)0;
      for(i = 0; i < num_strings; i++)
        {
        label = bg_strcat(label, strings[i]);
        if(i < num_strings - 1)
          {
          label = bg_strcat(label, " ");
          }
        }
      }
    else if(is_capture && (is_volume || is_switch))
      {
      label = bg_strdup(NULL, "Capture");
      }
    else
      {
      label = bg_strdup(NULL, "Unknown");
      }
    
    if(elem_index > 0)
      {
      tmp_label = bg_sprintf("%s %d", label, elem_index+1);
      free(label);
      label = tmp_label;
      }
    group = get_group(card, label);

    if(is_capture)
      {
      if(is_switch)
        group->capture_switch = create_control(hctl_elem);
      else if(is_volume)
        group->capture_volume = create_control(hctl_elem);
      else
        group->ctl = create_control(hctl_elem);
      }
    else if(is_playback)
      {
      if(is_switch)
        group->playback_switch = create_control(hctl_elem);
      else if(is_volume)
        group->playback_volume = create_control(hctl_elem);
      else
        group->ctl = create_control(hctl_elem);
      }
    else
      group->ctl = create_control(hctl_elem);

    free(label);
    bg_strbreak_free(strings);
    snd_ctl_elem_id_free(id);
    hctl_elem = snd_hctl_elem_next(hctl_elem);
    }
      
  
  return card;
  fail:
  alsa_card_destroy(card);
  return (alsa_card_t *)0;
  }

void alsa_card_destroy(alsa_card_t * c)
  {
  free(c);
  }

static void dump_ctl_elem_id(snd_ctl_elem_id_t * id)
  {
  fprintf(stderr, "  ID:\n");
  fprintf(stderr, "    numid:     %d\n", snd_ctl_elem_id_get_numid(id));
  fprintf(stderr, "    device:    %d\n", snd_ctl_elem_id_get_device(id));
  fprintf(stderr, "    subdevice: %d\n", snd_ctl_elem_id_get_subdevice(id));
  fprintf(stderr, "    name:      %s\n", snd_ctl_elem_id_get_name(id));
  fprintf(stderr, "    index:     %d\n", snd_ctl_elem_id_get_index(id));
  fprintf(stderr, "    interface: %s\n",
          snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(id)));
  }

static void dump_ctl_elem_info(snd_hctl_elem_t * hctl,
                               snd_ctl_elem_info_t * info)
  {
  snd_ctl_elem_type_t type;
  int i, num_items;
  fprintf(stderr, "  ELEM_INFO:\n");

  type = snd_ctl_elem_info_get_type(info);
  
  fprintf(stderr, "    Type: %s\n",
          snd_ctl_elem_type_name(type));
  fprintf(stderr, "    Owner: %d\n", snd_ctl_elem_info_get_owner(info));
  fprintf(stderr, "    Count: %d\n", snd_ctl_elem_info_get_count(info));
  //  fprintf(stderr, "  : %d\n", snd_ctl_elem_info_get_count(info));

  if(type == SND_CTL_ELEM_TYPE_INTEGER)
    {
    fprintf(stderr, "    Min: %ld, Max: %ld, Step: %ld\n",
            snd_ctl_elem_info_get_min(info),
            snd_ctl_elem_info_get_max(info),
            snd_ctl_elem_info_get_step(info));
    }
  else if(type == SND_CTL_ELEM_TYPE_INTEGER64)
    {
    fprintf(stderr, "    Min: %lld, Max: %lld, Step: %lld\n",
            snd_ctl_elem_info_get_min64(info),
            snd_ctl_elem_info_get_max64(info),
            snd_ctl_elem_info_get_step64(info));
    }
  else if(type == SND_CTL_ELEM_TYPE_ENUMERATED)
    {
    num_items = snd_ctl_elem_info_get_items(info);
    for(i = 0; i < num_items; i++)
      {
      snd_ctl_elem_info_set_item(info,i);
      snd_hctl_elem_info(hctl,info);
      fprintf(stderr, "    Item %d: %s\n", i+1,
              snd_ctl_elem_info_get_item_name(info));
      }
    }
  }

static void dump_hctl_elem(snd_hctl_elem_t * h)
  {
  snd_ctl_elem_id_t * id;
  snd_ctl_elem_info_t * info;
  
  snd_ctl_elem_id_malloc(&id);
  snd_hctl_elem_get_id(h, id);
  dump_ctl_elem_id(id);
  snd_ctl_elem_id_free(id);
  
  snd_ctl_elem_info_malloc(&info);
  snd_hctl_elem_info(h, info);
  dump_ctl_elem_info(h, info);
  snd_ctl_elem_info_free(info);
  }

static void dump_control(alsa_mixer_control_t * c)
  {
  fprintf(stderr, "HCTL:\n");
  dump_hctl_elem(c->hctl);
  }

static void dump_group(alsa_mixer_group_t * g)
  {
  if(g->playback_switch)
    {
    fprintf(stderr, "Playback switch:");
    dump_control(g->playback_switch);
    }
  if(g->playback_volume)
    {
    fprintf(stderr, "Playback volume:\n");
    dump_control(g->playback_volume);
    }
  if(g->capture_switch)
    {
    fprintf(stderr, "Capture switch:\n");
    dump_control(g->capture_switch);
    }
  if(g->capture_volume)
    {
    fprintf(stderr, "Capture volume:\n");
    dump_control(g->capture_volume);
    }
  if(g->ctl)
    {
    fprintf(stderr, "Control:\n"); 
    dump_control(g->ctl);
    }
  }

void alsa_card_dump(alsa_card_t * c)
  {
  int i;
  for(i = 0; i < c->num_groups; i++)
    {
    fprintf(stderr, "Group %d: %s\n", i+1, c->groups[i].label);
    dump_group(&(c->groups[i]));
    }
  
  }
