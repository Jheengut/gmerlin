/*****************************************************************
 
  qt_atom.c
 
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

#include <qt.h>
/*
  
  uint64_t size;
  uint64_t start_position; 
  uint32_t fourcc;
  uint8_t version;
  uint32_t flags;
*/



int bgav_qt_atom_read_header(bgav_input_context_t * input,
                             qt_atom_header_t * h)
  {
  uint32_t tmp_32;
  
  h->start_position = input->position;
  
  if(!bgav_input_read_32_be(input, &tmp_32))
    return 0;

  h->size = tmp_32;

  if(!bgav_input_read_fourcc(input, &(h->fourcc)))
    return 0;

  //  fprintf(stderr, "Read atom ");
  //  bgav_dump_fourcc(h->fourcc);
    
  if(tmp_32 == 1) /* 64 bit atom */
    {
    if(!bgav_input_read_64_be(input, &(h->size)))
      return 0;
    //    fprintf(stderr, " (64 bit)");
    }
  //  fprintf(stderr, "\n");
  return 1;
  }

void bgav_qt_atom_skip(bgav_input_context_t * input,
                       qt_atom_header_t * h)
  {
  bgav_input_skip(input, h->size - (input->position - h->start_position));
  }


void bgav_qt_atom_dump_header(qt_atom_header_t * h)
  {
  fprintf(stderr, "Size:           %lld\n", h->size);
  fprintf(stderr, "Start Position: %lld\n", h->start_position);
  fprintf(stderr, "Fourcc:         ");
  bgav_dump_fourcc(h->fourcc);
  fprintf(stderr, "\n");
  }
