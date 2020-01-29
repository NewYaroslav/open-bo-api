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
#ifndef OPEN_BO_API_HPP_INCLUDED
#define OPEN_BO_API_HPP_INCLUDED

#include "open-bo-api-indicators.hpp"
#include "open-bo-api-command-line-tools.hpp"

/* брокер intrade.bar */
#include "intrade-bar-api.hpp"
#include "intrade-bar-payout-model.hpp"

#include "ForexprostoolsApi.hpp"

namespace open_bo_api {
    using json = nlohmann::json;
    using Candle = xquotes_common::Candle;

    class IntradeBar {
    public:

        IntradeBar() {}

        /** \brief Получить список символов
         * \return список символов
         */
        inline const static std::vector<std::string> get_list_symbols() {
            return std::vector<std::string>(intrade_bar_common::currency_pairs.begin(),intrade_bar_common::currency_pairs.end());
        }
#if(0)
        /** \brief Получить процент выплат
         *
         * Проценты выплат варьируются обычно от 0 до 0.85, где 0.85 соответствует 85% выплате брокера
         * \param symbol_name Имя валютной пары
         * \param timestamp временную метку unix времени (GMT)
         * \param duration длительность опциона в секундах
         * \param amount размер ставки бинарного опциона
         * \return процент выплат, 0 если выплаты нет
         */
        inline const static double get_payout(
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const uint32_t duration,
                const double amount) {
            double payout = 0.0;
            if(payout_model::get_payout(
                payout,
                symbol_name,
                timestamp,
                duration,
                amount) == payout_model::OK) return payout;
            return 0.0;
        }
#endif
         /** \brief Получитьабсолютный размер ставки и процент выплат
         *
         * Проценты выплат варьируются обычно от 0 до 1.0, где 1.0 соответствует 100% выплате брокера
         * \param amount Размер ставки в абсолютном значении
         * \param payout Процент выплат
         * \param symbol_name Имя валютной пары
         * \param timestamp Метка времени (GMT)
         * \param duration Длительность опциона в секундах
         * \param balance Размер баланса
         * \param winrate Винрейт стратегии
         * \param attenuator Ослабление коэффициента Келли, желательно использовать значения не боьше 0.4
         * \return состояние выплаты (0 в случае успеха, иначе см. payout_model::IntradeBar::PayoutCancelType)
         */
        inline const static int get_amount(
                double &amount,
                double &payout,
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const uint32_t duration,
                const bool is_rub,
                const double balance,
                const double winrate,
                const double attenuator) {
            payout_model::IntradeBar pm;
            pm.set_rub_account_currency(is_rub);
            return pm.get_amount(
                amount,
                payout,
                symbol_name,
                timestamp,
                duration,
                balance,
                winrate,
                attenuator);
        }
    };

    class News {
    private:
        static inline std::recursive_mutex list_news_mutex;
        static inline ForexprostoolsApiEasy::NewsList news_data;
        static inline std::vector<ForexprostoolsApiEasy::News> list_news;
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
                std::lock_guard<std::recursive_mutex> lock(list_news_mutex);
                list_news.clear();
                int err = api.download_all_news(
                    start_timestamp,
                    stop_timestamp,
                    list_news);
                if(err == ForexprostoolsApi::OK) {
                    std::cout << "download: " << list_news.size() << std::endl;
                    news_data = ForexprostoolsApiEasy::NewsList(list_news);
                    return true;
                }
            }
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
            std::thread news_thread = std::thread([=]{
                const xtime::timestamp_t start_timestamp = timestamp - xtime::SECONDS_IN_DAY;
                const xtime::timestamp_t stop_timestamp = timestamp + xtime::SECONDS_IN_DAY;
                ForexprostoolsApi api(sert_file);
                for(uint32_t attempt = 0; attempt < 5; ++attempt) {
                    std::lock_guard<std::recursive_mutex> lock(list_news_mutex);
                    list_news.clear();
                    int err = api.download_all_news(
                        start_timestamp,
                        stop_timestamp,
                        list_news);
                    if(err == ForexprostoolsApi::OK) {
                        news_data = ForexprostoolsApiEasy::NewsList(list_news);
                        break;
                    }
                }
            });
            news_thread.detach();
        }

        /** \brief Проверить новости в заданных пределах времени
         *
         * \param is_news Имеет состояние true, если в заданных пределах времени есть новость, подходящая по указанным параметрам
         * \param symbol_name имя валютной пары
         * \param timestamp Текущее время (Метка времени)
         * \param indent_timestamp_past Максимальный отступ до метки времени
         * \param indent_timestamp_future Максимальный отступ после метки времени
         * \param is_only_select Использовать только выбранные уровни силы новости.
         * Если есть новость с другим уровнем силы, функция поместит в state NEWS_FOUND.
         * \param is_low Использовать слабые новости.
         * \param is_moderate Использовать новости средней силы.
         * \param is_high Использовать сильные новости.
         * \return Вернет true, если есть новость, подходящая по указанным параметрам
         */
        inline static bool check_news(
                bool &is_news,
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const xtime::timestamp_t indent_timestamp_past,
                const xtime::timestamp_t indent_timestamp_future,
                const bool is_only_select,
                const bool is_low,
                const bool is_moderate,
                const bool is_high) {
            is_news = false;
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
                    if(list_news.size() == 0) return false;
                    return true;
                }
            }

            /* раскладываем имя валютной пары на составляющие */
            std::string currency_1, currency_2;
            int err = ForexprostoolsApiEasy::get_currencies(
                symbol_name,
                currency_1,
                currency_2);
            if(err != ForexprostoolsApiEasy::OK) return false;

            for(size_t i = 0; i < list_news_data.size(); ++i) {
                if(list_news_data[i].currency != currency_1 &&
                    list_news_data[i].currency != currency_2) continue;
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::LOW) {
                    if(is_low) is_news = true;
                    else if(is_only_select) {
                        is_news = false;
                        return true;
                    }
                } else
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::MODERATE) {
                    if(is_moderate) is_news = true;
                    else if(is_only_select) {
                        is_news = false;
                        return true;
                    }
                } else
                if(list_news_data[i].level_volatility == ForexprostoolsApiEasy::HIGH) {
                    if(is_high) is_news = true;
                    else if(is_only_select) {
                        is_news = false;
                        return true;
                    }
                }
            }
            return true;
        }

    };
};

#endif // OPEN_BO_API_HPP_INCLUDED
