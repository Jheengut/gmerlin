/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
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

#include <config.h>
#include <upnp/devicepriv.h>
#include <upnp/mediaserver.h>
#include <string.h>

#include <gmerlin/utils.h>

/* Service actions */

#define ARG_SearchCaps        1
#define ARG_SortCaps          2
#define ARG_Id                3
#define ARG_ObjectID          4
#define ARG_BrowseFlag        5
#define ARG_Filter            6
#define ARG_StartingIndex     7
#define ARG_RequestedCount    8
#define ARG_SortCriteria      9
#define ARG_Result            10
#define ARG_NumberReturned    11
#define ARG_TotalMatches      12
#define ARG_UpdateID          13

static int GetSearchCapabilities(bg_upnp_service_t * s)
  {
  char * ret = calloc(1, 1);
  bg_upnp_service_set_arg_out_string(&s->req, ARG_SearchCaps, ret);
  return 0;
  }

static int GetSortCapabilities(bg_upnp_service_t * s)
  {
  char * ret = calloc(1, 1);
  bg_upnp_service_set_arg_out_string(&s->req, ARG_SortCaps, ret);
  return 0;
  }


static int GetSystemUpdateID(bg_upnp_service_t * s)
  {
  bg_upnp_service_set_arg_out_int(&s->req, ARG_Id, 1);
  return 0;
  }

/* DIDL stuff */

static xmlDocPtr didl_create()
  {
  
  xmlNsPtr upnp_ns;
  xmlNsPtr didl_ns;
  xmlNsPtr dc_ns;
  
  xmlDocPtr doc;  
  xmlNodePtr didl;
  
  doc = xmlNewDoc((xmlChar*)"1.0");
  didl = xmlNewDocRawNode(doc, NULL, (xmlChar*)"DIDL-Lite", NULL);
  xmlDocSetRootElement(doc, didl);

  dc_ns =
    xmlNewNs(didl,
             (xmlChar*)"http://purl.org/dc/elements/1.1/",
             (xmlChar*)"dc");

  upnp_ns =
    xmlNewNs(didl,
             (xmlChar*)"urn:schemas-upnp-org:metadata-1-0/upnp/",
             (xmlChar*)"upnp");
  didl_ns =
    xmlNewNs(didl,
             (xmlChar*)"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/",
             NULL);
  
  return doc;
  }

static xmlNodePtr didl_add_item(xmlDocPtr doc)
  {
  xmlNodePtr parent = bg_xml_find_next_doc_child(doc, NULL);
  xmlNodePtr node = xmlNewTextChild(parent, NULL, (xmlChar*)"item", NULL);
  return node;
  }

static xmlNodePtr didl_add_container(xmlDocPtr doc)
  {
  xmlNodePtr parent = bg_xml_find_next_doc_child(doc, NULL);
  xmlNodePtr node = xmlNewTextChild(parent, NULL, (xmlChar*)"container", NULL);
  return node;
  }

static void didl_add_property(xmlDocPtr doc,
                              xmlNodePtr node,
                              const char * ns,
                              const char * name,
                              const char * value)
  {
  xmlNsPtr xmlns = xmlSearchNs(doc, node, (const xmlChar *)ns);

  xmlNewTextChild(node, xmlns, (const xmlChar*)name,
                  (const xmlChar*)value);
  }

static void didl_set_class(xmlDocPtr doc,
                           xmlNodePtr node,
                           const char * klass)
  {
  didl_add_property(doc, node, "upnp", "class", klass);
  }

static void didl_set_title(xmlDocPtr doc,
                           xmlNodePtr node,
                           const char * title)
  {
  didl_add_property(doc, node, "dc", "title", title);
  }


static void didl_set_res(xmlDocPtr doc,
                         xmlNodePtr node,
                         void * f, const char * url_base)
  {
  bg_db_file_t * file = f;
  char * tmp_string;
  xmlNodePtr child;

  /* Location */
  tmp_string = bg_sprintf("%smedia?id=%"PRId64, url_base, bg_db_object_get_id(f));
  child = bg_xml_append_child_node(node, "res", tmp_string);
  free(tmp_string);

  /* Protocol info */
  tmp_string = bg_sprintf("http-get:*:%s:*", file->mimetype);
  BG_XML_SET_PROP(child, "protocolInfo", tmp_string);
  free(tmp_string);

  /* Size */
  tmp_string = bg_sprintf("%"PRId64, file->obj.size);
  BG_XML_SET_PROP(child, "size", tmp_string);
  free(tmp_string);
  }

