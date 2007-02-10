/*****************************************************************
 
  chapterlist_xml.c
 
  Copyright (c) 2003-2006 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
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

#include <parameter.h>
#include <streaminfo.h>
#include <utils.h>

#define CHAPTER_KEY "CHAPTER"
#define CHAPTERS_KEY "CHAPTERS"
#define NAME_KEY    "name"

void bg_chapter_list_2_xml(bg_chapter_list_t * list, xmlNodePtr xml_list)
  {
  char * tmp_string;
  int i;
  xmlNodePtr xml_chapter;

  xmlAddChild(xml_list, BG_XML_NEW_TEXT("\n"));
  
  for(i = 0; i < list->num_chapters; i++)
    {
    xml_chapter = xmlNewTextChild(xml_list, (xmlNsPtr)0,
                                  (xmlChar*)CHAPTER_KEY, NULL);
    
    if(list->chapters[i].name)
      BG_XML_SET_PROP(xml_chapter, NAME_KEY, list->chapters[i].name);

    tmp_string = bg_sprintf("%" PRId64, list->chapters[i].time);
    xmlAddChild(xml_chapter, BG_XML_NEW_TEXT(tmp_string));
    free(tmp_string);
    xmlAddChild(xml_list, BG_XML_NEW_TEXT("\n"));
    }
  
  }

bg_chapter_list_t *
bg_xml_2_chapter_list(xmlDocPtr xml_doc, xmlNodePtr xml_list)
  {
  int index;
  bg_chapter_list_t * ret;
  char * tmp_string;
  gavl_time_t time;
  xmlNodePtr node;
  
  ret = bg_chapter_list_create(0);
  
  node = xml_list->children;
  index = 0;
  while(node)
    {
    if(node->name && !BG_XML_STRCMP(node->name, CHAPTER_KEY))
      {
      tmp_string = (char*)xmlNodeListGetString(xml_doc, node->children, 1);
      sscanf(tmp_string, "%" PRId64, &time);
      xmlFree(tmp_string);

      tmp_string = BG_XML_GET_PROP(node, NAME_KEY);
      
      bg_chapter_list_insert(ret, index,
                             time, tmp_string);
      if(tmp_string)
        xmlFree(tmp_string);
      index++;
      }
    node = node->next;
    }
  return ret;
  }

void bg_chapter_list_save(bg_chapter_list_t * list, const char * filename)
  {
  xmlDocPtr  xml_doc;
  xmlNodePtr  xml_list;

  xml_doc = xmlNewDoc((xmlChar*)"1.0");
  xml_list = xmlNewDocRawNode(xml_doc, NULL, (xmlChar*)CHAPTERS_KEY, NULL);
  xmlDocSetRootElement(xml_doc, xml_list);
  
  bg_chapter_list_2_xml(list, xml_list);
  xmlSaveFile(filename, xml_doc);
  xmlFreeDoc(xml_doc);
  }

bg_chapter_list_t * bg_chapter_list_load(const char * filename)
  {
  xmlDocPtr xml_doc;
  xmlNodePtr node;

  bg_chapter_list_t * ret;

  xml_doc = xmlParseFile(filename);

  if(!xml_doc)
    return (bg_chapter_list_t *)0;

  node = xml_doc->children;

  if(BG_XML_STRCMP(node->name, CHAPTERS_KEY))
    {
    xmlFreeDoc(xml_doc);
    return (bg_chapter_list_t *)0;
    }

  ret = bg_xml_2_chapter_list(xml_doc, node);

  xmlFreeDoc(xml_doc);
  return ret;
  }
