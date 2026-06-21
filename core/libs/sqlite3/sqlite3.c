#include "sqlite3.h"
#include <stdlib.h>

int sqlite3_open(const char *filename, sqlite3 **ppDb) {
    (void)filename; (void)ppDb;
    return SQLITE_ERROR;
}

int sqlite3_close(sqlite3 *db) {
    (void)db;
    return SQLITE_ERROR;
}

int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte,
                       sqlite3_stmt **ppStmt, const char **pzTail) {
    (void)db; (void)sql; (void)nByte; (void)ppStmt; (void)pzTail;
    return SQLITE_ERROR;
}

int sqlite3_step(sqlite3_stmt *pStmt) {
    (void)pStmt;
    return SQLITE_DONE;
}

int sqlite3_finalize(sqlite3_stmt *pStmt) {
    (void)pStmt;
    return SQLITE_OK;
}

const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return (const unsigned char*)"";
}

const void *sqlite3_column_blob(sqlite3_stmt *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return NULL;
}

int sqlite3_column_bytes(sqlite3_stmt *pStmt, int iCol) {
    (void)pStmt; (void)iCol;
    return 0;
}

const char *sqlite3_errmsg(sqlite3 *db) {
    (void)db;
    return "sqlite3 not available";
}

int sqlite3_exec(sqlite3 *db, const char *sql,
                 int (*callback)(void*,int,char**,char**),
                 void *arg, char **errmsg) {
    (void)db; (void)sql; (void)callback; (void)arg; (void)errmsg;
    return SQLITE_ERROR;
}

int sqlite3_busy_timeout(sqlite3 *db, int ms) {
    (void)db; (void)ms;
    return SQLITE_OK;
}
