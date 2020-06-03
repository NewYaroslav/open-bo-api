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
#include "open-bo-api-virtual-account.hpp"
#include <csignal>

using json = nlohmann::json;

open_bo_api::Settings settings;

int main(int argc, char **argv) {
    settings.json_file_name = "auth.json";

    /* читаем настройки из файла JSON */
    //json json_settings; // JSON структура с настройками
    //if(!open_bo_api::open_json_file(settings.json_file_name, json_settings)) return EXIT_FAILURE;
    //settings.init(json_settings);
    //if(settings.check_error()) return EXIT_FAILURE;
    bool is_demo = false;
    open_bo_api::VirtualAccounts vas("test.db");
    std::cout << "vas size: " << vas.get_virtual_accounts().size() << std::endl;
    std::cout << "vas balance: " << vas.get_balance(is_demo) << std::endl;
    std::string strategy("TEST2");

    //double b = 10000;

    double b = vas.get_balance(is_demo);
    double max_error = 0;


    for(size_t i = 0; i < 1000; ++i) {

        xtime::timestamp_t date_time = xtime::get_timestamp() + i * xtime::SECONDS_IN_HOUR;

        double a = 0;
        vas.calc_amount(a, strategy, is_demo, 0.85, 0.6, 0.4);
        a = (double)((uint64_t)(a * 100.0d)) / 100.d;

        vas.make_bet(i, a, strategy, is_demo, 0.85, 0.6, 0.4, date_time, 3);
        b -= a;
        if(i % 100 < 58) {
        //if(1) {
            vas.set_win(i, date_time);
            const double p = a * 0.85;
            b += p;
            b += a;
        } else {
            vas.set_loss(i, date_time);
        }
        std::cout << "b " << b << " b-va " << vas.get_balance(is_demo) << std::endl;

        const double e = std::abs(b - vas.get_balance(is_demo));
        if(max_error < e) max_error  = e;

        if(b < vas.get_balance(is_demo)) {
            std::cout << "error " << e << std::endl;
        }
        vas.get_gain_per_day(date_time, is_demo, [&](
                    const uint64_t va_id,
                    const std::string &holder_name,
                    const double gain){
            std::cout << "id: " << va_id << " holder name " << holder_name << " gain " << gain << std::endl;
        });
        vas.push();
    }
    vas.push();
    std::cout << "max error " << max_error << std::endl;

    std::system("pause");
    return EXIT_SUCCESS;
}

