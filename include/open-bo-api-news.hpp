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
#ifndef OPEN_BO_API_NEWS_HPP_INCLUDED
#define OPEN_BO_API_NEWS_HPP_INCLUDED

#include "ForexprostoolsApi.hpp"
#include <mutex>
#include <atomic>

namespace open_bo_api {

    /** \brief Класс для работы с новостями
     */
    class News {
    private:
        static inline std::recursive_mutex list_news_mutex;
        static inline ForexprostoolsApiEasy::NewsList news_data;
        static inline std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);
    public:


        /** \brief Обновить список новостей
         *
         * \param timestamp Метка времени
         * \return Вернет true, если новости были загружены
         */
        inline static bool update(
                const xtime::timestamp_t timestamp,
                const std::string &sert_file = std::string("curl-ca-bundle.crt")) {
            const xtime::timestamp_t start_timestamp = timestamp - xtime::SECONDS_IN_DAY;
            const xtime::timestamp_t stop_timestamp = timestamp - xtime::SECONDS_IN_DAY;
            ForexprostoolsApi api(sert_file);
            for(uint32_t attempt = 0; attempt < 5; ++attempt) {
                std::vector<ForexprostoolsApiEasy::News> list_news;
                int err = api.download_all_news(
                    start_timestamp,
                    stop_timestamp,
                    list_news);
                if(err == ForexprostoolsApi::OK) {
                    std::lock_guard<std::recursive_mutex> lock(list_news_mutex);
                    news_data = ForexprostoolsApiEasy::NewsList(list_news);
                    is_error = false;
                    return true;
                }
            }
            is_error = true;
            return false;
        }

        /** \brief Обновить список новостей
         *
         * \param timestamp Метка времени
         */
        inline static void async_update(
                const xtime::timestamp_t timestamp,
                const std::string &sert_file = std::string("curl-ca-bundle.crt")) {
            /* запускаем асинхронное открытие сделки */
            std::thread news_thread = std::thread([&,sert_file,timestamp] {
                const xtime::timestamp_t start_timestamp = timestamp - xtime::SECONDS_IN_DAY;
                const xtime::timestamp_t stop_timestamp = timestamp + xtime::SECONDS_IN_DAY;
                ForexprostoolsApi api(sert_file);
                for(uint32_t attempt = 0; attempt < 5; ++attempt) {
                    std::vector<ForexprostoolsApiEasy::News> list_news;
                    int err = api.download_all_news(
                        start_timestamp,
                        stop_timestamp,
                        list_news);
                    if(err == ForexprostoolsApi::OK) {
                        std::lock_guard<std::recursive_mutex> lock(list_news_mutex);
                        news_data = ForexprostoolsApiEasy::NewsList(list_news);
                        is_error = false;
                        return;
                    }
                }
                is_error = true;
            });
            news_thread.detach();
        }

        /** \brief Проверить новости в заданных пределах времени и по заданным критериям
         *
         * \param symbol_name имя валютной пары
         * \param timestamp Текущее время (Метка времени)
         * \param indent_timestamp_past Максимальный отступ до метки времени
         * \param indent_timestamp_future Максимальный отступ после метки времени
         * \param is_only_select Использовать только выбранные уровни силы новости.
         * Если есть новость с другим уровнем силы, функция поместит в state NEWS_FOUND.
         * \param is_low Использовать слабые новости.
         * \param is_moderate Использовать новости средней силы.
         * \param is_high Использовать сильные новости.
         * \return Вернет true, если в заданных пределах времени есть новость, подходящая по указанным параметрам
         */
        inline static bool check_news_filter(
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const xtime::timestamp_t indent_timestamp_past,
                const xtime::timestamp_t indent_timestamp_future,
                const bool is_only_select,
                const bool is_low,
                const bool is_moderate,
                const bool is_high) {
            /* получаем массив новостей */
            std::vector<ForexprostoolsApiEasy::News> list_news_data;
            {
                std::lock_guard<std::recursive_mutex> lock(list_news_mutex);
                int err = 0;
                if((err = news_data.get_news(
                    timestamp,
                    indent_timestamp_past,
                    indent_timestamp_future,
                    list_news_data)) != ForexprostoolsApiEasy::OK) {
                    /* ошибка, нет данных новостей
                     * если новости были загружены с ошибкой, то это значит
                     * что система не работает!
                     * Поэтому, если флаг is_error вернем значение, якобы новость есть
                     * чтобы сделки не были открыты
                     */
                    if(is_error) return true;
                    return false;
                }
            }

            /* раскладываем имя валютной пары на составляющие */
            std::string currency_1, currency_2;
            int err = ForexprostoolsApiEasy::get_currencies(
                symbol_name,
                currency_1,
                currency_2);
            if(err != ForexprostoolsApiEasy::OK) return false;

            bool is_news = false;
            for(size_t i = 0; i < list_news_data.size(); ++i) {
                if(list_news_data[i].currency != currency_1 &&
                    list_news_data[i].currency != currency_2) continue;
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::LOW) {
                    if(is_low) is_news = true;
                    else if(is_only_select) return false;
                } else
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::MODERATE) {
                    if(is_moderate) is_news = true;
                    else if(is_only_select) return false;
                } else
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::HIGH) {
                    if(is_high) is_news = true;
                    else if(is_only_select) return false;
                }
            }
            return is_news;
        }

        /** \brief Проверить наличие ошибки
         * \return Вернет true, если есть ошибка
         */
        inline static bool check_error() {
            return is_error;
        }
    };
}

#endif // OPEN_BO_API_NEWS_HPP_INCLUDED
