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

int main(int argc, char **argv){
    sqlite3 *db = 0; // хэндл объекта соединение к БД
    char *err = 0;

    // открываем соединение
    if(sqlite3_open("my_cosy_database.dblite", &db)) {
        std::cerr << "Error opening / creating a database: " << sqlite3_errmsg(db) << std::endl;
    } else
    // выполняем SQL
    if(sqlite3_exec(db, SQL, 0, 0, &err)) {
        std::cerr << "Error SQL: " << err << std::endl;
        sqlite3_free(err);
    }
    // закрываем соединение
    sqlite3_close(db);
    return 0;
}
