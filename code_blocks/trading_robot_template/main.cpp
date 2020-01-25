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

using json = nlohmann::json;

int main(int argc, char **argv) {
    std::cout << "running an example robot template" << std::endl;

    std::string json_file_name; // Имя JSON файла
    json json_settings;         // JSON структура с настройками
    uint32_t number_bars = 100; // количество баров для инициализации исторических данных

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

    /* читаем настройки из файла JSON */
    if(!open_bo_api::open_json_file(json_file_name, json_settings)) return EXIT_FAILURE;

    /* получаем в отдельном потоке тики котировок и исторические данные брокера */
    intrade_bar::IntradeBarApi api(number_bars,[&](
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const intrade_bar::IntradeBarApi::EventType event,
                    const xtime::timestamp_t timestamp) {
        /* получаем бар по валютной паре GBPCAD из candles */
        xquotes_common::Candle candle = intrade_bar::IntradeBarApi::get_candle("GBPCAD", candles);
        /* получено событие ПОЛУЧЕНЫ ИСТОРИЧЕСКИЕ ДАННЫЕ */
        if(event == intrade_bar::IntradeBarApi::EventType::HISTORICAL_DATA_RECEIVED) {
            std::cout << "history bar: " << xtime::get_str_date_time(timestamp) << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "GBPCAD close: " << candle.close
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << "GBPCAD error" << std::endl;
            }
        } else
        /* получено событие НОВЫЙ ТИК */
        if(event == intrade_bar::IntradeBarApi::EventType::NEW_TICK) {
            std::cout << "new tick: " << xtime::get_str_date_time(timestamp) << std::endl;
            if(intrade_bar::IntradeBarApi::check_candle(candle)) {
                std::cout
                    << "GBPCAD close: " << candle.close
                    << " t: " << xtime::get_str_date_time(candle.timestamp)
                    << std::endl;
            } else {
                std::cout << "GBPCAD error" << std::endl;
            }
        }
    });

    int err_connect = api.connect(json_settings);
    std::cout << "connect code: " << err_connect << std::endl;
    std::cout << "user id: " << api.get_user_id() << std::endl;
    std::cout << "user hash: " << api.get_user_hash() << std::endl;
    std::cout << "balance: " << api.get_balance() << std::endl;
    std::cout << "is demo: " << api.demo_account() << std::endl;
    std::cout << "is account currency RUB: " << api.account_rub_currency() << std::endl;

    while(true) {
        std::this_thread::yield();
    }
    return EXIT_SUCCESS;
}
