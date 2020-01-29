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

namespace open_bo_api {

    /** \brief Инициализировать индикаторы
     *
     * \param symbols Массивы символов
     * \param parameters Массивы параметров
     * \param indicators Массив индикаторов
     */
    template<class INDICATOR_TYPE, class SYMBOLS_TYPE, class PARAMETERS_TYPE, class INDICATORS_TYPE>
    void init_indicators(
            const SYMBOLS_TYPE &symbols,
            const PARAMETERS_TYPE &parameters,
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

    /** \brief Инициализировать индикаторы c двумя параметрами
     *
     * \param symbols Массивы символов
     * \param parameters_1 Массивы параметров индикаторов
     * \param parameters_2 Массивы параметров индикаторов
     * \param indicators Массив индикаторов
     */
    template<class INDICATOR_TYPE, class SYMBOLS_TYPE, class PARAMETERS_TYPE, class INDICATORS_TYPE>
    void init_indicators(
            const SYMBOLS_TYPE &symbols,
            const PARAMETERS_TYPE &parameters_1,
            const PARAMETERS_TYPE &parameters_2,
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
        for(T parameter = min_parameter;
            parameter <= max_parameter;
            parameter += step_parameter) {
            parameters.push_back(parameter);
        }
        return parameters;
    }
};

#endif // OPEN-BO-API-INDICATORS_HPP_INCLUDED