typedef struct
  {
  const char * upnp_parent;
  xmlDocPtr doc;
  bg_upnp_device_t * dev;

  char ** filter;
  } query_t;

/* 
 *  UPNP IDs are build the following way:
 *
 *  object_id $ parent_id $ grandparent_id ...
 *  This was we can map the same db objects into arbitrary locations into the
 *  tree and they always have unique upnp IDs as required by the standard.
 */

static xmlNodePtr didl_add_object(xmlDocPtr didl, bg_db_object_t * obj, 
                                  const char * upnp_parent, const char * upnp_id,
                                  query_t * q)
  {
  const char * pos;
  char * tmp_string;
  xmlNodePtr node;
  bg_db_object_type_t type;
  type = bg_db_object_get_type(obj);

  if(type & (BG_DB_FLAG_CONTAINER|BG_DB_FLAG_VCONTAINER))
    node = didl_add_container(didl);
  else
    node = didl_add_item(didl);
  
  if(upnp_id)
    {
    BG_XML_SET_PROP(node, "id", upnp_id);
    pos = strchr(upnp_id, '$');
    if(pos)
      BG_XML_SET_PROP(node, "parentID", pos+1);
    else
      BG_XML_SET_PROP(node, "parentID", "-1");
    }
  else if(upnp_parent)
    {
    tmp_string = bg_sprintf("%"PRId64"$%s", obj->id, upnp_parent);
    BG_XML_SET_PROP(node, "id", tmp_string);
    free(tmp_string);
    BG_XML_SET_PROP(node, "parentID", upnp_parent);
    }
  /* Required */
  BG_XML_SET_PROP(node, "restricted", "1");

#if 1 
  if(type & (BG_DB_FLAG_CONTAINER|BG_DB_FLAG_VCONTAINER))
    {
    /* Optional */
    tmp_string = bg_sprintf("%d", obj->children);
    BG_XML_SET_PROP(node, "childCount", tmp_string);
    free(tmp_string);
    }
#endif
  switch(type)
    {
    case BG_DB_OBJECT_AUDIO_FILE:
      {
      bg_db_audio_file_t * o = (bg_db_audio_file_t *)obj;
      if(o->title)
        didl_set_title(didl, node,  o->title);
      else
        didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.item.audioItem.musicTrack");

      didl_add_property(didl, node, "upnp", "artist", o->artist);
      didl_add_property(didl, node, "upnp", "genre", o->genre);
      didl_add_property(didl, node, "upnp", "album", o->album);
      didl_set_res(didl, node, obj, q->dev->url_base);
      }
      break;
    case BG_DB_OBJECT_VIDEO_FILE:
    case BG_DB_OBJECT_PHOTO:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.item");
      break;
    case BG_DB_OBJECT_ROOT:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.container");
      break;
    case BG_DB_OBJECT_AUDIO_ALBUM:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.container.album.musicAlbum");
      break;
    case BG_DB_OBJECT_CONTAINER:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.container");
      break;
    case BG_DB_OBJECT_DIRECTORY:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));

      didl_set_class(didl, node,  "object.container.storageFolder");
      // didl_set_class(didl, node,  "object.container");
#if 0
      /* Optional */
      tmp_string = bg_sprintf("%"PRId64, obj->size);
      didl_add_property(didl, node, "upnp", "storageUsed", tmp_string);
      free(tmp_string);
#endif
      break;
    case BG_DB_OBJECT_PLAYLIST:
    case BG_DB_OBJECT_VFOLDER:
    case BG_DB_OBJECT_VFOLDER_LEAF:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.container");
      break;
    /* The following objects should never get returned */
    case BG_DB_OBJECT_OBJECT: 
    case BG_DB_OBJECT_FILE:
    case BG_DB_OBJECT_IMAGE_FILE:
    case BG_DB_OBJECT_ALBUM_COVER:
    case BG_DB_OBJECT_VIDEO_PREVIEW:
    case BG_DB_OBJECT_MOVIE_POSTER:
    case BG_DB_OBJECT_THUMBNAIL:
      didl_set_title(didl, node,  bg_db_object_get_label(obj));
      didl_set_class(didl, node,  "object.item");
      break;
    }

  return NULL;
  }


static void query_callback(void * priv, void * obj)
  {
  query_t * q = priv;
  didl_add_object(q->doc, obj, q->upnp_parent, NULL, q);
  }


