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
#ifndef OPEN_BO_API_COMMAND_LINE_TOOLS_HPP_INCLUDED
#define OPEN_BO_API_COMMAND_LINE_TOOLS_HPP_INCLUDED

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace open_bo_api {
    using json = nlohmann::json;

    /** \brief Открыть файл JSON
     *
     * Данная функция прочитает файл с JSON и запишет данные в JSON структуру
     * \param file_name Имя файла
     * \param auth_json Структура JSON с данными из файла
     * \return Вернет true в случае успешного завершения
     */
    bool open_json_file(const std::string &file_name, json &auth_json) {
        std::ifstream auth_file(file_name);
        if(!auth_file) {
            std::cerr << "open_bo_api::ifstream, open file error" << std::endl;
            return false;
        }
        try {
            auth_file >> auth_json;
        }
        catch (json::parse_error &e) {
            std::cerr << "open_bo_api::open_json_file, json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch (std::exception e) {
            std::cerr << "open_bo_api::open_json_file, json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch(...) {
            std::cerr << "open_bo_api::open_json_file, json parser error" << std::endl;
            auth_file.close();
            return false;
        }
        auth_file.close();
        return true;
    }

    /** \brief Обработать аргументы
     *
     * Данная функция обрабатывает аргументы от командной строки, возвращая
     * результат как пара ключ - значение.
     * \param argc количество аргуметов
     * \param argv вектор аргументов
     * \param f лябмда-функция для обработки аргументов командной строки
     * \return Вернет true если ошибок нет
     */
    bool process_arguments(
        const int argc,
        char **argv,
        std::function<void(
            const std::string &key,
            const std::string &value)> f) noexcept {
        if(argc <= 1) return false;
        bool is_error = true;
        for(int i = 1; i < argc; ++i) {
            std::string key = std::string(argv[i]);
            if(key.size() > 0 && (key[0] == '-' || key[0] == '/')) {
                uint32_t delim_offset = 0;
                if(key.size() > 2 && (key.substr(2) == "--") == 0) delim_offset = 1;
                std::string value;
                if((i + 1) < argc) value = std::string(argv[i + 1]);
                is_error = false;
                if(f != nullptr) f(key.substr(delim_offset), value);
            }
        }
        return !is_error;
    }
};


#endif // OPEN-BO-API-COMMAND-LINE-TOOLS_HPP_INCLUDED
