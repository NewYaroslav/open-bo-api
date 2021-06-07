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
#ifndef OPEN_BO_API_INDICATORS_HPP_INCLUDED
#define OPEN_BO_API_INDICATORS_HPP_INCLUDED

#include "xtechnical_indicators.hpp"
#include <limits>
#include <cmath>

namespace open_bo_api {

    /// Тип цены для индикатора
    enum class TypePriceIndicator {
        OPEN,
        HIGH,
        LOW,
        CLOSE,
        VOLUME
    };

    /** \brief Инициализировать индикаторы
     *
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     */
    template<class INDICATOR_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void init_indicators(
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters,
            INDICATORS_TYPE &indicators) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                indicators[symbols[symbol]][parameters[parameter]] =
                    INDICATOR_TYPE(parameters[parameter]);
            }
        }
    }

    /** \brief Функция для слияния данных свечей
     *
     * Данная функция мозволяет соединить данные из разных источников.
     * Недостающие данные в candles_src будут добавлены из candles_add.
     * \param symbols Массивы символов
     * \param candles_src Приритетный контейнер с данными. Ключ - имя символа, значение - бар/свеча
     * \param candles_add Второстепенный контейнер с данными. Ключ - имя символа, значение - бар/свеча
     * \param output_candles Контейнер с конечными данными
     */
    template<class SYMBOLS_TYPE, class CANDLE_TYPE>
    void merge_candles(
            const SYMBOLS_TYPE &symbols,
            const std::map<std::string, CANDLE_TYPE> &candles_src,
            const std::map<std::string, CANDLE_TYPE> &candles_add,
            std::map<std::string, CANDLE_TYPE> &output_candles) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it_src = candles_src.find(symbols[symbol]);
            auto it_add = candles_add.find(symbols[symbol]);
            if(it_src == candles_src.end() && it_add == candles_add.end()) continue;
            output_candles[symbols[symbol]] = it_src->second;
            if(it_add == candles_add.end()) continue;
            if(it_src->second.timestamp != it_add->second.timestamp) continue;
            auto it_new = output_candles.find(symbols[symbol]);
            it_new->second.close = it_src->second.close != 0.0 ? it_src->second.close : it_add->second.close != 0.0 ? it_add->second.close : 0.0;
            it_new->second.high = it_src->second.high != 0.0 ? it_src->second.high : it_add->second.high != 0.0 ? it_add->second.high : 0.0;
            it_new->second.low = it_src->second.low != 0.0 ? it_src->second.low : it_add->second.low != 0.0 ? it_add->second.low : 0.0;
            it_new->second.open = it_src->second.open != 0.0 ? it_src->second.open : it_add->second.open != 0.0 ? it_add->second.open : 0.0;
            it_new->second.volume = it_src->second.volume != 0.0 ? it_src->second.volume : it_add->second.volume != 0.0 ? it_add->second.volume : 0.0;
            it_new->second.timestamp = it_src->second.timestamp != 0 ? it_src->second.timestamp : it_add->second.timestamp != 0 ? it_add->second.timestamp : 0;
        }
    }

    /** \brief Функция для слияния данных свечей только по объему
     *
     * Данная функция мозволяет соединить данные из разных источников.
     * Недостающие данные в candles_src будут добавлены из candles_add.
     * \param symbols Массивы символов
     * \param candles_src Приритетный контейнер с данными. Ключ - имя символа, значение - бар/свеча
     * \param candles_add Второстепенный контейнер с данными. Ключ - имя символа, значение - бар/свеча
     * \param output_candles Контейнер с конечными данными
     */
    template<class SYMBOLS_TYPE, class CANDLE_TYPE>
    void merge_candles_only_volume(
            const SYMBOLS_TYPE &symbols,
            const std::map<std::string, CANDLE_TYPE> &candles_src,
            const std::map<std::string, CANDLE_TYPE> &candles_add,
            std::map<std::string, CANDLE_TYPE> &output_candles) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it_src = candles_src.find(symbols[symbol]);
            auto it_add = candles_add.find(symbols[symbol]);
            if(it_src == candles_src.end() && it_add == candles_add.end()) continue;
            output_candles[symbols[symbol]] = it_src->second;
            if(it_add == candles_add.end()) continue;
            if(it_src->second.timestamp != it_add->second.timestamp) continue;
            auto it_new = output_candles.find(symbols[symbol]);
            it_new->second.volume = it_src->second.volume != 0.0 ? it_src->second.volume : it_add->second.volume != 0.0 ? it_add->second.volume : 0.0;
        }
    }

    /** \brief Очистить состояние индикаторов
     *
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     */
    template<class SYMBOLS_TYPE, class PARAMETERS_TYPE, class INDICATORS_TYPE>
    void clear_indicators(
            const SYMBOLS_TYPE &symbols,
            const PARAMETERS_TYPE &parameters,
            INDICATORS_TYPE &indicators) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                indicators[symbols[symbol]][parameters[parameter]].clear();
            }
        }
    }

    /** \brief Обновить состояние индикаторов
     *
     * Данная функция сама очистить состояние индикатора, если поток цен был прерван
     * \param input Массив входящих данных
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void update_indicators(
            const std::map<std::string, CANDLE_TYPE> &input,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                int err = xtechnical::common::NO_INIT;
                switch(type) {
                case TypePriceIndicator::OPEN:
                    if(it->second.open != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.open);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::HIGH:
                    if(it->second.high != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.high);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::LOW:
                    if(it->second.low != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.low);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::CLOSE:
                    if(it->second.close != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.close);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::VOLUME:
                    if(it->second.volume != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.volume);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                }
            }
        }
    }

    /** \brief Обновить состояние индикаторов
     *
     * Данная функция сама очистит состояние индикатора, если поток цен был прерван
     * Также данная функция запишет NAN, если индикатор не был инициализирован
     * \param input Массив входящих данных
     * \param output Массив выходящих данных
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void update_indicators(
            const std::map<std::string, CANDLE_TYPE> &input,
            std::map<std::string, std::map<uint32_t, double>> &output,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                int err = xtechnical::common::NO_INIT;
                switch(type) {
                case TypePriceIndicator::OPEN:
                    if(it->second.open != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.open, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::HIGH:
                    if(it->second.high != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.high, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::LOW:
                    if(it->second.low != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.low, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::CLOSE:
                    if(it->second.close != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.close, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::VOLUME:
                    if(it->second.volume != 0) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.volume, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                }
                if(err != xtechnical::common::OK) {
                    output[symbols[symbol]][parameters[parameter]] = std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
    }

    /** \brief Обновить состояние индикаторов, содержащих EMA
     *
     * Данная функция сама очистит состояние индикатора, если поток цен был прерван
     * Также данная функция запишет NAN, если индикатор не был инициализирован
     * \param input Массив входящих данных
     * \param output Массив выходящих данных
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param offset_parameters_m1 Массив параметра смещения времени инициализации для графика м1
     * Данный параметр указывает, за сколько минут до начала дня по UTC индикатор может начать свою работу
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void update_indicators_witch_ema(
            const std::map<std::string, CANDLE_TYPE> &input,
            std::map<std::string, std::map<uint32_t, double>> &output,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters,
            const std::vector<uint32_t> &offset_parameters_m1,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                const xtime::timestamp_t start_time = xtime::get_first_timestamp_day(it->second.timestamp) - (offset_parameters_m1[parameter] * xtime::SECONDS_IN_MINUTE);
                const xtime::timestamp_t candle_time = xtime::get_first_timestamp_minute(it->second.timestamp);

                int err = xtechnical::common::NO_INIT;
                switch(type) {
                case TypePriceIndicator::OPEN:
                    if(it->second.open != 0 && candle_time >= start_time) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.open, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::HIGH:
                    if(it->second.high != 0 && candle_time >= start_time) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.high, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::LOW:
                    if(it->second.low != 0 && candle_time >= start_time) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.low, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::CLOSE:
                    if(it->second.close != 0 && candle_time >= start_time) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.close, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                case TypePriceIndicator::VOLUME:
                    if(it->second.volume != 0 && candle_time >= start_time) err = indicators[symbols[symbol]][parameters[parameter]].update(it->second.volume, output[symbols[symbol]][parameters[parameter]]);
                    else indicators[symbols[symbol]][parameters[parameter]].clear();
                    break;
                }
                if(err != xtechnical::common::OK) {
                    output[symbols[symbol]][parameters[parameter]] = std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
    }

    /** \brief Протестировать массив  индикаторов
     *
     * \param input Массив входящих данных
     * \param output Массив выходящих данных
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void test_indicators(
            const std::map<std::string, CANDLE_TYPE> &input,
            std::map<std::string, std::map<uint32_t, double>> &output,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter = 0;
                parameter < parameters.size();
                ++parameter) {
                int err = xtechnical::common::NO_INIT;
                switch(type) {
                case TypePriceIndicator::OPEN:
                    if(it->second.open != 0) err = indicators[symbols[symbol]][parameters[parameter]].test(it->second.open, output[symbols[symbol]][parameters[parameter]]);
                    break;
                case TypePriceIndicator::HIGH:
                    if(it->second.high != 0) err = indicators[symbols[symbol]][parameters[parameter]].test(it->second.high, output[symbols[symbol]][parameters[parameter]]);
                    break;
                case TypePriceIndicator::LOW:
                    if(it->second.low != 0) err = indicators[symbols[symbol]][parameters[parameter]].test(it->second.low, output[symbols[symbol]][parameters[parameter]]);
                    break;
                case TypePriceIndicator::CLOSE:
                    if(it->second.close != 0) err = indicators[symbols[symbol]][parameters[parameter]].test(it->second.close, output[symbols[symbol]][parameters[parameter]]);
                    break;
                case TypePriceIndicator::VOLUME:
                    if(it->second.volume != 0) err = indicators[symbols[symbol]][parameters[parameter]].test(it->second.volume, output[symbols[symbol]][parameters[parameter]]);
                    break;
                }
                if(err != xtechnical::common::OK) {
                    output[symbols[symbol]][parameters[parameter]] = std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
    }

    /** \brief Инициализировать индикаторы c двумя параметрами
     *
     * \param symbols Массивы символов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     */
    template<class INDICATOR_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void init_indicators(
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters_1,
            const std::vector<uint32_t> &parameters_2,
            INDICATORS_TYPE &indicators) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            for(uint32_t parameter_1 = 0;
                parameter_1 < parameters_1.size();
                ++parameter_1) {
                for(uint32_t parameter_2 = 0;
                    parameter_2 < parameters_2.size();
                    ++parameter_2) {
                    indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] =
                        INDICATOR_TYPE(parameters_1[parameter_1], parameters_2[parameter_2]);
                }
            }
        }
    }

    /** \brief Инициализировать индикаторы c двумя параметрами
     *
     * \param symbols Массивы символов
     * \param index_1 Массивы индексов для поиска параметров индикаторов
     * \param index_2 Массивы индексов для поиска параметров индикаторов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     */
    template<class INDICATOR_TYPE,
            class SYMBOLS_TYPE,
            class TYPE_1,
            class TYPE_2,
            class INDICATORS_TYPE>
    void init_indicators(
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &index_1,
            const std::vector<uint32_t> &index_2,
            const std::vector<TYPE_1> &parameters_1,
            const std::vector<TYPE_2> &parameters_2,
            INDICATORS_TYPE &indicators) {
        if(index_1.size() != parameters_1.size() || index_2.size() != parameters_2.size())
            return;
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            for(uint32_t parameter_1 = 0;
                parameter_1 < parameters_1.size();
                ++parameter_1) {
                for(uint32_t parameter_2 = 0;
                    parameter_2 < parameters_2.size();
                    ++parameter_2) {
                    indicators[symbols[symbol]][index_1[parameter_1]][index_2[parameter_2]] =
                        INDICATOR_TYPE(parameters_1[parameter_1], parameters_2[parameter_2]);
                }
            }
        }
    }

    /** \brief Очистить состояние индикаторов с двумя параметрами
     *
     * \param symbols Массивы символов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     */
    template<class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void clear_indicators(
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters_1,
            const std::vector<uint32_t> &parameters_2,
            INDICATORS_TYPE &indicators) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            for(uint32_t parameter_1 = 0;
                parameter_1 < parameters_1.size();
                ++parameter_1) {
                for(uint32_t parameter_2 = 0;
                    parameter_2 < parameters_2.size();
                    ++parameter_2) {
                    indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                }
            }
        }
    }

    /** \brief Обновить состояние индикаторов с двумя параметрами для настройки и тремя выходными значениями
     *
     * Данная функция подойдет для работы с индикатором Боллинджер
     * \param input Массив входящих данных
     * \param output_1 Массив выходящих данных 1
     * \param output_2 Массив выходящих данных 2
     * \param output_3 Массив выходящих данных 3
     * \param symbols Массивы символов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void update_indicators(
            const std::map<std::string, CANDLE_TYPE> &input,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_1,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_2,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_3,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters_1,
            const std::vector<uint32_t> &parameters_2,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter_1 = 0;
                parameter_1 < parameters_1.size();
                ++parameter_1) {
                for(uint32_t parameter_2 = 0;
                    parameter_2 < parameters_2.size();
                    ++parameter_2) {
                    int err = xtechnical::common::NO_INIT;
                    switch(type) {
                    case TypePriceIndicator::OPEN:
                        if(it->second.open != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].update(it->second.open,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        else indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                        break;
                    case TypePriceIndicator::HIGH:
                        if(it->second.high != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].update(it->second.high,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        else indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                        break;
                    case TypePriceIndicator::LOW:
                        if(it->second.low != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].update(it->second.low,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        else indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                        break;
                    case TypePriceIndicator::CLOSE:
                        if(it->second.close != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].update(it->second.close,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        else indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                        break;
                    case TypePriceIndicator::VOLUME:
                        if(it->second.volume != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].update(it->second.volume,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        else indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].clear();
                        break;
                    }
                    if(err != xtechnical::common::OK) {
                        output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                        output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                        output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            }
        }
    }


    /** \brief Протестировать состояние индикаторов с двумя параметрами для настройки и тремя выходными значениями
     *
     * Данная функция подойдет для работы с индикатором Боллинджер
     * \param input Массив входящих данных
     * \param output_1 Массив выходящих данных 1
     * \param output_2 Массив выходящих данных 2
     * \param output_3 Массив выходящих данных 3
     * \param symbols Массивы символов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     * \param type Тип цены
     */
    template<class CANDLE_TYPE,
            class SYMBOLS_TYPE,
            class INDICATORS_TYPE>
    void test_indicators(
            const std::map<std::string, CANDLE_TYPE> &input,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_1,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_2,
            std::map<std::string, std::map<uint32_t, std::map<uint32_t, double>>> &output_3,
            const SYMBOLS_TYPE &symbols,
            const std::vector<uint32_t> &parameters_1,
            const std::vector<uint32_t> &parameters_2,
            INDICATORS_TYPE &indicators,
            const TypePriceIndicator type) {
        for(uint32_t symbol = 0; symbol < symbols.size(); ++symbol) {
            auto it = input.find(symbols[symbol]);
            if(it == input.end()) continue;
            for(uint32_t parameter_1 = 0;
                parameter_1 < parameters_1.size();
                ++parameter_1) {
                for(uint32_t parameter_2 = 0;
                    parameter_2 < parameters_2.size();
                    ++parameter_2) {
                    int err = xtechnical::common::NO_INIT;
                    switch(type) {
                    case TypePriceIndicator::OPEN:
                        if(it->second.open != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].test(it->second.open,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        break;
                    case TypePriceIndicator::HIGH:
                        if(it->second.high != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].test(it->second.high,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        break;
                    case TypePriceIndicator::LOW:
                        if(it->second.low != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].test(it->second.low,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        break;
                    case TypePriceIndicator::CLOSE:
                        if(it->second.close != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].test(it->second.close,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        break;
                    case TypePriceIndicator::VOLUME:
                        if(it->second.volume != 0) err = indicators[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]].test(it->second.volume,
                            output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]],
                            output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]]);
                        break;
                    }
                    if(err != xtechnical::common::OK) {
                        output_1[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                        output_2[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                        output_3[symbols[symbol]][parameters_1[parameter_1]][parameters_2[parameter_2]] = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            }
        }
    }

    /** \brief Получить список параметров индикатора
     *
     * \param min_parameter Минимальный параметр
     * \param max_parameter Максимальный параметр
     * \param step_parameter Шаг параметра
     * \return Массив параметров индикатора
     */
    template<class T>
    inline const static std::vector<T> get_list_parameters(
            const T min_parameter,
            const T max_parameter,
            const T step_parameter) {
        std::vector<T> parameters;
#if(0)
        for(T parameter = min_parameter;
            parameter <= max_parameter;
            parameter += step_parameter) {
            parameters.push_back(parameter);
        }
#endif
        /* так точнее double считается, по идее нафиг не нужно */
        uint32_t index = 0;
        T parameter = min_parameter;
        while(parameter <= max_parameter) {
            parameters.push_back(parameter);
            ++index;
            parameter = min_parameter + step_parameter * (T)index;
        }
        return parameters;
    }
};

#endif // OPEN-BO-API-INDICATORS_HPP_INCLUDED
