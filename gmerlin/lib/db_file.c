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
#include <unistd.h>

#include <mediadb_private.h>
#include <gmerlin/log.h>
#include <gmerlin/utils.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


#define LOG_DOMAIN "db.file"

static bg_db_file_t * file_scan_internal(const char * directory,
                                         bg_db_file_t * files,
                                         int * num_p, int * alloc_p)
  {
  char * filename;
  DIR * d;
  struct dirent * e;
  struct stat st;
  bg_db_file_t * file;
  int num = *num_p;
  int alloc = *alloc_p;
  
  d = opendir(directory);
  if(!d)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Opening directory %s failed: %s",
           directory, strerror(errno));
    return files;
    }

  while((e = readdir(d)))
    {
    /* Skip hidden files and "." and ".." */
    if(e->d_name[0] == '.')
      continue;
    
    filename = bg_sprintf("%s/%s", directory, e->d_name);
    if(!stat(filename, &st))
      {
      /* Check for directory */
      if(S_ISDIR(st.st_mode))
        {
        files = file_scan_internal(filename,
                                   files, &num, &alloc);
        }
      else if(S_ISREG(st.st_mode))
        {
        /* Add file to list */
        if(num + 2 > alloc)
          {
          alloc += 256;
          files = realloc(files, alloc * sizeof(*files));
          memset(files + num, 0,
                 (alloc - num) * sizeof(*files));
          }
        file = files + num;

        file->path = filename;
        filename = NULL;
        file->size = st.st_size;
        file->mtime = st.st_mtime;
        num += 1;
        }
      }
    if(filename)
      free(filename);
    }
  
  closedir(d);
  *num_p = num;
  *alloc_p = alloc;
  return files;
  }

bg_db_file_t * bg_db_file_scan_directory(const char * directory,
                                         int * num)
  {
  bg_db_file_t * ret = NULL;
  int num_ret = 0;
  int ret_alloc = 0;
  ret = file_scan_internal(directory, ret, &num_ret, &ret_alloc);
  *num = num_ret;
  return ret;
  }

void bg_db_add_files(bg_db_t * db, bg_db_file_t * file, int num, bg_db_dir_t * dir)
  {
  
  }

void bg_db_update_files(bg_db_t * db, bg_db_file_t * file, int num, bg_db_dir_t * dir)
  {
  
  }