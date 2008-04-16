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

#include <string.h>
#include <stdlib.h>

#include <avdec_private.h>
#include <mxf.h>

#define LOG_DOMAIN "mxf"

#define DUMP_UNKNOWN

#define FREE(ptr) if(ptr) free(ptr);

/* Debug functions */

static void do_indent(int i)
  {
  while(i--)
    bgav_dprintf(" ");
  }

static void dump_ul(const uint8_t * u)
  {
  bgav_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15]);               
  }

static void dump_ul_nb(const uint8_t * u)
  {
  bgav_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15]);               
  }

static void dump_ul_ptr(const uint8_t * u, void * ptr)
  {
  bgav_dprintf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x (%p)\n",
               u[0], u[1], u[2], u[3], 
               u[4], u[5], u[6], u[7], 
               u[8], u[9], u[10], u[11], 
               u[12], u[13], u[14], u[15], ptr);            
  }

static void dump_date(uint64_t d)
  {
  bgav_dprintf("%04d-%02d-%02d %02d:%02d:%02d:%03d",
               (int)(d>>48),
               (int)(d>>40 & 0xff),
               (int)(d>>32 & 0xff),
               (int)(d>>24 & 0xff),
               (int)(d>>16 & 0xff),
               (int)(d>>8 & 0xff),
               (int)(d & 0xff));
  
  }

/* Read list of ULs */

static mxf_ul_t * read_refs(bgav_input_context_t * input, uint32_t * num)
  {
  mxf_ul_t * ret;
  if(!bgav_input_read_32_be(input, num))
    return (mxf_ul_t*)0;
  /* Skip size */
  bgav_input_skip(input, 4);
  if(!num)
    return (mxf_ul_t *)0;
  ret = malloc(sizeof(*ret) * *num);
  if(bgav_input_read_data(input, (uint8_t*)ret, sizeof(*ret) * *num) < sizeof(*ret) * *num)
    {
    free(ret);
    return (mxf_ul_t*)0;
    }
  return ret;
  }

/* Resolve references */

static mxf_metadata_t *
resolve_strong_ref(mxf_file_t * ret, mxf_ul_t u, mxf_metadata_type_t type)
  {
  int i;
  for(i = 0; i < ret->header.num_metadata; i++)
    {
    if(!memcmp(u, ret->header.metadata[i]->uid, 16) &&
       (type & ret->header.metadata[i]->type))
      return ret->header.metadata[i];
    }
  return (mxf_metadata_t*)0;
  }

static mxf_metadata_t *
package_by_ul(mxf_file_t * ret, mxf_ul_t u)
  {
  int i;
  mxf_package_t * mp;
  mxf_preface_t * p = (mxf_preface_t*)ret->preface;
  
  for(i = 0; i < ((mxf_content_storage_t*)(p->content_storage))->num_package_refs; i++)
    {
    mp = (mxf_package_t*)(((mxf_content_storage_t*)(p->content_storage))->packages[i]);
    
    if((mp->common.type == MXF_TYPE_SOURCE_PACKAGE) && !memcmp(u, mp->package_ul, 16))
      return (mxf_metadata_t*)mp;
    }
  return (mxf_metadata_t*)0;
  }

static mxf_metadata_t **
resolve_strong_refs(mxf_file_t * file, mxf_ul_t * u, int num, mxf_metadata_type_t type)
  {
  int i;
  mxf_metadata_t ** ret;

  if(!num)
    return (mxf_metadata_t**)0;

  ret = calloc(num, sizeof(*ret));
  
  for(i = 0; i < num; i++)
    ret[i] = resolve_strong_ref(file, u[i], type);
  return ret;
  }

/* Operational pattern ULs (taken from ingex) */

#define MXF_OP_L(def, name) \
    g_##name##_op_##def##_label

#define MXF_OP_L_LABEL(regver, complexity, byte14, qualifier) \
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, regver, 0x0d, 0x01, 0x02, 0x01, complexity, byte14, qualifier, 0x00}
    
    
/* OP Atom labels */
 
#define MXF_ATOM_OP_L(byte14) \
    MXF_OP_L_LABEL(0x02, 0x10, byte14, 0x00)

#if 0
static const mxf_ul_t MXF_OP_L(atom, complexity00) = 
    MXF_ATOM_OP_L(0x00);
    
static const mxf_ul_t MXF_OP_L(atom, complexity01) = 
    MXF_ATOM_OP_L(0x01);
    
static const mxf_ul_t MXF_OP_L(atom, complexity02) = 
    MXF_ATOM_OP_L(0x02);
    
static const mxf_ul_t MXF_OP_L(atom, complexity03) = 
    MXF_ATOM_OP_L(0x03);
#endif

//static const mxf_ul_t g_opAtomPrefix = MXF_ATOM_OP_L(0);

static const mxf_ul_t op_atom =
  { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0xff, 0x0d, 0x01, 0x02, 0x01, 0x10, 0xff, 0x00, 0x00 };

static int is_op_atom(const mxf_ul_t label)
  {
  int i;
  for(i = 0; i < 16; i++)
    {
    if((op_atom[i] != 0xff) && (op_atom[i] != label[i]))
      return 0;
    }
      
  //  return memcmp(&g_opAtomPrefix, label, 13) == 0;
  return 1;
  }

/* OP-1A labels */
 
#define MXF_1A_OP_L(qualifier) \
    MXF_OP_L_LABEL(0x01, 0x01, 0x01, qualifier)

#if 0
/* internal essence, stream file, multi-track */
static const mxf_ul_t MXF_OP_L(1a, qq09) = 
    MXF_1A_OP_L(0x09);
#endif

// static const mxf_ul_t g_op1APrefix = MXF_1A_OP_L(0);   

// #define MXF_OP_L_LABEL(regver, complexity, byte14, qualifier)
//{ 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, regver, 0x0d, 0x01, 0x02, 0x01, complexity, byte14, qualifier, 0x00}

static const mxf_ul_t op_1a =
  { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0xff,   0x0d, 0x01, 0x02, 0x01,       0x01,   0x01,      0xff, 0x00};

static int is_op_1a(const mxf_ul_t label)
  {
  int i;
  for(i = 0; i < 16; i++)
    {
    if((op_1a[i] != 0xff) && (op_1a[i] != label[i]))
      return 0;
    }
  return 1;
  //  return memcmp(&g_op1APrefix, label, 13) == 0;
  }

/* Partial ULs */

static const uint8_t mxf_klv_key[] = { 0x06,0x0e,0x2b,0x34 };

static const uint8_t mxf_essence_element_key[] =
  { 0x06,0x0e,0x2b,0x34,0x01,0x02,0x01,0x01,0x0d,0x01,0x03,0x01 };

/* Complete ULs */

static const uint8_t mxf_header_partition_pack_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x02 };

static const uint8_t mxf_filler_key[] =
  {0x06,0x0e,0x2b,0x34,0x01,0x01,0x01,0x01,0x03,0x01,0x02,0x10,0x01,0x00,0x00,0x00};

static const uint8_t mxf_primer_pack_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x05,0x01,0x00 };

static const uint8_t mxf_content_storage_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x18,0x00 };

static const uint8_t mxf_source_package_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x37,0x00 };

static const uint8_t mxf_material_package_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x36,0x00 };

static const uint8_t mxf_sequence_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x0F,0x00 };

static const uint8_t mxf_source_clip_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x11,0x00 };

static const uint8_t mxf_static_track_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x3B,0x00 };

static const uint8_t mxf_generic_track_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x04,0x01,0x02,0x02,0x00,0x00 };

static const uint8_t mxf_index_table_segment_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x10,0x01,0x00 };

static const uint8_t mxf_timecode_component_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x14,0x00 };

static const uint8_t mxf_identification_key[] =
  { 0x06,0x0e,0x2b,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x30,0x00 };

static const uint8_t mxf_essence_container_data_key[] =
{ 0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x23, 0x00 };

static const uint8_t mxf_preface_key[] =
{ 0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2f, 0x00 };

/* Descriptors */

static const uint8_t mxf_descriptor_multiple_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x44,0x00 };

static const uint8_t mxf_descriptor_generic_sound_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x42,0x00 };

static const uint8_t mxf_descriptor_cdci_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x28,0x00 };

static const uint8_t mxf_descriptor_rgba_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x29,0x00 };

static const uint8_t mxf_descriptor_mpeg2video_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x51,0x00 };

static const uint8_t mxf_descriptor_wave_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x48,0x00 };

static const uint8_t mxf_descriptor_aes3_key[] =
  { 0x06,0x0E,0x2B,0x34,0x02,0x53,0x01,0x01,0x0d,0x01,0x01,0x01,0x01,0x01,0x47,0x00 };

/* MPEG-4 extradata */
static const uint8_t mxf_sony_mpeg4_extradata[] =
  { 0x06,0x0e,0x2b,0x34,0x04,0x01,0x01,0x01,0x0e,0x06,0x06,0x02,0x02,0x01,0x00,0x00 };

#define UL_MATCH(d, key) !memcmp(d, key, sizeof(key))

#define UL_MATCH_MOD_REGVER(d, key) (!memcmp(d, key, 7) && !memcmp(&d[8], &key[8], 8))

static int match_ul(const mxf_ul_t u1, const mxf_ul_t u2, int len)
  {
  int i;
  for (i = 0; i < len; i++)
    {
    if (i != 7 && u1[i] != u2[i])
      return 0;
    }
  return 1;
  }

#if 0
/* Operational patterns */

typedef struct
  {
  mxf_ul_t ul;
  mxf_op_t op;
  } op_map_t;

op_map_t op_map[] =
  {
    {
      { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02, 0x0d, 0x01, 0x02, 0x01, 0x10, 0x02, 0x00, 0x00 },
      MXF_OP_ATOM
    },
    {
      { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x06, 0x02, 0x01, 0x40, 0x01, 0x09, 0x00 },
      MXF_OP_1a
    },
  };

static mxf_op_t get_op(mxf_ul_t ul)
  {
  int i;
  for(i = 0; i < sizeof(op_map)/sizeof(op_map[0]); i++)
    {
    if(!memcmp(ul, op_map[i].ul, 13))
      return op_map[i].op;
    }
  return MXF_OP_UNKNOWN;
  }

#endif

static const struct
  {
  mxf_op_t op;
  const char * name;
  }
op_names[] =
  {
    { MXF_OP_UNKNOWN, "Unknown" },
    { MXF_OP_1a,      "1a"      },
    { MXF_OP_1b,      "1b"      },
    { MXF_OP_1c,      "1c"      },
    { MXF_OP_2a,      "2a"      },
    { MXF_OP_2b,      "2b"      },
    { MXF_OP_2c,      "2c"      },
    { MXF_OP_3a,      "3a"      },
    { MXF_OP_3b,      "3b"      },
    { MXF_OP_3c,      "3c"      },
    { MXF_OP_ATOM,    "Atom"    },
  };

static const char * get_op_name(mxf_op_t op)
  {
  int i;
  for(i = 0; i < sizeof(op_names)/sizeof(op_names[0]); i++)
    {
    if(op == op_names[i].op)
      return op_names[i].name;
    }
  return op_names[0].name;
  }

