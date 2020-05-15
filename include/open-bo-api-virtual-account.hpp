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
#ifndef OPEN_BO_API_VIRTUAL_ACCOUNT_HPP_INCLUDED
#define OPEN_BO_API_VIRTUAL_ACCOUNT_HPP_INCLUDED

#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <cstring>
#include <memory>
#include <mutex>
#include <atomic>
#include <future>

#include "xtime.hpp"
#include "sqlite3.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace open_bo_api {

    /** \brief Класс для хранения данных аккаунта
     */
    class VirtualAccount {
    public:
        uint64_t va_id = 0;         /**< ID аккаунта */
        std::string holder_name;    /**< Имя владельца аккаунта */

        std::string note;

        double start_balance = 0.0d;    /**< Начальный депозит */
        double balance = 0.0d;          /**< Текущий депозит */
        double absolute_stop_loss = 0.0d;   /**< Абсолютный стоп лосс. Задается в единицах депозита */
        double absolute_take_profit = 0.0d; /**< Абсолютный тейк профит. Задается в единицах депозита */
        double kelly_attenuation_multiplier = 1.0d; /**< Множитель коэффициента ослабления Келли. Данный параметр влияет на общий уровень риска */
        double kelly_attenuation_limiter = 0.4d;    /**< Ограничение коэффициента ослабления Келли */
        double payout_limiter = 1.0d;               /**< Ограничитель процентов выплат. Это влияет на формулы расчета ставок, они не будут считать ставку выше, чем для данной выплаты */
        double winrate_limiter = 1.0d;              /**< Ограничитель процентов выплат */

        uint64_t wins = 0;
        uint64_t losses = 0;

        xtime::timestamp_t start_timestamp = 0; /**< Дата начала торговли */
        xtime::timestamp_t timestamp = 0;       /**< Последняя дата обновления баланса */

        std::set<std::string> list_strategies;  /**< Список используемых стратегий */

        bool demo = true;       /**< Использовать демо-аккаунт */
        bool enabled = false;   /**< Виртуальный аккаунт включен или выключен */

        //json j_data;            /**< Данные в формате json */
        std::map<xtime::timestamp_t, double> date_balance;  /**< Данные депозита по дням */

        std::map<uint64_t, double> mem_profit;
        std::map<uint64_t, double> mem_amount;

        VirtualAccount() {};

        /** \brief Рассчитать размер ставки
         *
         * \param amount Посчитанный размер ставки
         * \param strategy_name Имя стратегии
         * \param is_demo Использовать демо аккаунт
         * \param payout Процент выплаты брокера
         * \param winrate Винрейт
         * \param kelly_attenuation Коэффициент ослабления Келли
         * \return Вернет true в случае успеха
         */
        bool calc_amount(double &amount,
                         const std::string strategy_name,
                         const bool is_demo,
                         const double payout,
                         const double winrate,
                         const double kelly_attenuation) {
            amount = 0.0d;
            if(!enabled) return false;
            if(demo != is_demo) return false;
            if(absolute_stop_loss != 0 && balance < absolute_stop_loss) return false;
            if(absolute_take_profit != 0 && balance > absolute_take_profit) return false;
            if(list_strategies.find(strategy_name) == list_strategies.end()) return false;
            const double threshold_winrate = 1.0d/(1.0d + payout);
            if(winrate <= threshold_winrate) return false;
            if(list_strategies.find(strategy_name) == list_strategies.end()) return false;
            const double calc_payout = std::min(payout, payout_limiter);
            const double calc_kelly_attenuation = std::min(kelly_attenuation * kelly_attenuation_multiplier, kelly_attenuation_limiter);
            const double calc_winrate = std::min(winrate, winrate_limiter);
            const double calc_risk = (((1.0d + calc_payout) * calc_winrate - 1.0d) / calc_payout) * calc_kelly_attenuation;
            amount = balance * calc_risk;
            return true;
        }

        /** \brief Получить винрейт
         *
         * \return Винрейт
         */

        double get_winrate() {
            const uint64_t sum = wins + losses;
            return sum == 0 ? 0.0d : (double)wins / (double)sum;
        }

        const std::string convert_date_balance_to_str_json() const {
            std::string temp;
            try {
                json j;
                json &j_obj = j["data"]["daily_balance"];
                for(auto &it : date_balance) {
                    const std::string str_key(std::to_string(it.first));
                    j_obj[str_key] = it.second;
                }
                temp = j.dump();
            } catch(...) {
                temp = "{}";
            }
            return temp;
        }

        void convert_json_to_date_balance(const std::string &str_data) {
            try {
                date_balance.clear();
                json j;
                j = json::parse(str_data);
                auto it_data = j.find("data");
                if(it_data != j.end()) {
                    auto it_daily_balance = it_data->find("daily_balance");
                    if(it_daily_balance != it_data->end()) {
                        for(json::iterator element = it_daily_balance->begin();
                            element != it_daily_balance->end(); ++element) {
                            date_balance[std::stoll(element.key(),NULL,10)] = element.value();
                        }
                    }
                }
            } catch(...) {
                date_balance.clear();
            }
        }

    };

    /** \brief Класс для работы с массивом виртуальных аккаунтов
     */
    class VirtualAccounts {
    private:
        const char *va_name = "virtual_accounts";
        sqlite3 *db = 0;
        bool is_error = false;

        std::map<uint64_t, VirtualAccount> virtual_accounts;            /**< Массив виртуальных аккаунтов */
        std::map<uint64_t, VirtualAccount> callback_virtual_accounts;   /**< Массив виртуальных аккаунтов для callback */
        std::map<uint64_t, VirtualAccount> update_virtual_accounts;     /**< Массив виртуальных аккаунтов для callback */
        std::mutex virtual_accounts_mutex;
        std::mutex callback_virtual_accounts_mutex;
        std::mutex update_virtual_accounts_mutex;
        std::mutex va_editot_mutex;

        std::atomic<bool> is_update_virtual_accounts;
        std::atomic<bool> is_update_va_completed;

        std::future<void> update_va_future;
        std::atomic<bool> is_stop_command;  /**< Команда завершения работы */

        /** \brief Разобрать строку списка
         *
         * \param value Список
         * \param elemet_list Элементы списка
         */
        void parse_list(std::string value, std::set<std::string> &elemet_list) noexcept {
            if(value.back() != ',') value += ",";
            std::size_t start_pos = 0;
            while(true) {
                std::size_t found_beg = value.find_first_of(",", start_pos);
                if(found_beg != std::string::npos) {
                    std::size_t len = found_beg - start_pos;
                    if(len > 0) elemet_list.insert(value.substr(start_pos, len));
                    start_pos = found_beg + 1;
                } else break;
            }
        }

        void va_callback(int argc, char **argv, char **key_name) {
            const uint64_t VA_ID_MAX = 0xFFFFFFFFFFFFFFFF;
            const uint64_t va_id = strncmp(key_name[0],"id",2) == 0 ? atoll(argv[0]) : VA_ID_MAX;
            if(va_id == VA_ID_MAX) return;
            VirtualAccount va;
            va.va_id = va_id;
            va.holder_name = std::string(argv[1]);
            va.note = std::string(argv[2]);
            va.start_balance = atof(argv[3]);
            va.balance = atof(argv[4]);
            va.absolute_stop_loss = atof(argv[5]);
            va.absolute_take_profit = atof(argv[6]);
            va.kelly_attenuation_multiplier = atof(argv[7]);
            va.kelly_attenuation_limiter = atof(argv[8]);
            va.payout_limiter = atof(argv[9]);
            va.winrate_limiter = atof(argv[10]);
            parse_list(argv[11], va.list_strategies);
            va.demo = atoi(argv[12]) == 0 ? false : true;
            va.enabled = atoi(argv[13]) == 0 ? false : true;
            va.start_timestamp = atoll(argv[14]);
            va.timestamp = atoll(argv[15]);
            va.wins = atoll(argv[16]);
            va.losses = atoll(argv[17]);
            va.convert_json_to_date_balance(argv[18]);
            callback_virtual_accounts[va_id] = va;
        }

        bool update_va_balance(const VirtualAccount &va) {
            if(!db) return false;
            if(is_error) return false;
            std::string buffer("UPDATE virtual_accounts SET ");
            buffer += "balance = ";
            buffer += std::to_string(va.balance);
            buffer += ", timestamp = ";
            buffer += std::to_string(va.timestamp);
            buffer += ", wins = ";
            buffer += std::to_string(va.wins);
            buffer += ", losses = ";
            buffer += std::to_string(va.losses);
            buffer += ", json = '";
            buffer += va.convert_date_balance_to_str_json();
            buffer += "'";
            buffer += " WHERE id = ";
            buffer += std::to_string(va.va_id);
            buffer += "; ";
            //buffer += "; SELECT * from virtual_accounts";

            std::lock_guard<std::mutex> lock(callback_virtual_accounts_mutex);
            callback_virtual_accounts.clear();
            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "VirtualAccounts SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                //std::cout << "VirtualAccounts UPDATE created successfully" << std::endl;
            }
            return true;
        }

    public:

        static int callback(void *userdata, int argc, char **argv, char **key_name) {
            VirtualAccounts* app = static_cast<VirtualAccounts*>(userdata);
            if(app) app->va_callback(argc, argv, key_name);
#           if(0) // нужно только для отладки
            if(0) {
                for(int i = 0; i< argc; i++) {
                    printf("%s = %s\n", key_name[i], argv[i] ? argv[i] : "NULL");
                }
                printf("\n");
            }
#           endif
            return 0;
        }

        /** \brief Инициализация класса
         *
         * \param database_name Файл базы данных виртуальных аккаунтов
         */
        VirtualAccounts(const std::string database_name) {
            is_stop_command = false;
            is_update_virtual_accounts = false;
            is_update_va_completed = false;

            char *err = 0;
            /* таблица для хранения виртуальных аккаунтов */
            const char *create_table_sql =
                "CREATE TABLE IF NOT EXISTS virtual_accounts ("
                "id                             INT     PRIMARY KEY NOT NULL,"
                "holder_name                    TEXT    NOT NULL,"
                "note                           TEXT    NOT NULL,"
                "start_balance                  REAL    NOT NULL default '0',"
                "balance                        REAL    NOT NULL default '0',"
                "absolute_stop_loss             REAL    NOT NULL default '0',"
                "absolute_take_profit           REAL    NOT NULL default '0',"
                "kelly_attenuation_multiplier   REAL    NOT NULL default '0',"
                "kelly_attenuation_limiter      REAL    NOT NULL default '0',"
                "payout_limiter                 REAL    NOT NULL default '0',"
                "winrate_limiter                REAL    NOT NULL default '0',"
                "list_strategies                TEXT    NOT NULL,"
                "demo               INTEGER NOT NULL,"
                "enabled            INTEGER NOT NULL,"
                "start_timestamp    INTEGER NOT NULL,"
                "timestamp          INTEGER NOT NULL,"
                "wins               INTEGER NOT NULL,"
                "losses             INTEGER NOT NULL,"
                "json               TEXT    NOT NULL); ";
            /* открываем и возможно еще создаем таблицу */
            if(sqlite3_open(database_name.c_str(), &db) != SQLITE_OK) {
                std::cerr << "VirtualAccounts Error opening / creating a database: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_close(db);
                db = 0;
                is_error = true;
                return;
            } else
            /* создаем таблицу в базе данных, если она еще не создана */
            if(sqlite3_exec(db, create_table_sql, callback, this, &err) != SQLITE_OK) {
                std::cerr << "VirtualAccounts SQL error: " << err << std::endl;
                sqlite3_free(err);
                sqlite3_close(db);
                db = 0;
                is_error = true;
                return;
            } else {
                //std::cout << "VirtualAccounts Table created successfully" << std::endl;
            }

            /* читаем данные */
            {
                std::lock_guard<std::mutex> lock(callback_virtual_accounts_mutex);
                const char *select_sql = "SELECT * from virtual_accounts";
                if(sqlite3_exec(db, select_sql, callback, this, &err) != SQLITE_OK) {
                    std::cerr << "VirtualAccounts SQL error: " << err << std::endl;
                    sqlite3_free(err);
                    sqlite3_close(db);
                    db = 0;
                    is_error = true;
                    return;
                } else {
                    //std::cout << "VirtualAccounts Operation done successfully" << std::endl;
                }

                std::lock_guard<std::mutex> lock2(virtual_accounts_mutex);
                virtual_accounts = callback_virtual_accounts;
            }

            /* создаем поток обработки событий */
            update_va_future = std::async(std::launch::async,[&]() {
                while(!is_stop_command) {
                    /* проверим, нет ли изменений в счетах */
                    if(is_update_virtual_accounts) {
                        is_update_virtual_accounts = false;

                        /* блокируем редактирование счетов */
                        std::lock_guard<std::mutex> lock(va_editot_mutex);

                        /* копируем данные счетов */
                        std::map<uint64_t, VirtualAccount> update_virtual_accounts;
                        {
                            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
                            update_virtual_accounts = virtual_accounts;
                        }

                        /* обновляем данные в базе данных */
                        for(auto &it : update_virtual_accounts) {
                            if(it.second.enabled) update_va_balance(it.second);
                        }
                        is_update_va_completed = true;
                    }
                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } // while(!is_stop_command)
            });
        };

        ~VirtualAccounts() {
            push(true);
            is_stop_command = true;
            if(update_va_future.valid()) {
                try {
                    update_va_future.wait();
                    update_va_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "VirtualAccounts error: ~VirtualAccounts(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "VirtualAccounts error: ~VirtualAccounts()" << std::endl;
                }
            }

            /* в самом конце закроем базу данных
             * в начале деинициализации еще может быть запись в БД
             * поэтому лучше закрыть после завершения потока
             */
            if(db) sqlite3_close(db);
        }

        inline bool check_errors() {
            return is_error;
        }

        /** \brief Проверить наличие аккаунтов
         *
         * \return Вернет true, если аккаунты есть в наличии
         */
        bool check_accounts() {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
            if(virtual_accounts.size() == 0) return false;
            return true;
        }

        /** \brief Проверить баланс виртуальных аккаунтов
         *
         * \param total_balance Общий баланас торговли
         * \param demo Демо аккаунт
         * \return Вернет true, если баланс виртуальных аккаунтов меньше или равен реальному балансу
         */
        bool check_balance(const double total_balance, const bool demo) {
            double sum_balance = 0.0d;
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) sum_balance += it.second.balance;
            }
            if(sum_balance > total_balance) return false;
            return true;
        }

        /** \brief Получить баланс виртуальных аккаунтов
         *
         * \param demo Демо аккаунт
         * \return Баланс виртуальных аккаунтов
         */
        double get_balance(const bool demo) {
            double sum_balance = 0.0d;
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) sum_balance += it.second.balance;
            }
            return sum_balance;
        }

        bool add_virtual_account(VirtualAccount &va) {
            std::lock_guard<std::mutex> lock(va_editot_mutex);

            if(!db) return false;
            if(is_error) return false;
            std::string buffer("INSERT INTO virtual_accounts ("
                "id,holder_name,note,start_balance,balance,"
                "absolute_stop_loss,absolute_take_profit,"
                "kelly_attenuation_multiplier,kelly_attenuation_limiter,"
                "payout_limiter,winrate_limiter,list_strategies,"
                "demo,enabled,start_timestamp,timestamp,"
                "wins,losses,json) ");
            va.va_id = 0;

            {
                std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
                if(!virtual_accounts.empty()) {
                    auto it = virtual_accounts.begin();
                    auto it_end = std::next(it, virtual_accounts.size() - 1);
                    va.va_id = it_end->first;
                    ++va.va_id;
                }
            }

            buffer += "VALUES (";
            buffer += std::to_string(va.va_id);
            buffer += ",'";
            buffer += va.holder_name;
            buffer += "','";
            buffer += va.note;
            buffer += "',";
            buffer += std::to_string(va.start_balance);
            buffer += ",";
            buffer += std::to_string(va.balance);
            buffer += ",";
            buffer += std::to_string(va.absolute_stop_loss);
            buffer += ",";
            buffer += std::to_string(va.absolute_take_profit);
            buffer += ",";
            buffer += std::to_string(va.kelly_attenuation_multiplier);
            buffer += ",";
            buffer += std::to_string(va.kelly_attenuation_limiter);
            buffer += ",";
            buffer += std::to_string(va.payout_limiter);
            buffer += ",";
            buffer += std::to_string(va.winrate_limiter);
            buffer += ",'";
            for(auto strategy_name : va.list_strategies) {
                buffer += strategy_name;
                buffer += ",";
            }
            buffer += "',";
            if(va.demo) buffer += "1,";
            else buffer += "0,";
            if(va.enabled) buffer += "1,";
            else buffer += "0,";
            buffer += std::to_string(va.start_timestamp);
            buffer += ",";
            buffer += std::to_string(va.timestamp);
            buffer += ",";
            buffer += std::to_string(va.wins);
            buffer += ",";
            buffer += std::to_string(va.losses);
            buffer += ",'";
            buffer += va.convert_date_balance_to_str_json();
            buffer += "'";
            buffer += "); ";
            buffer += "SELECT * from virtual_accounts";


            std::lock_guard<std::mutex> lock2(callback_virtual_accounts_mutex);
            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                std::cout << "Records created successfully" << std::endl;
            }

            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock3(virtual_accounts_mutex);
            virtual_accounts = callback_virtual_accounts;
            return true;
        }

        bool update_virtual_account(const VirtualAccount &va) {
            std::lock_guard<std::mutex> lock(va_editot_mutex);

            if(!db) return false;
            if(is_error) return false;
            std::string buffer("UPDATE virtual_accounts SET ");
            buffer += "holder_name = '";
            buffer += va.holder_name;
            buffer += "', note = '";
            buffer += va.note;
            buffer += "', start_balance = ";
            buffer += std::to_string(va.start_balance);
            buffer += ", balance = ";
            buffer += std::to_string(va.balance);
            buffer += ", absolute_stop_loss = ";
            buffer += std::to_string(va.absolute_stop_loss);
            buffer += ", absolute_take_profit = ";
            buffer += std::to_string(va.absolute_take_profit);
            buffer += ", kelly_attenuation_multiplier = ";
            buffer += std::to_string(va.kelly_attenuation_multiplier);
            buffer += ", kelly_attenuation_limiter = ";
            buffer += std::to_string(va.kelly_attenuation_limiter);
            buffer += ", payout_limiter = ";
            buffer += std::to_string(va.payout_limiter);
            buffer += ", winrate_limiter = ";
            buffer += std::to_string(va.winrate_limiter);
            buffer += ", list_strategies = '";
            for(auto strategy_name : va.list_strategies) {
                buffer += strategy_name;
                buffer += ",";
            }
            buffer += "', demo = ";
            if(va.demo) buffer += "1, enabled = ";
            else buffer += "0, enabled = ";
            if(va.enabled) buffer += "1, start_timestamp = ";
            else buffer += "0, start_timestamp = ";
            buffer += std::to_string(va.start_timestamp);
            buffer += ", timestamp = ";
            buffer += std::to_string(va.timestamp);
            buffer += ", wins = ";
            buffer += std::to_string(va.wins);
            buffer += ", losses = ";
            buffer += std::to_string(va.losses);
            buffer += ", json = '";
            buffer += va.convert_date_balance_to_str_json();
            buffer += "'";
            buffer += " WHERE id = ";
            buffer += std::to_string(va.va_id);
            buffer += "; SELECT * from virtual_accounts";

            std::lock_guard<std::mutex> lock2(callback_virtual_accounts_mutex);
            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                std::cout << "UPDATE created successfully" << std::endl;
            }

            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock3(virtual_accounts_mutex);
            virtual_accounts = callback_virtual_accounts;
            return true;
        }

        bool delete_virtual_account(const uint64_t va_id) {
            std::lock_guard<std::mutex> lock(va_editot_mutex);

            if(!db) return false;
            if(is_error) return false;
            std::string buffer("DELETE from virtual_accounts WHERE id = ");
            buffer += std::to_string(va_id);
            buffer += "; SELECT * from virtual_accounts";


            std::lock_guard<std::mutex> lock2(callback_virtual_accounts_mutex);
            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "VirtualAccounts SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                //std::cout << "VirtualAccounts DELETE created successfully" << std::endl;
            }

            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock3(virtual_accounts_mutex);
            virtual_accounts = callback_virtual_accounts;
            return true;
        }

        bool delete_virtual_account(const VirtualAccount &va) {
            return delete_virtual_account(va.va_id);
        }

        inline std::map<uint64_t, VirtualAccount> get_virtual_accounts() {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);
            return virtual_accounts;
        }


        /** \brief Рассчитать размер ставки
         *
         * \param amount Посчитанный размер ставки
         * \param strategy_name Имя стратегии
         * \param demo Использовать демо аккаунт
         * \param payout Процент выплаты брокера
         * \param winrate Винрейт
         * \param kelly_attenuation Коэффициент ослабления Келли
         * \return Вернет true в случае успеха
         */
        bool calc_amount(double &amount,
                         const std::string &strategy_name,
                         const bool demo,
                         const double payout,
                         const double winrate,
                         const double kelly_attenuation) {
            amount = 0.0d;
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            if(virtual_accounts.size() == 0) return false;
            double sum_amount = 0.0d;
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    double temp = 0.0d;
                    if(it.second.calc_amount(
                        temp,
                        strategy_name,
                        demo,
                        payout,
                        winrate,
                        kelly_attenuation)) {
                        sum_amount += temp;
                    }
                }
            }
            if(sum_amount > 0.0d) {
                amount = sum_amount;
                return true;
            }
            return false;
        };

        /** \brief Сделать ставку
         *
         * \param id_deal Уникальный номер сделки
         * \param amount Посчитанный размер ставки
         * \param strategy_name Имя стратегии
         * \param demo Использовать демо аккаунт
         * \param payout Процент выплаты брокера
         * \param winrate Винрейт
         * \param kelly_attenuation Коэффициент ослабления Келли
         * \param date Метка времени даты
         * \param precision Точность, указывать число кратное 10
         * \return Вернет true в случае успеха
         */
        bool make_bet(const uint64_t id_deal,
                      const double amount,
                      const std::string &strategy_name,
                      const bool demo,
                      const double payout,
                      const double winrate,
                      const double kelly_attenuation,
                      const xtime::timestamp_t date,
                      const uint64_t precision = 2) {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            if(virtual_accounts.size() == 0) return false;

            /* найдем ставку для каждого аккаунта */
            double sum_amount = 0.0d;
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    double temp = 0.0d;
                    if(it.second.calc_amount(
                        temp,
                        strategy_name,
                        demo,
                        payout,
                        winrate,
                        kelly_attenuation)) {
                        sum_amount += temp;
                    }
                }
            }
            if(sum_amount == 0.0d) return false;

            const uint64_t factor = std::pow(10, precision);

            const double coarsening_sum_amount = (double)((uint64_t)(sum_amount * (double)factor)) / (double)factor;
            const double coarsening_sum_profit = (double)((uint64_t)(coarsening_sum_amount * payout * (double)factor)) / (double)factor;

            const double error_sum_amount = std::abs(sum_amount - coarsening_sum_amount);
            const double error_sum_profit = std::abs((sum_amount * payout) - coarsening_sum_profit);

            /* выведем соотношение от общей ставки для каждого аккаунта */
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    double temp = 0.0d;
                    if(it.second.calc_amount(
                        temp,
                        strategy_name,
                        demo,
                        payout,
                        winrate,
                        kelly_attenuation)) {
                        const double p = temp / sum_amount;
                        temp = p * amount - p * error_sum_amount;
                        it.second.balance -= temp;
                        it.second.mem_amount[id_deal] = temp;
                        it.second.mem_profit[id_deal] = temp * payout - p * error_sum_profit;
                        it.second.timestamp = date;
                        it.second.date_balance[xtime::get_first_timestamp_day(date)] = it.second.balance;
                    } // if
                }
            }

            return true;
        }

        /** \brief Установить выиграш ставки
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \param callback Функция для обратного вызова
         * \return Вернет true в случае успеха
         */
        bool set_win(
                const uint64_t id_deal,
                const xtime::timestamp_t date,
                std::function<void(const VirtualAccount &va)> callback = nullptr) {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            for(auto &it : virtual_accounts) {
                auto it_amount = it.second.mem_amount.find(id_deal);
                auto it_profit = it.second.mem_profit.find(id_deal);

                if((it_amount != it.second.mem_amount.end() &&
                    it_profit != it.second.mem_profit.end()) &&
                    it.second.enabled &&
                    it_amount->second > 0.0d &&
                    it_profit->second > 0.0d) {
                    it.second.balance += it_amount->second;
                    it.second.balance += it_profit->second;
                    it.second.timestamp = date;
                    it.second.wins++;
                    it.second.date_balance[xtime::get_first_timestamp_day(date)] = it.second.balance;
                    if(callback != nullptr) callback(it.second);
                }

                if(it_amount != it.second.mem_amount.end()) it.second.mem_amount.erase(id_deal);
                if(it_profit != it.second.mem_profit.end()) it.second.mem_profit.erase(id_deal);
            }

            return true;
        }

        /** \brief Установить проигрыш ставки
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \param callback Функция для обратного вызова
         * \return Вернет true в случае успеха
         */
        bool set_loss(
                const uint64_t id_deal,
                const xtime::timestamp_t date,
                std::function<void(const VirtualAccount &va)> callback = nullptr) {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            for(auto &it : virtual_accounts) {
                auto it_amount = it.second.mem_amount.find(id_deal);
                auto it_profit = it.second.mem_profit.find(id_deal);

                if((it_amount != it.second.mem_amount.end() &&
                    it_profit != it.second.mem_profit.end()) &&
                    it.second.enabled &&
                    it_amount->second > 0.0d &&
                    it_profit->second > 0.0d) {
                    it.second.timestamp = date;
                    it.second.losses++;
                    it.second.date_balance[xtime::get_first_timestamp_day(date)] = it.second.balance;
                    if(callback != nullptr) callback(it.second);
                }

                if(it_amount != it.second.mem_amount.end()) it.second.mem_amount.erase(id_deal);
                if(it_profit != it.second.mem_profit.end()) it.second.mem_profit.erase(id_deal);
            }

            return true;
        }

        /** \brief Установить ничью
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \param callback Функция для обратного вызова
         * \return Вернет true в случае успеха
         */
        bool set_standoff(
                const uint64_t id_deal,
                const xtime::timestamp_t date,
                std::function<void(const VirtualAccount &va)> callback = nullptr) {
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            for(auto &it : virtual_accounts) {
                auto it_amount = it.second.mem_amount.find(id_deal);
                auto it_profit = it.second.mem_profit.find(id_deal);

                if((it_amount != it.second.mem_amount.end() &&
                    it_profit != it.second.mem_profit.end()) &&
                    it.second.enabled &&
                    it_amount->second > 0.0d &&
                    it_profit->second > 0.0d) {
                    it.second.balance += it_amount->second;
                    it.second.timestamp = date;
                    it.second.losses++;
                    it.second.date_balance[xtime::get_first_timestamp_day(date)] = it.second.balance;
                    if(callback != nullptr) callback(it.second);
                }
                if(it_amount != it.second.mem_amount.end()) it.second.mem_amount.erase(id_deal);
                if(it_profit != it.second.mem_profit.end()) it.second.mem_profit.erase(id_deal);
            }

            return true;
        }

        /** \brief Загрузить изменения виртуальных счетов в базу данных
         *
         * \param is_wait Флаг ожидания результата
         */
        void push(const bool is_wait = false) {
            is_update_va_completed = false;
            is_update_virtual_accounts = true;
            while(is_wait && !is_update_va_completed && !is_stop_command) {
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        /** \brief Получить усиление за день
         *
         * \param date Дата
         * \param demo Демо счет
         * \param callback Функция обратного вызова
         */
        void get_gain_per_day(
                const xtime::timestamp_t date,
                const bool demo,
                std::function<void(
                    const uint64_t va_id,
                    const std::string &holder_name,
                    const double gain)> callback) {
            if(callback == nullptr) return;
            /* блокируем доступ к virtual_accounts из других потоков */
            std::lock_guard<std::mutex> lock(virtual_accounts_mutex);

            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    /* если данных баланса по дням нет, значит торговли не было */
                    if(it.second.date_balance.size() == 0) {
                        callback(it.second.va_id, it.second.holder_name, 1.0d);
                        continue;
                    }

                    auto it_date = it.second.date_balance.find(xtime::get_first_timestamp_day(date));
                    /* за указанный день не было торгов, то значит усилени единичное */
                    if(it_date == it.second.date_balance.end()) {
                        callback(it.second.va_id, it.second.holder_name, 1.0d);
                        continue;
                    }
                    /* если это первый торговый день, то усиление измеряется относительно стартвого баланса */
                    if(it_date == it.second.date_balance.begin()) {
                        const double gain = it.second.start_balance == 0.0 ? 1.0 : it_date->second / it.second.start_balance;
                        callback(it.second.va_id, it.second.holder_name, gain);
                        continue;
                    }
                    auto it_date_start = it_date;
                    std::advance(it_date_start, -1);
                    const double gain = it_date_start->second == 0.0 ? 1.0 : it_date->second / it_date_start->second;
                    callback(it.second.va_id, it.second.holder_name, gain);
                }
            }
        }

    };
};

#endif // OPEN_BO_API_VIRTUAL_ACCOUNT_HPP_INCLUDED
