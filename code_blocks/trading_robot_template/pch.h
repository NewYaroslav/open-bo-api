#ifndef PCH_H_INCLUDED
#define PCH_H_INCLUDED

#include <fstream>
#include <sstream>

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
#include "client_wss.hpp"
#include <openssl/ssl.h>
#include <boost/asio/ssl.hpp>

#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>

#include <xquotes_common.hpp>
#include <xtime.hpp>

#include "intrade-bar-https-api.hpp"
#include "intrade-bar-websocket-api.hpp"

#include <intrade-bar-common.hpp>
#include <intrade-bar-logger.hpp>

#include <wincrypt.h>
#include <stdio.h>
#include "utf8.h" // http://utfcpp.sourceforge.net/

#endif // PCH_H_INCLUDED
