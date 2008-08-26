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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <config.h>
#include <translation.h>
#include <plugin.h>
#include <utils.h>
#include <log.h>
#include <bggavl.h>

#define LOG_DOMAIN "fv_transform"

#define MODE_ROTATE 0
#define MODE_AFFINE 1
#define MODE_PERSPECTIVE 2

typedef struct
  {
  
  bg_read_video_func_t read_func;
  void * read_data;
  int read_stream;
  
  gavl_video_format_t format;
  
  gavl_video_frame_t * frame;
  gavl_image_transform_t * transform;
  gavl_video_options_t * opt;
  
  int changed;
  int quality;
  int scale_order;
  gavl_scale_mode_t scale_mode;

  int mode;
  
  float bg_color[4];
  float sar;
  
  /* Some transforms can be described by a matrix */
  double matrix[2][3];
  double matrix3[3][3];

  /* Rotate */
  float rotate_angle;

  /* Perspective */

  double perspective_tr[2];
  double perspective_tl[2];
  double perspective_br[2];
  double perspective_bl[2];
  
  /* Generic affine */
  double affine_xx;
  double affine_xy;
  double affine_yx;
  double affine_yy;
  double affine_ox;
  double affine_oy;
    
  } transform_t;

static void * create_transform()
  {
  transform_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->transform = gavl_image_transform_create();
  ret->opt = gavl_image_transform_get_options(ret->transform);
  return ret;
  }

static void destroy_transform(void * priv)
  {
  transform_t * vp;
  vp = (transform_t *)priv;
  if(vp->frame)
    gavl_video_frame_destroy(vp->frame);

  gavl_image_transform_destroy(vp->transform);
  
  free(vp);
  }

