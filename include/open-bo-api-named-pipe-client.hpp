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
#ifndef OPEN_BO_API_NAMED_PIPE_CLIENT_HPP_INCLUDED
#define OPEN_BO_API_NAMED_PIPE_CLIENT_HPP_INCLUDED

#include <iostream>
#include <windows.h>
#include <mutex>
#include <atomic>
#include <future>
#include <system_error>
#include <vector>
#include <queue>

namespace open_bo_api {

    class NamedPipeClient {
    private:
        HANDLE pipe = INVALID_HANDLE_VALUE;
        std::future<void> named_pipe_future;    /**< Поток для обработки сообщений */
        std::atomic<bool> is_reset;             /**< Команда завершения работы */
        std::atomic<bool> is_connect;

        std::queue<std::string> queue_messages;
        std::mutex queue_messages_mutex;

        /** \brief Класс настроек соединения
         */
        class Config {
        public:
            std::string name;   /**< Имя */
            size_t buffer_size; /**< Размер буфера для чтения и записи */

            Config() : name("server"), buffer_size(1024) {
            };
        } config;

        /** \brief Инициализировать сервер
         *
         * \param config Настройки сервера
         * \return Вернет true, если инициализация прошла успешно
         */
        bool init(Config &config) {
            if(named_pipe_future.valid()) return false;

            //if(on_open == nullptr ||
            //    on_message == nullptr ||
            //    on_close == nullptr ||
            //    on_error == nullptr) return false;

            std::string pipename("\\\\.\\pipe\\");
            if(config.name.find("\\") != std::string::npos) return false;
            pipename += config.name;
            if(pipename.length() > 256) return false;

            named_pipe_future = std::async(std::launch::async,[
                    &,
                    pipename,
                    config]() {
                while(!is_reset) {
                    /* устанавливаем связь с сервером */
                    while(!is_reset) {
                        pipe = CreateFile(
                            (LPCSTR)pipename.c_str(), // имя канала
                            GENERIC_READ |  // read and write access
                            GENERIC_WRITE,
                            0,              // no sharing
                            NULL,           // default security attributes
                            OPEN_EXISTING,  // opens existing pipe
                            0,              // default attributes
                            NULL);          // no template file

                        /* Выходим из цикла, если есть соединение (хендл валидный) */
                        if(pipe != INVALID_HANDLE_VALUE) break;

                        /* Повторяем попытку, если возникает ошибка, отличная от ERROR_PIPE_BUSY */
                        if(GetLastError() != ERROR_PIPE_BUSY) {
                            //on_error(std::error_code(static_cast<int>(GetLastError()), std::generic_category()));
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            continue;
                        }

                        const size_t DELAY = 1000;
                        /* Все экземпляры канала заняты, поэтому подождите */
                        if(!WaitNamedPipe((LPCSTR)pipename.c_str(), DELAY))  {
                            std::cerr << "Could not open pipe: " << (DELAY / 1000) << " second wait timed out." << std::endl;
                            continue;
                        }

                    } // while
                    if(is_reset) return;

                    /* связь с сервером установлена */

                    /* устанавливаем режим чтения
                     * Клиентская сторона именованного канала начинается в байтовом режиме,
                     * даже если серверная часть находится в режиме сообщений.
                     * Чтобы избежать проблем с получением данных,
                     * установите на стороне клиента также режим сообщений.
                     * Чтобы изменить режим канала, клиент канала должен
                     * открыть канал только для чтения с доступом
                     * GENERIC_READ и FILE_WRITE_ATTRIBUTES
                     */
                    DWORD mode = PIPE_READMODE_MESSAGE;
                    BOOL success = SetNamedPipeHandleState(
                        pipe,
                        &mode,
                        NULL,     // не устанавливать максимальные байты
                        NULL);    // не устанавливайте максимальное время

                    if(!success) {
                        on_error(std::error_code(static_cast<int>(GetLastError()), std::generic_category()));
                        continue;
                    }

                    is_connect = true;
                    on_open();

                    if(is_reset) {
                        is_connect = false;
                        on_close();
                        CloseHandle(pipe);
                    }


                    while(!is_reset && is_connect) {
                        /* отправляем данные */
                        size_t queue_messages_size = 0;
                        {
                            std::lock_guard<std::mutex> lock(queue_messages_mutex);
                            queue_messages_size = queue_messages.size();
                        }
                        if(queue_messages_size != 0) {
                            std::string str;
                            {
                                std::lock_guard<std::mutex> lock(queue_messages_mutex);
                                str = queue_messages.front();
                                queue_messages.pop();
                            }
                            DWORD bytes_written = 0;
                            BOOL success = WriteFile(
                                pipe,
                                str.c_str(),    // буфер для записи
                                str.size(),     // количество байтов для записи
                                &bytes_written,         // количество записанных байтов
                                NULL);                  // не перекрывается I/O

                            DWORD err = GetLastError();
                            if(!success || str.size() != bytes_written) {
                                /* ошибка записи, закрываем соединение */
                                on_error(std::error_code(static_cast<int>(err), std::generic_category()));
                                is_connect = false;
                                break;
                            }
                        }

                        /* проверяем наличие данных в кнале */
                        DWORD bytes_to_read = 0;
                        success = PeekNamedPipe(pipe,NULL,0,NULL,&bytes_to_read,NULL);
                        DWORD err = GetLastError();
                        if(!success) {
                            /* если соединение закрыто, вернется ERROR_PIPE_NOT_CONNECTED */
                            if(err == ERROR_PIPE_NOT_CONNECTED)  break;
                        }
                        if(bytes_to_read == 0) continue;

                        /* читаем данные */
                        DWORD bytes_read = 0;
                        std::vector<char> buf(config.buffer_size);
                        success = ReadFile(
                            pipe,
                            &buf[0],
                            config.buffer_size,
                            &bytes_read,
                            NULL);

                        if(is_reset) break;
                        err = GetLastError();

                        if(!success) {
                            /* если соединение закрыто, вернется ERROR_PIPE_NOT_CONNECTED */
                            if(err == ERROR_PIPE_NOT_CONNECTED)  break;
                            else if(err != ERROR_MORE_DATA) continue;
                        }
                        on_message(std::string(buf.begin(),buf.begin() + bytes_read));
                    } // while
                    is_connect = false;
                    on_close();
                    CloseHandle(pipe);
                    break;
                }
            });
            return true;
        }
    public:

