/*****************************************************************
 
  charset.c
 
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

#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include <avdec_private.h>

#define BYTES_INCREMENT 10

static char * do_convert(iconv_t cd, char * in_string, int len)
  {
  char * ret;

  char *inbuf;
  char *outbuf;
  int alloc_size;
  int output_pos;
  
  size_t inbytesleft;
  size_t outbytesleft;

  alloc_size = len + BYTES_INCREMENT;

  inbytesleft  = len;
  outbytesleft = alloc_size;

  ret    = malloc(alloc_size);
  inbuf  = in_string;
  outbuf = ret;
  
  while(1)
    {
    if(iconv(cd, &inbuf, &inbytesleft,
             &outbuf, &outbytesleft) == (size_t)-1)
      {
      switch(errno)
        {
        case E2BIG:
          output_pos = (int)(outbuf - ret);

          alloc_size   += BYTES_INCREMENT;
          outbytesleft += BYTES_INCREMENT;

          ret = realloc(ret, alloc_size);
          outbuf = &ret[output_pos];
          break;
        case EILSEQ:
          fprintf(stderr, "Invalid Multibyte sequence\n");
          break;
        case EINVAL:
          fprintf(stderr, "Incomplete Multibyte sequence\n");
          break;
        }
      }
    if(!inbytesleft)
      break;
    }
  /* Zero terminate */

  output_pos = (int)(outbuf - ret);
  
  if(!outbytesleft)
    {
    alloc_size++;
    ret = realloc(ret, alloc_size);
    outbuf = &ret[output_pos];
    }
  *outbuf = '\0';
  return ret;
  }

char * bgav_convert_string(const char * str, int len,
                           char * in_charset, char * out_charset)
  {
  iconv_t cd;
  char * ret;
  char * tmp_string;

  if(len < 0)
    len = strlen(str);

  tmp_string = malloc(len+1);
  memcpy(tmp_string, str, len);
  tmp_string[len] = '\0';

  cd = iconv_open(out_charset, in_charset);
  ret = do_convert(cd, tmp_string, len);
  iconv_close(cd);
  free(tmp_string);
  return ret;
  }