static const bg_parameter_info_t parameters[] =
  {
    {
      .gettext_domain = PACKAGE,
      .gettext_directory = LOCALE_DIR,
      .name = "general",
      .long_name =   TRS("General"),
      .type = BG_PARAMETER_SECTION,
    },
    {
      .name =        "mode",
      .long_name =   TRS("Transformation mode"),
      .opt =         "tm",
      .type =        BG_PARAMETER_STRINGLIST,
      .flags = BG_PARAMETER_SYNC,
      .multi_names = (const char*[]){ "rotate", "affine", "perspective",
                                      (char*)0 },
      .multi_labels = (const char*[]){ TRS("Rotate"),
                                       TRS("Generic affine"),
                                       TRS("Perspective"),
                                       (char*)0 },
      .val_default = { .val_str = "rotate" },
      .help_string = TRS("Choose Transformation method. Each method can be configured in it's section."),
      
    },
    {
    .name =        "scale_mode",
    .long_name =   TRS("Interpolation mode"),
    .opt =         "im",
    .type =        BG_PARAMETER_STRINGLIST,
    .flags = BG_PARAMETER_SYNC,
    .multi_names = BG_GAVL_TRANSFORM_MODE_NAMES,
    .multi_labels = BG_GAVL_TRANSFORM_MODE_LABELS,
    .val_default = { .val_str = "auto" },
    .help_string = TRS("Choose interpolation method. Auto means to choose based on the conversion quality. Nearest is fastest, Bicubic is slowest."),
    },
    {
      .name = "quality",
      .long_name = TRS("Quality"),
      .type = BG_PARAMETER_SLIDER_INT,
      .flags = BG_PARAMETER_SYNC,
      .val_min =     { .val_i = GAVL_QUALITY_FASTEST },
      .val_max =     { .val_i = GAVL_QUALITY_BEST },
      .val_default = { .val_i = GAVL_QUALITY_DEFAULT },
    },
    {
      .name = "bg_color",
      .long_name = TRS("Background color"),
      .type = BG_PARAMETER_COLOR_RGBA,
      .flags = BG_PARAMETER_SYNC,
      .val_default = { .val_color = { 0.0, 0.0, 0.0, 1.0 } },
    },
    {
    .name =        "mode_rotate",
    .long_name =   TRS("Rotate"),
    .type =        BG_PARAMETER_SECTION,
    },
    {
      .name      =  "rotate_angle",
      .long_name =  TRS("Angle"),
      .type =        BG_PARAMETER_SLIDER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 0.0 },
      .val_min = { .val_f = -360.0 },
      .val_max = { .val_f = 360.0 },
      .num_digits = 2,
    },
    {
    .name =        "mode_affine",
    .long_name =   TRS("Generic affine"),
    .type =        BG_PARAMETER_SECTION,
    },
    {
      .name      =  "affine_xx",
      .long_name =  TRS("X -> X"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 1.0 },
      .val_min = { .val_f =   -2.0 },
      .val_max = { .val_f =    2.0 },
      .num_digits = 3,
    },
    {
      .name      =  "affine_xy",
      .long_name =  TRS("X -> Y"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 0.0 },
      .val_min = { .val_f =   -2.0 },
      .val_max = { .val_f =    2.0 },
      .num_digits = 3,
    },
    {
      .name      =  "affine_yx",
      .long_name =  TRS("Y -> X"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 0.0 },
      .val_min = { .val_f =   -2.0 },
      .val_max = { .val_f =    2.0 },
      .num_digits = 3,
    },
    {
      .name      =  "affine_yy",
      .long_name =  TRS("Y -> Y"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 1.0 },
      .val_min = { .val_f =   -2.0 },
      .val_max = { .val_f =    2.0 },
      .num_digits = 3,
    },
    {
      .name      =  "affine_ox",
      .long_name =  TRS("X Offset"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 0.0 },
      .val_min = { .val_f =   -1.0 },
      .val_max = { .val_f =    1.0 },
      .num_digits = 3,
      .help_string = TRS("Normalized X offset. 1 corresponds to image with."),
    },
    {
      .name      =  "affine_oy",
      .long_name =  TRS("Y Offset"),
      .type =        BG_PARAMETER_FLOAT,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_f = 0.0 },
      .val_min = { .val_f =   -1.0 },
      .val_max = { .val_f =    1.0 },
      .num_digits = 3,
      .help_string = TRS("Normalized Y offset. 1 corresponds to image height."),
    },
    {
    .name =        "mode_perspective",
    .long_name =   TRS("Perspective"),
    .type =        BG_PARAMETER_SECTION,
    },
    {
      .name      =  "perspective_tl",
      .long_name =  TRS("Top left"),
      .type =        BG_PARAMETER_POSITION,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_pos = { 0.0, 0.0 } },
      .num_digits = 2,
      .help_string = TRS("Top left corner in normalized image coordinates"),
    },
    {
      .name      =  "perspective_tr",
      .long_name =  TRS("Top right"),
      .type =        BG_PARAMETER_POSITION,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_pos = { 1.0, 0.0 } },
      .num_digits = 2,
      .help_string = TRS("Top right corner in normalized image coordinates"),
    },
    {
      .name      =  "perspective_bl",
      .long_name =  TRS("Bottom left"),
      .type =        BG_PARAMETER_POSITION,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_pos = { 0.0, 1.0 } },
      .num_digits = 2,
      .help_string = TRS("Bottom left corner in normalized image coordinates"),
    },
    {
      .name      =  "perspective_br",
      .long_name =  TRS("Bottom right"),
      .type =        BG_PARAMETER_POSITION,
      .flags =     BG_PARAMETER_SYNC,
      .val_default = { .val_pos = { 1.0, 1.0 } },
      .num_digits = 2,
      .help_string = TRS("Bottom right corner in normalized image coordinates"),
    },
    { /* End of parameters */ },
  };
static const bg_parameter_info_t * get_parameters_transform(void * priv)
  {
  return parameters;
  }

