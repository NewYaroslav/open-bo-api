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

namespace open_bo_api {
    using json = nlohmann::json;

    class Settings {
    public:
        std::string json_file_name; /**< Имя JSON файла */
        std::string news_sert_file = "curl-ca-bundle.crt";
        std::string intrade_bar_sert_file = "curl-ca-bundle.crt";
        std::string intrade_bar_cookie_file = "cookie/intrade-bar.cookie";
        std::string intrade_bar_bets_log_file = "logger/intrade-bar-bets.log";
        std::string intrade_bar_work_log_file = "logger/intrade-bar-https-work.log";
        std::string intrade_bar_websocket_log_file = "logger/intrade-bar-websocket.log";
        std::string intrade_bar_email;
        std::string intrade_bar_password;
        std::string trading_robot_work_log_file = "logger/trading_robot_work_file.log";
        uint32_t intrade_bar_number_bars = 100; /**< количество баров м1 для инициализации исторических данных */
        bool is_intrade_bar_demo_account = true;
        bool is_intrade_bar_rub_currency = true;

        Settings() {};

        void init(json &j) {
            try {
                //
                if(j["news"]["sert_file"] != nullptr) {
                    news_sert_file = j["news"]["sert_file"];
                }
                //
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
                //
                if(j["trading_robot"]["work_log_file"] != nullptr) {
                    trading_robot_work_log_file = j["trading_robot"]["work_log_file"];
                }
                //
            }
            catch(...) {

            }
        }

        Settings(json &j) {
            init(j);
        }
    };
}

#endif // OPEN_BO_API_SETTINGS_HPP_INCLUDED
