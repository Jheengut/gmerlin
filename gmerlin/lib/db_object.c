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

#include <mediadb_private.h>
#include <gmerlin/log.h>
#include <gmerlin/utils.h>
#include <string.h>


#define LOG_DOMAIN "db.object"

    /*
  int64_t id;
  bg_db_object_type_t type;
  
  int64_t ref_id;
  int64_t parent_id;
  int num_children;  // -1 for non-containers

  int64_t size;
  gavl_time_t duration;
  char * label;
    */

void bg_db_object_init(bg_db_object_storage_t * obj)
  {
  memset(obj, 0, sizeof(*obj));
  obj->obj.id   = -1;
  }

const bg_db_object_class_t * bg_db_object_get_class(bg_db_object_type_t t)
  {
  switch(t)
    {
    case BG_DB_OBJECT_OBJECT:
      return NULL;
      break;
    case BG_DB_OBJECT_FILE:
      return &bg_db_file_class;
      break;
    case BG_DB_OBJECT_AUDIO_FILE:
      return &bg_db_audio_file_class;
      break;
    case BG_DB_OBJECT_VIDEO_FILE:
      break;
    case BG_DB_OBJECT_PHOTO_FILE:
      break;
    case BG_DB_OBJECT_CONTAINER:
      break;
    case BG_DB_OBJECT_DIRECTORY: 
      return &bg_db_dir_class;
      break;
    case BG_DB_OBJECT_PLAYLIST:
      break;
    case BG_DB_OBJECT_AUDIO_ALBUM:
      return &bg_db_audio_album_class;
      break;
    }
  return NULL;
  }

void bg_object_set_size(void * obj, int64_t size)
  {
  bg_db_object_t * o = obj;
  o->size = size;
  }

void bg_object_set_duration(void * obj, gavl_time_t d)
  {
  bg_db_object_t * o = obj;
  o->duration = duration;
  }

int64_t bg_object_get_id(void * obj)
  {
  bg_db_object_t * o = obj;
  return o->id;
  }



void bg_object_set_type(void * obj, bg_db_object_type_t t)
  {
  bg_db_object_t * o = obj;
  const bg_db_object_class_t * c = bg_db_object_get_class(t);
  if(c->parent != o->klass)
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "bg_object_set_type failed");
  o->klass = c;
  o->type = t;
  }

void bg_db_object_create(bg_db_t * db, bg_db_object_t * obj, int64_t parent_id)
  {
  obj->parent_id = parent_id;
  obj->id = bg_sqlite_get_next_id(db->db, "OBJECTS");
  }

static int object_query_callback(void * data, int argc, char **argv, char **azColName)
  {
  int i;
  bg_db_object_t * ret = data;
  
  for(i = 0; i < argc; i++)
    {
    BG_DB_SET_QUERY_INT("ID",          id);
    BG_DB_SET_QUERY_INT("TYPE",        type);
    BG_DB_SET_QUERY_INT("REF_ID",      ref_id);
    BG_DB_SET_QUERY_INT("PARENT_ID",   parent_id);
    BG_DB_SET_QUERY_INT("CHILDREN",    children);
    BG_DB_SET_QUERY_INT("SIZE",        size);
    BG_DB_SET_QUERY_INT("DURATION",    duration);
    BG_DB_SET_QUERY_STRING("LABEL",    label);
    }
  ret->found = 1;
  return 0;
  }

int bg_db_object_query(bg_db_t * db, bg_db_object_t * obj)
  {
  char * sql;
  int result;

  if(obj->flags & BG_DB_OBJ_FLAG_QUERIED)
    return 1;
  
  obj->found = 0;
  sql = sqlite3_mprintf("select * from OBJECTS where ID = %"PRId64";",
                        obj->id);
  result = bg_sqlite_exec(db->db, sql, object_query_callback, obj);
  sqlite3_free(sql);
  if(!result || !obj->found)
    return 0;

  obj->flags |= BG_DB_OBJ_FLAG_QUERIED;
  
  return 1;
  }

