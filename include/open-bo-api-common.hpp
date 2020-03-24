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
#ifndef OPEN_BO_API_COMMON_HPP_INCLUDED
#define OPEN_BO_API_COMMON_HPP_INCLUDED

#include <xquotes_common.hpp>
#include <intrade-bar-logger.hpp>
#include <nlohmann/json.hpp>
#include <xtime.hpp>

namespace open_bo_api {
    using json = nlohmann::json;
    using Candle = xquotes_common::Candle;
    using Logger = intrade_bar::Logger;

    enum class ListBrokers {
        NOT_SELECTED = -1,
        INTRADE_BAR = 0,
        OLYMP_TRADE = 1,
    };
}

#endif // OPEN_BO_API_COMMON_HPP_INCLUDED