/* Stream types */

typedef struct
  {
  mxf_ul_t ul;
  bgav_stream_type_t type;
  } stream_entry_t;

/* SMPTE RP224 http://www.smpte-ra.org/mdd/index.html */
static const stream_entry_t mxf_data_definition_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x01,0x00,0x00,0x00 }, BGAV_STREAM_VIDEO },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x01,0x03,0x02,0x02,0x02,0x00,0x00,0x00 }, BGAV_STREAM_AUDIO },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x05,0x01,0x03,0x02,0x02,0x02,0x02,0x00,0x00 }, BGAV_STREAM_AUDIO },
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, BGAV_STREAM_UNKNOWN /* CODEC_TYPE_DATA */},
};

static const stream_entry_t * match_stream(const stream_entry_t * se, const mxf_ul_t u1)
  {
  int i = 0;

  //  fprintf(stderr, "Match stream: ");
  //  dump_ul(u1);
  //  fprintf(stderr, "\n");
  
  while(se[i].type != BGAV_STREAM_UNKNOWN)
    {
    if(match_ul(se[i].ul, u1, 16))
      return &se[i];
    i++;
    }
  return (stream_entry_t*)0;
  }

typedef struct
  {
  mxf_ul_t ul;
  int match_len;
  uint32_t fourcc;
  } codec_entry_t;

static const codec_entry_t mxf_video_codec_uls[] =
  {
    /* PictureEssenceCoding */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x01,0x11,0x00 },
      14, BGAV_MK_FOURCC('m', 'p', 'g', 'v')}, /* MP@ML Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x01,0x02,0x01,0x01 },
      14, BGAV_MK_FOURCC('m', 'p', 'g', 'v') }, /* D-10 50Mbps PAL */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x03,0x03,0x00 },
      14, BGAV_MK_FOURCC('m', 'p', 'g', 'v') }, /* MP@HL Long GoP */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x04,0x02,0x00 },
      14, BGAV_MK_FOURCC('m', 'p', 'g', 'v') }, /* 422P@HL I-Frame */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x03,0x04,0x01,0x02,0x02,0x01,0x20,0x02,0x03 },
      14, BGAV_MK_FOURCC('m', 'p', '4', 'v') }, /* XDCAM proxy_pal030926.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x02,0x02,0x01,0x02,0x00 },
      13, BGAV_MK_FOURCC('d', 'v', 'c', 'p') }, /* DV25 IEC PAL */
      //    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x01,0x02,0x02,0x03,0x01,0x01,0x00 }, 14, CODEC_ID_JPEG2000 }, /* JPEG2000 Codestream */
      //    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x01,0x02,0x01,0x7F,0x00,0x00,0x00 }, 13, CODEC_ID_RAWVIDEO }, /* Uncompressed */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, 0x00 },
  };

static const codec_entry_t mxf_audio_codec_uls[] =
  {
    /* SoundEssenceCompression */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x00,0x00,0x00,0x00 },
      13, BGAV_MK_FOURCC('s', 'o', 'w', 't') }, /* Uncompressed */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x01,0x7F,0x00,0x00,0x00 },
      13, BGAV_MK_FOURCC('s', 'o', 'w', 't') },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x07,0x04,0x02,0x02,0x01,0x7E,0x00,0x00,0x00 },
      13, BGAV_MK_FOURCC('t', 'w', 'o', 's') }, /* From Omneon MXF file */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x04,0x04,0x02,0x02,0x02,0x03,0x01,0x01,0x00 },
      15, BGAV_MK_FOURCC('a', 'l', 'a', 'w') }, /* XDCAM Proxy C0023S01.mxf */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x01,0x00 },
      15, BGAV_MK_FOURCC('.', 'a', 'c', '3') },
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x05,0x00 },
      15, BGAV_MK_FOURCC('.', 'm', 'p', '3') }, /* MP2 or MP3 */
    //{ { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x04,0x02,0x02,0x02,0x03,0x02,0x1C,0x00 },
    // 15, CODEC_ID_DOLBY_E }, /* Dolby-E */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, 0, 0x00 },
  };

static const codec_entry_t  mxf_picture_essence_container_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x02,0x0D,0x01,0x03,0x01,0x02,0x04,0x60,0x01 },
      14, BGAV_MK_FOURCC('m', 'p', 'g', 'v') }, /* MPEG-ES Frame wrapped */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x02,0x41,0x01 },
      14, BGAV_MK_FOURCC('d', 'v', 'c', 'p') }, /* DV 625 25mbps */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, 0x00 },
};

static const codec_entry_t  mxf_sound_essence_container_uls[] = {
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x06,0x01,0x00 },
      14, BGAV_MK_FOURCC('s', 'o', 'w', 't') }, /* BWF Frame wrapped */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x02,0x0D,0x01,0x03,0x01,0x02,0x04,0x40,0x01 },
      14, BGAV_MK_FOURCC('.', 'm', 'p', '3') }, /* MPEG-ES Frame wrapped, 0x40 ??? stream id */
    { { 0x06,0x0E,0x2B,0x34,0x04,0x01,0x01,0x01,0x0D,0x01,0x03,0x01,0x02,0x01,0x01,0x01 },
      14, BGAV_MK_FOURCC('s', 'o', 'w', 't') }, /* D-10 Mapping 50Mbps PAL Extended Template */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },  0, 0x00 },
};

static const codec_entry_t * match_codec(const codec_entry_t * ce, const mxf_ul_t u1)
  {
  int i = 0;
  //  fprintf(stderr, "Match codec: ");
  //  dump_ul(u1);
  //  fprintf(stderr, "\n");
  while(ce[i].fourcc)
    {
    if(match_ul(ce[i].ul, u1, ce[i].match_len))
      return &ce[i];
    i++;
    }
  return (codec_entry_t*)0;
  }

static char * read_utf16_string(bgav_input_context_t * input, int len)
  {
  bgav_charset_converter_t * cnv;
  char * str, * ret;
  cnv = bgav_charset_converter_create(input->opt, "UTF-16BE", "UTF-8");
  if(!cnv)
    {
    bgav_input_skip(input, len);
    return (char*)0;
    }
  str = malloc(len);
  if(bgav_input_read_data(input, (uint8_t*)str, len) < len)
    {
    bgav_charset_converter_destroy(cnv);
    return (char*)0;
    }

  ret = bgav_convert_string(cnv, str, len, (int *)0);
  bgav_charset_converter_destroy(cnv);
  free(str);
  return ret;
  }

int bgav_mxf_klv_read(bgav_input_context_t * input, mxf_klv_t * ret)
  {
  uint8_t c;
  int64_t len;
  /* Key */
  if(bgav_input_read_data(input, ret->key, 16) < 16)
    return 0;

  /* Length */
  if(!bgav_input_read_8(input, &c))
    return 0;

  if(c & 0x80) /* Long */
    {
    len = c & 0x0F;
    ret->length = 0;
    if(len > 8)
      return 0;
    while(len--)
      {
      if(!bgav_input_read_8(input, &c))
        return 0;
      ret->length = (ret->length<<8) | c;
      }
    }
  else
    ret->length = c;

  ret->endpos = input->position + ret->length;
  
  /* Value follows */
  return 1;
  }

void bgav_mxf_klv_dump(int indent, mxf_klv_t * ret)
  {
  bgav_diprintf(indent, "Key: ");
  dump_ul_nb(ret->key);
  bgav_dprintf(", Length: %" PRId64 "\n", ret->length);
  }

int bgav_mxf_sync(bgav_input_context_t * input)
  {
  uint8_t data[4];
  while(1)
    {
    if(bgav_input_get_data(input, data, 4) < 4)
      return 0;
    if(UL_MATCH(data, mxf_klv_key))
      break;
    bgav_input_skip(input, 1);
    }
  return 1;
  }

/* Partition */

int bgav_mxf_partition_read(bgav_input_context_t * input,
                            mxf_klv_t * parent,
                            mxf_partition_t * ret)
  {
  if(!bgav_input_read_16_be(input, &ret->major_version) ||
     !bgav_input_read_16_be(input, &ret->minor_version) ||
     !bgav_input_read_32_be(input, &ret->kag_size) ||
     !bgav_input_read_64_be(input, &ret->this_partition) ||
     !bgav_input_read_64_be(input, &ret->previous_partition) ||
     !bgav_input_read_64_be(input, &ret->footer_partition) ||
     !bgav_input_read_64_be(input, &ret->header_byte_count) ||
     !bgav_input_read_64_be(input, &ret->index_byte_count) ||
     !bgav_input_read_32_be(input, &ret->index_sid) ||
     !bgav_input_read_64_be(input, &ret->body_offset) ||
     !bgav_input_read_32_be(input, &ret->body_sid) ||
     (bgav_input_read_data(input, ret->operational_pattern, 16) < 16) 
     )
    return 0;
  
  ret->essence_container_types = read_refs(input, &ret->num_essence_container_types);
  return 1;
  }

void bgav_mxf_partition_dump(int indent, mxf_partition_t * ret)
  {
  int i;
  bgav_diprintf(indent, "Partition\n");
  bgav_diprintf(indent+2, "major_version:       %d\n", ret->major_version);
  bgav_diprintf(indent+2, "minor_version:       %d\n", ret->minor_version);
  bgav_diprintf(indent+2, "kag_size:            %d\n", ret->kag_size);
  bgav_diprintf(indent+2, "this_partition:      %" PRId64 " \n", ret->this_partition);
  bgav_diprintf(indent+2, "previous_partition:  %" PRId64 " \n", ret->previous_partition);
  bgav_diprintf(indent+2, "footer_partition:    %" PRId64 " \n", ret->footer_partition);
  bgav_diprintf(indent+2, "header_byte_count:    %" PRId64 " \n", ret->header_byte_count);
  bgav_diprintf(indent+2, "index_byte_count:     %" PRId64 " \n", ret->index_byte_count);
  bgav_diprintf(indent+2, "index_sid:           %d\n", ret->index_sid);
  bgav_diprintf(indent+2, "body_offset:         %" PRId64 " \n", ret->body_offset);
  bgav_diprintf(indent+2, "body_sid:            %d\n", ret->body_sid);
  bgav_diprintf(indent+2, "operational_pattern: ");
  dump_ul(ret->operational_pattern);
  
  bgav_diprintf(indent+2, "Essence containers: %d\n", ret->num_essence_container_types);
  for(i = 0; i < ret->num_essence_container_types; i++)
    {
    bgav_diprintf(indent+4, "Essence container: ");
    dump_ul(ret->essence_container_types[i]);
    
    }

  
  }

void bgav_mxf_partition_free(mxf_partition_t * ret)
  {
  FREE(ret->essence_container_types);
  }

/* Primer pack */