        std::function<void()> on_open;
        std::function<void(const std::string &in_message)> on_message;
        std::function<void()> on_close;
        std::function<void(const std::error_code &)> on_error;

        /** \brief Конструктор класса
         * \param name Имя именнованного канала
         * \param buffer_size Размер буфера
         */
        NamedPipeClient(
            const std::string &name,
            const size_t buffer_size = 1024) {
            is_reset = false;
            is_connect = false;
            config.name = name;
            config.buffer_size = buffer_size;
        }

        /** \brief Отправить сообщение
         * \param out_message Сообщение
         * \return Вернет true в случае успеха
         */
        bool send(const std::string &out_message) {
            if(!is_connect) return false;
            std::lock_guard<std::mutex> lock(queue_messages_mutex);
            queue_messages.push(out_message);
            return true;
        }

        void close() {
            is_reset = true;
        }

        /** \brief Запустить сервер
         */
        bool start() {
            return init(config);
        }

        /** \brief Остановить сервер
         */
        void stop() {
            is_reset = true;
            if(named_pipe_future.valid()) {
                try {
                    named_pipe_future.wait();
                    named_pipe_future.get();
                }
                catch(...) {}
            }
            is_reset = false;
        }

        /** \brief Проверить соединение
         * \return Вернет true, если есть соединение
         */
        inline bool check_connect() {
            return is_connect;
        }

        inline HANDLE get_handle() {
            return pipe;
        }

        ~NamedPipeClient() {
            stop();
        }
    };
}

#endif // OPEN_BO_API_NAMED_PIPE_CLIENT_HPP_INCLUDED
