#ifndef SQLITE3_H
#define SQLITE3_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

#define SQLITE_OK           0
#define SQLITE_ERROR        1
#define SQLITE_ROW          100
#define SQLITE_DONE         101
#define SQLITE_BUSY         5

int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_close(sqlite3 *db);
int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte,
                       sqlite3_stmt **ppStmt, const char **pzTail);
int sqlite3_step(sqlite3_stmt *pStmt);
int sqlite3_finalize(sqlite3_stmt *pStmt);
const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol);
const void *sqlite3_column_blob(sqlite3_stmt *pStmt, int iCol);
int sqlite3_column_bytes(sqlite3_stmt *pStmt, int iCol);
const char *sqlite3_errmsg(sqlite3 *db);
int sqlite3_exec(sqlite3 *db, const char *sql,
                 int (*callback)(void*,int,char**,char**),
                 void *arg, char **errmsg);
int sqlite3_busy_timeout(sqlite3 *db, int ms);

#ifdef __cplusplus
}
#endif

#endif