int bgav_mxf_primer_pack_read(bgav_input_context_t * input,
                              mxf_primer_pack_t * ret)
  {
  int i;
  uint32_t len;
  if(!bgav_input_read_32_be(input, &ret->num_entries) ||
     !bgav_input_read_32_be(input, &len) ||
     (len != 18))
    return 0;
  ret->entries = malloc(ret->num_entries * sizeof(*ret->entries));

  for(i = 0; i < ret->num_entries; i++)
    {
    if(!bgav_input_read_16_be(input, &ret->entries[i].localTag) ||
       (bgav_input_read_data(input, ret->entries[i].uid, 16) < 16))
      return 0;
    }
  return 1;
  }

void bgav_mxf_primer_pack_dump(int indent, mxf_primer_pack_t * ret)
  {
  int i;
  bgav_diprintf(indent, "Primer pack (%d entries)\n", ret->num_entries);
  for(i = 0; i < ret->num_entries; i++)
    {
    bgav_diprintf(indent+2, "LocalTag: %04x, UID: ", ret->entries[i].localTag);
    dump_ul(ret->entries[i].uid);
    
    }
  }

void bgav_mxf_primer_pack_free(mxf_primer_pack_t * ret)
  {
  if(ret->entries) free(ret->entries);
  }

/* Header metadata */

static int read_header_metadata(bgav_input_context_t * input,
                                mxf_file_t * ret, mxf_klv_t * klv,
                                int (*read_func)(bgav_input_context_t * input,
                                                 mxf_file_t * ret, mxf_metadata_t * m,
                                                 int tag, int size, uint8_t * uid),
                                int struct_size, mxf_metadata_type_t type)
  {
  uint16_t tag, len;
  int64_t end_pos;
  mxf_ul_t uid = {0};

  mxf_metadata_t * m;
  if(struct_size)
    {
    m = calloc(1, struct_size);
    m->type = type;
    }
  else
    m = (mxf_metadata_t *)0;
  
  while(input->position < klv->endpos)
    {
    if(!bgav_input_read_16_be(input, &tag) ||
       !bgav_input_read_16_be(input, &len))
      return 0;
    end_pos = input->position + len;

    if(!len) continue;

    if(tag > 0x7FFF)
      {
      int i;
      for(i = 0; i < ret->header.primer_pack.num_entries; i++)
        {
        if(ret->header.primer_pack.entries[i].localTag == tag)
          memcpy(uid, ret->header.primer_pack.entries[i].uid, 16);
        }
      }
    else if((tag == 0x3C0A) && m)
      {
      if(bgav_input_read_data(input, m->uid, 16) < 16)
        return 0;
      //      fprintf(stderr, "Got UID: ");dump_ul(m->uid);fprintf(stderr, "\n");
      //      fprintf(stderr, "Skip UID:\n");
      //      bgav_input_skip_dump(input, 16);
      }
    else if((tag == 0x0102) && m)
      {
      if(bgav_input_read_data(input, m->generation_ul, 16) < 16)
        return 0;
      //      fprintf(stderr, "Got UID: ");dump_ul(m->uid);fprintf(stderr, "\n");
      //      fprintf(stderr, "Skip UID:\n");
      //      bgav_input_skip_dump(input, 16);
      }
    else if(!read_func(input, ret, m, tag, len, uid))
      return 0;

    if(input->position < end_pos)
      bgav_input_skip(input, end_pos - input->position);
    }

  if(m)
    {
    ret->header.metadata =
      realloc(ret->header.metadata,
              (ret->header.num_metadata+1) * sizeof(*ret->header.metadata));
    ret->header.metadata[ret->header.num_metadata] = m;
    ret->header.num_metadata++;
    }
  
  return 1;
  }


/* Header metadata */

/* Content storage */

static int read_content_storage(bgav_input_context_t * input,
                                mxf_file_t * ret, mxf_metadata_t * m,
                                int tag, int size, uint8_t * uid)
 {
 mxf_content_storage_t * p = (mxf_content_storage_t *)m;
 switch(tag)
   {
   case 0x1902:
     if(!(p->essence_container_data_refs =
          read_refs(input, &p->num_essence_container_data_refs)))
       return 0;
     break;
     
   case 0x1901:
     if(!(p->package_refs = read_refs(input, &p->num_package_refs)))
       return 0;
     break;
   default:
#ifdef DUMP_UNKNOWN
     bgav_dprintf("Unknown local tag in content storage: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
   }
 return 1;
 }

void bgav_mxf_content_storage_dump(int indent, mxf_content_storage_t * s)
  {
  int i;
  bgav_diprintf(indent, "Content storage\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(s->common.generation_ul); 

  if(s->num_package_refs)
    {
    bgav_diprintf(indent+2, "Package refs:  %d\n", s->num_package_refs);
    for(i = 0; i < s->num_package_refs; i++)
      {
      do_indent(indent+4); dump_ul_ptr(s->package_refs[i], s->packages[i]);
      }
    }
  if(s->num_essence_container_data_refs)
    {
    bgav_diprintf(indent+2, "Essence container refs: %d\n", s->num_essence_container_data_refs);
    for(i = 0; i < s->num_essence_container_data_refs; i++)
      {
      do_indent(indent+4); dump_ul_ptr(s->essence_container_data_refs[i], s->essence_containers);
      }
    }
  }

int bgav_mxf_content_storage_resolve_refs(mxf_file_t * file, mxf_content_storage_t * s)
  {
  s->packages = resolve_strong_refs(file, s->package_refs, s->num_package_refs,
                                    MXF_TYPE_SOURCE_PACKAGE | MXF_TYPE_MATERIAL_PACKAGE);
  s->essence_containers = resolve_strong_refs(file, s->essence_container_data_refs, s->num_essence_container_data_refs,
                                              MXF_TYPE_ESSENCE_CONTAINER_DATA);
  
  return 1;
  }




void bgav_mxf_content_storage_free(mxf_content_storage_t * s)
  {
  FREE(s->package_refs);
  FREE(s->essence_container_data_refs);
  FREE(s->packages);
  FREE(s->essence_containers);
  }

/* Material package */

static int read_material_package(bgav_input_context_t * input,
                                 mxf_file_t * ret, mxf_metadata_t * m,
                                 int tag, int size, uint8_t * uid)
  {
  mxf_package_t * p = (mxf_package_t *)m;
  
  switch(tag)
    {
    case 0x4401:
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, (uint8_t*)p->package_ul, 16) < 16)
        return 0;
      break;
    case 0x4404:
      if(!bgav_input_read_64_be(input, &p->modification_date))
        return 0;
      break;
    case 0x4405:
      if(!bgav_input_read_64_be(input, &p->creation_date))
        return 0;
      break;
    case 0x4402:
      p->generic_name = read_utf16_string(input, size);
      break;
    case 0x4403:
      if(!(p->track_refs = read_refs(input, &p->num_track_refs)))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     bgav_dprintf("Unknown local tag in material package: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
   }
 return 1;
 }

static int read_source_package(bgav_input_context_t * input,
                               mxf_file_t * ret, mxf_metadata_t * m,
                               int tag, int size, uint8_t * uid)
  {
  mxf_package_t * p = (mxf_package_t *)m;
  
  switch(tag)
    {
    case 0x4404:
      if(!bgav_input_read_64_be(input, &p->modification_date))
        return 0;
      break;
    case 0x4405:
      if(!bgav_input_read_64_be(input, &p->creation_date))
        return 0;
      break;
    case 0x4402:
      p->generic_name = read_utf16_string(input, size);
      break;
    case 0x4403:
      if(!(p->track_refs = read_refs(input, &p->num_track_refs)))
        return 0;
      break;
    case 0x4401:
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, (uint8_t*)p->package_ul, 16) < 16)
        return 0;
      break;
    case 0x4701:
      if(bgav_input_read_data(input, (uint8_t*)p->descriptor_ref, 16) < 16)
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     bgav_dprintf("Unknown local tag in source package: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
      break;
   }
  return 1;
  }


void bgav_mxf_package_dump(int indent, mxf_package_t * p)
  {
  int i;
  if(p->common.type == MXF_TYPE_SOURCE_PACKAGE)
    bgav_diprintf(indent, "Source package:\n");
  else if(p->common.type == MXF_TYPE_MATERIAL_PACKAGE)
    bgav_diprintf(indent, "Material package:\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(p->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(p->common.generation_ul); 

  bgav_diprintf(indent+2, "Package UID:       "); dump_ul(p->package_ul); 
  bgav_diprintf(indent+2, "Creation date:     "); dump_date(p->creation_date); bgav_dprintf("\n");
  bgav_diprintf(indent+2, "Modification date: "); dump_date(p->modification_date); bgav_dprintf("\n");
  bgav_diprintf(indent+2, "%d tracks\n", p->num_track_refs);
  for(i = 0; i < p->num_track_refs; i++)
    {
    bgav_diprintf(indent+4, "Track: "); dump_ul_ptr(p->track_refs[i], p->tracks[i]); 
    }
  bgav_diprintf(indent+2, "Descriptor ref: "); dump_ul_ptr(p->descriptor_ref, p->descriptor);
  bgav_diprintf(indent+2, "Generic Name:   %s\n",
                (p->generic_name ? p->generic_name : "Unknown")); 
  }

void bgav_mxf_package_free(mxf_package_t * p)
  {
  FREE(p->track_refs);
  FREE(p->tracks);
  FREE(p->generic_name);
  }

int bgav_mxf_package_resolve_refs(mxf_file_t * file, mxf_package_t * s)
  {
  s->tracks = resolve_strong_refs(file, s->track_refs, s->num_track_refs, MXF_TYPE_TRACK);
  s->descriptor = resolve_strong_ref(file, s->descriptor_ref,
                                     MXF_TYPE_DESCRIPTOR | MXF_TYPE_MULTIPLE_DESCRIPTOR);
  return 1;
  }


/* source clip */

void bgav_mxf_source_clip_dump(int indent, mxf_source_clip_t * s)
  {
  do_indent(indent);   bgav_dprintf("Source clip:\n");
  bgav_diprintf(indent+2, "UID:                "); dump_ul(s->common.uid);
  bgav_diprintf(indent+2, "Generation UL:      "); dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "source_package_ref: "); dump_ul_ptr(s->source_package_ref, s->source_package);
  bgav_diprintf(indent+2, "data_definition_ul: "); dump_ul(s->data_definition_ul);
  bgav_diprintf(indent+2, "duration:           %" PRId64 "\n", s->duration);
  bgav_diprintf(indent+2, "start_pos:          %" PRId64 "\n", s->start_position);
  bgav_diprintf(indent+2, "source_track_id:    %d\n", s->source_track_id);
  
  }

void bgav_mxf_source_clip_free(mxf_source_clip_t * s)
  {

  }

static int read_source_clip(bgav_input_context_t * input,
                            mxf_file_t * ret, mxf_metadata_t * m,
                            int tag, int size, uint8_t * uid)
  {
  mxf_source_clip_t * s = (mxf_source_clip_t *)m;
  switch(tag)
    {
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x0202:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->duration))
        return 0;
      break;
    case 0x1201:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->start_position))
        return 0;
      break;
    case 0x1101:
      /* UMID, only get last 16 bytes */
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, s->source_package_ref, 16) < 16)
        return 0;
      break;
    case 0x1102:
      if(!bgav_input_read_32_be(input, &s->source_track_id))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     bgav_dprintf("Unknown local tag in source clip: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
    }
  return 1;
  }