static void add_to_parent(bg_db_t * db, bg_db_object_t * obj)
  {
  bg_db_object_t obj1;
  if(obj->parent_id > 0)
    {
    bg_db_object_init(&obj1);
    obj1.id = obj->parent_id;
    if(bg_db_object_query(db, &obj1))
      {
      obj1.children++;

      if(obj->size > 0)
        obj1.size += obj->size;

      if(obj->duration > 0)
        obj1.duration += obj->duration;
      }
    bg_db_object_update(db, &obj1);
    }
  }

static void delete_from_parent(bg_db_t * db, bg_db_object_t * obj)
  {
  bg_db_object_t obj1;

  if(obj->parent_id > 0)
    {
    bg_db_object_init(&obj1);
    obj1.id = obj->parent_id;
    if(bg_db_object_query(db, &obj1))
      {
      obj1.children--;

      if(!obj1.children && (obj1.type & BG_DB_FLAG_NO_EMPTY))
        bg_db_object_delete(db, &obj1);
      else
        {
        if(obj->size > 0)
          obj1.size -= obj->size;

        if(obj->duration > 0)
          obj1.duration -= obj->duration;
        }
      bg_db_object_free(&obj1);
      }
    }
  }
  
void bg_db_object_add(bg_db_t * db, bg_db_object_t * obj)
  {
  char * sql;
  int result;
  
  sql = sqlite3_mprintf("INSERT INTO OBJECTS "
                        "( ID, TYPE, REF_ID, PARENT_ID, CHILDREN, SIZE, DURATION, LABEL ) "
                        "VALUES "
                        "( %"PRId64", %d, %"PRId64", %"PRId64", %d, %"PRId64", %"PRId64", %Q);",
                        obj->id, obj->type, obj->ref_id, obj->parent_id, obj->children, obj->size,
                        obj->duration, obj->label);
  result = bg_sqlite_exec(db->db, sql, NULL, NULL);
  sqlite3_free(sql);

  /* Add to parent container */
  add_to_parent(db, obj);
  }

void bg_db_object_update(bg_db_t * db, bg_db_object_t * obj)
  {
  char * sql;
  int result;


  sql = sqlite3_mprintf("UPDATE OBJECTS SET CHILDREN = %d, SIZE = %"PRId64", DURATION = %"PRId64", LABEL = %Q   WHERE ID = %"PRId64";",
                        obj->children, obj->size,
                        obj->duration, obj->label, obj->id);
  result = bg_sqlite_exec(db->db, sql, NULL, NULL);
  sqlite3_free(sql);
  }

void bg_db_object_delete(bg_db_t * db, bg_db_object_t * obj)
  {
  bg_sqlite_id_tab_t  tab;
  bg_db_object_t obj1;
  int i;
  char * sql;
  
  bg_sqlite_id_tab_init(&tab);
  
  /* Delete referenced objects and children */
  sql =
    sqlite3_mprintf("select ID from OBJECTS where (REF_ID = %"PRId64" | PARENT_ID = %"PRId64");",
                    obj->id, obj->id);
  
  for(i = 0; i < tab.num_val; i++)
    {
    bg_db_object_init(&obj1);
    obj1.id = tab.val[i];
    if(!bg_db_object_query(db, &obj1))
      continue;

    if(obj1.parent_id == obj->id)
      obj1.flags |= BG_DB_OBJ_FLAG_DONT_PROPAGATE;
    
    bg_db_object_delete(db, &obj1);

    if(obj1.parent_id == obj->id)
      obj1.flags &= BG_DB_OBJ_FLAG_DONT_PROPAGATE;
    
    bg_db_object_free(&obj1);
    }

  /* Remove from parent container */
  if(!(obj->flags & BG_DB_OBJ_FLAG_DONT_PROPAGATE))
    delete_from_parent(db, obj);

  bg_sqlite_delete_by_id(db->db, "OBJECTS", obj->id);
  }

void bg_db_object_free(bg_db_object_t * obj)
  {
  if(obj->label)
    free(obj->label);
  }