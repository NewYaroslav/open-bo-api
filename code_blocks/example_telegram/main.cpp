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
#include "pch.h"
#include "open-bo-api.hpp"
#include "open-bo-api-telegram.hpp"
#include <csignal>

using json = nlohmann::json;

open_bo_api::Settings settings;

int main(int argc, char **argv) {
    settings.json_file_name = "auth.json";

    /* читаем настройки из файла JSON */
    json json_settings; // JSON структура с настройками
    if(!open_bo_api::open_json_file(settings.json_file_name, json_settings)) return EXIT_FAILURE;
    settings.init(json_settings);
    if(settings.check_error()) return EXIT_FAILURE;

    std::cout << settings.telegram_token << std::endl;
    std::cout << settings.telegram_proxy << std::endl;
    std::cout << settings.telegram_sert_file << std::endl;
    std::cout << settings.telegram_chats_id_file << std::endl;


    open_bo_api::TelegramApi telegram(settings.telegram_token, settings.telegram_proxy, settings.telegram_proxy_pwd, settings.telegram_sert_file, settings.telegram_chats_id_file);
    /* обновим char id, если кто то еще написал боту */
    telegram.update_chats_id_store();

    /* отправим сообщение */
    telegram.send_message("USERNAME", "Hello!!!");
    telegram.send_message("USERNAME", u8"Я твой бот");
    std::system("pause");
    return EXIT_SUCCESS;
}

