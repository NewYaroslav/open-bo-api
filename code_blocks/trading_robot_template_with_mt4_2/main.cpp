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
#include <csignal>

using json = nlohmann::json;
/* тип индикатора RSI, этот индиатор использует внутри себя SMA и тип данных double */
using RSI_TYPE = xtechnical_indicators::RSI<double,xtechnical_indicators::SMA<double>>;

/* класс с настройками */
open_bo_api::Settings settings;

/* обработчики сигналов */
void signal_handler_termination(int signal);
void signal_handler_abnormal(int signal);
void signal_handler_external_interrupt(int signal);
void signal_handler_erroneous_arithmetic_operation(int signal);
void signal_handler_invalid_memory_access(int signal);
void atexit_handler();

int main(int argc, char **argv) {
    std::signal(SIGTERM, signal_handler_termination);
    std::signal(SIGABRT, signal_handler_abnormal);
    std::signal(SIGINT, signal_handler_external_interrupt);
    std::signal(SIGFPE, signal_handler_erroneous_arithmetic_operation);
    std::signal(SIGSEGV, signal_handler_invalid_memory_access);
    atexit(atexit_handler);

    std::cout << "running an example robot template" << std::endl;

#if(0)
    /* обрабатываем аргументы командой строки */
    if(!open_bo_api::process_arguments(
            argc,
            argv,
            [&](
                const std::string &key,
                const std::string &value) {
        /* аргумент json_file указываает на файл с настройками json */
        if(key == "json_file" || key == "jf") {
            settings.json_file_name = value;
        }
    })) {
        std::cerr << "Error! No parameters!" << std::endl;
        return EXIT_FAILURE;
    }
#else
    settings.json_file_name = "auth.json";
#endif

    /* читаем настройки из файла JSON */
    json json_settings; // JSON структура с настройками
    if(!open_bo_api::open_json_file(settings.json_file_name, json_settings)) return EXIT_FAILURE;
    settings.init(json_settings);
    if(settings.check_error()) return EXIT_FAILURE;

    /* Переменные для торговой стратегии */
    std::vector<std::string> intrade_bar_symbols = open_bo_api::IntradeBar::get_list_symbols(); // массив имен символов брокера IntradeBar
    std::map<std::string, std::map<uint32_t, RSI_TYPE>> rsi_indicators; // массив индикаторов RSI
    std::vector<uint32_t> rsi_periods = open_bo_api::get_list_parameters<uint32_t>(10, 100, 1); // массив периодов индикатора, от 10 до 100 включительно с шагом 1
    open_bo_api::init_indicators<RSI_TYPE>(
            intrade_bar_symbols,
            rsi_periods,
            rsi_indicators); // инициализируем индикаторы RSI

    /* параметры для торговли по стратегии */
    const uint32_t PERIOD_RSI = 10; // возьмем для нашей стратегии период индикатора 10 минут
    const uint32_t DURATION = 180;  // размер ставки 180 секунд или 3 минуты
    const double WINRATE = 0.56;    // винрейт стратегии
    const double KELLY_ATTENUATOR = 0.4;

    /* инициализируем список новостей */
    open_bo_api::News::async_update(xtime::get_timestamp(), settings.news_sert_file);

    /* API для работы с брокером intrade.bar */
    open_bo_api::IntradeBar::Api intrade_bar_api(
            settings.intrade_bar_number_bars,
            nullptr,
            false,
            settings.intrade_bar_sert_file,
            settings.intrade_bar_cookie_file,
            settings.intrade_bar_bets_log_file,
            settings.intrade_bar_work_log_file,
            settings.intrade_bar_websocket_log_file);

    /* создаем мост между метатрейдером и программой,
     * получаем в отдельном потоке тики котировок и исторические данные брокера
     */
    open_bo_api::MtBridge MetaTrader(
            settings.mt_bridge_port,
            settings.intrade_bar_number_bars,
                    [&](const std::map<std::string, open_bo_api::Candle> &candles,
                        const open_bo_api::MtBridge::EventType event,
                        const xtime::timestamp_t timestamp) {
        /* вспомогательные переменные */
        const uint32_t second = xtime::get_second_minute(timestamp); // секунда минуты
        static bool is_block_open_bo = false;
        static bool is_block_open_bo_one_deal = false;

        /* проходим в цикле по всем символа брокера Intradebar */
        for(size_t symbol = 0; symbol < intrade_bar_symbols.size(); ++symbol) {
            std::string &symbol_name = intrade_bar_symbols[symbol];

            /* получаем бар */
            open_bo_api::Candle candle = open_bo_api::IntradeBar::Api::get_candle(symbol_name, candles);

            switch(event) {
            /* получено событие "ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ" */
            case open_bo_api::MtBridge::EventType::HISTORICAL_DATA_RECEIVED:
                is_block_open_bo = false;
                is_block_open_bo_one_deal = false;
                /* проверяем бар на наличие данных */
                if(!open_bo_api::IntradeBar::Api::check_candle(candle)) {
                    /* данных нет, очищаем внутреннее состояние индикатора */
                    rsi_indicators[symbol_name][PERIOD_RSI].clear();
                    std::cerr << "check candle error" << std::endl;
                    break;
                }
                /* обновляем состояние всех индикаторов*/
                for(auto &item : rsi_indicators[symbol_name]) {
                    item.second.update(candle.close);
                }
                break;
            /* получено событие "НОВЫЙ ТИК" */
            case open_bo_api::MtBridge::EventType::NEW_TICK:
                /* получаем метку времени начала бара */
                const xtime::timestamp_t bar_opening_timestamp = second == 0 ?
                        timestamp - xtime::SECONDS_IN_MINUTE :
                        timestamp;

                /* ждем 0 или 59 секунду бара */
                if(second == 59 || second == 0) {
                    /* блокируем открытие сделки, т.к. мы решили в одну минуту делать не более 1 сделки */
                    if(is_block_open_bo_one_deal) break;

                    /* получаем размер ставки и процент выплат */
                    double amount = 0, payout = 0;
                    int err_open_bo = open_bo_api::IntradeBar::get_amount(
                        amount,
                        payout,
                        symbol_name,
                        timestamp,
                        DURATION,
                        intrade_bar_api.account_rub_currency(),
                        intrade_bar_api.get_balance(),
                        WINRATE,
                        KELLY_ATTENUATOR);

                    /* если нет процента выплат, пропускаем сделку */
                    if(err_open_bo != open_bo_api::IntradeBar::ErrorType::OK) {
                        const std::string message(
                            symbol_name +
                            " low payout: " + std::to_string(payout) +
                            ", date:" + xtime::get_str_date_time(timestamp));
                        std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        break;
                    }

                    /* проверяем 59 секунду */
                    if(xtime::get_second_minute(timestamp) == 59) {
                        double amount_next = 0, payout_next = 0;
                        err_open_bo = open_bo_api::IntradeBar::get_amount(
                            amount_next,
                            payout_next,
                            symbol_name,
                            timestamp + 1,
                            DURATION,
                            intrade_bar_api.account_rub_currency(),
                            intrade_bar_api.get_balance(),
                            WINRATE,
                            KELLY_ATTENUATOR);
                        /* если на 59 секунде процент выплат хороший, то пропускаем 59 секунду */
                        if(err_open_bo == open_bo_api::IntradeBar::ErrorType::OK && payout <= payout_next) break;
                        /* на 59 секунде условия лучше, чем на 0 секунде */
                        is_block_open_bo = true;
                    } else {
                        /* пропускаем 0 секунду, так как была сделка на 59 секунде */
                        if(is_block_open_bo) break;
                    }

                    /* проверяем бар на наличие данных */
                    if(!open_bo_api::IntradeBar::Api::check_candle(candle)) {
                        std::cerr << "check candle error" << std::endl;
                        break;
                    }

                    /* обновляем состояние индикатора */
                    double rsi_out = 50;
                    int err = rsi_indicators[symbol_name][PERIOD_RSI].test(candle.close, rsi_out);
                    if(err != xtechnical_common::OK) break;

                    /* реализуем простую стратегию */
                    if(rsi_out > 70) {
                        /* фильтр новостей */
                        if(open_bo_api::News::check_news_filter(
                            symbol_name,
                            bar_opening_timestamp,
                            xtime::SECONDS_IN_MINUTE * 30,
                            xtime::SECONDS_IN_MINUTE * 30,
                            false,
                            false,
                            true,
                            true)) {
                            const std::string message(symbol_name + " SELL signal filtered by news filter: " + xtime::get_str_date_time(timestamp));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            break;
                        }

                        /* открываем сделку */
                        uint64_t api_bet_id = 0;
                        int err = intrade_bar_api.open_bo(
                            symbol_name,
                            "RSI",
                            amount,
                            intrade_bar_common::SELL,
                            DURATION,
                            api_bet_id,
                            [=](const open_bo_api::IntradeBar::Api::Bet &bet) {
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::WIN) {
                                const std::string message(
                                    symbol_name +
                                    " SELL, status: WIN, broker id: " + std::to_string(bet.broker_bet_id) +
                                    ", api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            } else
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::LOSS) {
                                const std::string message(
                                    symbol_name +
                                    " SELL, status: LOSS, broker id: " + std::to_string(bet.broker_bet_id) +
                                    ", api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            } else
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::OPENING_ERROR) {
                                const std::string message(
                                    symbol_name +
                                    " SELL, status: ERROR, api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            }
                        });
                        if(err != open_bo_api::IntradeBar::ErrorType::OK) {
                            const std::string message(
                                    symbol_name +
                                    " BUY, status: ERROR, code: " + std::to_string(err));
                                std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else {
                            const std::string message(
                                symbol_name +
                                " SELL deal open, amount: " + std::to_string(amount) +
                                ", api id: " + std::to_string(api_bet_id) +
                                ", date: " + xtime::get_str_date_time(timestamp));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            is_block_open_bo_one_deal = true;
                        }
                    } else
                    if(rsi_out < 30) {
                        /* фильтр новостей */
                        if(open_bo_api::News::check_news_filter(
                            symbol_name,
                            bar_opening_timestamp,
                            xtime::SECONDS_IN_MINUTE * 30,
                            xtime::SECONDS_IN_MINUTE * 30,
                            false,
                            false,
                            true,
                            true)) {
                            const std::string message(symbol_name + " BUY signal filtered by news filter: " + xtime::get_str_date_time(timestamp));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            break;
                        }

                        /* открываем сделку */
                        uint64_t api_bet_id = 0;
                        int err = intrade_bar_api.open_bo(
                            symbol_name,
                            "RSI",
                            amount,
                            intrade_bar_common::BUY,
                            DURATION,
                            api_bet_id,
                            [=](const open_bo_api::IntradeBar::Api::Bet &bet){
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::WIN) {
                                const std::string message(
                                    symbol_name +
                                    " BUY, status: WIN, broker id: " + std::to_string(bet.broker_bet_id) +
                                    ", api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            } else
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::LOSS) {
                                const std::string message(
                                    symbol_name +
                                    " BUY, status: LOSS, broker id: " + std::to_string(bet.broker_bet_id) +
                                    ", api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            } else
                            if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::OPENING_ERROR) {
                                const std::string message(
                                    symbol_name +
                                    " BUY, status: ERROR, api id: " + std::to_string(bet.api_bet_id));
                                std::cout << message << std::endl;
                                open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            }
                        });
                        if(err != open_bo_api::IntradeBar::ErrorType::OK) {
                            const std::string message(
                                    symbol_name +
                                    " BUY, status: ERROR, code: " + std::to_string(err));
                                std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else {
                            const std::string message(
                                symbol_name +
                                " BUY deal open, amount: " + std::to_string(amount) +
                                ", api id: " + std::to_string(api_bet_id) +
                                ", date: " + xtime::get_str_date_time(timestamp));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                            is_block_open_bo_one_deal = true;
                        }
                    }
                }
                break;
            };
        } // for symbol

        /* загружаем новости */
        if(event == open_bo_api::MtBridge::EventType::NEW_TICK && second == 0) {
            open_bo_api::News::async_update(timestamp);
        }
    });

    /* подключаемся к брокеру, чтобы можно было совершать сделки */
    int err_connect = intrade_bar_api.connect(
        settings.intrade_bar_email,
        settings.intrade_bar_password,
        settings.is_intrade_bar_demo_account,
        settings.is_intrade_bar_rub_currency);
    if(err_connect != open_bo_api::IntradeBar::ErrorType::OK) {
        std::cerr << "error connecting to broker intrade.bar" << std::endl;
        const std::string err_message("error connecting to broker intrade.bar, code: " + std::to_string(err_connect));
        open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
        return EXIT_FAILURE;
    }

    /* выводим некоторую информацию об аккаунте */
    std::cout << "balance: " << intrade_bar_api.get_balance() << std::endl;
    std::cout << "is demo: " << intrade_bar_api.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << intrade_bar_api.account_rub_currency() << std::endl;

    /* тут обрабатываем ошибки и прочее */
    while(true) {
        if(open_bo_api::News::check_error()) {
            const std::string err_message("error downloading news");
            std::cerr << err_message << std::endl;
            open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
            return EXIT_FAILURE;
        }
        std::this_thread::yield();
    }
    return EXIT_SUCCESS;
}

void signal_handler_termination(int signal) {
    const std::string err_message("termination request, sent to the program, code: " + std::to_string(signal));
    std::cerr << err_message << std::endl;
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    exit(EXIT_FAILURE);
}

void signal_handler_abnormal(int signal) {
    const std::string err_message("abnormal termination condition, as is e.g. initiated by std::abort(), code: " + std::to_string(signal));
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    std::cerr << err_message << std::endl;
    exit(EXIT_FAILURE);
}

void signal_handler_external_interrupt(int signal) {
    const std::string err_message("external interrupt, usually initiated by the user, code: " + std::to_string(signal));
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    std::cerr << err_message << std::endl;
    exit(EXIT_FAILURE);
}

void signal_handler_erroneous_arithmetic_operation(int signal) {
    const std::string err_message("erroneous arithmetic operation such as divide by zero, code: " + std::to_string(signal));
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    std::cerr << err_message << std::endl;
    exit(EXIT_FAILURE);
}

void signal_handler_invalid_memory_access(int signal) {
    const std::string err_message("invalid memory access (segmentation fault), code: " + std::to_string(signal));
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    std::cerr << err_message << std::endl;
    exit(EXIT_FAILURE);
}

void atexit_handler() {
    const std::string err_message("program closure");
    open_bo_api::Logger::log(settings.trading_robot_work_log_file, err_message);
    std::cerr << err_message << std::endl;
}
