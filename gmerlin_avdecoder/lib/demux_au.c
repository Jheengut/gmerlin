/*****************************************************************
 
  demux_au.c
 
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

#include <avdec_private.h>
#include <stdlib.h>
#include <stdio.h>

#define BLOCKS_PER_PACKET 1024

/* AU demuxer */

typedef struct
  {
  uint32_t magic;       /* magic number */
  uint32_t hdr_size;    /* size of this header */ 
  uint32_t data_size;   /* length of data (optional) */ 
  uint32_t encoding;    /* data encoding format */
  uint32_t sample_rate; /* samples per second */
  uint32_t channels;    /* number of interleaved channels */
  } Audio_filehdr;

typedef struct
  {
  uint32_t data_start;
  uint32_t data_size;
  int samples_per_block;
  int bytes_per_second;
  } au_priv_t;

/* Define the magic number */ 
#define AUDIO_FILE_MAGIC ((uint32_t)0x2e736e64)

/* Define the encoding fields */ 
#define AUDIO_FILE_ENCODING_MULAW_8      (1)  /* 8-bit ISDN u-law */
#define AUDIO_FILE_ENCODING_LINEAR_8     (2)  /* 8-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_16    (3)  /* 16-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_24    (4)  /* 24-bit linear PCM */ 
#define AUDIO_FILE_ENCODING_LINEAR_32    (5)  /* 32-bit linear PCM */ 
#define AUDIO_FILE_ENCODING_FLOAT        (6)  /* 32-bit IEEE floating point */ 
#define AUDIO_FILE_ENCODING_DOUBLE       (7)  /* 64-bit IEEE floating point */ 
#define AUDIO_FILE_ENCODING_ADPCM_G721   (23) /* 4-bit CCITT g.721 ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G722   (24) /* CCITT g.722 ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G723_3 (25) /* CCITT g.723 3-bit ADPCM */ 
#define AUDIO_FILE_ENCODING_ADPCM_G723_5 (26) /* CCITT g.723 5-bit ADPCM */ 
#define AUDIO_FILE_ENCODING_ALAW_8       (27) /* 8-bit ISDN A-law */

static int probe_au(bgav_input_context_t * input)
  {
  uint8_t test_data[4];
  if(bgav_input_get_data(input, test_data, 4) < 4)
    return 0;
  if((test_data[0] == '.') &&
     (test_data[1] == 's') &&
     (test_data[2] == 'n') &&
     (test_data[3] == 'd'))
    return 1;
  return 0;
  }

static gavl_time_t pos_2_time(bgav_demuxer_context_t * ctx, int64_t pos)
  {
  au_priv_t * priv;
  priv = (au_priv_t*)(ctx->priv);
  
  return ((pos - priv->data_start) * GAVL_TIME_SCALE * priv->samples_per_block) /
    (ctx->tt->current_track->audio_streams[0].data.audio.format.samplerate * 
     ctx->tt->current_track->audio_streams[0].data.audio.block_align);
  
  }

static int64_t time_2_pos(bgav_demuxer_context_t * ctx, gavl_time_t time)
  {
  au_priv_t * priv;
  priv = (au_priv_t*)(ctx->priv);
  return priv->data_start +
    (time *
     ctx->tt->current_track->audio_streams[0].data.audio.format.samplerate *
     ctx->tt->current_track->audio_streams[0].data.audio.block_align)/
    (priv->samples_per_block * GAVL_TIME_SCALE);
  
  }