char * def_result =
  "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
  " xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
  " xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
  "<container id=\"1\" parentID=\"0\" childCount=\"2\" restricted=\"1\">\n"
  "<dc:title>My Music</dc:title>\n"
  "<upnp:class>object.container.storageFolder</upnp:class>\n"
  "<dc:storageUsed>100000000</dc:storageUsed>\n"
  "<upnp:writeStatus>PROTECTED</upnp:writeStatus>\n"
  "</container>\n"
  "</DIDL-Lite>";

char * def_root =
  "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
  " xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
  " xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
  "<container id=\"0\" parentID=\"-1\" childCount=\"1\" restricted=\"1\">\n"
  "<dc:title>Root</dc:title>\n"
  "<upnp:class>object.container</upnp:class>\n"
  "</container>\n"
  "</DIDL-Lite>";


static int Browse(bg_upnp_service_t * s)
  {
  bg_mediaserver_t * priv;
  int64_t id;
  char * ret;  
  query_t q;
  const char * ObjectID = bg_upnp_service_get_arg_in_string(&s->req, ARG_ObjectID);
  const char * BrowseFlag = bg_upnp_service_get_arg_in_string(&s->req, ARG_BrowseFlag);
  const char * Filter = bg_upnp_service_get_arg_in_string(&s->req, ARG_Filter);
  int StartingIndex = bg_upnp_service_get_arg_in_int(&s->req, ARG_StartingIndex);
  int RequestedCount = bg_upnp_service_get_arg_in_int(&s->req, ARG_RequestedCount);

  int NumberReturned;
  int TotalMatches = 1;
  
  priv = s->dev->priv;
  
  fprintf(stderr, "Browse: Id: %s, Flag: %s, Filter: %s, Start: %d, Num: %d\n",
          ObjectID, BrowseFlag, Filter, StartingIndex, RequestedCount);

  q.doc = didl_create();
  q.dev = s->dev;

  if(strcmp(Filter, "*"))
    q.filter = bg_strbreak(Filter, ',');
  
  id = strtoll(ObjectID, NULL, 10);
  
  if(!strcmp(BrowseFlag, "BrowseMetadata"))
    {
    bg_db_object_t * obj =
      bg_db_object_query(priv->db, id);
    
    didl_add_object(q.doc, obj, NULL, ObjectID, &q);
    bg_db_object_unref(obj);
    NumberReturned = 1;
    TotalMatches = 1;
    }
  else
    {
    q.upnp_parent = ObjectID;
    NumberReturned =
      bg_db_query_children(priv->db, id,
                           query_callback, &q,
                           StartingIndex, RequestedCount, &TotalMatches);
    }
  
  
  ret = bg_xml_save_to_memory(q.doc);
  
  bg_upnp_service_set_arg_out_int(&s->req, ARG_NumberReturned, NumberReturned);
  bg_upnp_service_set_arg_out_int(&s->req, ARG_TotalMatches, TotalMatches);
  
  fprintf(stderr, "didl-test:\n%s\n", ret);
  
  xmlFreeDoc(q.doc);

  if(q.filter)
    bg_strbreak_free(q.filter);
  
  bg_upnp_service_set_arg_out_string(&s->req, ARG_Result, ret);
  bg_upnp_service_set_arg_out_int(&s->req, ARG_UpdateID, 0);

  //  fprintf(stderr, "didl:\n%s\n", ret);
  return 1;
  }


/* Initialize service description */

