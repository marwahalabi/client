/*
 * cynapses libc functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file csync_private.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncInternalAPI csync internal API
 *
 * @{
 */

#ifndef _CSYNC_PRIVATE_H
#define _CSYNC_PRIVATE_H

#include <unordered_map>
#include <QHash>
#include <stdint.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <map>

#include "common/syncjournaldb.h"
#include "config_csync.h"
#include "std/c_lib.h"
#include "std/c_private.h"
#include "csync.h"
#include "csync_misc.h"

#include "csync_macros.h"

#include <QRegularExpression>

/**
 * How deep to scan directories.
 */
#define MAX_DEPTH 100

#define CSYNC_STATUS_INIT 1 << 0
#define CSYNC_STATUS_UPDATE 1 << 1
#define CSYNC_STATUS_RECONCILE 1 << 2
#define CSYNC_STATUS_PROPAGATE 1 << 3

#define CSYNC_STATUS_DONE (CSYNC_STATUS_INIT | \
                           CSYNC_STATUS_UPDATE | \
                           CSYNC_STATUS_RECONCILE | \
                           CSYNC_STATUS_PROPAGATE)

enum csync_replica_e {
  LOCAL_REPLICA,
  REMOTE_REPLICA
};

/**
 * @brief csync public structure
 */
struct OCSYNC_EXPORT csync_s {
  struct FileMapHash { uint operator()(const QByteArray &a) const { return qHash(a); } };
  class FileMap : public std::unordered_map<QByteArray, std::unique_ptr<csync_file_stat_t>, FileMapHash> {
  public:
      csync_file_stat_t *findFile(const QByteArray &key) const {
          auto it = find(key);
          return it != end() ? it->second.get() : nullptr;
      }
  };

  struct {
      csync_auth_callback auth_function = nullptr;
      void *userdata = nullptr;
      csync_update_callback update_callback = nullptr;
      void *update_callback_userdata = nullptr;

      /* hooks for checking the white list (uses the update_callback_userdata) */
      int (*checkSelectiveSyncBlackListHook)(void*, const QByteArray &) = nullptr;
      int (*checkSelectiveSyncNewFolderHook)(void *, const QByteArray & /* path */, OCC::RemotePermissions) = nullptr;


      csync_vio_opendir_hook remote_opendir_hook = nullptr;
      csync_vio_readdir_hook remote_readdir_hook = nullptr;
      csync_vio_closedir_hook remote_closedir_hook = nullptr;
      void *vio_userdata = nullptr;

      /* hook for comparing checksums of files during discovery */
      csync_checksum_hook checksum_hook = nullptr;
      void *checksum_userdata = nullptr;

  } callbacks;

  OCC::SyncJournalDb *statedb;

  c_strlist_t *excludes = nullptr; /* list of individual patterns collected from all exclude files */
  struct TraversalExcludes {
      void prepare(c_strlist_t *excludes);

      QRegularExpression regexp_exclude;
      c_strlist_t *list_patterns_with_slashes = nullptr;
      /* FIXME ^^ at a later point use QRegularExpression too if those become popular */
  } parsed_traversal_excludes;

  struct {
    std::map<QByteArray, QByteArray> folder_renamed_to; // map from->to
    std::map<QByteArray, QByteArray> folder_renamed_from; // map to->from
  } renames;

  struct {
    char *uri = nullptr;
    FileMap files;
  } local;

  struct {
    FileMap files;
    bool read_from_db = false;
    OCC::RemotePermissions root_perms; /* Permission of the root folder. (Since the root folder is not in the db tree, we need to keep a separate entry.) */
  } remote;

  /* replica we are currently walking */
  enum csync_replica_e current = LOCAL_REPLICA;

  /* Used in the update phase so changes in the sub directories can be notified to
     parent directories */
  csync_file_stat_t *current_fs = nullptr;

  /* csync error code */
  enum csync_status_codes_e status_code = CSYNC_STATUS_OK;

  char *error_string = nullptr;

  int status = CSYNC_STATUS_INIT;
  volatile bool abort = false;

  /**
   * Specify if it is allowed to read the remote tree from the DB (default to enabled)
   */
  bool read_remote_from_db = false;

  bool ignore_hidden_files = true;

  csync_s(const char *localUri, OCC::SyncJournalDb *statedb);
  ~csync_s();
  int reinitialize();

  // For some reason MSVC references the copy constructor and/or the assignment operator
  // if a class is exported. This is a problem since unique_ptr isn't copyable.
  // Explicitly disable them to fix the issue.
  // https://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/e39ab33d-1aaf-4125-b6de-50410d9ced1d
  csync_s(const csync_s &) = delete;
  csync_s &operator=(const csync_s &) = delete;
};

/*
 * context for the treewalk function
 */
struct _csync_treewalk_context_s
{
    csync_treewalk_visit_func *user_visitor;
    int instruction_filter;
    void *userdata;
};
typedef struct _csync_treewalk_context_s _csync_treewalk_context;

void set_errno_from_http_errcode( int err );

/**
 * }@
 */
#endif /* _CSYNC_PRIVATE_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
