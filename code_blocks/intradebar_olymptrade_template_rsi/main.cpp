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
using RSI_TYPE = xtechnical_indicators::RSI<double, xtechnical_indicators::SMA<double>>;

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

    /* массив имен символов брокера IntradeBar */
    std::vector<std::string> intrade_bar_symbols = open_bo_api::IntradeBar::get_list_symbols();

    std::map<std::string, std::map<uint32_t, RSI_TYPE>> rsi_indicators; // массив индикаторов RSI
    std::vector<uint32_t> rsi_periods = open_bo_api::get_list_parameters<uint32_t>(10, 100, 1); // массив периодов индикатора, от 10 до 100 включительно с шагом 1
    std::map<std::string, std::map<uint32_t, double>> rsi_output;

    /* инициализируем индикаторы RSI */
    open_bo_api::init_indicators<RSI_TYPE>(
            intrade_bar_symbols,
            rsi_periods,
            rsi_indicators);

    /* параметры для торговли по стратегии */
    const uint32_t PERIOD_RSI = 10; // возьмем для нашей стратегии период индикатора 10 минут
    const uint32_t DURATION = 180;  // размер ставки 180 секунд или 3 минуты
    const double WINRATE = 0.56;    // винрейт стратегии
    const double KELLY_ATTENUATOR = 0.4;

    double total_balance = 0;
    double intrade_bar_balance = 0;
    double olymp_trade_balance = 0;

    /* инициализируем список новостей */
    open_bo_api::News::async_update(xtime::get_timestamp(), settings.news_sert_file);

    /* работает с брокером олимптрейд */
    open_bo_api::OlympTrade::Api olymp_trade_api(settings.olymp_trade_port);
    if(settings.is_olymp_trade) {
        if(!olymp_trade_api.wait()) return EXIT_FAILURE;
        olymp_trade_api.set_demo_account(settings.is_olymp_trade_demo_account);
    }

    /* получаем в отдельном потоке тики котировок и исторические данные брокера */
    open_bo_api::IntradeBar::Api intrade_bar_api(
                    settings.intrade_bar_number_bars,
                    [&](const std::map<std::string, open_bo_api::Candle> &candles,
                        const open_bo_api::IntradeBar::Api::EventType event,
                        const xtime::timestamp_t timestamp) {
        //
        //static bool is_block_open_bo = false;
        static bool is_block_open_bo_one_deal = false;

        const uint32_t second = xtime::get_second_minute(timestamp);

        /* обрабатываем все индикаторы */
        switch(event) {

        /* получен бар исторических данных для всех символов */
        case open_bo_api::IntradeBar::Api::EventType::HISTORICAL_DATA_RECEIVED:
            open_bo_api::update_indicators(
                candles,
                rsi_output,
                intrade_bar_symbols,
                rsi_periods,
                rsi_indicators,
                open_bo_api::TypePriceIndicator::CLOSE);

            intrade_bar_api.update_balance();

            //
            //is_block_open_bo = false;
            is_block_open_bo_one_deal = false;

            static double last_total_balance = total_balance;
            if(total_balance != last_total_balance) {
                std::cout << "total balance " << total_balance << std::endl;
                std::cout << "intrade.bar balance " << intrade_bar_balance << std::endl;
                std::cout << "olymp trade balance " << olymp_trade_balance << std::endl;
                last_total_balance = total_balance;
            }
            break;

        /* получен тик (секунда) новых данных для всех символов */
        case open_bo_api::IntradeBar::Api::EventType::NEW_TICK:

            /* ждем 59 секунду или начало минуты */
            if(second != 58 && second != 59 && second != 0) break;

            intrade_bar_balance = intrade_bar_api.get_balance();
            olymp_trade_balance = olymp_trade_api.get_balance();
            total_balance = intrade_bar_balance + olymp_trade_balance;

            /* тестируем индикаторы, результат тестирования в rsi_output*/
            open_bo_api::test_indicators(
                candles,
                rsi_output,
                intrade_bar_symbols,
                rsi_periods,
                rsi_indicators,
                open_bo_api::TypePriceIndicator::CLOSE);

            /* реализуем стратегию торговли RSI на конец бара с периодом 10.
             * Каждую минуту может быть не больше 1 сигнала (просто потому что для примера), если сигналов больше - пропускаем остальные
             */

            /* получаем метку времени начала бара
             * так как цена закрытия бара приходится на 0 секунду следующего бара,
             * иногда это нужно
             */
            const xtime::timestamp_t bar_opening_timestamp = second == 0 ?
                timestamp - xtime::SECONDS_IN_MINUTE :
                xtime::get_first_timestamp_minute(timestamp);

            for(size_t symbol = 0; symbol < intrade_bar_symbols.size(); ++symbol) {
                /* блокируем открытие сделки, т.к. мы решили в одну минуту делать не более 1 сделки */
                if(is_block_open_bo_one_deal) break;

#if(0)
                /* имя символа */
                std::string &symbol_name = intrade_bar_symbols[symbol];

                /* параметры для открытия сделки */
                open_bo_api::ListBrokers broker = open_bo_api::ListBrokers::NOT_SELECTED;
                double payout = 0, amount = 0;

                open_bo_api::calc_brokers_competition(
                    broker,
                    amount,
                    payout,
                    olymp_trade,
                    settings,
                    symbol_name,
                    timestamp,
                    DURATION,
                    intrade_bar_api.account_rub_currency(),
                    total_balance,
                    WINRATE,
                    KELLY_ATTENUATOR);

                /* получаем размер ставки и процент выплат */
                double intrade_bar_amount = 0, intrade_bar_payout = 0;
                int err_intrade_bar_open_bo =
                    open_bo_api::IntradeBar::get_amount(
                        intrade_bar_amount,
                        intrade_bar_payout,
                        symbol_name,
                        timestamp,
                        DURATION,
                        intrade_bar_api.account_rub_currency(),
                        total_balance,
                        WINRATE,
                        KELLY_ATTENUATOR);

                double olymp_trade_amount = 0, olymp_trade_payout = 0;
                int err_olymp_trade_open_bo =
                    open_bo_api::OlympTrade::get_amount(
                        olymp_trade_amount,
                        olymp_trade_payout,
                        olymp_trade,
                        symbol_name,
                        DURATION,
                        total_balance,
                        WINRATE,
                        KELLY_ATTENUATOR);

                double payout = 0, amount = 0;
                if(intrade_bar_payout > 0 && intrade_bar_payout >= olymp_trade_payout && intrade_bar_amount > 0) {
                    payout = intrade_bar_payout;
                    amount = intrade_bar_amount;
                    broker = open_bo_api::ListBrokers::INTRADE_BAR;
                } else
                if(olymp_trade_payout > 0 && olymp_trade_payout >= intrade_bar_payout && olymp_trade_amount > 0) {
                    payout = olymp_trade_payout;
                    amount = olymp_trade_amount;
                    broker = open_bo_api::ListBrokers::OLYMP_TRADE;
                } else {
                    amount = 0;
                    payout = std::max(olymp_trade_payout, intrade_bar_payout);
                }

                /* если нет процента выплат, пропускаем сделку */
                if(broker == open_bo_api::ListBrokers::NOT_SELECTED) {
                    const std::string message(
                        symbol_name +
                        " low payout: " + std::to_string(payout) +
                        ", date:" + xtime::get_str_date_time(timestamp));
                    std::cout << message << std::endl;
                    open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    continue;
                }

                /* проверяем 59 секунду */
                if(second == 59) {
                    double intrade_bar_amount_next = 0, intrade_bar_payout_next = 0;
                    int err_open_bo = open_bo_api::IntradeBar::get_amount(
                        intrade_bar_amount_next,
                        intrade_bar_payout_next,
                        symbol_name,
                        timestamp + 1,
                        DURATION,
                        intrade_bar_api.account_rub_currency(),
                        total_balance,
                        WINRATE,
                        KELLY_ATTENUATOR);
                    /* если на 59 секунде процент выплат хороший, то пропускаем 59 секунду */
                    if(err_open_bo == open_bo_api::IntradeBar::ErrorType::OK &&
                        intrade_bar_payout <= intrade_bar_payout_next &&
                        olymp_trade_payout <= intrade_bar_payout_next) break;
                    /* на 59 секунде условия лучше, чем на 0 секунде */
                    is_block_open_bo = true;
                } else {
                    /* пропускаем 0 секунду, так как была сделка на 59 секунде */
                    if(is_block_open_bo) break;
                }

                /* проверяем возможность совершения сделки */
                if(broker == open_bo_api::ListBrokers::INTRADE_BAR) {
                    if(amount >= intrade_bar_balance) {
                        std::cout
                            << "intrade.bar account balance is insufficient broker, amount: "
                            << amount
                            << " balance: "
                            << intrade_bar_balance
                            << std::endl;
                        continue;

                    }
                } else
                if(broker == open_bo_api::ListBrokers::OLYMP_TRADE) {
                    if(amount >= olymp_trade_balance) {
                        std::cout
                            << "olymptrade account balance is insufficient broker, amount: "
                            << amount
                            << " balance: "
                            << olymp_trade_balance
                            << std::endl;
                        continue;
                    }
                }

#endif

                /* имя символа */
                std::string &symbol_name = intrade_bar_symbols[symbol];

                /* получаем значение индикатора */
                double rsi_out = rsi_output[symbol_name][PERIOD_RSI];
                if(std::isnan(rsi_out)) continue;

                /* реализуем простую стратегию, когда RSI выходит за уровни 70 или 30 */
                const int BUY = 1;
                const int SELL = -1;
                const int NEUTRAL = 0;
                int strategy_state = NEUTRAL;
                if(rsi_out > 70) strategy_state = SELL;
                else if(rsi_out < 30) strategy_state = BUY;
                if(strategy_state == NEUTRAL) continue;

                std::string signal_type = strategy_state == SELL ? "SELL" : "BUY";

                /* фильтр новостей, фильтруем новости
                 * со средней и большой волатильностью в пределах 30 минут
                 */
                if(open_bo_api::News::check_news_filter(
                    symbol_name,
                    bar_opening_timestamp,
                    xtime::SECONDS_IN_MINUTE * 30,
                    xtime::SECONDS_IN_MINUTE * 30,
                    false,
                    false,
                    true,
                    true)) {
                    const std::string message(symbol_name + " " + signal_type + " signal filtered by news filter: " + xtime::get_str_date_time(timestamp));
                    std::cout << message << std::endl;
                    open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    continue;
                }

                /* параметры для открытия сделки */
                open_bo_api::ListBrokers broker = open_bo_api::ListBrokers::NOT_SELECTED;
                double payout = 0, amount = 0;

                /* получаем размер ставки, брокера и процент выплат */
                open_bo_api::StateCompetition state_competition = open_bo_api::calc_brokers_competition(
                        broker,
                        amount,
                        payout,
                        intrade_bar_api,
                        olymp_trade_api,
                        settings,
                        symbol_name,
                        timestamp,
                        DURATION,
                        intrade_bar_api.account_rub_currency(),
                        total_balance,
                        WINRATE,
                        KELLY_ATTENUATOR);

                if(state_competition == open_bo_api::StateCompetition::NO_BROKERS) {
                    std::cout << "NO_BROKERS" << std::endl;
                    continue;
                } else
                if(state_competition == open_bo_api::StateCompetition::WAIT_CLOSING_PRICE) {
                    std::cout << "WAIT_CLOSING_PRICE" << std::endl;
                    continue;
                } else
                if(state_competition == open_bo_api::StateCompetition::CANCEL) {
                    std::cout << "CANCEL" << std::endl;
                    continue;
                } else
                /* проверяем на низкий процент выплат */
                if(state_competition == open_bo_api::StateCompetition::LOW_PAYMENT) {
                    const std::string message(
                        symbol_name +
                        " low payout: " + std::to_string(payout) +
                        ", date:" + xtime::get_str_date_time(timestamp));
                    std::cout << message << std::endl;
                    open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    continue;
                } else
                /* проверяем на низкий баланс */
                if(state_competition == open_bo_api::StateCompetition::LOW_DEPOSIT_BALANCE) {
                    if(broker == open_bo_api::ListBrokers::INTRADE_BAR) {
                        const std::string message(
                            "intrade.bar account balance is insufficient broker, amount: " +
                            std::to_string(amount) +
                            " balance: " +
                            std::to_string(intrade_bar_balance) +
                            ", date:" + xtime::get_str_date_time(timestamp));
                        std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    } else
                    if(broker == open_bo_api::ListBrokers::OLYMP_TRADE) {
                        const std::string message(
                            "olymptrade account balance is insufficient broker, amount: " +
                            std::to_string(amount) +
                            " balance: " +
                            std::to_string(olymp_trade_balance) +
                            ", date:" + xtime::get_str_date_time(timestamp));
                        std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    }
                    continue;
                } else
                if(state_competition != open_bo_api::StateCompetition::OK) {
                    std::cout << "!= OK" << std::endl;
                    continue;
                }

                /* открываем сделку и логируем все ее состояния */
                if(broker == open_bo_api::ListBrokers::INTRADE_BAR) {
                    uint64_t api_bet_id = 0;
                    int err = intrade_bar_api.open_bo(
                            symbol_name, // имя символа
                            "RSI", // заметка, например имя стратегии
                            amount, // размер ставки
                            strategy_state, // направление сделки
                            DURATION, // длительность экспирации
                            api_bet_id, // уникальный номер сделки в пределах библиотеки API
                            [=](const open_bo_api::IntradeBar::Api::Bet &bet) {
                        /* состояния сделки после запроса на сервер
                         * (удачная сделкка, не удачная или была ошибка открытия сделки
                         */
                        if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::WIN) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: WIN, broker id: " + std::to_string(bet.broker_bet_id) +
                                ", api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else
                        if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::LOSS) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: LOSS, broker id: " + std::to_string(bet.broker_bet_id) +
                                ", api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else
                        if(bet.bet_status == open_bo_api::IntradeBar::BetStatus::OPENING_ERROR) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: ERROR, api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        }
                    });

                    /* Если ошибка возникла раньше, чем мы успели отправить запрос,
                     * логируем ее. Также запишем в лог момент вызова метода для открытия сделки
                      */
                    if(err != open_bo_api::IntradeBar::ErrorType::OK) {
                        const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: ERROR, code: " + std::to_string(err));
                            std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    } else {
                        const std::string message(
                            symbol_name + " " +
                            signal_type +
                            " deal open, amount: " + std::to_string(amount) +
                            ", api id: " + std::to_string(api_bet_id) +
                            ", date: " + xtime::get_str_date_time(timestamp));
                        std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        is_block_open_bo_one_deal = true;
                    }
                } else
                if(broker == open_bo_api::ListBrokers::OLYMP_TRADE) {
                   uint64_t api_bet_id = 0;
                   int err = olymp_trade_api.open_bo(
                            symbol_name, // имя символа
                            "RSI", // заметка, например имя стратегии
                            amount, // размер ставки
                            strategy_state, // направление сделки
                            DURATION, // длительность экспирации
                            api_bet_id, // уникальный номер сделки в пределах библиотеки API
                            [=](const open_bo_api::OlympTrade::Bet &bet) {
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::UNKNOWN_STATE) {
                            std::cout << "UNKNOWN_STATE" << std::endl;
                        } else
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::OPENING_ERROR) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: ERROR, api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::WAITING_COMPLETION) {
                            std::cout << "WAITING_COMPLETION" << std::endl;
                        } else
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::CHECK_ERROR) {
                            std::cout << "CHECK_ERROR" << std::endl;
                        } else
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::WIN) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: WIN, broker id: " + std::to_string(bet.broker_bet_id) +
                                ", api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        } else
                        if(bet.bet_status == open_bo_api::OlympTrade::BetStatus::LOSS) {
                            const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: LOSS, broker id: " + std::to_string(bet.broker_bet_id) +
                                ", api id: " + std::to_string(bet.api_bet_id));
                            std::cout << message << std::endl;
                            open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        }
                    });
                    if(err != open_bo_api::OlympTrade::ErrorType::OK) {
                        const std::string message(
                                symbol_name + " " +
                                signal_type +
                                ", status: ERROR, code: " + std::to_string(err));
                            std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                    } else {
                        const std::string message(
                            symbol_name + " " +
                            signal_type +
                            " deal open, amount: " + std::to_string(amount) +
                            ", api id: " + std::to_string(api_bet_id) +
                            ", date: " + xtime::get_str_date_time(timestamp));
                        std::cout << message << std::endl;
                        open_bo_api::Logger::log(settings.trading_robot_work_log_file, message);
                        is_block_open_bo_one_deal = true;
                    }
                }
            } // for symbol
            break;
        }

        /* загружаем новости */
        if(event == open_bo_api::IntradeBar::Api::EventType::NEW_TICK && second == 0) {
            open_bo_api::News::async_update(timestamp, settings.news_sert_file);
        }
    },
    false,
    settings.intrade_bar_sert_file,
    settings.intrade_bar_cookie_file,
    settings.intrade_bar_bets_log_file,
    settings.intrade_bar_work_log_file,
    settings.intrade_bar_websocket_log_file);

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
    if(settings.is_intrade_bar) {
        std::cout << "<<<<intrade.bar>>>>" << std::endl;
        std::cout << "balance: " << intrade_bar_api.get_balance() << std::endl;
        std::cout << "is demo: " << intrade_bar_api.demo_account() << std::endl;
        std::cout << "is account currency RUB: " << intrade_bar_api.account_rub_currency() << std::endl;
    }
    if(settings.is_olymp_trade) {
        std::cout << "<<<<olymp trade>>>>" << std::endl;
        std::cout << "uuid: " << olymp_trade_api.get_test_uuid() << std::endl;
        std::cout << "account_id_real: " << olymp_trade_api.get_account_id_real() << std::endl;
        std::cout << "is_demo: " << olymp_trade_api.demo_account() << std::endl;
    }

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
