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
#include "xtime.hpp"
#include "sqlite3.h"

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

        uint64_t wins = 0;
        uint64_t losses = 0;

        xtime::timestamp_t start_timestamp = 0; /**< Дата начала торговли */
        xtime::timestamp_t timestamp = 0;       /**< Последняя дата обновления баланса */

        std::set<std::string> list_strategies;  /**< Список используемых стратегий */

        bool demo = true;       /**< Использовать демо-аккаунт */
        bool enabled = false;   /**< Виртуальный аккаунт включен или выключен */

        std::map<uint64_t, std::vector<double>> mem_profit;
        std::map<uint64_t, std::vector<double>> mem_amount;

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
            const double calc_risk = (((1.0d + calc_payout) * winrate - 1.0d) / calc_payout) * calc_kelly_attenuation;
            amount = balance * calc_risk;
            return true;
        }

        double get_winrate() {
            const uint64_t sum = wins + losses;
            return sum == 0 ? 0.0d : (double)wins / (double)sum;
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
            parse_list(argv[10], va.list_strategies);
            va.demo = atoi(argv[11]) == 0 ? false : true;
            va.enabled = atoi(argv[12]) == 0 ? false : true;
            va.start_timestamp = atoll(argv[13]);
            va.timestamp = atoll(argv[14]);
            va.wins = atoll(argv[15]);
            va.losses = atoll(argv[16]);
            callback_virtual_accounts[va_id] = va;
        }

    public:

        static int callback(void *userdata, int argc, char **argv, char **key_name) {
            VirtualAccounts* app = static_cast<VirtualAccounts*>(userdata);
            if(app) app->va_callback(argc, argv, key_name);
            if(0) {
                for(int i = 0; i< argc; i++) {
                    printf("%s = %s\n", key_name[i], argv[i] ? argv[i] : "NULL");
                }
                printf("\n");
            }
            return 0;
        }

        /** \brief Инициализация класса
         *
         * \param database_name Файл базы данных виртуальных аккаунтов
         */
        VirtualAccounts(const std::string database_name) {
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
                "list_strategies                TEXT    NOT NULL,"
                "demo               INTEGER NOT NULL,"
                "enabled            INTEGER NOT NULL,"
                "start_timestamp    INTEGER NOT NULL,"
                "timestamp          INTEGER NOT NULL,"
                "wins               INTEGER NOT NULL,"
                "losses             INTEGER NOT NULL ); ";
            /* открываем и возможно еще создаем таблицу */
            if(sqlite3_open(database_name.c_str(), &db) != SQLITE_OK) {
                std::cerr << "Error opening / creating a database: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_close(db);
                db = 0;
                is_error = true;
                return;
            } else
            /* создаем таблицу в базе данных, если она еще не создана */
            if(sqlite3_exec(db, create_table_sql, callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                sqlite3_close(db);
                db = 0;
                is_error = true;
                return;
            } else {
                std::cout << "Table created successfully" << std::endl;
            }

            /* читаем данные */
            const char *select_sql = "SELECT * from virtual_accounts";
            if(sqlite3_exec(db, select_sql, callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                sqlite3_close(db);
                db = 0;
                is_error = true;
                return;
            } else {
                std::cout << "Operation done successfully" << std::endl;
            }

            virtual_accounts = callback_virtual_accounts;
        };

        ~VirtualAccounts() {
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
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) sum_balance += it.second.balance;
            }
            if(sum_balance > total_balance) return false;
            return true;
        }

        bool add_virtual_account(VirtualAccount &va) {
            if(!db) return false;
            if(is_error) return false;
            std::string buffer("INSERT INTO virtual_accounts ("
                "id,holder_name,note,start_balance,balance,"
                "absolute_stop_loss,absolute_take_profit,"
                "kelly_attenuation_multiplier,kelly_attenuation_limiter,"
                "payout_limiter,list_strategies,"
                "demo,enabled,start_timestamp,timestamp,"
                "wins,losses) ");
            va.va_id = 0;
            if(!virtual_accounts.empty()) {
                auto it = virtual_accounts.begin();
                auto it_end = std::next(it, virtual_accounts.size() - 1);
                va.va_id = it_end->first;
                ++va.va_id;
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
            buffer += "); ";
            buffer += "SELECT * from virtual_accounts";

            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                std::cout << "Records created successfully" << std::endl;
            }

            virtual_accounts = callback_virtual_accounts;

            return true;
        }

        bool update_virtual_account(const VirtualAccount &va) {
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
            buffer += " WHERE id = ";
            buffer += std::to_string(va.va_id);
            buffer += "; SELECT * from virtual_accounts";

            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                std::cout << "UPDATE created successfully" << std::endl;
            }

            virtual_accounts = callback_virtual_accounts;

            return true;
        }

        bool delete_virtual_account(const uint64_t va_id) {
            if(!db) return false;
            if(is_error) return false;
            std::string buffer("DELETE from virtual_accounts WHERE id = ");
            buffer += std::to_string(va_id);
            buffer += "; SELECT * from virtual_accounts";

            callback_virtual_accounts.clear();

            char *err = 0;
            if(sqlite3_exec(db, buffer.c_str(), callback, this, &err) != SQLITE_OK) {
                std::cerr << "SQL error: " << err << std::endl;
                sqlite3_free(err);
                return false;
            } else {
                std::cout << "DELETE created successfully" << std::endl;
            }

            virtual_accounts = callback_virtual_accounts;
            return true;
        }

        bool delete_virtual_account(const VirtualAccount &va) {
            return delete_virtual_account(va.va_id);
        }

        inline std::map<uint64_t, VirtualAccount> get_virtual_accounts() {
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
                         const std::string strategy_name,
                         const bool demo,
                         const double payout,
                         const double winrate,
                         const double kelly_attenuation) {
            amount = 0.0d;
            if(virtual_accounts.size() == 0) return false;
            double sum_amount = 0.0d;
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    double temp_amount = 0.0d;
                    if(it.second.calc_amount(
                        temp_amount,
                        strategy_name,
                        is_demo,
                        payout,
                        winrate,
                        kelly_attenuation)) {
                        sum_amount += temp_amount;
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
         * \return Вернет true в случае успеха
         */
        bool make_bet(const uint64_t id_deal,
                      const double amount,
                      const std::string strategy_name,
                      const bool demo,
                      const double payout,
                      const double winrate,
                      const double kelly_attenuation,
                      const xtime::timestamp_t date) {
            if(virtual_accounts.size() == 0) return false;
            /* найдем ставку для каждого аккаунта */
            std::vector<double> temp_amount(virtual_accounts.size(), 0.0d);
            double sum_amount = 0.0d;
            for(auto &it : virtual_accounts) {
                if(it.second.enabled && it.second.demo == demo) {
                    double temp = 0.0d;
                    if(it.second.calc_amount(
                        temp,
                        strategy_name,
                        is_demo,
                        payout,
                        winrate,
                        kelly_attenuation)) {
                        it.second.mem_amount[id_deal] = temp;
                        sum_amount += temp;
                    }
                }
            }
            if(sum_amount == 0.0d) return false;

            /* выведем соотношение от общей ставки для каждого аккаунта */
            for(auto &it : virtual_accounts) {
                double temp = it.second.mem_amount[id_deal];
                if(it.second.enabled && it.second.demo == demo && temp > 0.0d) {
                    temp = (temp / sum_amount) * amount;
                    it.second.balance -= temp[i];
                    it.second.mem_amount[id_deal] = temp;
                    it.second.mem_profit[id_deal] = temp * payout;
                    it.second.timestamp = date;
                }
            }

            /* обновляем данные в файлах */
            return true;
        }

        /** \brief Установить выиграш ставки
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \return Вернет true в случае успеха
         */
        bool set_win(
                const uint64_t id_deal,
                const xtime::timestamp_t date) {
            bool is_update = false;
            for(auto &it : virtual_accounts) {
                double amount = it.second.mem_amount[id_deal];
                double profit = it.second.mem_profit[id_deal];
                if(it.second.enabled && amount > 0.0d && profit > 0.0d) {
                    it.second.balance += amount;
                    it.second.balance += profit;
                    it.second.mem_amount.erase(id_deal);
                    it.second.mem_profit.erase(id_deal);
                    it.second.timestamp = date;
                    is_update = true;
                }
            }

            /* обновляем данные в файлах */
            if(is_update) {

            }
            return true;
        }

        /** \brief Установить проигрыш ставки
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \return Вернет true в случае успеха
         */
        bool set_loss(
                const uint64_t id_deal,
                const xtime::timestamp_t date) {
            bool is_update = false;
            for(auto &it : virtual_accounts) {
                double amount = it.second.mem_amount[id_deal];
                double profit = it.second.mem_profit[id_deal];
                if(it.second.enabled && amount > 0.0d && profit > 0.0d) {
                    it.second.timestamp = date;
                    is_update = true;
                }
                it.second.mem_amount.erase(id_deal);
                it.second.mem_profit.erase(id_deal);
            }

            /* обновляем данные в файлах */
            if(is_update) {

            }
            return true;
        }

        /** \brief Установить ничью
         *
         * \param id_deal Уникальный номер сделки
         * \param date Метка времени даты
         * \return Вернет true в случае успеха
         */
        bool set_standoff(
                const uint64_t id_deal,
                const xtime::timestamp_t date) {
            bool is_update = false;
            for(auto &it : virtual_accounts) {
                auto it_amount = it.second.mem_amount.find(id_deal);
                auto it_profit = it.second.mem_profit.find(id_deal);
                if(it_amount != it.second.mem_amount.end() && it_profit != it.second.mem_profit.end())
                if((it_amount != it.second.mem_amount.end() &&
                    it_profit != it.second.mem_profit.end()) &&
                    it.second.enabled &&
                    it_amount.second > 0.0d &&
                    it_profit.second > 0.0d) {
                    it.second.balance += it_amount.second;
                    it.second.timestamp = date;
                    is_update = true;
                }
                if(it_amount != it.second.mem_amount.end()) it.second.mem_amount.erase(id_deal);
                if(it_profit != it.second.mem_profit.end()) it.second.mem_profit.erase(id_deal);
            }

            /* обновляем данные в файлах */
            if(is_update) {

            }
            return true;
        }

    };
};

#endif // OPEN_BO_API_VIRTUAL_ACCOUNT_HPP_INCLUDED
