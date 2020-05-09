#ifndef PCH_H_INCLUDED
#define PCH_H_INCLUDED

#include <fstream>
#include <sstream>

#include <csignal>

#include <map>
#include <list>
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>

#include <thread>
#include <mutex>
#include <atomic>

#include <cctype>
#include <algorithm>
#include <random>
#include <ctime>
#include <limits>

#include <curl/curl.h>
#include <client_ws.hpp>
#include <client_wss.hpp>
#include <openssl/ssl.h>
#include <boost/asio/ssl.hpp>

#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>

#include <xquotes_common.hpp>
#include <xtime.hpp>

/* брокер intrade.bar */
#include <intrade-bar-common.hpp>
#include <intrade-bar-logger.hpp>
#include <intrade-bar-api.hpp>
#include <intrade-bar-payout-model.hpp>
#include <intrade-bar-https-api.hpp>
#include <intrade-bar-websocket-api-v2.hpp>

#include <wincrypt.h>
#include <stdio.h>
#include "utf8.h" // http://utfcpp.sourceforge.net/

#include "ForexprostoolsApi.hpp"

#include "open-bo-api.hpp"
#include "open-bo-api-news.hpp"
#include "open-bo-api-settings.hpp"
#include "open-bo-api-indicators.hpp"
#include "open-bo-api-command-line-tools.hpp"

#include "xtechnical_common.hpp"
#include "xtechnical_indicators.hpp"
#include "xtechnical_correlation.hpp"
#include "xtechnical_normalization.hpp"
#include "xtechnical_statistics.hpp"

#endif // PCH_H_INCLUDED