static void init_service_desc(bg_upnp_service_desc_t * d)
  {
  bg_upnp_sv_val_t  val;
  bg_upnp_sv_desc_t * sv;
  bg_upnp_sa_desc_t * sa;

  /*
    bg_upnp_service_desc_add_sv(d, "TransferIDs",
                                BG_UPNP_SV_EVENT,
                                BG_UPNP_SV_STRING);

   */
  
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_ObjectID",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_Result",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  //  Optional
  //  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_SearchCriteria",
  //                              BG_UPNP_SV_ARG_ONLY,
  //                              BG_UPNP_SV_STRING);

  sv = bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_BrowseFlag",
                                   BG_UPNP_SV_ARG_ONLY,
                                   BG_UPNP_SV_STRING);

  val.s = "BrowseMetadata";
  bg_upnp_sv_desc_add_allowed(sv, &val);
  val.s = "BrowseDirectChildren";
  bg_upnp_sv_desc_add_allowed(sv, &val);
  
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_Filter",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_SortCriteria",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_Index",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_INT4);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_Count",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_INT4);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_UpdateID",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_INT4);
  /*
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_TransferID",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_INT4);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_TransferStatus",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_TransferLength",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_TransferTotal",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_TagValueList",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "A_ARG_TYPE_URI",
                              BG_UPNP_SV_ARG_ONLY,
                              BG_UPNP_SV_STRING);
  */

  bg_upnp_service_desc_add_sv(d, "SearchCapabilities",
                              0, BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "SortCapabilities",
                              0, BG_UPNP_SV_STRING);
  bg_upnp_service_desc_add_sv(d, "SystemUpdateID",
                              BG_UPNP_SV_EVENT, BG_UPNP_SV_INT4);
  /*
  bg_upnp_service_desc_add_sv(d, "ContainerUpdateIDs",
                              BG_UPNP_SV_EVENT, BG_UPNP_SV_STRING);
  */

  /* Actions */

  sa = bg_upnp_service_desc_add_action(d, "GetSearchCapabilities",
                                       GetSearchCapabilities);

  bg_upnp_sa_desc_add_arg_out(sa, "SearchCaps",
                              "SearchCapabilities", 0,
                              ARG_SearchCaps);

  sa = bg_upnp_service_desc_add_action(d, "GetSortCapabilities",
                                       GetSortCapabilities);
  bg_upnp_sa_desc_add_arg_out(sa, "SortCaps",
                              "SortCapabilities", 0,
                              ARG_SortCaps);
  
  sa = bg_upnp_service_desc_add_action(d, "GetSystemUpdateID",
                                       GetSystemUpdateID);

  bg_upnp_sa_desc_add_arg_out(sa, "Id",
                              "SystemUpdateID", 0,
                              ARG_Id);

  sa = bg_upnp_service_desc_add_action(d, "Browse", Browse);
  bg_upnp_sa_desc_add_arg_in(sa, "ObjectID",
                             "A_ARG_TYPE_ObjectID", 0,
                             ARG_ObjectID);
  bg_upnp_sa_desc_add_arg_in(sa, "BrowseFlag",
                             "A_ARG_TYPE_BrowseFlag", 0,
                             ARG_BrowseFlag);
  bg_upnp_sa_desc_add_arg_in(sa, "Filter",
                             "A_ARG_TYPE_Filter", 0,
                             ARG_Filter);
  bg_upnp_sa_desc_add_arg_in(sa, "StartingIndex",
                             "A_ARG_TYPE_Index", 0,
                             ARG_StartingIndex);
  bg_upnp_sa_desc_add_arg_in(sa, "RequestedCount",
                             "A_ARG_TYPE_Count", 0,
                             ARG_RequestedCount);
  bg_upnp_sa_desc_add_arg_in(sa, "SortCriteria",
                             "A_ARG_TYPE_SortCriteria", 0,
                             ARG_SortCriteria);
  
  bg_upnp_sa_desc_add_arg_out(sa, "Result",
                              "A_ARG_TYPE_Result", 0,
                              ARG_Result);
  bg_upnp_sa_desc_add_arg_out(sa, "NumberReturned",
                              "A_ARG_TYPE_Count", 0,
                              ARG_NumberReturned);
  bg_upnp_sa_desc_add_arg_out(sa, "TotalMatches",
                              "A_ARG_TYPE_Count", 0,
                              ARG_TotalMatches);
  bg_upnp_sa_desc_add_arg_out(sa, "UpdateID",
                              "A_ARG_TYPE_UpdateID", 0,
                              ARG_UpdateID);
  
  /*

    sa = bg_upnp_service_desc_add_action(d, "Search");
    sa = bg_upnp_service_desc_add_action(d, "CreateObject");
    sa = bg_upnp_service_desc_add_action(d, "DestroyObject");
    sa = bg_upnp_service_desc_add_action(d, "UpdateObject");
    sa = bg_upnp_service_desc_add_action(d, "ImportResource");
    sa = bg_upnp_service_desc_add_action(d, "ExportResource");
    sa = bg_upnp_service_desc_add_action(d, "StopTransferResource");
    sa = bg_upnp_service_desc_add_action(d, "GetTransferProgress");
  */

  
  
  }


void bg_upnp_service_init_content_directory(bg_upnp_service_t * ret,
                                            const char * name,
                                            bg_db_t * db)
  {
  bg_upnp_service_init(ret, name, "ContentDirectory", 1);
  init_service_desc(&ret->desc);
  bg_upnp_service_start(ret);
  }