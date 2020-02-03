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

using BB_TYPE = xtechnical_indicators::BollingerBands<double>;

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

    std::map<std::string, std::map<uint32_t, std::map<uint32_t, BB_TYPE>>> bb_indicators;
    std::vector<uint32_t> bb_periods = open_bo_api::get_list_parameters<uint32_t>(10, 100, 1);
    std::vector<uint32_t> bb_factors = open_bo_api::get_list_parameters<uint32_t>(10, 30, 1);
    std::vector<double> bb_init_factors = open_bo_api::get_list_parameters<double>(1.0, 3.0, 0.1);
    std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> bb_output_tl;
    std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> bb_output_ml;
    std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> bb_output_bl;

    /* инициализируем индикаторы RSI */
    open_bo_api::init_indicators<BB_TYPE>(
            intrade_bar_symbols,
            bb_periods,
            bb_factors,
            bb_periods,
            bb_init_factors,
            bb_indicators);

    BB_TYPE iBB1020(10,2.0);
    BB_TYPE iBB2022(20,2.2);
    BB_TYPE iBB5019(50,1.9);

    /* получаем в отдельном потоке тики котировок и исторические данные брокера */
    open_bo_api::IntradeBar::Api intrade_bar_api(
                    settings.intrade_bar_number_bars,
                    [&](const std::map<std::string, open_bo_api::Candle> &candles,
                        const open_bo_api::IntradeBar::Api::EventType event,
                        const xtime::timestamp_t timestamp) {
        /* получаем бар */
        open_bo_api::Candle candle_1 = open_bo_api::IntradeBar::Api::get_candle("EURUSD", candles);
        open_bo_api::Candle candle_2 = open_bo_api::IntradeBar::Api::get_candle("AUDCAD", candles);
        open_bo_api::Candle candle_3 = open_bo_api::IntradeBar::Api::get_candle("GBPJPY", candles);
        double tl = 0, ml = 0, bl = 0;
        /* обрабатываем все индикаторы */
        switch(event) {

        /* получен бар исторических данных для всех символов */
        case open_bo_api::IntradeBar::Api::EventType::HISTORICAL_DATA_RECEIVED:

            open_bo_api::update_indicators(
                candles,
                bb_output_tl,
                bb_output_ml,
                bb_output_bl,
                intrade_bar_symbols,
                bb_periods,
                bb_factors,
                bb_indicators,
                open_bo_api::TypePriceIndicator::CLOSE);
            //
            if(open_bo_api::IntradeBar::Api::check_candle(candle_1)) iBB1020.update(candle_1.close, tl, ml, bl);
            else iBB1020.clear();
            std::cout << "EURUSD (hist) BB1020, tl: " << tl << " / " << bb_output_tl["EURUSD"][10][20] << " date: " << xtime::get_str_date(timestamp) << std::endl;

            if(open_bo_api::IntradeBar::Api::check_candle(candle_2)) iBB2022.update(candle_2.close, tl, ml, bl);
            else iBB2022.clear();
            std::cout << "AUDCAD (hist) BB2022, tl: " << tl << " / " << bb_output_tl["AUDCAD"][20][22] << " date: " << xtime::get_str_date(timestamp) << std::endl;

            if(open_bo_api::IntradeBar::Api::check_candle(candle_3)) iBB5019.update(candle_3.close, tl, ml, bl);
            else iBB5019.clear();
            std::cout << "GBPJPY (hist) BB5019, tl: " << tl << " / " << bb_output_tl["GBPJPY"][50][19] << " date: " << xtime::get_str_date(timestamp) << std::endl;
            break;

        /* получен тик (секунда) новых данных для всех символов */
        case open_bo_api::IntradeBar::Api::EventType::NEW_TICK:
            open_bo_api::test_indicators(
                candles,
                bb_output_tl,
                bb_output_ml,
                bb_output_bl,
                intrade_bar_symbols,
                bb_periods,
                bb_factors,
                bb_indicators,
                open_bo_api::TypePriceIndicator::CLOSE);
            //
            if(open_bo_api::IntradeBar::Api::check_candle(candle_1)) iBB1020.test(candle_1.close, tl, ml, bl);
            else iBB1020.clear();
            std::cout << "EURUSD (tick) BB1020, tl: " << tl << " / " << bb_output_tl["EURUSD"][10][20] << " date: " << xtime::get_str_date_time(timestamp) << std::endl;

            if(open_bo_api::IntradeBar::Api::check_candle(candle_2)) iBB2022.test(candle_2.close, tl, ml, bl);
            else iBB2022.clear();
            std::cout << "AUDCAD (tick) BB2022, tl: " << tl << " / " << bb_output_tl["AUDCAD"][20][22] << " date: " << xtime::get_str_date_time(timestamp) << std::endl;

            if(open_bo_api::IntradeBar::Api::check_candle(candle_3)) iBB5019.test(candle_3.close, tl, ml, bl);
            else iBB5019.clear();
            std::cout << "GBPJPY (tick) BB5019, tl: " << tl << " / " << bb_output_tl["GBPJPY"][50][19] << " date: " << xtime::get_str_date_time(timestamp) << std::endl;
            break;
        }
    },
    false,
    settings.intrade_bar_sert_file,
    settings.intrade_bar_cookie_file,
    settings.intrade_bar_bets_log_file,
    settings.intrade_bar_work_log_file,
    settings.intrade_bar_websocket_log_file);

    while(true) {
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