static int open_au(bgav_demuxer_context_t * ctx,
                   bgav_redirector_context_t ** redir)
  {
  Audio_filehdr hdr;
  au_priv_t * priv;
  bgav_stream_t * as;
  int samples_per_block = 0;

  /* Create track */
  ctx->tt = bgav_track_table_create(1);
  
  if(!bgav_input_read_fourcc(ctx->input, &(hdr.magic)) ||
     !bgav_input_read_32_be(ctx->input, &(hdr.hdr_size)) ||
     !bgav_input_read_32_be(ctx->input, &(hdr.data_size)) ||
     !bgav_input_read_32_be(ctx->input, &(hdr.encoding)) ||
     !bgav_input_read_32_be(ctx->input, &(hdr.sample_rate)) ||
     !bgav_input_read_32_be(ctx->input, &(hdr.channels)))
    return 0;

  /* Get codec */
  
  switch(hdr.encoding)
    {
    case AUDIO_FILE_ENCODING_MULAW_8:      /* 8-bit ISDN u-law */
      as = bgav_track_add_audio_stream(ctx->tt->current_track);
      samples_per_block = 1;
      as->fourcc = BGAV_MK_FOURCC('u', 'l', 'a', 'w');
      as->data.audio.block_align = hdr.channels;
      break;
    case AUDIO_FILE_ENCODING_ALAW_8:       /* 8-bit ISDN A-law */
      as = bgav_track_add_audio_stream(ctx->tt->current_track);
      samples_per_block = 1;
      as->fourcc = BGAV_MK_FOURCC('a', 'l', 'a', 'w');
      as->data.audio.block_align = hdr.channels;
      
      break;
    case AUDIO_FILE_ENCODING_LINEAR_8:     /* 8-bit linear PCM */
      as = bgav_track_add_audio_stream(ctx->tt->current_track);
      samples_per_block = 1;
      as->fourcc = BGAV_MK_FOURCC('t', 'w', 'o', 's');
      as->data.audio.block_align = hdr.channels;
      as->data.audio.bits_per_sample = 8;

      break;
    case AUDIO_FILE_ENCODING_LINEAR_16:    /* 16-bit linear PCM */
      as = bgav_track_add_audio_stream(ctx->tt->current_track);
      samples_per_block = 1;
      as->fourcc = BGAV_MK_FOURCC('t', 'w', 'o', 's');
      as->data.audio.block_align = hdr.channels * 2;
      as->data.audio.bits_per_sample = 16;
      
      break;
#if 0
    case AUDIO_FILE_ENCODING_LINEAR_24:    /* 24-bit linear PCM */ 
      break;
    case AUDIO_FILE_ENCODING_LINEAR_32:    /* 32-bit linear PCM */ 
      break;
    case AUDIO_FILE_ENCODING_FLOAT:        /* 32-bit IEEE floating point */ 
      break;
    case AUDIO_FILE_ENCODING_DOUBLE:       /* 64-bit IEEE floating point */ 
      break;
    case AUDIO_FILE_ENCODING_ADPCM_G721:   /* 4-bit CCITT g.721 ADPCM */ 
      break;
    case AUDIO_FILE_ENCODING_ADPCM_G722:   /* CCITT g.722 ADPCM */ 
      break;
    case AUDIO_FILE_ENCODING_ADPCM_G723_3: /* CCITT g.723 3-bit ADPCM */ 
      break;
    case AUDIO_FILE_ENCODING_ADPCM_G723_5: /* CCITT g.723 5-bit ADPCM */ 
      break;
#endif
    default:
      fprintf(stderr, "Unsupported encoding %d\n", hdr.encoding);
      return 0;
    }

  /* Get audio format */
  
  as->data.audio.format.samplerate = hdr.sample_rate;
  as->data.audio.format.num_channels = hdr.channels;
  
  priv = calloc(1, sizeof(*priv));
  ctx->priv = priv;

  /* Get data start and duration */

  priv->data_start = hdr.hdr_size;
  priv->data_size  = hdr.data_size;

  if(priv->data_size == 0xFFFFFFFF)
    priv->data_size = ctx->input->total_bytes;
  priv->samples_per_block = samples_per_block;
  if(priv->data_size)
    ctx->tt->current_track->duration = pos_2_time(ctx, priv->data_start + priv->data_size);
  if(ctx->input->input->seek_byte)
    ctx->can_seek = 1;
  
  /* Skip everything until data section */

  if(hdr.hdr_size > 24)
    bgav_input_skip(ctx->input, hdr.hdr_size - 24);
  priv->data_start = ctx->input->position;
  return 1;
  }

static int next_packet_au(bgav_demuxer_context_t * ctx)
  {
  bgav_packet_t * p;
  bgav_stream_t * s;
  int bytes_read;
  au_priv_t * priv;
  s = &(ctx->tt->current_track->audio_streams[0]);
  p = bgav_packet_buffer_get_packet_write(s->packet_buffer);

  priv = (au_priv_t*)(ctx->priv);
  
  bgav_packet_alloc(p, s->data.audio.block_align * BLOCKS_PER_PACKET);

  p->timestamp = pos_2_time(ctx, ctx->input->position);
  p->keyframe = 1;
  bytes_read = bgav_input_read_data(ctx->input, p->data,
                                    s->data.audio.block_align * BLOCKS_PER_PACKET);
  p->data_size = bytes_read;
  bgav_packet_done_write(p);
  if(!bytes_read)
    return 0;
  return 1;
  }

static void seek_au(bgav_demuxer_context_t * ctx, gavl_time_t time)
  {
  bgav_stream_t * s;
  int64_t position;
  au_priv_t * priv;
  s = &(ctx->tt->current_track->audio_streams[0]);
  priv = (au_priv_t*)(ctx->priv);
  
  position = time_2_pos(ctx, time);
  position -= priv->data_start;
  position /= s->data.audio.block_align;
  position *= s->data.audio.block_align;
  position += priv->data_start;
  bgav_input_seek(ctx->input, position, SEEK_SET);
  s->time = pos_2_time(ctx, position);
  }

static void close_au(bgav_demuxer_context_t * ctx)
  {
  au_priv_t * priv;
  priv = (au_priv_t*)(ctx->priv);
  free(priv);
  }

bgav_demuxer_t bgav_demuxer_au =
  {
    probe:       probe_au,
    open:        open_au,
    next_packet: next_packet_au,
    seek:        seek_au,
    close:       close_au
  };
