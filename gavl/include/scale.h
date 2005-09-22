/*****************************************************************

  scale.h

  Copyright (c) 2005 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

*****************************************************************/

#include "video.h"

/* Typedefs */

typedef struct gavl_video_scale_context_s gavl_video_scale_context_t;

typedef void (*gavl_video_scale_scanline_func)(gavl_video_scale_context_t*);

typedef float (*gavl_video_scale_get_weight)(gavl_video_options_t * opt, double t);

gavl_video_scale_get_weight gavl_video_scale_get_weight_func(gavl_video_options_t * opt,
                                                             int * num_points);

/* Scale functions */

typedef struct
  {
  gavl_video_scale_scanline_func scale_rgb_15;
  gavl_video_scale_scanline_func scale_rgb_16;
  gavl_video_scale_scanline_func scale_uint8_x_1;
  gavl_video_scale_scanline_func scale_uint8_x_3;
  gavl_video_scale_scanline_func scale_uint8_x_4;
  gavl_video_scale_scanline_func scale_uint16_x_1;
  gavl_video_scale_scanline_func scale_uint16_x_3;
  gavl_video_scale_scanline_func scale_uint16_x_4;
  gavl_video_scale_scanline_func scale_float_x_3;
  gavl_video_scale_scanline_func scale_float_x_4;

  /* Bits needed for the integer scaling coefficient */
  int bits_rgb_15;
  int bits_rgb_16;
  int bits_uint8;
  int bits_uint16;
  } gavl_scale_func_tab_t;

typedef struct
  {
  gavl_scale_func_tab_t funcs_x;
  gavl_scale_func_tab_t funcs_y;
  gavl_scale_func_tab_t funcs_xy;
  } gavl_scale_funcs_t;

void gavl_init_scale_funcs_nearest_c(gavl_scale_funcs_t * tab);

void gavl_init_scale_funcs_bilinear_c(gavl_scale_funcs_t * tab);

void gavl_init_scale_funcs_quadratic_c(gavl_scale_funcs_t * tab);

void gavl_init_scale_funcs_bicubic_c(gavl_scale_funcs_t * tab);

void gavl_init_scale_funcs_generic_c(gavl_scale_funcs_t * tab);
#ifdef ARCH_X86

void gavl_init_scale_funcs_mmxext(gavl_scale_funcs_t * tab,
                                  gavl_scale_mode_t scale_mode,
                                  int scale_x, int scale_y, int min_scanline_width);
void gavl_init_scale_funcs_mmx(gavl_scale_funcs_t * tab,
                               gavl_scale_mode_t scale_mode,
                               int scale_x, int scale_y, int min_scanline_width);
#endif

typedef struct
  {
  float fac_f; /* Scaling coefficient with high precision */
  int fac_i;   /* Scaling coefficient with lower precision */
  } gavl_video_scale_factor_t;

typedef struct
  {
  int index; /* Index of the first row/column */
  gavl_video_scale_factor_t * factor;
  } gavl_video_scale_pixel_t;

typedef struct
  {
  int pixels_alloc;
  int factors_alloc;
  int num_pixels; /* Number of pixels (rows/columns) in the output area */
  gavl_video_scale_factor_t * factors;
  gavl_video_scale_pixel_t  * pixels;
  int factors_per_pixel;
  } gavl_video_scale_table_t;

void gavl_video_scale_table_init(gavl_video_scale_table_t * tab,
                                 gavl_video_options_t * opt,
                                 double src_off, double src_size,
                                 int dst_size,
                                 int src_width);

void gavl_video_scale_table_init_int(gavl_video_scale_table_t * tab,
                                     int bits);

void gavl_video_scale_table_get_src_indices(gavl_video_scale_table_t * tab,
                                            int * start, int * size);

void gavl_video_scale_table_shift_indices(gavl_video_scale_table_t * tab,
                                          int shift);



void gavl_video_scale_table_cleanup(gavl_video_scale_table_t * tab);

/* For debugging */

void gavl_video_scale_table_dump(gavl_video_scale_table_t * tab);

/* Data needed by the scaline function. */

typedef struct
  {
  /* Offsets and advances are ALWAYS in bytes */
  int src_advance, dst_advance;
  int src_offset,  dst_offset;
  } gavl_video_scale_offsets_t;

/*
 *  Scale context is for one plane of one field.
 *  This means, that depending on the video format, we have 1 - 6 scale contexts.
 */

struct gavl_video_scale_context_s
  {
  /* Data initialized at start */
  gavl_video_scale_table_t table_h;
  gavl_video_scale_table_t table_v;
  gavl_video_scale_scanline_func func1;
  gavl_video_scale_scanline_func func2;

  gavl_video_scale_offsets_t offset1;
  gavl_video_scale_offsets_t offset2;
  
  /* Rectangles */
  gavl_rectangle_f_t src_rect;
  gavl_rectangle_i_t dst_rect;

  /* Number of filter taps */
  int num_taps;
    
  /* Indices of source and destination planes inside the frame. Can be 0 for chroma channels of
     packed YUV formats */
  
  int src_frame_plane, dst_frame_plane;

  int plane; /* Plane */
  
  /* Advances */

  gavl_video_scale_offsets_t * offset;
  
  /* Temporary buffer */
  uint8_t * buffer;
  int buffer_alloc;
  int buffer_stride;
  
  /* Size of temporary buffer in pixels */
  int buffer_width;
  int buffer_height;

  int num_directions;

  /* Minimum and maximum values for clipping.
     Values can be different for different components */
  
  uint32_t min_values[4];
  uint32_t max_values[4];

  /* These are used by the generic scaler */

  int64_t tmp[4]; /* For accumulating values */

  /* For copying */
  int bytes_per_line;
  
  /* Data changed during scaling */
  uint8_t * src;
  int src_stride;

  uint8_t * dst;
  int scanline;
  int dst_size;
  };

int gavl_video_scale_context_init(gavl_video_scale_context_t*,
                                  gavl_video_options_t * opt,
                                  int plane,
                                  const gavl_video_format_t * input_format,
                                  const gavl_video_format_t * output_format,
                                  gavl_scale_funcs_t * funcs,
                                  int src_field, int dst_field,
                                  int src_fields, int dst_fields);

void gavl_video_scale_context_cleanup(gavl_video_scale_context_t * ctx);

void gavl_video_scale_context_scale(gavl_video_scale_context_t * ctx,
                                    gavl_video_frame_t * src, gavl_video_frame_t * dst);

struct gavl_video_scaler_s
  {
  gavl_video_options_t opt;

  /* a context is obtained with contexts[field][plane] */
  gavl_video_scale_context_t contexts[2][GAVL_MAX_PLANES];
  
  int num_planes;

  /* If src_fields > dst_fields, we deinterlace */
  int src_fields;
  int dst_fields;
    
  gavl_video_frame_t * src;
  gavl_video_frame_t * dst;

  gavl_video_frame_t * src_field;
  gavl_video_frame_t * dst_field;
  
  gavl_video_format_t src_format;
  gavl_video_format_t dst_format;

  gavl_rectangle_i_t dst_rect;
  //  gavl_rectangle_f_t src_rect;

  };

