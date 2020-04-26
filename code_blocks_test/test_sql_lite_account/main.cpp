/*
* open-bo-api - C++ API for working with binary options brokers
*
* Copyright (c) 2020 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include <iostream>
#include <stdio.h>
#include "sqlite3.h"

const char* SQL = "CREATE TABLE IF NOT EXISTS foo(a,b,c); INSERT INTO FOO VALUES(1,2,3); INSERT INTO FOO SELECT * FROM FOO;";

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for(i = 0; i< argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv){
    sqlite3 *db = 0; // хэндл объекта соединение к БД
    char *err = 0;

    std::string database_path("");
    std::string database_name(database_path + "virtual_accounts.db");

    /* таблица для хранения виртуальных аккаунтов */
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS virtual_accounts ("
        "id                 INT     PRIMARY KEY NOT NULL,"
        "holder_name        TEXT    NOT NULL default '',"
        "start_balance      REAL    NOT NULL default '0',"
        "balance            REAL    NOT NULL default '0',"
        "start_timestamp    INTEGER NOT NULL,"
        "timestamp          INTEGER NOT NULL);";

    // открываем соединение
    if(sqlite3_open(database_name.c_str(), &db)) {
        std::cerr << "Error opening / creating a database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    } else
    // выполняем SQL
    if(sqlite3_exec(db, create_table_sql, callback, 0, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return 1;
    } else {
        std::cout << "Table created successfully" << std::endl;
    }

    /* Create SQL statement */
    const char *insert_table_sql =
        "BEGIN "
        "IF NOT EXISTS (SELECT * FROM virtual_accounts WHERE id = 1) "
        "BEGIN "
        "INSERT INTO virtual_accounts (id,holder_name,start_balance,balance,start_timestamp,timestamp) "
        "VALUES (1, 'Tester', 1000.0, 1000.0, 12345,12345); "
        //"INSERT INTO virtual_accounts (id,holder_name,start_balance,balance,start_timestamp,timestamp) "
        //"VALUES (2, 'Tester2', 220.0, 220.0, 123456,123456); "
        "END"
        "END";
    // выполняем SQL
    if(sqlite3_exec(db, insert_table_sql, callback, 0, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return 1;
    } else {
        std::cout << "Records created successfully" << std::endl;
    }

    const char *select_sql = "SELECT * from virtual_accounts";
    if(sqlite3_exec(db, select_sql, callback, 0, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return 1;
    } else {
        std::cout << "Operation done successfully" << std::endl;
    }
    // закрываем соединение
    sqlite3_close(db);
    return 0;
}