static void
set_parameter_transform(void * priv, const char * name,
                        const bg_parameter_value_t * val)
  {
  transform_t * vp;
  gavl_scale_mode_t scale_mode;
  
  vp = (transform_t *)priv;

  if(!name)
    return;

  else if(!strcmp(name, "scale_mode"))
    {
    scale_mode = bg_gavl_string_to_scale_mode(val->val_str);
    if(vp->scale_mode != scale_mode)
      {
      vp->scale_mode = scale_mode;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "quality"))
    {
    if(vp->quality != val->val_i)
      {
      vp->quality = val->val_i;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "bg_color"))
    {
    memcpy(vp->bg_color, val->val_color, sizeof(vp->bg_color));
    }
  else if(!strcmp(name, "mode"))
    {
    int new_mode = 0;
    if(!strcmp(val->val_str, "rotate"))
      new_mode = MODE_ROTATE;
    else if(!strcmp(val->val_str, "affine"))
      new_mode = MODE_AFFINE;
    else if(!strcmp(val->val_str, "perspective"))
      new_mode = MODE_PERSPECTIVE;

    if(new_mode != vp->mode)
      {
      vp->mode = new_mode;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "rotate_angle"))
    {
    if(vp->rotate_angle != val->val_f)
      {
      vp->rotate_angle = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_xx"))
    {
    if(vp->affine_xx != val->val_f)
      {
      vp->affine_xx = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_xy"))
    {
    if(vp->affine_xy != val->val_f)
      {
      vp->affine_xy = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_yx"))
    {
    if(vp->affine_yx != val->val_f)
      {
      vp->affine_yx = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_yy"))
    {
    if(vp->affine_yy != val->val_f)
      {
      vp->affine_yy = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_ox"))
    {
    if(vp->affine_ox != val->val_f)
      {
      vp->affine_ox = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "affine_oy"))
    {
    if(vp->affine_oy != val->val_f)
      {
      vp->affine_oy = val->val_f;
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "perspective_tl"))
    {
    if((vp->perspective_tl[0] != val->val_pos[0]) ||
       (vp->perspective_tl[1] != val->val_pos[1]))
      {
      vp->perspective_tl[0] = val->val_pos[0];
      vp->perspective_tl[1] = val->val_pos[1];
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "perspective_tr"))
    {
    if((vp->perspective_tr[0] != val->val_pos[0]) ||
       (vp->perspective_tr[1] != val->val_pos[1]))
      {
      vp->perspective_tr[0] = val->val_pos[0];
      vp->perspective_tr[1] = val->val_pos[1];
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "perspective_bl"))
    {
    if((vp->perspective_bl[0] != val->val_pos[0]) ||
       (vp->perspective_bl[1] != val->val_pos[1]))
      {
      vp->perspective_bl[0] = val->val_pos[0];
      vp->perspective_bl[1] = val->val_pos[1];
      vp->changed = 1;
      }
    }
  else if(!strcmp(name, "perspective_br"))
    {
    if((vp->perspective_br[0] != val->val_pos[0]) ||
       (vp->perspective_br[1] != val->val_pos[1]))
      {
      vp->perspective_br[0] = val->val_pos[0];
      vp->perspective_br[1] = val->val_pos[1];
      vp->changed = 1;
      }
    }

  }

static void connect_input_port_transform(void * priv,
                                    bg_read_video_func_t func,
                                    void * data, int stream, int port)
  {
  transform_t * vp;
  vp = (transform_t *)priv;

  if(!port)
    {
    vp->read_func = func;
    vp->read_data = data;
    vp->read_stream = stream;
    }
  
  }

static void set_input_format_transform(void * priv,
                                gavl_video_format_t * format, int port)
  {
  transform_t * vp;
  vp = (transform_t *)priv;

  if(!port)
    {
    gavl_video_format_copy(&vp->format, format);

    if(vp->frame)
      {
      gavl_video_frame_destroy(vp->frame);
      vp->frame = (gavl_video_frame_t*)0;
      }
    vp->sar = (double)format->pixel_width / (double)format->pixel_height;
    vp->changed = 1;
    }
  }

static void get_output_format_transform(void * priv,
                                 gavl_video_format_t * format)
  {
  transform_t * vp;
  vp = (transform_t *)priv;
  
  gavl_video_format_copy(format, &vp->format);
  }

static void transform_func_matrix(void * priv,
                                  double dst_x, double dst_y,
                                  double * src_x, double * src_y)
  {
  transform_t * vp;
  vp = (transform_t *)priv;
  *src_x = dst_x * vp->matrix[0][0] + dst_y * vp->matrix[0][1] + vp->matrix[0][2];
  *src_y = dst_x * vp->matrix[1][0] + dst_y * vp->matrix[1][1] + vp->matrix[1][2];
  }

static void matrixmult(double src1[2][3], double src2[2][3], double dst[2][3])
  {
  int i, j;

  for(i = 0; i < 2; i++)
    {
    for(j = 0; j < 3; j++)
      {
      dst[i][j] = 
        src1[i][0] * src2[0][j] +
        src1[i][1] * src2[1][j];
      }
    dst[i][2] += src1[i][2];
    }

  }

static void matrix_invert(double src[2][3], double dst[2][3])
  {
  double det;
  det = src[0][0] * src[1][1] - src[1][0] * src[0][1];

  if(det == 0.0)
    {
    dst[0][0] = 1.0;
    dst[0][1] = 0.0;
    dst[1][0] = 0.0;
    dst[1][1] = 1.0;
    dst[0][2] = 0.0;
    dst[1][2] = 0.0;
    return;
    }

  dst[0][0] = src[1][1] / det;
  dst[0][1] = -src[0][1] / det;
  dst[1][0] = -src[1][0] / det;
  dst[1][1] = src[0][0] / det;
  dst[0][2] = (src[0][1] * src[1][2] - src[0][2] * src[1][1]) / det;
  dst[1][2] = (src[0][2] * src[1][0] - src[0][0] * src[1][2]) / det;
  }

static void init_transform_matrix(transform_t * vp, double mat[2][3])
  {
  double mat1[2][3];
  double mat2[2][3];
  double mat3[2][3];
  
  /* Pixel coordinates -> Undistorted centered coordinates */
  mat1[0][0] = 1.0 * vp->sar;
  mat1[1][0] = 0.0;
  mat1[0][1] = 0.0;
  mat1[1][1] = 1.0;
  mat1[0][2] = -0.5 * vp->format.image_width * vp->sar;
  mat1[1][2] = -0.5 * vp->format.image_height;
  
  /* Undistorted centered coordinates -> Pixel coordinates */
  mat2[0][0] = 1.0 / vp->sar;
  mat2[1][0] = 0.0;
  mat2[0][1] = 0.0;
  mat2[1][1] = 1.0;
  mat2[0][2] = 0.5 * vp->format.image_width;
  mat2[1][2] = 0.5 * vp->format.image_height;
  
  matrixmult(mat, mat1, mat3);
  matrixmult(mat2, mat3, vp->matrix);
  
  
  }

static void init_matrix_rotate(transform_t * vp)
  {
  double mat[2][3];
  double sin_angle, cos_angle;

  sin_angle = sin(vp->rotate_angle / 180.0 * M_PI);
  cos_angle = cos(vp->rotate_angle / 180.0 * M_PI);


  mat[0][0] = cos_angle;
  mat[1][0] = sin_angle;
  mat[0][1] = -sin_angle;
  mat[1][1] = cos_angle;
  mat[0][2] = 0.0;
  mat[1][2] = 0.0;

  init_transform_matrix(vp, mat);
  
  }

static void init_matrix_affine(transform_t * vp)
  {
  double mat1[2][3];
  double mat2[2][3];
  mat1[0][0] = vp->affine_xx;
  mat1[1][0] = vp->affine_yx;
  mat1[0][1] = vp->affine_xy;
  mat1[1][1] = vp->affine_yy;
  mat1[0][2] = vp->affine_ox * vp->format.image_width;
  mat1[1][2] = vp->affine_oy * vp->format.image_height;

  matrix_invert(mat1, mat2);

  init_transform_matrix(vp, mat2);
  }

/* Perspective: Ported from the Gimp */


static void transform_func_matrix3(void * priv,
                                   double x,
                                   double y,
                                   double *newx,
                                   double *newy)
  {
  double  w;
  transform_t * vp;
  vp = (transform_t *)priv;
  w = vp->matrix3[2][0] * x + vp->matrix3[2][1] * y + vp->matrix3[2][2];
  
  if (w == 0.0)
    w = 1.0;
  else
    w = 1.0/w;

  *newx = (vp->matrix3[0][0] * x +
           vp->matrix3[0][1] * y +
           vp->matrix3[0][2]) * w;
  *newy = (vp->matrix3[1][0] * x +
           vp->matrix3[1][1] * y +
           vp->matrix3[1][2]) * w;
}

static void
matrix3_mult(double src1[3][3], double src2[3][3],
             double dst[3][3])
  {
  int         i, j;
  double      t1, t2, t3;

  for (i = 0; i < 3; i++)
    {
    t1 = src1[i][0];
    t2 = src1[i][1];
    t3 = src1[i][2];
    
    for (j = 0; j < 3; j++)
      {
      dst[i][j]  = t1 * src2[0][j];
      dst[i][j] += t2 * src2[1][j];
      dst[i][j] += t3 * src2[2][j];
      }
    }
  }

static void
transform_matrix_perspective(double matrix[3][3],
                             int width,
                             int height,
                             // double t_x1,
                             // double t_y1,
                             double tl[2],
                             // double t_x2,
                             // double t_y2,
                             double tr[2],
                             // double t_x3,
                             // double t_y3,
                             double bl[2],
                             // double t_x4,
                             // double t_y4,
                             double br[2]
                             )
  {
  double base[3][3];
  double trafo[3][3];
  double     scalex;
  double     scaley;
  
  scalex = scaley = 1.0;
  
  if (width > 0)
    scalex = 1.0 / (double) width;
  
  if (height > 0)
    scaley = 1.0 / (double) height;

  base[0][0] = scalex;
  base[0][1] = 0.0;
  base[0][2] = 0.0;
  base[1][0] = 0.0;
  base[1][1] = scaley;
  base[1][2] = 0.0;
  base[2][0] = 0.0;
  base[2][1] = 0.0;
  base[2][2] = 1.0;
    
  // gimp_matrix3_scale(matrix, scalex, scaley);
  
  /* Determine the perspective transform that maps from
   * the unit cube to the transformed coordinates
   */
    {
    double dx1, dx2, dx3, dy1, dy2, dy3;

    dx1 = tr[0] - br[0];
    dx2 = bl[0] - br[0];
    dx3 = tl[0] - tr[0] + br[0] - bl[0];

    dy1 = tr[1] - br[1];
    dy2 = bl[1] - br[1];
    dy3 = tl[1] - tr[1] + br[1] - bl[1];

    /*  Is the mapping affine?  */
    if ((dx3 == 0.0) && (dy3 == 0.0))
      {
      trafo[0][0] = tr[0] - tl[0];
      trafo[0][1] = br[0] - tr[0];
      trafo[0][2] = tl[0];
      trafo[1][0] = tr[1] - tl[1];
      trafo[1][1] = br[1] - tr[1];
      trafo[1][2] = tl[1];
      trafo[2][0] = 0.0;
      trafo[2][1] = 0.0;
      }
    else
      {
      double det1, det2;

      det1 = dx3 * dy2 - dy3 * dx2;
      det2 = dx1 * dy2 - dy1 * dx2;

      trafo[2][0] = (det2 == 0.0) ? 1.0 : det1 / det2;
      
      det1 = dx1 * dy3 - dy1 * dx3;
      
      trafo[2][1] = (det2 == 0.0) ? 1.0 : det1 / det2;
      
      trafo[0][0] = tr[0] - tl[0] + trafo[2][0] * tr[0];
      trafo[0][1] = bl[0] - tl[0] + trafo[2][1] * bl[0];
      trafo[0][2] = tl[0];
      
      trafo[1][0] = tr[1] - tl[1] + trafo[2][0] * tr[1];
      trafo[1][1] = bl[1] - tl[1] + trafo[2][1] * bl[1];
      trafo[1][2] = tl[1];
      }

    trafo[2][2] = 1.0;
  }
    
  matrix3_mult(trafo, base, matrix);
}

static double
matrix3_determinant(double matrix[3][3])
  {
  double determinant;

  determinant  = (matrix[0][0] *
                  (matrix[1][1] * matrix[2][2] -
                   matrix[1][2] * matrix[2][1]));
  determinant -= (matrix[1][0] *
                  (matrix[0][1] * matrix[2][2] -
                   matrix[0][2] * matrix[2][1]));
  determinant += (matrix[2][0] *
                  (matrix[0][1] * matrix[1][2] -
                   matrix[0][2] * matrix[1][1]));

  return determinant;
}


static void
matrix3_invert (double mat[3][3], double inv[3][3])
  {
  double     det;
  
  det = matrix3_determinant (mat);
  
  if (det == 0.0)
    return;
  
  det = 1.0 / det;
  
  inv[0][0] =   (mat[1][1] * mat[2][2] -
                 mat[1][2] * mat[2][1]) * det;

  inv[1][0] = - (mat[1][0] * mat[2][2] -
                 mat[1][2] * mat[2][0]) * det;

  inv[2][0] =   (mat[1][0] * mat[2][1] -
                 mat[1][1] * mat[2][0]) * det;

  inv[0][1] = - (mat[0][1] * mat[2][2] -
                 mat[0][2] * mat[2][1]) * det;

  inv[1][1] =   (mat[0][0] * mat[2][2] -
                 mat[0][2] * mat[2][0]) * det;

  inv[2][1] = - (mat[0][0] * mat[2][1] -
                 mat[0][1] * mat[2][0]) * det;

  inv[0][2] =   (mat[0][1] * mat[1][2] -
                 mat[0][2] * mat[1][1]) * det;

  inv[1][2] = - (mat[0][0] * mat[1][2] -
                 mat[0][2] * mat[1][0]) * det;

  inv[2][2] =   (mat[0][0] * mat[1][1] -
                 mat[0][1] * mat[1][0]) * det;

}


static void init_perspective(transform_t * vp)
  {
  double tl[2];
  double tr[2];
  double bl[2];
  double br[2];

  double mat[3][3];
  
  tl[0] = vp->perspective_tl[0] * vp->format.image_width;
  tl[1] = vp->perspective_tl[1] * vp->format.image_height;

  tr[0] = vp->perspective_tr[0] * vp->format.image_width;
  tr[1] = vp->perspective_tr[1] * vp->format.image_height;

  bl[0] = vp->perspective_bl[0] * vp->format.image_width;
  bl[1] = vp->perspective_bl[1] * vp->format.image_height;

  br[0] = vp->perspective_br[0] * vp->format.image_width;
  br[1] = vp->perspective_br[1] * vp->format.image_height;
  
  transform_matrix_perspective(mat,
                               vp->format.image_width,
                               vp->format.image_height,
                               // double t_x1,
                               // double t_y1,
                               tl,
                               // double t_x2,
                               // double t_y2,
                               tr,
                               // double t_x3,
                               // double t_y3,
                               bl,
                               // double t_x4,
                               // double t_y4,
                               br
                               );

  matrix3_invert(mat, vp->matrix3);
  }

static void init_transform(transform_t * vp)
  {
  gavl_image_transform_func func;
  switch(vp->mode)
    {
    case MODE_ROTATE:
      init_matrix_rotate(vp);
      func = transform_func_matrix;
      break;
    case MODE_AFFINE:
      init_matrix_affine(vp);
      func = transform_func_matrix;
      break;
    case MODE_PERSPECTIVE:
      init_perspective(vp);
      func = transform_func_matrix3;
    }

  gavl_video_options_set_scale_mode(vp->opt, vp->scale_mode);
  gavl_video_options_set_quality(vp->opt, vp->quality);
  
  gavl_image_transform_init(vp->transform, &vp->format,
                            func, vp);
  
  vp->changed = 0;
  }

static int read_video_transform(void * priv, gavl_video_frame_t * frame,
                         int stream)
  {
  transform_t * vp;
  vp = (transform_t *)priv;
  
  if(!vp->frame)
    {
    vp->frame = gavl_video_frame_create(&vp->format);
    gavl_video_frame_clear(vp->frame, &vp->format);
    }
  if(!vp->read_func(vp->read_data, vp->frame, vp->read_stream))
    return 0;

  if(vp->changed)
    init_transform(vp);
  
  gavl_video_frame_fill(frame, &vp->format, vp->bg_color);
  
  gavl_image_transform_transform(vp->transform, vp->frame, frame);

  gavl_video_frame_copy_metadata(frame, vp->frame);
  
  return 1;
  }

const bg_fv_plugin_t the_plugin = 
  {
    .common =
    {
      BG_LOCALE,
      .name =      "fv_transform",
      .long_name = TRS("Transform"),
      .description = TRS("Transform the image with different methods"),
      .type =     BG_PLUGIN_FILTER_VIDEO,
      .flags =    BG_PLUGIN_FILTER_1,
      .create =   create_transform,
      .destroy =   destroy_transform,
      .get_parameters =   get_parameters_transform,
      .set_parameter =    set_parameter_transform,
      .priority =         1,
    },
    
    .connect_input_port = connect_input_port_transform,
    
    .set_input_format = set_input_format_transform,
    .get_output_format = get_output_format_transform,

    .read_video = read_video_transform,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;