int bgav_mxf_source_clip_resolve_refs(mxf_file_t * file, mxf_source_clip_t * s)
  {
  s->source_package = package_by_ul(file, s->source_package_ref);
  return 1;
  }


/* Timecode component */

static int read_timecode_component(bgav_input_context_t * input,
                                   mxf_file_t * ret, mxf_metadata_t * m,
                                   int tag, int size, uint8_t * uid)
  {
  mxf_timecode_component_t * s = (mxf_timecode_component_t *)m;
  switch(tag)
    {
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x0202:
      if(!bgav_input_read_64_be(input, &s->duration))
        return 0;
      break;
    case 0x1502:
      if(!bgav_input_read_16_be(input, &s->rounded_timecode_base))
        return 0;
      break;
    case 0x1501:
      if(!bgav_input_read_64_be(input, &s->start_timecode))
        return 0;
      break;
    case 0x1503:
      if(!bgav_input_read_8(input, &s->drop_frame))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
     bgav_dprintf("Unknown local tag in timecode component: %04x, %d bytes\n", tag, size);
     if(size)
       bgav_input_skip_dump(input, size);
#endif
     break;
    }
  return 1;
  }

void bgav_mxf_timecode_component_dump(int indent, mxf_timecode_component_t * s)
  {
  do_indent(indent);   bgav_dprintf("Timecode component\n");
  bgav_diprintf(indent+2, "UID:                   "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:         "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "data_definition_ul:    "); dump_ul(s->data_definition_ul); 
  bgav_diprintf(indent+2, "duration:              %"PRId64"\n", s->duration);
  bgav_diprintf(indent+2, "rounded_timecode_base: %d\n",
                                    s->rounded_timecode_base);
  bgav_diprintf(indent+2, "start_timecode:        %"PRId64"\n", s->start_timecode);
  bgav_diprintf(indent+2, "drop_frame:            %d\n", s->drop_frame);
  }

int bgav_mxf_timecode_component_resolve_refs(mxf_file_t * file, mxf_timecode_component_t * s)
  {
  return 1;
  }

void bgav_mxf_timecode_component_free(mxf_timecode_component_t * s)
  {
  
  }

/* track */

void bgav_mxf_track_dump(int indent, mxf_track_t * t)
  {
  bgav_diprintf(indent, "Track\n");
  bgav_diprintf(indent+2, "UID:           "); dump_ul(t->common.uid); 
  bgav_diprintf(indent+2, "Generation UL: "); dump_ul(t->common.generation_ul); 

  bgav_diprintf(indent+2, "Track ID:      %d\n", t->track_id);
  bgav_diprintf(indent+2, "Name:          %s\n", (t->name ? t->name : "Unknown"));
  bgav_diprintf(indent+2, "Track number:  [%02x %02x %02x %02x]\n",
                                    t->track_number[0], t->track_number[1],
                                    t->track_number[2], t->track_number[3]);
  bgav_diprintf(indent+2, "Edit rate:     %d/%d\n", t->edit_rate_num, t->edit_rate_den);
  bgav_diprintf(indent+2, "Sequence ref:  ");dump_ul_ptr(t->sequence_ref, t->sequence);
  bgav_diprintf(indent+2, "num_packets:   %d\n", t->num_packets);
  bgav_diprintf(indent+2, "max_size:      %"PRId64"\n", t->max_packet_size);
  }

void bgav_mxf_track_free(mxf_track_t * t)
  {
  FREE(t->name);
  }

int bgav_mxf_track_resolve_refs(mxf_file_t * file, mxf_track_t * s)
  {
  s->sequence = resolve_strong_ref(file, s->sequence_ref, MXF_TYPE_SEQUENCE);
  return 1;
  }

static int read_track(bgav_input_context_t * input,
                      mxf_file_t * ret, mxf_metadata_t * m,
                      int tag, int size, uint8_t * uid)
  {
  mxf_track_t * t = (mxf_track_t *)m;

  switch(tag)
    {
    case 0x4801:
      if(!bgav_input_read_32_be(input, &t->track_id))
        return 0;
      break;
    case 0x4802:
      t->name = read_utf16_string(input, size);
      break;
    case 0x4804:
      if(bgav_input_read_data(input, t->track_number, 4) < 4)
        return 0;
      break;
    case 0x4B01:
      if(!bgav_input_read_32_be(input, &t->edit_rate_den) ||
         !bgav_input_read_32_be(input, &t->edit_rate_num))
        return 0;
      break;
    case 0x4803:
      if(bgav_input_read_data(input, t->sequence_ref, 16) < 16)
        return 0;
      break;
    case 0x4b02:
      if(!bgav_input_read_64_be(input, &t->origin))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      bgav_dprintf("Unknown local tag in track: %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1; 
  }


/* Sequence */

void bgav_mxf_sequence_dump(int indent, mxf_sequence_t * s)
  {
  int i;
  bgav_diprintf(indent, "Sequence\n");
  bgav_diprintf(indent+2, "UID:                "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:      "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "data_definition_ul: ");dump_ul(s->data_definition_ul);
  bgav_diprintf(indent+2, "Structural components (%d):\n", s->num_structural_component_refs);
  for(i = 0; i < s->num_structural_component_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(s->structural_component_refs[i], s->structural_components[i]);
    }
  bgav_diprintf(indent+2, "Type: %s\n",
                                    (s->stream_type == BGAV_STREAM_AUDIO ? "Audio" :
                                     (s->stream_type == BGAV_STREAM_VIDEO ? "Video" :
                                      (s->is_timecode ? "Timecode" : "Unknown"))));
  }

void bgav_mxf_sequence_free(mxf_sequence_t * s)
  {
  if(s->structural_component_refs)
    free(s->structural_component_refs);
  if(s->structural_components)
    free(s->structural_components);
  }

int bgav_mxf_sequence_resolve_refs(mxf_file_t * file, mxf_sequence_t * s)
  {
  const stream_entry_t * se;
  
  s->structural_components = resolve_strong_refs(file, s->structural_component_refs,
                                                 s->num_structural_component_refs,
                                                 MXF_TYPE_SOURCE_CLIP | MXF_TYPE_TIMECODE_COMPONENT);
  
  se = match_stream(mxf_data_definition_uls, s->data_definition_ul);
  if(se)
    s->stream_type = se->type;
  else if(s->structural_components && (s->structural_components[0]->type == MXF_TYPE_TIMECODE_COMPONENT))
    s->is_timecode = 1;
  return 1;
  }


static int read_sequence(bgav_input_context_t * input,
                         mxf_file_t * ret, mxf_metadata_t * m,
                         int tag, int size, uint8_t * uid)
  {
  mxf_sequence_t * s = (mxf_sequence_t *)m;

  switch(tag)
    {
    case 0x0202:
      if(!bgav_input_read_64_be(input, (uint64_t*)&s->duration))
        return 0;
      break;
    case 0x0201:
      if(bgav_input_read_data(input, s->data_definition_ul, 16) < 16)
        return 0;
      break;
    case 0x1001:
      if(!(s->structural_component_refs = read_refs(input, &s->num_structural_component_refs)))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      bgav_dprintf("Unknown local tag in sequence : %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

/* Identification */

static int read_identification(bgav_input_context_t * input,
                               mxf_file_t * ret, mxf_metadata_t * m,
                               int tag, int size, uint8_t * uid)
  {
  mxf_identification_t * s = (mxf_identification_t *)m;

  switch(tag)
    {
    case 0x3c01:
      s->company_name = read_utf16_string(input, size);
      break;
    case 0x3c02:
      s->product_name = read_utf16_string(input, size);
      break;
    case 0x3c04:
      s->version_string = read_utf16_string(input, size);
      break;
    case 0x3c09:
      if(bgav_input_read_data(input, s->this_generation_ul, 16) < 16)
        return 0;
      break;
    case 0x3c05:
      if(bgav_input_read_data(input, s->product_ul, 16) < 16)
        return 0;
      break;
    case 0x3c06:
      if(!bgav_input_read_64_be(input, &s->modification_date))
        return 0;
      break;
    default:
#ifdef DUMP_UNKNOWN
      bgav_dprintf("Unknown local tag in identification : %04x, %d bytes\n", tag, size);
      if(size)
        bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

void bgav_mxf_identification_dump(int indent, mxf_identification_t * s)
  {
  bgav_diprintf(indent, "Identification\n");

  bgav_diprintf(indent+2, "UID:               "); dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:     "); dump_ul(s->common.generation_ul); 

  bgav_diprintf(indent+2, "thisGenerationUID: "); dump_ul(s->this_generation_ul);
  bgav_diprintf(indent+2, "Company name:      %s\n",
                                    (s->company_name ? s->company_name :
                                     "(unknown"));
  bgav_diprintf(indent+2, "Product name:      %s\n",
                                    (s->product_name ? s->product_name :
                                     "(unknown"));
  bgav_diprintf(indent+2, "Version string:    %s\n",
                                    (s->version_string ? s->version_string :
                                     "(unknown"));

  bgav_diprintf(indent+2, "ProductUID:        "); dump_ul(s->product_ul);
  
  bgav_diprintf(indent+2, "ModificationDate:  "); dump_date(s->modification_date);bgav_dprintf("\n");
  
  }

void bgav_mxf_identification_free(mxf_identification_t * s)
  {
  FREE(s->company_name);
  FREE(s->product_name);
  FREE(s->version_string);
  }

int bgav_mxf_identification_resolve_refs(mxf_file_t * file, mxf_identification_t * s)
  {
  return 1;
  }

/* Descriptor */

void bgav_mxf_descriptor_dump(int indent, mxf_descriptor_t * d)
  {
  int i;
  do_indent(indent);   bgav_dprintf("Descriptor\n");
  bgav_diprintf(indent+2, "UID:                    ");dump_ul(d->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:          ");dump_ul(d->common.generation_ul); 
  bgav_diprintf(indent+2, "essence_container_ul:   ");dump_ul(d->essence_container_ul);
  bgav_diprintf(indent+2, "clip_wrapped:           %d\n", d->clip_wrapped);
  bgav_diprintf(indent+2, "essence_codec_ul:       ");dump_ul(d->essence_codec_ul);
  bgav_diprintf(indent+2, "Sample rate:            %d/%d\n", d->sample_rate_num, d->sample_rate_den); 
  bgav_diprintf(indent+2, "Aspect ratio:           %d/%d\n", d->aspect_ratio_num, d->aspect_ratio_den);
  bgav_diprintf(indent+2, "Image size:             %dx%d\n", d->width, d->height);
  bgav_diprintf(indent+2, "Bits per sample:        %d\n", d->bits_per_sample);
  bgav_diprintf(indent+2, "Channels:               %d\n", d->channels);
  bgav_diprintf(indent+2, "Locked:                 %d\n", d->locked);
  bgav_diprintf(indent+2, "Frame layout:           %d\n", d->frame_layout);
  bgav_diprintf(indent+2, "Field dominance:        %d\n", d->field_dominance);
  bgav_diprintf(indent+2, "Active bits per sample: %d\n", d->active_bits_per_sample);
  bgav_diprintf(indent+2, "Horizontal subsampling: %d\n", d->horizontal_subsampling);
  bgav_diprintf(indent+2, "Vertical subsampling:   %d\n", d->vertical_subsampling);
  bgav_diprintf(indent+2, "linked track ID:        %d\n", d->linked_track_id);
  bgav_diprintf(indent+2, "Container duration:     %"PRId64"\n", d->container_duration);
  bgav_diprintf(indent+2, "Block align:            %d\n", d->block_align);
  bgav_diprintf(indent+2, "Avg BPS:                %d\n", d->avg_bps);
  
  bgav_diprintf(indent+2, "Subdescriptor refs:     %d\n", d->num_subdescriptor_refs);

  for(i = 0; i < d->num_subdescriptor_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(d->subdescriptor_refs[i], d->subdescriptors[i]);
    }

  bgav_diprintf(indent+2, "Video line map:         %d entries\n", d->video_line_map_size);
  
  for(i = 0; i < d->video_line_map_size; i++)
    {
    bgav_diprintf(indent+4, "Entry: %d\n", d->video_line_map[i]);
    }

  }

int bgav_mxf_descriptor_resolve_refs(mxf_file_t * file, mxf_descriptor_t * d)
  {
  d->subdescriptors = resolve_strong_refs(file, d->subdescriptor_refs, d->num_subdescriptor_refs,
                                          MXF_TYPE_DESCRIPTOR);
  if(d->essence_container_ul[15] > 0x01)
    d->clip_wrapped = 1;
  return 1;
  }


static int read_pixel_layout(bgav_input_context_t * input, mxf_descriptor_t * d)
  {
  uint8_t code;
  uint8_t w;
  
  do
    {
    if(!bgav_input_read_8(input, &code))
      return 0;
    
    switch (code)
      {
      case 0x52: /* R */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      case 0x47: /* G */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      case 0x42: /* B */
        if(!bgav_input_read_8(input, &w))
          return 0;
        d->bits_per_sample += w;
        break;
      default:
        bgav_input_skip(input, 1);
        break;
      }
    } while (code != 0); /* SMPTE 377M E.2.46 */
  return 1;
  }

static int read_descriptor(bgav_input_context_t * input,
                           mxf_file_t * ret, mxf_metadata_t * m,
                           int tag, int size, uint8_t * uid)
  {
  int i;
  mxf_descriptor_t * d = (mxf_descriptor_t *)m;
  
  switch(tag)
    {
    case 0x3F01:
      if(!(d->subdescriptor_refs = read_refs(input, &d->num_subdescriptor_refs)))
        return 0;
      break;
    case 0x3002:
      if(!bgav_input_read_64_be(input, &d->container_duration))
        return 0;
      break;
    case 0x3004:
      if(bgav_input_read_data(input, d->essence_container_ul, 16) < 16)
        return 0;
      break;
    case 0x3006:
      if(!bgav_input_read_32_be(input, &d->linked_track_id))
        return 0;
      break;
    case 0x3201: /* PictureEssenceCoding */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3203:
      if(!bgav_input_read_32_be(input, &d->width))
        return 0;
      break;
    case 0x3202:
      if(!bgav_input_read_32_be(input, &d->height))
        return 0;
      break;
    case 0x3301:
      if(!bgav_input_read_32_be(input, &d->active_bits_per_sample))
        return 0;
      break;
    case 0x3302:
      if(!bgav_input_read_32_be(input, &d->horizontal_subsampling))
        return 0;
      break;
    case 0x3308:
      if(!bgav_input_read_32_be(input, &d->vertical_subsampling))
        return 0;
      break;
    case 0x320E:
      if(!bgav_input_read_32_be(input, &d->aspect_ratio_num) ||
         !bgav_input_read_32_be(input, &d->aspect_ratio_den))
        return 0;
      break;
    case 0x3D02:
      if(!bgav_input_read_8(input, &d->locked))
        return 0;
      break;
    case 0x3D0A:
      if(!bgav_input_read_16_be(input, &d->block_align))
        return 0;
      break;
    case 0x3D09:
      if(!bgav_input_read_32_be(input, &d->avg_bps))
        return 0;
      break;
    case 0x3001:
    case 0x3D03:
      if(!bgav_input_read_32_be(input, &d->sample_rate_num) ||
         !bgav_input_read_32_be(input, &d->sample_rate_den))
        return 0;
      break;
    case 0x3D06: /* SoundEssenceCompression */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3D07:
      if(!bgav_input_read_32_be(input, &d->channels))
        return 0;
      break;
    case 0x3D01:
      if(!bgav_input_read_32_be(input, &d->bits_per_sample))
        return 0;
      break;
    case 0x3401:
      if(!read_pixel_layout(input, d))
        return 0;
      break;
    case 0x320c:
      if(!bgav_input_read_8(input, &d->frame_layout))
        return 0;
      break;
    case 0x3212:
      if(!bgav_input_read_8(input, &d->field_dominance))
        return 0;
      break;
    case 0x320d:
      if(!bgav_input_read_32_be(input, &d->video_line_map_size))
        return 0;
      bgav_input_skip(input, 4);
      d->video_line_map = malloc(d->video_line_map_size * sizeof(*d->video_line_map));
      for(i = 0; i < d->video_line_map_size; i++)
        {
        if(!bgav_input_read_32_be(input, &d->video_line_map[i]))
          return 0;
        }
      break;
    default:
      /* Private uid used by SONY C0023S01.mxf */
      if(UL_MATCH(uid, mxf_sony_mpeg4_extradata))
        {
        d->ext_data = malloc(size);
        d->ext_size = size;
        if(bgav_input_read_data(input, d->ext_data, size) < size)
          return 0;
        }
#ifdef DUMP_UNKNOWN
      else
        {
        bgav_dprintf("Unknown local tag in descriptor : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
        }
#endif
      break;
    }
  return 1;
  }

void bgav_mxf_descriptor_free(mxf_descriptor_t * d)
  {
  FREE(d->subdescriptor_refs);
  FREE(d->subdescriptors);
  FREE(d->ext_data);
  FREE(d->video_line_map);
  }

/*
 *  Essence Container Data
 */

void bgav_mxf_essence_container_data_dump(int indent, mxf_essence_container_data_t * s)
  {
  bgav_diprintf(indent, "Essence Container Data:\n");
  bgav_diprintf(indent+2, "UID:                  ");dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:        "); dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "Linked Package:       ");dump_ul_ptr(s->linked_package_ref, s->linked_package); 
  bgav_diprintf(indent+2, "IndexSID:             %d\n", s->index_sid);
  bgav_diprintf(indent+2, "BodySID:              %d\n", s->body_sid);
  }

int bgav_mxf_essence_container_data_resolve_refs(mxf_file_t * file, mxf_essence_container_data_t * d)
  {
  d->linked_package = package_by_ul(file, d->linked_package_ref);
  return 1;
  }

void bgav_mxf_essence_container_data_free(mxf_essence_container_data_t * s)
  {
  
  }


static int read_essence_container_data(bgav_input_context_t * input,
                           mxf_file_t * ret, mxf_metadata_t * m,
                           int tag, int size, uint8_t * uid)
  {
  mxf_essence_container_data_t * d = (mxf_essence_container_data_t *)m;

  switch(tag)
    {
    case 0x2701:
      /* UMID, only get last 16 bytes */
      bgav_input_skip(input, 16);
      if(bgav_input_read_data(input, d->linked_package_ref, 16) < 16)
        return 0;
      break;
    case 0x3f06:
      if(!bgav_input_read_32_be(input, &d->index_sid))
        return 0;
      break;
    case 0x3f07:
      if(!bgav_input_read_32_be(input, &d->body_sid))
        return 0;
      break;
    default:
      /* Private uid used by SONY C0023S01.mxf */
#ifdef DUMP_UNKNOWN
        bgav_dprintf("Unknown local tag in essence container data : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }

/* Preface */

void bgav_mxf_preface_dump(int indent, mxf_preface_t * s)
  {
  int i;
  do_indent(indent);   bgav_dprintf("Preface:\n");
  bgav_diprintf(indent+2, "UID:                  ");dump_ul(s->common.uid); 
  bgav_diprintf(indent+2, "Generation UL:        ");dump_ul(s->common.generation_ul); 
  bgav_diprintf(indent+2, "Last modified date:    ");dump_date(s->last_modified_date); bgav_dprintf("\n");
  bgav_diprintf(indent+2, "Version:               %d\n", s->version);
  bgav_diprintf(indent+2, "Identifications:       %d\n", s->num_identification_refs);
  for(i = 0; i < s->num_identification_refs; i++)
    {
    do_indent(indent+4); dump_ul_ptr(s->identification_refs[i], s->identifications[i]);
    }
  bgav_diprintf(indent+2, "Content storage:       ");dump_ul_ptr(s->content_storage_ref, s->content_storage); 
  bgav_diprintf(indent+2, "Operational pattern:   ");dump_ul_nb(s->operational_pattern_ul);bgav_dprintf(" %s\n", get_op_name(s->operational_pattern));
  bgav_diprintf(indent+2, "Essence containers:    %d\n", s->num_essence_container_types);
  for(i = 0; i < s->num_essence_container_types; i++)
    {
    bgav_diprintf(indent+4, "Essence containers: "); dump_ul(s->essence_container_types[i]);
    }
  bgav_diprintf(indent+2, "Primary package:      ");dump_ul(s->primary_package_ul);
  
  }

int bgav_mxf_preface_resolve_refs(mxf_file_t * file, mxf_preface_t * d)
  {
  d->identifications = resolve_strong_refs(file, d->identification_refs, d->num_identification_refs,
                                           MXF_TYPE_IDENTIFICATION);
  d->content_storage = resolve_strong_ref(file, d->content_storage_ref, MXF_TYPE_CONTENT_STORAGE);

  if(is_op_1a(d->operational_pattern_ul))
    d->operational_pattern = MXF_OP_1a;
  else if(is_op_atom(d->operational_pattern_ul))
    d->operational_pattern = MXF_OP_ATOM;
  return 1;
  }

void bgav_mxf_preface_free(mxf_preface_t * d)
  {
  FREE(d->identification_refs);
  FREE(d->essence_container_types);
  
  FREE(d->identifications);
  
  FREE(d->dm_schemes);
  }

static int read_preface(bgav_input_context_t * input,
                        mxf_file_t * ret, mxf_metadata_t * m,
                        int tag, int size, uint8_t * uid)
  {
  mxf_preface_t * d = (mxf_preface_t *)m;

  switch(tag)
    {
    case 0x3b02:
      if(!bgav_input_read_64_be(input, &d->last_modified_date))
        return 0;
      break;
    case 0x3b05:
      if(!bgav_input_read_16_be(input, &d->version))
        return 0;
      break;
    case 0x3b06:
      if(!(d->identification_refs = read_refs(input, &d->num_identification_refs)))
        return 0;
      break;
    case 0x3b03:
      if(bgav_input_read_data(input, d->content_storage_ref, 16) < 16)
        return 0;
      break;
    case 0x3b08:
      if(bgav_input_read_data(input, d->primary_package_ul, 16) < 16)
        return 0;
      break;
    case 0x3b09:
      if(bgav_input_read_data(input, d->operational_pattern_ul, 16) < 16)
        return 0;
      break;
    case 0x3b0a:
      if(!(d->essence_container_types = read_refs(input, &d->num_essence_container_types)))
        return 0;
      break;
    case 0x3b0b:
      if(!(d->dm_schemes = read_refs(input, &d->num_dm_schemes)))
        return 0;
      break;
  
#if 0
    case 0x3F01:
      if(!(d->subdescriptors_refs = read_refs(input, &d->num_subdescriptors)))
        return 0;
      break;
    case 0x3004:
      if(bgav_input_read_data(input, d->essence_container_ul, 16) < 16)
        return 0;
      break;
    case 0x3201: /* PictureEssenceCoding */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3203:
      if(!bgav_input_read_32_be(input, &d->width))
        return 0;
      break;
    case 0x3202:
      if(!bgav_input_read_32_be(input, &d->height))
        return 0;
      break;
    case 0x320E:
      if(!bgav_input_read_32_be(input, &d->aspect_ratio_num) ||
         !bgav_input_read_32_be(input, &d->aspect_ratio_den))
        return 0;
      break;
    case 0x3D03:
      if(!bgav_input_read_32_be(input, &d->sample_rate_num) ||
         !bgav_input_read_32_be(input, &d->sample_rate_den))
        return 0;
      break;
    case 0x3D06: /* SoundEssenceCompression */
      if(bgav_input_read_data(input, d->essence_codec_ul, 16) < 16)
        return 0;
      break;
    case 0x3D07:
      if(!bgav_input_read_32_be(input, &d->channels))
        return 0;
      break;
    case 0x3D01:
      if(!bgav_input_read_32_be(input, &d->bits_per_sample))
        return 0;
      break;
    case 0x3401:
      if(!read_pixel_layout(input, d))
        return 0;
      break;
#endif
    default:
#ifdef DUMP_UNKNOWN
        bgav_dprintf("Unknown local tag in preface : %04x, %d bytes\n", tag, size);
        if(size)
          bgav_input_skip_dump(input, size);
#endif
      break;
    }
  return 1;
  }




/* Index table segment */

static int read_index_table_segment(bgav_input_context_t * input,
                                    mxf_file_t * ret, mxf_klv_t * klv)
  {
  uint16_t tag, len;
  int64_t end_pos;
  mxf_ul_t uid = {0};
  int i;
  
  mxf_index_table_segment_t * idx;

  idx = calloc(1, sizeof(*idx));
  
  while(input->position < klv->endpos)
    {
    if(!bgav_input_read_16_be(input, &tag) ||
       !bgav_input_read_16_be(input, &len))
      return 0;
    end_pos = input->position + len;
    
    if(!len) continue;
    
    if(tag > 0x7FFF)
      {
      int i;
      for(i = 0; i < ret->header.primer_pack.num_entries; i++)
        {
        if(ret->header.primer_pack.entries[i].localTag == tag)
          memcpy(uid, ret->header.primer_pack.entries[i].uid, 16);
        }
      }
    else if(tag == 0x3C0A)
      {
      if(bgav_input_read_data(input, idx->uid, 16) < 16)
        return 0;
      //      fprintf(stderr, "Got UID: ");dump_ul(m->uid);fprintf(stderr, "\n");
      //      fprintf(stderr, "Skip UID:\n");
      //      bgav_input_skip_dump(input, 16);
      }
    else if(tag == 0x3f0b) // IndexEditRate
      {
      if(!bgav_input_read_32_be(input, &idx->edit_rate_den) ||
         !bgav_input_read_32_be(input, &idx->edit_rate_num))
        return 0;
      }
    else if(tag == 0x3f0c) // IndexStartPosition
      {
      if(!bgav_input_read_64_be(input, &idx->start_position))
        return 0;
      }
    else if(tag == 0x3f0d) // IndexDuration
      {
      if(!bgav_input_read_64_be(input, &idx->duration))
        return 0;
      }
    else if(tag == 0x3f05) // EditUnitByteCount
      {
      if(!bgav_input_read_32_be(input, &idx->edit_unit_byte_count))
        return 0;
      }
    else if(tag == 0x3f06) // IndexSID
      {
      if(!bgav_input_read_32_be(input, &idx->index_sid))
        return 0;
      }
    else if(tag == 0x3f07) // BodySID
      {
      if(!bgav_input_read_32_be(input, &idx->body_sid))
        return 0;
      }
    else if(tag == 0x3f08) // SliceCount
      {
      if(!bgav_input_read_8(input, &idx->slice_count))
        return 0;
      }
    else if(tag == 0x3f09) // DeltaEntryArray
      {
      if(!bgav_input_read_32_be(input, &idx->num_delta_entries))
        return 0;
      bgav_input_skip(input, 4);

      idx->delta_entries =
        malloc(idx->num_delta_entries * sizeof(idx->delta_entries));
      for(i = 0; i < idx->num_delta_entries; i++)
        {
        if(!bgav_input_read_8(input, &idx->delta_entries[i].pos_table_index) ||
           !bgav_input_read_8(input, &idx->delta_entries[i].slice) ||
           !bgav_input_read_32_be(input, &idx->delta_entries[i].element_delta))
          return 0;
        }
      }
    else
      {
      fprintf(stderr, "Skipping unknown tag %04x (len %d) in index\n",
              tag, len);
      bgav_input_skip_dump(input, len);
      }
    if(input->position < end_pos)
      bgav_input_skip(input, end_pos - input->position);
    }

  ret->index_segments =
    realloc(ret->index_segments,
            (ret->num_index_segments+1) * sizeof(*ret->index_segments));
  ret->index_segments[ret->num_index_segments] = idx;
  ret->num_index_segments ++;
  
  return 1;
  }

void bgav_mxf_index_table_segment_dump(int indent, mxf_index_table_segment_t * idx)
  {
  int i;
  bgav_diprintf(indent, "Index table segment:\n");
  bgav_diprintf(indent+2, "UID: "); dump_ul(idx->uid), 
  bgav_diprintf(indent+2, "edit_rate: %d/%d",
                                    idx->edit_rate_num, idx->edit_rate_den);
  bgav_diprintf(indent+2, "start_position: %"PRId64"\n",
                                    idx->start_position);
  bgav_diprintf(indent+2, "duration: %"PRId64"\n",
                                    idx->duration);
  bgav_diprintf(indent+2, "edit_unit_byte_count: %d\n",
                                    idx->edit_unit_byte_count);
  bgav_diprintf(indent+2, "index_sid: %d\n",
                                    idx->index_sid);
  bgav_diprintf(indent+2, "body_sid: %d\n",
                                    idx->body_sid);
  bgav_diprintf(indent+2, "slice_count: %d\n",
                                    idx->slice_count);

  if(idx->num_delta_entries)
    {
    bgav_diprintf(indent+2, "Delta entries: %d\n", idx->num_delta_entries);
    for(i = 0; i < idx->num_delta_entries; i++)
      {
      bgav_diprintf(indent+4, "I: %d, S: %d, delta: %d\n",
                                        idx->delta_entries[i].pos_table_index,
                                        idx->delta_entries[i].slice,
                                        idx->delta_entries[i].element_delta);
      }
    }
  }

void bgav_mxf_index_table_segment_free(mxf_index_table_segment_t * idx)
  {
  if(idx->delta_entries) free(idx->delta_entries);
  
  }


/* File */

#if 0
static mxf_package_t * get_source_package(mxf_file_t * file, mxf_source_clip_t * c)
  {
  int i;
  mxf_package_t * ret;
  for(i = 0; i < file->preface->content_storage->num_package_refs; i++)
    {
    ret = resolve_strong_ref(file, file->content_storage->package_refs[i], MXF_TYPE_SOURCE_PACKAGE);
    if(ret && !memcmp(ret->package_ul, c->source_package_ref, 16))
      {
      return ret;
      break;
      }
    }
  return (mxf_package_t *)0;
  }

static mxf_track_t * get_source_track(mxf_file_t * file, mxf_package_t * p,
                                      mxf_source_clip_t * c)
  {
  int i;
  mxf_track_t * ret;
  for(i = 0; i < p->num_track_refs; i++)
    {
    if(!(ret = resolve_strong_ref(file, p->track_refs[i], MXF_TYPE_TRACK)))
      {
      return (mxf_track_t *)0;
      }
    else if(ret->track_id == c->source_track_id)
      {
      return ret;
      break;
      }
    } 
  return (mxf_track_t*)0;
  }

static mxf_descriptor_t * get_source_descriptor(mxf_file_t * file, mxf_package_t * p, mxf_source_clip_t * c)
  {
  int i;
  mxf_descriptor_t * desc;
  mxf_descriptor_t * sub_desc;

  desc = resolve_strong_ref(file, p->descriptor_ref, MXF_TYPE_DESCRIPTOR | MXF_TYPE_MULTIPLE_DESCRIPTOR);
  if(!desc)
    return desc;
  else if(desc->common.type == MXF_TYPE_DESCRIPTOR)
    return desc;
  else if(desc->common.type == MXF_TYPE_MULTIPLE_DESCRIPTOR)
    {
    for(i = 0; i < desc->num_subdescriptor_refs; i++)
      {
      if(!(sub_desc = resolve_strong_ref(file, desc->subdescriptor_refs[i], MXF_TYPE_DESCRIPTOR)))
        {
        return (mxf_descriptor_t *)0;
        }
      else if(sub_desc->linked_track_id == c->source_track_id)
        {
        return sub_desc;
        break;
        }
      } 
    }
  return (mxf_descriptor_t*)0;
  }

#endif

static int resolve_refs(mxf_file_t * ret, const bgav_options_t * opt)
  {
  int i;

  /* First round */

  for(i = 0; i < ret->header.num_metadata; i++)
    {
    switch(ret->header.metadata[i]->type)
      {
      case MXF_TYPE_MATERIAL_PACKAGE:
        if(!bgav_mxf_package_resolve_refs(ret, (mxf_package_t*)(ret->header.metadata[i])))
          return 0;
        ret->num_source_packages++;
        break;
      case MXF_TYPE_SOURCE_PACKAGE:
        if(!bgav_mxf_package_resolve_refs(ret, (mxf_package_t*)(ret->header.metadata[i])))
          return 0;
        ret->num_material_packages++;
        break;
      case MXF_TYPE_TIMECODE_COMPONENT:
        if(!bgav_mxf_timecode_component_resolve_refs(ret, (mxf_timecode_component_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_PREFACE:
        if(!bgav_mxf_preface_resolve_refs(ret, (mxf_preface_t*)(ret->header.metadata[i])))
          return 0;
        ret->preface = ret->header.metadata[i];
        break;
      case MXF_TYPE_CONTENT_STORAGE:
        if(!bgav_mxf_content_storage_resolve_refs(ret, (mxf_content_storage_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_SEQUENCE:
        {
        mxf_sequence_t * s;
        s = (mxf_sequence_t*)ret->header.metadata[i];
        if(!bgav_mxf_sequence_resolve_refs(ret, s))
          return 0;
        if(s->num_structural_component_refs > ret->max_sequence_components)
          ret->max_sequence_components = s->num_structural_component_refs;
        }
        break;
      case MXF_TYPE_MULTIPLE_DESCRIPTOR:
        if(!bgav_mxf_descriptor_resolve_refs(ret, (mxf_descriptor_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_DESCRIPTOR:
        ret->num_descriptors++;
        if(!bgav_mxf_descriptor_resolve_refs(ret, (mxf_descriptor_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_TRACK:
        if(!bgav_mxf_track_resolve_refs(ret, (mxf_track_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_IDENTIFICATION:
        if(!bgav_mxf_identification_resolve_refs(ret, (mxf_identification_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_CRYPTO_CONTEXT:
      case MXF_TYPE_SOURCE_CLIP:
      case MXF_TYPE_ESSENCE_CONTAINER_DATA:
        break;
      }
    }
  
  /* Second round */
  for(i = 0; i < ret->header.num_metadata; i++)
    {
    switch(ret->header.metadata[i]->type)
      {
      case MXF_TYPE_SOURCE_CLIP:
        if(!bgav_mxf_source_clip_resolve_refs(ret, (mxf_source_clip_t*)(ret->header.metadata[i])))
          return 0;
        break;
      case MXF_TYPE_ESSENCE_CONTAINER_DATA:
        if(!bgav_mxf_essence_container_data_resolve_refs(ret, (mxf_essence_container_data_t*)(ret->header.metadata[i])))
          return 0;
        break;

      case MXF_TYPE_MATERIAL_PACKAGE:
      case MXF_TYPE_SOURCE_PACKAGE:
      case MXF_TYPE_TIMECODE_COMPONENT:
      case MXF_TYPE_PREFACE:
      case MXF_TYPE_CONTENT_STORAGE:
      case MXF_TYPE_SEQUENCE:
      case MXF_TYPE_MULTIPLE_DESCRIPTOR:
      case MXF_TYPE_DESCRIPTOR:
      case MXF_TYPE_TRACK:
      case MXF_TYPE_IDENTIFICATION:
      case MXF_TYPE_CRYPTO_CONTEXT:
        break;
      }
    }
  
  return 1;
  }

uint32_t bgav_mxf_get_audio_fourcc(mxf_descriptor_t * d)
  {
  const codec_entry_t * ce;

  ce = match_codec(mxf_audio_codec_uls, d->essence_codec_ul);
  if(ce)
    return ce->fourcc;

  ce = match_codec(mxf_sound_essence_container_uls, d->essence_container_ul);
  
  if(ce)
    return ce->fourcc;
  
  return 0;
  }

uint32_t bgav_mxf_get_video_fourcc(mxf_descriptor_t * d)
  {
  const codec_entry_t * ce;

  ce = match_codec(mxf_video_codec_uls, d->essence_codec_ul);
  if(ce)
    return ce->fourcc;

  ce = match_codec(mxf_picture_essence_container_uls, d->essence_container_ul);
  
  if(ce)
    return ce->fourcc;
  
  return 0;

  }

static int bgav_mxf_finalize(mxf_file_t * ret, const bgav_options_t * opt)
  {

  if(!resolve_refs(ret, opt))
    return 0;

  if(!ret->preface || !((mxf_preface_t*)(ret->preface))->content_storage)
    return 0;

  
#if 0
  /*
   *  Loop over all Material (= output) packages. Each material package will
   *  become a bgav_track_t
   */
  
  for(i = 0; i < ret->content_storage->num_package_refs; i++)
    {
    mp = resolve_strong_ref(ret, ret->content_storage->package_refs[i], MXF_TYPE_MATERIAL_PACKAGE);
    if(!mp)
      continue;
    
    APPEND_ARRAY(ret->material_packages, mp, ret->num_material_packages);
    
    for(j = 0; j < mp->num_track_refs; j++)
      {
      /* Get track */
      mt = resolve_strong_ref(ret, mp->track_refs[j], MXF_TYPE_TRACK);

      if(!mt)
        {
        bgav_log(opt, BGAV_LOG_WARNING, LOG_DOMAIN, "Could not resolve track %d for material package", j);
        continue;
        }
      APPEND_ARRAY(mp->tracks, mt, mp->num_tracks);

      if(!mt->sequence)
        {
        if(!(mt->sequence =
             resolve_strong_ref(ret, mt->sequence_ref, MXF_TYPE_SEQUENCE)))
          {
          bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not resolve sequence for material track %d", j);
          return 0;
          }
        }
      /* Loop over clips */
      for(k = 0; k < mt->sequence->num_structural_component_refs; k++)
        {
        //        fprintf(stderr, "Find component: ");
        //        dump_ul(mt->sequence->structural_components_refs[k]);
        //        fprintf(stderr, "\n");
        
        component = resolve_strong_ref(ret,
                                       mt->sequence->structural_component_refs[k], MXF_TYPE_SOURCE_CLIP);
        if(!component)
          continue;
        
        APPEND_ARRAY(mt->sequence->structural_components,
                     component, mt->sequence->num_structural_components);

        /* Get source package */
        if(!component->source_package)
          component->source_package = get_source_package(ret, component);

        if(!component->source_package)
          {
          bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not resolve source package for clip %d of material track %d", k, j);
          continue;
          }

        /* Get source track */
        if(!component->source_track)
          component->source_track = get_source_track(ret, component->source_package, component);

        if(!component->source_track)
          {
          bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not resolve source track for clip %d of material track %d", k, j);
          continue;
          }

        st = component->source_track;
        
        /* From the sequence of the source track, we get the track type */
        if(!st->sequence)
          {
          if(!(st->sequence =
               resolve_strong_ref(ret, st->sequence_ref, MXF_TYPE_SEQUENCE)))
            {
            bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not resolve sequence for source track");
            return 0;
            }
          }
        if(st->sequence->stream_type == BGAV_STREAM_UNKNOWN)
          {
          }
        
        /* Get source descriptor */
        if(!component->source_descriptor)
          component->source_descriptor = get_source_descriptor(ret, component->source_package, component);

        if(!component->source_descriptor)
          {
          bgav_log(opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not resolve source descriptor for clip %d of material track %d", k, j);
          continue;
          }
        if(st->sequence->stream_type != BGAV_STREAM_UNKNOWN)
          {
          if(!component->source_descriptor->fourcc)
            {
            const codec_entry_t * ce;
            ce = match_codec(mxf_codec_uls,
                             component->source_descriptor->essence_codec_ul);
            if(ce)
              component->source_descriptor->fourcc = ce->fourcc;
            else if(st->sequence->stream_type == BGAV_STREAM_AUDIO)
              {
              ce = match_codec(mxf_picture_essence_container_uls,
                               component->source_descriptor->essence_container_ref);
              
              if(ce)
                component->source_descriptor->fourcc = ce->fourcc;
              }
            else if(st->sequence->stream_type == BGAV_STREAM_VIDEO)
              {
              ce = match_codec(mxf_sound_essence_container_uls,
                               component->source_descriptor->essence_container_ref);
              
              if(ce)
                component->source_descriptor->fourcc = ce->fourcc;
              }
            }
          }
        }
      }
    }
#endif
  return 1;
  }

static void update_source_track(mxf_file_t * f, mxf_klv_t * klv)
  {
  int i, j;
  mxf_track_t * t;
  mxf_package_t * p;
  mxf_preface_t * preface;
  mxf_content_storage_t * cs;
  
  int done = 0;
  /* Find track */
  preface = (mxf_preface_t*)f->preface;
  cs = (mxf_content_storage_t *)preface->content_storage;
  for(i = 0; i < cs->num_package_refs; i++)
    {
    if(cs->packages[i] &&
       cs->packages[i]->type == MXF_TYPE_SOURCE_PACKAGE)
      {
      p = (mxf_package_t*)(cs->packages[i]);
      for(j = 0; j < p->num_track_refs; j++)
        {
        t = (mxf_track_t*)(p->tracks[j]);
        if(!memcmp(t->track_number, &klv->key[12], 4))
          {
          done = 1;
          break;
          }
        }
      }
    if(done)
      break;
    }
  if(!done)
    return;
  t->num_packets++;
  if(klv->length > t->max_packet_size)
    t->max_packet_size = klv->length;
  }

int bgav_mxf_file_read(bgav_input_context_t * input,
                       mxf_file_t * ret)
  {
  mxf_klv_t klv;
  int64_t last_pos, header_start_pos;

  if(!input->input->seek_byte)
    {
    bgav_log(input->opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Cannot decode MXF file from non seekable source");
    return 0;
    }
  /* Read header partition pack */
  if(!bgav_mxf_sync(input))
    {
    fprintf(stderr, "End of file reached\n");
    return 0;
    }
  if(!bgav_mxf_klv_read(input, &klv))
    return 0;

  if(UL_MATCH(klv.key, mxf_header_partition_pack_key))
    {
    if(!bgav_mxf_partition_read(input, &klv, &ret->header_partition))
      return 0;
    //    fprintf(stderr, "Got header partition\n");
    }
  else
    {
    bgav_log(input->opt, BGAV_LOG_ERROR, LOG_DOMAIN, "Could not find header partition");
    return 0;
    }
  header_start_pos = 0;

  while(1) /* Look for primer pack */
    {
    last_pos = input->position;
    
    if(!bgav_mxf_klv_read(input, &klv))
      break;

    if(UL_MATCH(klv.key, mxf_primer_pack_key))
      {
      if(!bgav_mxf_primer_pack_read(input, &ret->header.primer_pack))
        return 0;
      //      fprintf(stderr, "Got primer pack\n");
      header_start_pos = last_pos;
      break;
      }
    else if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
      {
      //      fprintf(stderr, "Filler key: %ld\n", klv.length);
      bgav_input_skip(input, klv.length);
      }
    else
      {
#ifdef DUMP_UNKNOWN
      bgav_dprintf("Unknown header chunk:\n");
      bgav_mxf_klv_dump(0, &klv);
      bgav_input_skip_dump(input, klv.length);
#else
      bgav_input_skip(input, klv.length);
#endif
      }
    }
  
  /* Read header metadata */
  while((input->position - header_start_pos < ret->header_partition.header_byte_count))
    {
    last_pos = input->position;
    
    if(!bgav_mxf_klv_read(input, &klv))
      break;
    if(1)
      {
      if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
        {
        //        fprintf(stderr, "Filler key: %ld\n", klv.length);
        bgav_input_skip(input, klv.length);
        }
      else if(UL_MATCH(klv.key, mxf_content_storage_key))
        {
        if(!read_header_metadata(input, ret, &klv,
                                 read_content_storage,
                                 sizeof(mxf_content_storage_t),
                                 MXF_TYPE_CONTENT_STORAGE))
          return 0;
        }
      else if(UL_MATCH(klv.key, mxf_source_package_key))
        {
        //        fprintf(stderr, "mxf_source_package_key\n");
        // bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_source_package, sizeof(mxf_package_t),
                                 MXF_TYPE_SOURCE_PACKAGE))
          return 0;
        }
      else if(UL_MATCH(klv.key, mxf_essence_container_data_key))
        {
        //        fprintf(stderr, "mxf_essence_container_data_key\n");
        // bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_essence_container_data, sizeof(mxf_essence_container_data_t),
                                 MXF_TYPE_ESSENCE_CONTAINER_DATA))
          return 0;
        }
      else if(UL_MATCH(klv.key, mxf_material_package_key))
        {
        //        fprintf(stderr, "mxf_material_package_key\n");
        //        bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_material_package, sizeof(mxf_package_t),
                                 MXF_TYPE_MATERIAL_PACKAGE))
          return 0;

        }
      else if(UL_MATCH(klv.key, mxf_sequence_key))
        {
        //        fprintf(stderr, "mxf_sequence_key\n");
        if(!read_header_metadata(input, ret, &klv,
                                 read_sequence, sizeof(mxf_sequence_t),
                                 MXF_TYPE_SEQUENCE))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_source_clip_key))
        {
        //        bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_source_clip, sizeof(mxf_source_clip_t),
                                 MXF_TYPE_SOURCE_CLIP))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_timecode_component_key))
        {
        //        fprintf(stderr, "mxf_timecode_component_key\n");
        // bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_timecode_component, sizeof(mxf_timecode_component_t),
                                 MXF_TYPE_TIMECODE_COMPONENT))
          return 0;
        }
      else if(UL_MATCH(klv.key, mxf_static_track_key))
        {
        //  fprintf(stderr, "mxf_static_track_key\n");
        //  bgav_input_skip_dump(input, klv.length);

        if(!read_header_metadata(input, ret, &klv,
                                 read_track, sizeof(mxf_track_t),
                                 MXF_TYPE_TRACK))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_preface_key))
        {
        //  fprintf(stderr, "mxf_static_track_key\n");
        //  bgav_input_skip_dump(input, klv.length);

        if(!read_header_metadata(input, ret, &klv,
                                 read_preface, sizeof(mxf_preface_t),
                                 MXF_TYPE_PREFACE))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_generic_track_key))
        {
        //        fprintf(stderr, "mxf_generic_track_key\n");
        //        bgav_input_skip_dump(input, klv.length);
        if(!read_header_metadata(input, ret, &klv,
                                 read_source_clip, sizeof(mxf_track_t),
                                 MXF_TYPE_TRACK))
          return 0;
        }
      else if(UL_MATCH(klv.key, mxf_descriptor_multiple_key))
        {
        if(!read_header_metadata(input, ret, &klv,
                                 read_descriptor, sizeof(mxf_descriptor_t),
                                 MXF_TYPE_MULTIPLE_DESCRIPTOR))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_identification_key))
        {
        if(!read_header_metadata(input, ret, &klv,
                                 read_identification, sizeof(mxf_identification_t),
                                 MXF_TYPE_IDENTIFICATION))
          return 0;
        
        }
      else if(UL_MATCH(klv.key, mxf_descriptor_generic_sound_key) ||
              UL_MATCH(klv.key, mxf_descriptor_cdci_key) ||
              UL_MATCH(klv.key, mxf_descriptor_rgba_key) ||
              UL_MATCH(klv.key, mxf_descriptor_mpeg2video_key) ||
              UL_MATCH(klv.key, mxf_descriptor_wave_key) ||
              UL_MATCH(klv.key, mxf_descriptor_aes3_key))
        {
        if(!read_header_metadata(input, ret, &klv,
                                 read_descriptor, sizeof(mxf_descriptor_t),
                                 MXF_TYPE_DESCRIPTOR))
          return 0;
        }
      else
        {
#ifdef DUMP_UNKNOWN
        bgav_dprintf("Unknown metadata chunk:\n");
        bgav_mxf_klv_dump(0, &klv);
        bgav_input_skip_dump(input, klv.length);
#else
        bgav_input_skip(input, klv.length);
#endif
        }
      }
    }
  //  fprintf(stderr, "Header done\n");
  
  ret->data_start = input->position;
  
  if(!bgav_mxf_finalize(ret, input->opt))
    {
    fprintf(stderr, "Finalizing failed");
    return 0;
    }
  /* Read rest */
  while(1)
    {
    if(!bgav_mxf_klv_read(input, &klv))
      break;
    
    if(UL_MATCH_MOD_REGVER(klv.key, mxf_filler_key))
      {
      //      fprintf(stderr, "Filler key: %ld\n", klv.length);
      bgav_input_skip(input, klv.length);
      }
    else if(UL_MATCH(klv.key, mxf_index_table_segment_key))
      {
      if(!read_index_table_segment(input, ret, &klv))
        return 0;
      }
    else if(UL_MATCH(klv.key, mxf_essence_element_key))
      {
      update_source_track(ret, &klv);

      bgav_dprintf("Essence element for track %02x %02x %02x %02x (%ld bytes)\n",
                   klv.key[12], klv.key[13], klv.key[14], klv.key[15], klv.length);
      bgav_input_skip(input, klv.length);
      }
    else
      {
      bgav_input_skip(input, klv.length);
      bgav_dprintf("Unknown KLV: "); bgav_mxf_klv_dump(0, &klv);
      }
    }
  
  bgav_input_seek(input, ret->data_start, SEEK_SET);
  
  return 1;
  }

void bgav_mxf_file_free(mxf_file_t * ret)
  {
  int i;

  bgav_mxf_partition_free(&ret->header_partition);
  bgav_mxf_primer_pack_free(&ret->header.primer_pack);
  
  if(ret->index_segments)
    {
    for(i = 0; i < ret->num_index_segments; i++)
      {
      bgav_mxf_index_table_segment_free(ret->index_segments[i]);
      free(ret->index_segments[i]);
      }
    free(ret->index_segments);
    }
  
  if(ret->header.metadata)
    {
    for(i = 0; i < ret->header.num_metadata; i++)
      {
      switch(ret->header.metadata[i]->type)
        {
        case MXF_TYPE_MATERIAL_PACKAGE:
        case MXF_TYPE_SOURCE_PACKAGE:
          bgav_mxf_package_free((mxf_package_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_PREFACE:
          bgav_mxf_preface_free((mxf_preface_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_CONTENT_STORAGE:
          bgav_mxf_content_storage_free((mxf_content_storage_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_SOURCE_CLIP:
          bgav_mxf_source_clip_free((mxf_source_clip_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_TIMECODE_COMPONENT:
          bgav_mxf_timecode_component_free((mxf_timecode_component_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_SEQUENCE:
          bgav_mxf_sequence_free((mxf_sequence_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_MULTIPLE_DESCRIPTOR:
        case MXF_TYPE_DESCRIPTOR:
          bgav_mxf_descriptor_free((mxf_descriptor_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_ESSENCE_CONTAINER_DATA:
          bgav_mxf_essence_container_data_free((mxf_essence_container_data_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_TRACK:
          bgav_mxf_track_free((mxf_track_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_CRYPTO_CONTEXT:
          break;
        case MXF_TYPE_IDENTIFICATION:
          bgav_mxf_identification_free((mxf_identification_t*)(ret->header.metadata[i]));
          break;
        }
      if(ret->header.metadata[i])
        free(ret->header.metadata[i]);
      }
    free(ret->header.metadata);
    }

  }

void bgav_mxf_file_dump(mxf_file_t * ret)
  {
  int i;
  bgav_dprintf("\nMXF File structure\n"); 
  bgav_dprintf("source packages:                 %d\n", ret->num_source_packages);
  bgav_dprintf("material packages:               %d\n", ret->num_material_packages);
  bgav_dprintf("maximum components per sequence: %d\n", ret->max_sequence_components);
  
  bgav_dprintf("  Header "); bgav_mxf_partition_dump(2, &ret->header_partition);

  bgav_mxf_primer_pack_dump(2, &ret->header.primer_pack);

  if(ret->header.metadata)
    {
    for(i = 0; i < ret->header.num_metadata; i++)
      {
      switch(ret->header.metadata[i]->type)
        {
        case MXF_TYPE_MATERIAL_PACKAGE:
          bgav_dprintf("  Material");
          bgav_mxf_package_dump(2, (mxf_package_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_SOURCE_PACKAGE:
          bgav_dprintf("  Source");
          bgav_mxf_package_dump(2, (mxf_package_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_SOURCE_CLIP:
          bgav_mxf_source_clip_dump(2, (mxf_source_clip_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_TIMECODE_COMPONENT:
          bgav_mxf_timecode_component_dump(2, (mxf_timecode_component_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_PREFACE:
          bgav_mxf_preface_dump(2, (mxf_preface_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_CONTENT_STORAGE:
          bgav_mxf_content_storage_dump(2, (mxf_content_storage_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_SEQUENCE:
          bgav_mxf_sequence_dump(2, (mxf_sequence_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_MULTIPLE_DESCRIPTOR:
          bgav_mxf_descriptor_dump(2, (mxf_descriptor_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_DESCRIPTOR:
          bgav_mxf_descriptor_dump(2, (mxf_descriptor_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_TRACK:
          bgav_mxf_track_dump(2, (mxf_track_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_IDENTIFICATION:
          bgav_mxf_identification_dump(2, (mxf_identification_t*)(ret->header.metadata[i]));
          break;
        case MXF_TYPE_ESSENCE_CONTAINER_DATA:
          bgav_mxf_essence_container_data_dump(2, (mxf_essence_container_data_t*)(ret->header.metadata[i]));
          break;

        case MXF_TYPE_CRYPTO_CONTEXT:
          break;
        }
      }
    }
  if(ret->num_index_segments)
    {
    for(i = 0; i < ret->num_index_segments; i++)
      {
      bgav_mxf_index_table_segment_dump(0, ret->index_segments[i]);
      }
    }
  }

bgav_stream_t * bgav_mxf_find_stream(mxf_file_t * f, bgav_demuxer_context_t * t, mxf_ul_t ul)
  {
  uint32_t stream_id;
  if(!UL_MATCH(ul, mxf_essence_element_key))
    return (bgav_stream_t *)0;

  if(((mxf_preface_t*)(f->preface))->operational_pattern == MXF_OP_ATOM)
    {
    bgav_stream_t * ret = (bgav_stream_t *)0;
    if(t->tt->cur->num_audio_streams)
      ret = t->tt->cur->audio_streams;
    if(t->tt->cur->num_video_streams)
      ret = t->tt->cur->video_streams;
    if(t->tt->cur->num_subtitle_streams)
      ret = t->tt->cur->subtitle_streams;
    if(ret && ret->action == BGAV_STREAM_MUTE)
      return (bgav_stream_t *)0;
    return ret;
    }

  stream_id = ul[12] << 24 |
    ul[13] << 16 |
    ul[14] <<  8 |
    ul[15];
  return bgav_track_find_stream(t, stream_id);
  }

