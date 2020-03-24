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
#ifndef OPEN_BO_API_COMPETITION_HPP_INCLUDED
#define OPEN_BO_API_COMPETITION_HPP_INCLUDED

#include "open-bo-api-common.hpp"
#include "open-bo-api-settings.hpp"
#include "open-bo-api-brokers.hpp"
#include "intrade-bar-payout-model.hpp"

namespace open_bo_api {

    /** \brief Состояния конкуренции брокеров
     */
    enum class StateCompetition {
        OK = 0,                     /**< Все в порядке */
        NO_BROKERS = -1,            /**< Не указаны брокеры для торговли */
        WAIT_CLOSING_PRICE = -2,    /**< Ожидание цены закрытия */
        LOW_PAYMENT = -3,           /**< Низкий процент выплат */
        LOW_DEPOSIT_BALANCE = -4,   /**< Низкий баланс депозита */
        CANCEL = -5,
    };

    /** \brief Получить конкурирующий размер ставки и процент выплат
      *
      * Данная функция находит наилучшее условие из предоставленных для торговли брокеров.
      * 1. Функция ориентирована на работу с ценой закрытия минутного бара.
      * 2. Для брокера olymptrade проверка начинается с 58 секунды до 0 секунды.
      * 3. Для брокера intrade.bar проверка начинается с 59 секунды до 0 секунды.
      * Проценты выплат варьируются обычно от 0 до 1.0, где 1.0 соответствует 100% выплате брокера.
      *
      * \param broker Возвращаемое значение, тип брокера
      * \param amount Возвращаемое значение, размер ставки в абсолютном значении
      * \param payout Возвращаемое значение, процент выплат
      * \param intrade_bar Ссылка на класс брокера Intrade.bar
      * \param olymp_trade Ссылка на класс брокера OlympTrade
      * \param symbol_name Имя валютной пары
      * \param timestamp Метка времени (GMT)
      * \param duration Длительность опциона в секундах
      * \param is_rub Рублевый счет
      * \param balance Размер баланса
      * \param winrate Винрейт стратегии
      * \param attenuator Ослабление коэффициента Келли, желательно использовать значения не боьше 0.4
      * \return вернет true в случае наличия хороших условий для открытия сделки
      */
    StateCompetition calc_brokers_competition(
            ListBrokers &broker,
            double &amount,
            double &payout,
            open_bo_api::IntradeBar::Api &intrade_bar,
            open_bo_api::OlympTrade::Api &olymp_trade,
            const Settings &settings,
            const std::string &symbol_name,
            const xtime::timestamp_t timestamp,
            const uint32_t duration,
            const bool is_rub,
            const double balance,
            const double winrate,
            const double attenuator) {
        /* инициализируем выходные данные */
        amount = 0;
        payout = 0;
        broker = open_bo_api::ListBrokers::NOT_SELECTED;

        /* получаем секунду минуты */
        const uint32_t second = xtime::get_second_minute(timestamp);

        /* проверяем время */
        if(settings.is_olymp_trade && (second < 58 && second != 0)) return StateCompetition::WAIT_CLOSING_PRICE;
        else if(settings.is_intrade_bar && (second != 59 && second != 0)) return StateCompetition::WAIT_CLOSING_PRICE;
        /* проверяем наличие брокеров */
        else if(!settings.is_olymp_trade && !settings.is_intrade_bar) return StateCompetition::NO_BROKERS;

        /* получаем проценты выплат за текущее время */
        double intrade_bar_amount = 0, intrade_bar_payout = 0;
        double intrade_bar_next_amount = 0, intrade_bar_next_payout = 0;

        open_bo_api::IntradeBar::get_amount(
                intrade_bar_amount,
                intrade_bar_payout,
                symbol_name,
                timestamp,
                duration,
                is_rub,
                balance,
                winrate,
                attenuator);

        if ((settings.is_olymp_trade && second >= 58) ||
            (!settings.is_olymp_trade && second == 59)) {
                open_bo_api::IntradeBar::get_amount(
                    intrade_bar_next_amount,
                    intrade_bar_next_payout,
                    symbol_name,
                    xtime::get_first_timestamp_minute(timestamp) +
                    xtime::SECONDS_IN_MINUTE,
                    duration,
                    is_rub,
                    balance,
                    winrate,
                    attenuator);
        }

        double olymp_trade_amount = 0, olymp_trade_payout = 0;
        open_bo_api::OlympTrade::get_amount(
                olymp_trade_amount,
                olymp_trade_payout,
                olymp_trade,
                symbol_name,
                duration,
                balance,
                winrate,
                attenuator);

        const double intrade_bar_balance = intrade_bar.get_balance();
        const double olymp_trade_balance = olymp_trade.get_balance();

        if (settings.is_olymp_trade &&
            olymp_trade_payout > intrade_bar_next_payout &&
            olymp_trade_payout > intrade_bar_payout &&
            olymp_trade_amount > 0) {
            payout = olymp_trade_payout;
            amount = olymp_trade_amount;
            broker = open_bo_api::ListBrokers::OLYMP_TRADE;
            if(amount >= olymp_trade_balance)
                return StateCompetition::LOW_DEPOSIT_BALANCE;
            return StateCompetition::OK;
        } else
        if (settings.is_intrade_bar &&
            intrade_bar_payout > intrade_bar_next_payout &&
            intrade_bar_amount > 0) {
            payout = intrade_bar_payout;
            amount = intrade_bar_amount;
            broker = open_bo_api::ListBrokers::INTRADE_BAR;
            if(amount >= intrade_bar_balance)
                return StateCompetition::LOW_DEPOSIT_BALANCE;
            return StateCompetition::OK;
        } else
        if (settings.is_intrade_bar &&
            second == 59 &&
            intrade_bar_payout <= intrade_bar_next_payout &&
            intrade_bar_next_amount > 0) {
            payout = intrade_bar_next_payout;
            amount = intrade_bar_next_amount;
            broker = open_bo_api::ListBrokers::INTRADE_BAR;
            if(amount >= intrade_bar_balance)
                return StateCompetition::LOW_DEPOSIT_BALANCE;
            return StateCompetition::CANCEL;
        } else {
            amount = 0;
            payout = std::max(olymp_trade_payout, intrade_bar_payout);
            return StateCompetition::LOW_PAYMENT;
        }
        return StateCompetition::OK;
    }

}

#endif // OPEN_BO_API_COMPETITION_HPP_INCLUDED
