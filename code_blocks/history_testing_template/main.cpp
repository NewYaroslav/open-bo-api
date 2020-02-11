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
#include "open-bo-api-history-testing.hpp"
#include <csignal>

using json = nlohmann::json;
/* тип индикатора RSI, этот индиатор использует внутри себя SMA и тип данных double */
using RSI_TYPE = xtechnical_indicators::RSI<double, xtechnical_indicators::SMA<double>>;

/* класс с настройками */
open_bo_api::Settings settings;

int main(int argc, char **argv) {
    std::cout << "running an example history testing template" << std::endl;

    //std::string json_file_name("auth.json");
    settings.json_file_name = "auth.json";

    /* читаем настройки из файла JSON */
    json json_settings; // JSON структура с настройками
    if(!open_bo_api::open_json_file(settings.json_file_name, json_settings)) return EXIT_FAILURE;
#if(1)
    settings.init(json_settings);
    if(settings.check_error()) {
        std::cerr << "settings error" << std::endl;
        return EXIT_FAILURE;
    }
#endif
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

    /* получаем в отдельном потоке тики котировок и исторические данные брокера */
    open_bo_api::HistoryTester history_tester(
                    settings.history_tester_storage_path,
                    intrade_bar_symbols,
                    open_bo_api::HistoryTester::StorageType::QHS5,
                    settings.history_tester_time_speed,
                    settings.history_tester_start_timestamp,
                    settings.history_tester_stop_timestamp,
                    settings.history_tester_number_bars,
                    [&](const std::map<std::string, open_bo_api::Candle> &candles,
                        const open_bo_api::HistoryTester::EventType event,
                        const xtime::timestamp_t timestamp) {
        //
        static bool is_block_open_bo = false;
        static bool is_block_open_bo_one_deal = false;

        const uint32_t second = xtime::get_second_minute(timestamp);

        /* обрабатываем все индикаторы */
        switch(event) {

        /* получен бар исторических данных для всех символов */
        case open_bo_api::HistoryTester::EventType::HISTORICAL_DATA_RECEIVED:
            open_bo_api::update_indicators(
                candles,
                rsi_output,
                intrade_bar_symbols,
                rsi_periods,
                rsi_indicators,
                open_bo_api::TypePriceIndicator::CLOSE);

            std::cout << open_bo_api::HistoryTester::get_candle("EURUSD", candles).close << std::endl;

            /* еще загрузим данные депозита */
            //history_tester.update_balance();

            /* обнулим флаги */
            is_block_open_bo = false;
            is_block_open_bo_one_deal = false;

            //std::cout << "get history canlde: " << xtime::get_str_date_time(timestamp) << std::endl;
            break;

        /* получен тик (секунда) новых данных для всех символов */
        case open_bo_api::HistoryTester::EventType::NEW_TICK:
            //std::cout << "get tick: " << xtime::get_str_date_time(timestamp) << std::endl;
            /* ждем 59 секунду или начало минуты */
            if(second != 59 && second != 0) break;

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

                /* получаем значение индикатора */
                double rsi_out = rsi_output[intrade_bar_symbols[symbol]][PERIOD_RSI];
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

                std::cout << intrade_bar_symbols[symbol] << " " << strategy_state << std::endl;
            } // for symbol
            break;
        }
    });


    /* тут обрабатываем ошибки и прочее */
    while(true) {
        std::this_thread::yield();
        // добавим сон
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        if(history_tester.is_testing_stopped()) break;
    }
    return EXIT_SUCCESS;
}

