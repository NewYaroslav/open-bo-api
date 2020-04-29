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
#ifndef OPEN_BO_API_SETTINGS_HPP_INCLUDED
#define OPEN_BO_API_SETTINGS_HPP_INCLUDED

#include <iostream>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace open_bo_api {
    using json = nlohmann::json;

    class Settings {
    private:
        bool is_error = false;
    public:
        std::string environmental_variable;
        std::string json_file_name; /**< Имя JSON файла */

        /* настройки торгового робота */
        std::string trading_robot_work_log_file = "logger/trading_robot_work_file.log";
        std::string trading_robot_work_path = "logger/";
        double trading_robot_absolute_stop_loss = 0.0;  /**< Абсолютный стоп-лосс. Если депозит опустится ниже данного значения, робот может перестать торговать */
        double trading_robot_relative_stop_loss = 0.0;
        double trading_robot_balance_offset = 0.0;  /**< Смещение баланса. Данный параметр смещает уровень баланса на указанное число. Это может быть полезно, когда часть депозита лежит не у брокера */
        double trading_robot_payout_limiter = 1.0;  /**< Ограничитель процентов выплат. Это влияет на формулы расчета ставок, они не будут считать ставку выше, чем для данной выплаты */

        /* настройки новостей */
        std::string news_sert_file = "curl-ca-bundle.crt";

        /* настройки для брокера intrade.bar */
        std::string intrade_bar_sert_file = "curl-ca-bundle.crt";
        std::string intrade_bar_cookie_file = "cookie/intrade-bar.cookie";
        std::string intrade_bar_bets_log_file = "logger/intrade-bar-bets.log";
        std::string intrade_bar_work_log_file = "logger/intrade-bar-https-work.log";
        std::string intrade_bar_websocket_log_file = "logger/intrade-bar-websocket.log";
        std::string intrade_bar_email;
        std::string intrade_bar_password;
        uint32_t intrade_bar_number_bars = 100;         /**< количество баров м1 для инициализации исторических данных */
        bool is_intrade_bar_demo_account = true;        /**< Флаг использования демо счета */
        bool is_intrade_bar_rub_currency = true;        /**< Флаг использования рублевого счета */
        bool is_intrade_bar_open_equal_close = true;    /**< Флаг, по умолчанию true. Если флаг установлен, то цена открытия бара равна цене закрытия предыдущего бара */
        bool is_intrade_bar_merge_hist_witch_stream = false;    /**< Флаг, по умолчанию false. Если флаг установлен, то исторический бар будет слит с баром из потока котировок для события обновления исторических цен. Это повышает стабильность потока исторических цен */
        bool is_intrade_bar = false;                    /**< Флаг использования брокера intrade.bar */

        /* настройки для брокера olymp trade */
        uint32_t olymp_trade_port = 8080;   /**< Порт сервера для подключения к расширению брокера Olymptrade */
        bool is_olymp_trade_demo_account = true;
        bool is_olymp_trade = false;        /**< Флаг использования брокера olymptrade */

        uint32_t mt_bridge_port = 5555;     /**< Порт "Моста" для подключения к MetaTrader4 */
        bool is_mt_bridge = true;           /**< Флаг использования "моста" для подключения к MetaTrader4 */

        /* настройки для telegram */
        std::string telegram_token;
        std::string telegram_proxy;
        std::string telegram_proxy_pwd;
        std::string telegram_sert_file = "curl-ca-bundle.crt";
        std::string telegram_chats_id_file = "telegram/save_chats_id.json";

        std::string history_tester_storage_path;
        double history_tester_time_speed = 1.0;
        uint32_t history_tester_number_bars = 100;
        xtime::timestamp_t history_tester_start_timestamp = 0;
        xtime::timestamp_t history_tester_stop_timestamp = 0;

        Settings() {};

        void init(json &j) {
            try {
                if(j["environmental_variable"] != nullptr) {
                    environmental_variable = j["environmental_variable"];
                }
                //
                if(j["telegram"]["token"] != nullptr) {
                    telegram_token = j["telegram"]["token"];
                }
                if(j["telegram"]["proxy"] != nullptr) {
                    telegram_proxy = j["telegram"]["proxy"];
                }
                if(j["telegram"]["proxy_pwd"] != nullptr) {
                    telegram_proxy_pwd = j["telegram"]["proxy_pwd"];
                }
                if(j["telegram"]["sert_file"] != nullptr) {
                    telegram_sert_file = j["telegram"]["sert_file"];
                }
                if(j["telegram"]["chats_id_file"] != nullptr) {
                    telegram_chats_id_file = j["telegram"]["chats_id_file"];
                }
                //
                if(j["mt_bridge"]["port"] != nullptr) {
                    mt_bridge_port = j["mt_bridge"]["port"];
                }
                if(j["mt_bridge"]["use"] != nullptr) {
                    is_mt_bridge = j["mt_bridge"]["use"];
                }
                //
                if(j["news"]["sert_file"] != nullptr) {
                    news_sert_file = j["news"]["sert_file"];
                }

                /* загрузим настройки для брокера intrade.bar */
                if(j["intrade_bar"]["email"] != nullptr) {
                    intrade_bar_email = j["intrade_bar"]["email"];
                }
                if(j["intrade_bar"]["password"] != nullptr) {
                    intrade_bar_password = j["intrade_bar"]["password"];
                }
                if(j["intrade_bar"]["sert_file"] != nullptr) {
                    intrade_bar_sert_file = j["intrade_bar"]["sert_file"];
                }
                if(j["intrade_bar"]["cookie_file"] != nullptr) {
                    intrade_bar_cookie_file = j["intrade_bar"]["cookie_file"];
                }
                if(j["intrade_bar"]["bets_log_file"] != nullptr) {
                    intrade_bar_bets_log_file = j["intrade_bar"]["bets_log_file"];
                }
                if(j["intrade_bar"]["work_log_file"] != nullptr) {
                    intrade_bar_work_log_file = j["intrade_bar"]["work_log_file"];
                }
                if(j["intrade_bar"]["websocket_log_file"] != nullptr) {
                    intrade_bar_websocket_log_file = j["intrade_bar"]["websocket_log_file"];
                }
                if(j["intrade_bar"]["demo_account"] != nullptr) {
                    is_intrade_bar_demo_account = j["intrade_bar"]["demo_account"];
                }
                if(j["intrade_bar"]["rub_currency"] != nullptr) {
                    is_intrade_bar_rub_currency = j["intrade_bar"]["rub_currency"];
                }
                if(j["intrade_bar"]["number_bars"] != nullptr) {
                    intrade_bar_number_bars = j["intrade_bar"]["number_bars"];
                }
                if(j["intrade_bar"]["open_equal_close"] != nullptr) {
                    is_intrade_bar_open_equal_close = j["intrade_bar"]["open_equal_close"];
                }
                if(j["intrade_bar"]["merge_hist_witch_stream"] != nullptr) {
                    is_intrade_bar_merge_hist_witch_stream = j["intrade_bar"]["merge_hist_witch_stream"];
                }
                if(j["intrade_bar"]["use"] != nullptr) {
                    is_intrade_bar = j["intrade_bar"]["use"];
                }

                /* загрузим настройки для брокера olymp trade */
                if(j["olymp_trade"]["port"] != nullptr) {
                    olymp_trade_port = j["olymp_trade"]["port"];
                }
                if(j["olymp_trade"]["demo_account"] != nullptr) {
                    is_olymp_trade_demo_account = j["olymp_trade"]["demo_account"];
                }
                if(j["olymp_trade"]["use"] != nullptr) {
                    is_olymp_trade = j["olymp_trade"]["use"];
                }

                //
                if(j["trading_robot"]["work_log_file"] != nullptr) {
                    trading_robot_work_log_file = j["trading_robot"]["work_log_file"];
                }
                if(j["trading_robot"]["work_path"] != nullptr) {
                    trading_robot_work_path = j["trading_robot"]["work_path"];
                }
                if(j["trading_robot"]["absolute_stop_loss"] != nullptr) {
                    trading_robot_absolute_stop_loss = j["trading_robot"]["absolute_stop_loss"];
                }
                if(j["trading_robot"]["relative_stop_loss"] != nullptr) {
                    trading_robot_relative_stop_loss = j["trading_robot"]["relative_stop_loss"];
                }
                if(j["trading_robot"]["balance_offset"] != nullptr) {
                    trading_robot_balance_offset = j["trading_robot"]["balance_offset"];
                }
                if(j["trading_robot"]["payout_limiter"] != nullptr) {
                    trading_robot_payout_limiter = j["trading_robot"]["payout_limiter"];
                }

                //
                if(j["history_tester"]["storage_path"] != nullptr) {
                    history_tester_storage_path = j["history_tester"]["storage_path"];
                }
                if(j["history_tester"]["time_speed"] != nullptr) {
                    history_tester_time_speed = j["history_tester"]["time_speed"];
                }
                if(j["history_tester"]["number_bars"] != nullptr) {
                    history_tester_number_bars = j["history_tester"]["number_bars"];
                }
                if(j["history_tester"]["start_timestamp"] != nullptr) {
                    std::string date = j["history_tester"]["start_timestamp"];
                    xtime::convert_str_to_timestamp(date, history_tester_start_timestamp);
                }
                if(j["history_tester"]["stop_timestamp"] != nullptr) {
                    std::string date = j["history_tester"]["stop_timestamp"];
                    xtime::convert_str_to_timestamp(date, history_tester_stop_timestamp);
                }
            }
            catch (json::parse_error &e) {
                std::cerr << "open_bo_api::Settings, json parser error: " << std::string(e.what()) << std::endl;
                is_error = true;
                return;
            }
            catch (std::exception e) {
                std::cerr << "open_bo_api::Settings, json parser error: " << std::string(e.what()) << std::endl;
                is_error = true;
                return;
            }
            catch(...) {
                std::cerr << "open_bo_api::Settings, json parser error" << std::endl;
                is_error = true;
                return;
            }
            if(environmental_variable.size() != 0) {
                const char* env_ptr = std::getenv(environmental_variable.c_str());
                if(env_ptr == NULL) {
                    std::cerr << "open_bo_api::Settings, error, no environment variable!" << std::endl;
                    is_error = true;
                    return;
                }
                news_sert_file = std::string(env_ptr) + "/" + news_sert_file;
                intrade_bar_sert_file = std::string(env_ptr) + "/" + intrade_bar_sert_file;
                intrade_bar_cookie_file = std::string(env_ptr) + "/" + intrade_bar_cookie_file;
                intrade_bar_bets_log_file = std::string(env_ptr) + "/" + intrade_bar_bets_log_file;
                intrade_bar_work_log_file = std::string(env_ptr) + "/" + intrade_bar_work_log_file;
                intrade_bar_websocket_log_file = std::string(env_ptr) + "/" + intrade_bar_websocket_log_file;
                trading_robot_work_log_file = std::string(env_ptr) + "/" + trading_robot_work_log_file;
				telegram_sert_file = std::string(env_ptr) + "/" + telegram_sert_file;
				telegram_chats_id_file = std::string(env_ptr) + "/" + telegram_chats_id_file;
				trading_robot_work_path = std::string(env_ptr) + "/" + trading_robot_work_path;
            }
        }

        Settings(json &j) {
            init(j);
        }

        /** \brief Проверить наличие ошибки
         * \return Вернет true, если есть ошибка
         */
        inline bool check_error() {
            return is_error;
        }

        std::string get_date_name(const xtime::timestamp_t timestamp) {
            xtime::DateTime date_time(timestamp);
            std::string temp;
            temp += std::to_string(date_time.year);
            temp += "_";
            temp += std::to_string(date_time.month);
            temp += "_";
            temp += std::to_string(date_time.day);
            return temp;
        }

        std::string get_date_name() {
            return get_date_name(xtime::get_timestamp());
        }

        std::string get_work_log_file_name() {
            std::string temp;
            temp += trading_robot_work_path;
            temp += "/";
            temp += get_date_name();
            temp += ".log";
            return temp;
        }
    };
}

#endif // OPEN_BO_API_SETTINGS_HPP_INCLUDED
