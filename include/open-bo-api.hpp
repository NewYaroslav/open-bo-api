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
#ifndef OPEN_BO_API_HPP_INCLUDED
#define OPEN_BO_API_HPP_INCLUDED


#include "open-bo-api-news.hpp"
#include "open-bo-api-settings.hpp"
#include "open-bo-api-indicators.hpp"
#include "open-bo-api-command-line-tools.hpp"

/* брокер intrade.bar */
#include "intrade-bar-common.hpp"
#include "intrade-bar-api.hpp"
#include "intrade-bar-payout-model.hpp"

#include <mt-bridge.hpp>

namespace open_bo_api {
    using json = nlohmann::json;
    using Candle = xquotes_common::Candle;
    using Logger = intrade_bar::Logger;

    typedef mt_bridge::MetatraderBridge<xquotes_common::Candle> MtBridge;   /**< Класс Моста между Metatrader и программой со стандартным классом для хранения баров */

    class IntradeBar {
    public:
        using Api = intrade_bar::IntradeBarApi;
        using BetStatus = intrade_bar::IntradeBarHttpApi::BetStatus;
        using ErrorType = intrade_bar_common::ErrorType;

        IntradeBar() {}

        /** \brief Получить список символов
         * \return список символов
         */
        inline const static std::vector<std::string> get_list_symbols() {
            return std::vector<std::string>(intrade_bar_common::currency_pairs.begin(),intrade_bar_common::currency_pairs.end());
        }
#if(0)
        /** \brief Получить процент выплат
         *
         * Проценты выплат варьируются обычно от 0 до 0.85, где 0.85 соответствует 85% выплате брокера
         * \param symbol_name Имя валютной пары
         * \param timestamp временную метку unix времени (GMT)
         * \param duration длительность опциона в секундах
         * \param amount размер ставки бинарного опциона
         * \return процент выплат, 0 если выплаты нет
         */
        inline const static double get_payout(
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const uint32_t duration,
                const double amount) {
            double payout = 0.0;
            if(payout_model::get_payout(
                payout,
                symbol_name,
                timestamp,
                duration,
                amount) == payout_model::OK) return payout;
            return 0.0;
        }
#endif
         /** \brief Получитьабсолютный размер ставки и процент выплат
         *
         * Проценты выплат варьируются обычно от 0 до 1.0, где 1.0 соответствует 100% выплате брокера
         * \param amount Размер ставки в абсолютном значении
         * \param payout Процент выплат
         * \param symbol_name Имя валютной пары
         * \param timestamp Метка времени (GMT)
         * \param duration Длительность опциона в секундах
         * \param balance Размер баланса
         * \param winrate Винрейт стратегии
         * \param attenuator Ослабление коэффициента Келли, желательно использовать значения не боьше 0.4
         * \return состояние выплаты (0 в случае успеха, иначе см. payout_model::IntradeBar::PayoutCancelType)
         */
        inline const static int get_amount(
                double &amount,
                double &payout,
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp,
                const uint32_t duration,
                const bool is_rub,
                const double balance,
                const double winrate,
                const double attenuator) {
            payout_model::IntradeBar pm;
            pm.set_rub_account_currency(is_rub);
            return pm.get_amount(
                amount,
                payout,
                symbol_name,
                timestamp,
                duration,
                balance,
                winrate,
                attenuator);
        }
    };
};

#endif // OPEN_BO_API_HPP_INCLUDED
