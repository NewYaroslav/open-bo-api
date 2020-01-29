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
#include "open-bo-api.hpp"


int main(int argc, char **argv) {
    std::cout << "running an example robot template" << std::endl;
    using json = nlohmann::json;
    /* тип индикатора RSI, этот индиатор использует внутри себя SMA и тип данных double */
    using RSI_TYPE = xtechnical_indicators::RSI<double,xtechnical_indicators::SMA<double>>;

    std::string json_file_name;     // Имя JSON файла
    json json_settings;             // JSON структура с настройками
    uint32_t number_bars = 100;     // количество баров м1 для инициализации исторических данных
    uint32_t bet_request_delay = 1; // Задержка на открытие ставки, секунды

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
            json_file_name = value;
        }
    })) {
        std::cerr << "Error! No parameters!" << std::endl;
        return EXIT_FAILURE;
    }
#else
    json_file_name = "auth.json";
#endif

    /* читаем настройки из файла JSON */
    if(!open_bo_api::open_json_file(json_file_name, json_settings)) return EXIT_FAILURE;

    /* массив имен символов брокера IntradeBar */
    std::vector<std::string> intrade_bar_symbols = open_bo_api::IntradeBar::get_list_symbols();

    /* массив индикаторов RSI */
    std::map<std::string, std::map<uint32_t, RSI_TYPE>> rsi_indicators;

    /* массив периодов индикатора, от 10 до 100 включительно с шагом 1 */
    std::vector<uint32_t> rsi_periods = open_bo_api::get_list_parameters<uint32_t>(10, 100, 1);

    /* инициализируем индикаторы RSI */
    open_bo_api::init_indicators<RSI_TYPE>(
            intrade_bar_symbols,
            rsi_periods,
            rsi_indicators);

    const uint32_t PERIOD_RSI = 10; // возьмем для нашей стратегии период индикатора 10 минут
    const uint32_t DURATION = 180;  // размер ставки 180 секунд или 3 минуты
    const double WINRATE = 0.56;    // винрейт стратегии
    const double KELLY_ATTENUATOR = 0.4;

    if(!open_bo_api::News::update(xtime::get_timestamp())) {
        std::cerr << "download news error" << std::endl;
        intrade_bar::Logger::log("test_robot_template.json", (std::string)"download news error");
    }

    /* получаем в отдельном потоке тики котировок и исторические данные брокера */
    intrade_bar::IntradeBarApi intrade_bar_api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {
        static bool is_block_open_bo = false;
        static bool is_block_open_bo_one_deal = false;
        static bool is_download_news = false;
        const uint32_t second = xtime::get_second_minute(timestamp);

        /* проходим в цикле по всем символа брокера Intradebar */
        for(size_t symbol = 0; symbol < intrade_bar_symbols.size(); ++symbol) {
            std::string &symbol_name = intrade_bar_symbols[symbol];
            /* получаем бар */
            open_bo_api::Candle candle = intrade_bar::IntradeBarApi::get_candle(symbol_name, candles);
            switch(event) {
            /* получено событие "ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ" */
            case intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED:
                is_block_open_bo = false;
                is_block_open_bo_one_deal = false;
                /* проверяем бар на наличие данных */
                if(!intrade_bar::IntradeBarApi::check_candle(candle)) {
                    /* данных нет, очищаем внутреннее состояние индикатора */
                    rsi_indicators[symbol_name][PERIOD_RSI].clear();
                    continue;
                }
                /* обновляем состояние всех индикаторов*/
                for(auto& item : rsi_indicators[symbol_name]) {
                    item.second.update(candle.close);
                }
                break;
            /* получено событие "НОВЫЙ ТИК" */
            case intrade_bar::IntradeBarApi::EventType::NEW_TICK:
                /* ждем 0 или 59 секундe бара */
                if(second == 59 || second == 0) {
                    bool is_news = false;
                    /* фильтр новостей */
                    const xtime::timestamp_t bar_timestamp = second == 0 ?
                        timestamp - xtime::SECONDS_IN_MINUTE :
                        timestamp;
                    if(!open_bo_api::News::check_news(
                        is_news,
                        symbol_name,
                        bar_timestamp,
                        xtime::SECONDS_IN_MINUTE * 30,
                        xtime::SECONDS_IN_MINUTE * 30,
                        false,
                        false,
                        true,
                        true)) {
                        std::cerr << "filter news error" << std::endl;
                        intrade_bar::Logger::log("test_robot_template.json",(std::string)"filter news error");
                        break;
                    }

                    if(is_news) {
                        std::cerr << "news filter: " << xtime::get_str_date_time(timestamp) << std::endl;
                        intrade_bar::Logger::log("test_robot_template.json","news filter: " + xtime::get_str_date_time(timestamp));
                        break;
                    }

                    /* получаем процент выплат */
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

                    /* если нет процента выплат, пропускаем код */
                    if(err_open_bo != intrade_bar_common::OK) {
                        intrade_bar::Logger::log("test_robot_template.json",(std::string)"low payout");
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
                        if(err_open_bo == intrade_bar_common::OK && payout <= payout_next) break;;
                        /* на 59 секунде условия лучше, чем на 0 секунде */
                        is_block_open_bo = true;
                    } else {
                        /* пропускаем 0 секунду, так как была сделка на 59 секунде */
                        if(is_block_open_bo) break;
                    }

                    /* проверяем бар на наличие данных */
                    if(!intrade_bar::IntradeBarApi::check_candle(candle)) break;

                    /* обновляем состояние индикатора */
                    double rsi_out = 50;
                    int err = rsi_indicators[symbol_name][PERIOD_RSI].test(candle.close, rsi_out);
                    if(err != xtechnical_common::OK) break;

                    if(is_block_open_bo_one_deal) break;

                    /* реализуем простую стратегию */
                    if(rsi_out > 70) {
                        std::cout << symbol_name << " SELL " << xtime::get_str_date_time(timestamp) << std::endl;
                        uint32_t api_bet_id = 0;
                        int err = intrade_bar_api.open_bo(
                            symbol_name,
                            "RSI",
                            amount,
                            intrade_bar_common::SELL,
                            DURATION,
                            api_bet_id,
                            [=](const intrade_bar::IntradeBarApi::Bet &bet) {
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                std::cout << symbol_name << " SELL WIN, broker id: " << bet.broker_bet_id << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " SELL WIN, broker id: " + std::to_string(bet.broker_bet_id));
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                std::cout << symbol_name << " SELL LOSS, broker id: " << bet.broker_bet_id << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " SELL LOSS, broker id: " + std::to_string(bet.broker_bet_id));
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                std::cout << symbol_name << " SELL error" << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " SELL ERROR");
                            }
                        });
                        if(err != xtechnical_common::OK) {
                            std::cerr << "error open bo: " << err << std::endl;
                        } else {
                            std::cout << symbol_name << " SELL " << amount << " OK, id: " << api_bet_id << std::endl;
                            is_block_open_bo_one_deal = true;
                        }
                    } else
                    if(rsi_out < 30) {
                        std::cout << symbol_name << " BUY " << xtime::get_str_date_time(timestamp) << std::endl;
                        uint32_t api_bet_id = 0;
                        int err = intrade_bar_api.open_bo(
                            symbol_name,
                            "RSI",
                            amount,
                            intrade_bar_common::BUY,
                            DURATION,
                            api_bet_id,
                            [=](const intrade_bar::IntradeBarApi::Bet &bet){
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                std::cout << symbol_name << " BUY WIN, broker id: " << bet.broker_bet_id << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " BUY WIN, broker id: " + std::to_string(bet.broker_bet_id));
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                std::cout << symbol_name << " BUY LOSS, broker id: " << bet.broker_bet_id << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " BUY LOSS, broker id: " + std::to_string(bet.broker_bet_id));
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                std::cout << symbol_name << " BUY error" << std::endl;
                                intrade_bar::Logger::log("test_robot_template.json", symbol_name + " BUY ERROR");
                            }
                        });
                        if(err != xtechnical_common::OK) {
                            std::cerr << "error open bo: " << err << std::endl;
                            intrade_bar::Logger::log("test_robot_template.json", "error open bo: " + std::to_string(err));
                        } else {
                            std::cout << symbol_name << " BUY " << amount << " OK, id: " << api_bet_id << std::endl;
                            intrade_bar::Logger::log("test_robot_template.json", symbol_name + " BUY " + std::to_string(amount) + " OK, id: " + std::to_string(api_bet_id));
                            is_block_open_bo_one_deal = true;
                        }
                    }
                }
                break;
            };
        } // for symbol

        /* загружаем новости */
        if((event == intrade_bar::IntradeBarApi::EventType::NEW_TICK &&
            second == 0) || !is_download_news) {
            open_bo_api::News::async_update(timestamp);
            is_download_news = true;
        }

    });

    std::cout << "email: " << json_settings["email"] << std::endl;
    std::cout << "password: " << json_settings["password"] << std::endl;

    int err_connect = intrade_bar_api.connect(json_settings);
    std::cout << "connect code: " << err_connect << std::endl;
    std::cout << "user id: " << intrade_bar_api.get_user_id() << std::endl;
    std::cout << "user hash: " << intrade_bar_api.get_user_hash() << std::endl;
    std::cout << "balance: " << intrade_bar_api.get_balance() << std::endl;
    std::cout << "is demo: " << intrade_bar_api.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << intrade_bar_api.account_rub_currency() << std::endl;

    while(true) {
        std::this_thread::yield();
    }
    return EXIT_SUCCESS;
}
