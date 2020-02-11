#ifndef OPEN_BO_API_HISTORY_TESTING_HPP_INCLUDED
#define OPEN_BO_API_HISTORY_TESTING_HPP_INCLUDED

#include "xquotes_history.hpp"
#include "easy_bo_standard_tester.hpp"
#include "intrade-bar-payout-model.hpp"
#include "grandcapital-payout-model.hpp"

namespace open_bo_api {

    class HistoryTester {
    public:

        /// Тип брокера
        enum class BrokerType {
            INTRADE_BAR,    /**< Брокер intrade.bar */
            GRAND_CAPITAL,
        };

        /// Состояния сделки
        enum class BetStatus {
            UNKNOWN_STATE,
            OPENING_ERROR,
            CHECK_ERROR,        /**< Ошибка проверки результата сделки */
            WAITING_COMPLETION,
            WIN,
            LOSS,
        };

        /** \brief Класс для хранения информации по сделке
         */
        class Bet {
        public:
            uint64_t api_bet_id = 0;
            uint64_t broker_bet_id = 0;
            std::string symbol_name;
            int contract_type = 0;                      /**< Тип контракта BUY или SELL */
            uint32_t duration = 0;                      /**< Длительность контракта в секундах */
            xtime::timestamp_t opening_timestamp = 0;   /**< Метка времени начала контракта */
            xtime::timestamp_t closing_timestamp = 0;   /**< Метка времени конца контракта */
            double amount = 0;                          /**< Размер ставки в RUB или USD */
            bool is_demo_account = false;               /**< Флаг демо аккаунта */
            bool is_rub_currency = false;               /**< Флаг рублевого счета */
            BetStatus bet_status = BetStatus::UNKNOWN_STATE;

            Bet() {};
        };

    private:
        std::future<void> callback_future;
        std::atomic<bool> is_stop_command;
        std::atomic<bool> is_stopped = ATOMIC_VAR_INIT(false);

        std::mutex open_bo_future_mutex;
        std::vector<std::future<void>> open_bo_future;
        std::atomic<bool> is_open_bo_future_shutdown = ATOMIC_VAR_INIT(false);

        /** \brief Очистить список запросов
         */
        void clear_open_bo_future() {
            std::lock_guard<std::mutex> lock(open_bo_future_mutex);
            size_t index = 0;
            while(index < open_bo_future.size()) {
                try {
                    if(open_bo_future[index].valid()) {
                        open_bo_future[index].get();
                        open_bo_future.erase(open_bo_future.begin() + index);
                        continue;
                    }
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: open_bo_api::HistoryTester::clear_request_future, what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: open_bo_api::HistoryTester::clear_request_future()" << std::endl;
                }
                ++index;
            }
        }

        std::mutex hist_mutex;
        std::map<std::string,std::shared_ptr<xquotes_history::QuotesHistory<>>> hist;
        std::vector<std::string> all_symbols;

        std::atomic<double> time_increment_delay;
        std::atomic<uint64_t> server_timestamp;
        std::atomic<double> offset_timestamp;
        //std::atomic<uint64_t> open_bo_timestamp;

        /** \brief Обновить время сервера
         */
        void update_server_timestamp() {
            ++server_timestamp;
        }

        std::mutex brokers_mutex;
        std::map<BrokerType, std::shared_ptr<easy_bo::StandardTester>> brokers;

        /** \brief Имитация загрузки исторических данных
         *
         * \param candles Массив баров. Размерность: индекс символа, бары
         * \param date_timestamp Конечная дата загрузки
         * \param number_bars Количество баров
         */
        void download_historical_data(
                std::vector<std::vector<xquotes_common::Candle>> &candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars) {

            const xtime::timestamp_t first_timestamp = xtime::get_first_timestamp_minute(date_timestamp);
            const xtime::timestamp_t start_timestamp = first_timestamp - (number_bars - 1) * xtime::SECONDS_IN_MINUTE;
            const xtime::timestamp_t stop_timestamp = first_timestamp;
            candles.resize(all_symbols.size());

            for(uint32_t symbol_index = 0;
                symbol_index < all_symbols.size();
                ++symbol_index) {
                std::vector<xquotes_common::Candle> raw_candles;
                for(xtime::timestamp_t t = start_timestamp;
                    t <= stop_timestamp;
                    t += xtime::SECONDS_IN_MINUTE) {
                    raw_candles.push_back(get_timestamp_candle(all_symbols[symbol_index], t));
                }
                candles[symbol_index] = raw_candles;
            }
        }

        /** \brief Имитация загрузки исторических данных
         */
        void download_historical_data(
                std::vector<std::map<std::string,xquotes_common::Candle>> &array_candles,
                const xtime::timestamp_t date_timestamp,
                const uint32_t number_bars) {
            std::vector<std::vector<xquotes_common::Candle>> candles;
            download_historical_data(candles, date_timestamp, number_bars);

            const xtime::timestamp_t first_timestamp = xtime::get_first_timestamp_minute(date_timestamp);
            const xtime::timestamp_t start_timestamp = first_timestamp - (number_bars - 1) * xtime::SECONDS_IN_MINUTE;
            array_candles.resize(number_bars);
            for(uint32_t symbol_index = 0;
                symbol_index < all_symbols.size();
                ++symbol_index) {
                for(size_t i = 0; i < array_candles.size(); ++i) {
                    array_candles[i][all_symbols[symbol_index]].timestamp = i * xtime::SECONDS_IN_MINUTE + start_timestamp;
                }
                for(size_t i = 0; i < candles[symbol_index].size(); ++i) {
                    const uint32_t index = (candles[symbol_index][i].timestamp - start_timestamp) / xtime::SECONDS_IN_MINUTE;
                    array_candles[index][all_symbols[symbol_index]] = candles[symbol_index][i];
                }
            }
        }

    public:

        /// Типы События
        enum class EventType {
            NEW_TICK,                   /**< Получен новый тик */
            HISTORICAL_DATA_RECEIVED,   /**< Получены исторические данные */
        };

        /// Тип хранилища исторических данных
        enum class StorageType {
            QHS4,   /**< Хранилище цен баров */
            QHS5,   /**< Хранилище цен баров и объема */
        };

        HistoryTester(
                const std::string &path,
                const std::vector<std::string> &symbols,
                //const StorageType storage_type,
                const double time_speed,
                const xtime::timestamp_t start_timestamp,
                const xtime::timestamp_t stop_timestamp,
                const uint32_t number_bars,
                std::function<void(
                    const std::map<std::string,xquotes_common::Candle> &candles,
                    const EventType event,
                    const xtime::timestamp_t timestamp)> callback = nullptr) {
            is_stop_command = false;

            time_increment_delay = 1000 / time_speed;
            server_timestamp = start_timestamp;
            //open_bo_timestamp = server_timestamp;

            /* инициализируем хранилища исторических даных */
            all_symbols = symbols;
            {
                std::lock_guard<std::mutex> lock(hist_mutex);
                for(size_t i = 0; i < symbols.size(); ++i) {
                    std::string storage_path(path + "//" + symbols[i]);

                    //switch(storage_type) {
                     //   case StorageType::QHS4:
                     //   storage_path += ".qhs4";

                     //   hist[symbols[i]] = std::make_shared<xquotes_history::QuotesHistory<>>(
                     //       storage_path,
                     //       xquotes_history::PRICE_OHLC,
                     //       xquotes_history::USE_COMPRESSION);
                      //      break;
                     //   case StorageType::QHS5:

                        hist[symbols[i]] = std::make_shared<xquotes_history::QuotesHistory<>>(
                            storage_path,
                            xquotes_history::PRICE_OHLCV,
                            xquotes_history::USE_COMPRESSION);
                            break;
                    //};
                }
            }

            callback_future = std::async(std::launch::async,[&, number_bars, start_timestamp, callback]() {
                /* сначала инициализируем исторические данные */
                uint32_t hist_data_number_bars = number_bars;
                while(!is_stop_command) {
                    /* первым делом грузим исторические данные в несколько потоков */
                    const xtime::timestamp_t init_date_timestamp =
                        xtime::get_first_timestamp_minute(get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;
                    std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                    download_historical_data(array_candles, init_date_timestamp, hist_data_number_bars);

                    /* далее отправляем загруженные данные в callback */
                    const xtime::timestamp_t candle_start_timestamp =
                        init_date_timestamp - (hist_data_number_bars - 1) * xtime::SECONDS_IN_MINUTE;

                    for(size_t i = 0; i < array_candles.size(); ++i) {
                        const xtime::timestamp_t timestamp = i * xtime::SECONDS_IN_MINUTE + candle_start_timestamp;
                        if(callback != nullptr) callback(array_candles[i], EventType::HISTORICAL_DATA_RECEIVED, timestamp);
                    }

                    /* обновим состояние баланса */
                    {
                        std::lock_guard<std::mutex> lock(brokers_mutex);
                        for(auto &it : brokers) {
                            it.second->update_timestamp(get_server_timestamp());
                        }
                    }

                    const xtime::timestamp_t end_date_timestamp =
                        xtime::get_first_timestamp_minute(get_server_timestamp()) -
                        xtime::SECONDS_IN_MINUTE;
                    if(end_date_timestamp == init_date_timestamp) break;
                    hist_data_number_bars = (end_date_timestamp - init_date_timestamp) / xtime::SECONDS_IN_MINUTE;
					std::this_thread::yield();
                }

                /* далее занимаемся получением новызх тиков */
                xtime::timestamp_t last_timestamp = (xtime::timestamp_t)(get_server_timestamp() + 0.5);
                uint64_t last_minute = last_timestamp / xtime::SECONDS_IN_MINUTE;
                while(!is_stop_command && get_server_timestamp() <= stop_timestamp) {
                    xtime::ftimestamp_t server_ftimestamp = get_server_timestamp();
                    xtime::timestamp_t timestamp = (xtime::timestamp_t)(server_ftimestamp + 0.5);
                    if(timestamp <= last_timestamp) {
                        /* реализуем задержку на секунду и
                         * затем делаем приращение метки врмени сервера
                         */
						std::this_thread::sleep_for(std::chrono::milliseconds(time_increment_delay));
						update_server_timestamp();
                        continue;
                    }

                    /* обновим состояние баланса */
                    {
                        std::lock_guard<std::mutex> lock(brokers_mutex);
                        for(auto &it : brokers) {
                            it.second->update_timestamp(get_server_timestamp());
                        }
                    }

                    /* начало новой секунды,
                     * собираем актуальные цены бара и вызываем callback
                     */
                    last_timestamp = timestamp;

                    std::map<std::string,xquotes_common::Candle> candles;
                    for(uint32_t symbol_index = 0;
                        symbol_index < all_symbols.size();
                        ++symbol_index) {
                        candles[all_symbols[symbol_index]] =
                            get_timestamp_candle(all_symbols[symbol_index],
                                timestamp);
                    }

                    /* вызов callback */
                    //open_bo_timestamp = timestamp;
                    if(callback != nullptr) callback(candles, EventType::NEW_TICK, timestamp);

                    /* загрузка исторических данных и повторный вызов callback,
                     * если нужно
                     */
                    uint64_t server_minute = timestamp / xtime::SECONDS_IN_MINUTE;
                    if(server_minute <= last_minute) {
                        std::this_thread::yield();
                        continue;
                    }
                    last_minute = server_minute;

                    /* загружаем исторические данные в несколько потоков */
                    const xtime::timestamp_t download_date_timestamp =
                        xtime::get_first_timestamp_minute(timestamp) -
                        xtime::SECONDS_IN_MINUTE;
                    //std::cout << "download_historical_data: " << xtime::get_str_date_time(download_date_timestamp) << std::endl;
                    std::vector<std::map<std::string,xquotes_common::Candle>> array_candles;
                    download_historical_data(array_candles, download_date_timestamp, 1);
                    if(callback != nullptr) callback(array_candles[0], EventType::HISTORICAL_DATA_RECEIVED, download_date_timestamp);
					std::this_thread::yield();
                }
                is_stopped = true;
            });
        }

        ~HistoryTester() {
            is_stop_command = true;
            if(callback_future.valid()) {
                try {
                    callback_future.wait();
                    callback_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "Error: open_bo_api::~HistoryTester(), what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Error: open_bo_api::~HistoryTester()" << std::endl;
                }
            }
        }

        inline bool is_testing_stopped() {
            return is_stopped;
        }

        /** \brief Получить метку времени ПК
         *
         * Данный метод возвращает метку времени сервера. Часовая зона: UTC/GMT
         * \return Метка времени сервера
         */
        inline xtime::timestamp_t get_server_timestamp() {
            return server_timestamp;
        }

        /** \brief Получить бар по имени
         *
         * \param symbol_name Имя валютной пары
         * \return candles Карта баров валютных пар
         */
        inline const static xquotes_common::Candle get_candle(
                const std::string &symbol_name,
                const std::map<std::string, xquotes_common::Candle> &candles) {
            auto it = candles.find(symbol_name);
            if(it == candles.end()) return xquotes_common::Candle();
            if(it->second.close == 0 || it->second.timestamp == 0) return xquotes_common::Candle();
            return it->second;
        }

        /** \brief Проверить бар
         * \param candle Бар
         * \return Вернет true, если данные по бару корректны
         */
        inline const static bool check_candle(xquotes_common::Candle &candle) {
            if(candle.close == 0 || candle.timestamp == 0) return false;
            return true;
        }

        /** \brief Получить бар по метке времени
         *
         * \param symbol_name Имя символа
         * \param timestamp Метка времени бара
         * \return Цена
         */
        inline xquotes_common::Candle get_timestamp_candle(
                const std::string &symbol_name,
                const xtime::timestamp_t timestamp) {
            xquotes_common::Candle candle;
            candle.timestamp =  xtime::get_first_timestamp_minute(timestamp);
            std::lock_guard<std::mutex> lock(hist_mutex);
            if(hist.find(symbol_name) == hist.end()) return candle;
            hist[symbol_name]->get_candle(candle,
                xtime::get_first_timestamp_minute(timestamp));
            return candle;
        }

        /** \brief Получить массив баров всех валютных пар по метке времени
         * \param timestamp Метка времени
         * \return Массив всех баров
         */
        std::map<std::string, xquotes_common::Candle> get_candles(const xtime::timestamp_t timestamp) {
            std::map<std::string, xquotes_common::Candle> candles;
            for(uint32_t symbol_index = 0;
                symbol_index < all_symbols.size();
                ++symbol_index) {
                candles[all_symbols[symbol_index]] = get_timestamp_candle(
                    all_symbols[symbol_index],
                    timestamp);
            }
            return candles;
        }

        /** \brief Имитировать подключение к брокеру
         * \param broker_type Тип брокера
         * \param deposit Начальный депозит
         */
        void connect(const BrokerType broker_type, const double deposit) {
            std::lock_guard<std::mutex> lock(brokers_mutex);
            brokers[broker_type] = std::make_shared<easy_bo::StandardTester>(deposit);
        }


        /** \brief Получить баланс счета
         * \param broker_type Тип брокера
         * \return Баланс аккаунта
         */
        inline double get_balance(const BrokerType broker_type) {
            std::lock_guard<std::mutex> lock(brokers_mutex);
            if(brokers.find(broker_type) == brokers.end()) return 0;
            return brokers[broker_type]->get_balance_curve().back();
        }
// открытие сделок пока не реализовано
#if(0)
        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param broker_type Тип брокера
         * \param symbol Символ
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const BrokerType broker_type,
                const std::string &symbol,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                std::function<void(const Bet &bet)> callback = nullptr) {
            std::string note;
            uint32_t api_bet_id = 0;
            return http_api.async_open_bo_sprint(
                symbol,
                note,
                amount,
                contract_type,
                duration,
                api_bet_id,
                callback);
        }

        /** \brief Открыть бинарный опцион
         *
         * Данный метод открывает бинарный опцион типа Спринт
         * \param broker_type Тип брокера
         * \param symbol Символ
         * \param amount Размер ставки
         * \param contract_type Тип контракта (BUY или SELL)
         * \param duration Длительность экспирации опциона
         * \param callback Функция для обратного вызова
         * \return Код ошибки
         */
        int open_bo(
                const BrokerType broker_type,
                const std::string &symbol,
                const std::string &note,
                const double amount,
                const int contract_type,
                const uint32_t duration,
                uint32_t &api_bet_id,
                std::function<void(const Bet &bet)> callback = nullptr) {

            /* проверяем время открытия */
            uint32_t second = xtime::get_second_minute((xtime::timestamp_t)open_bo_timestamp);
            xtime::timestamp_t bo_open_timestamp = (second == 58 || second == 59) ?
                xtime::get_first_timestamp_minute((xtime::timestamp_t)open_bo_timestamp) :
                second == 0 ? xtime::get_first_timestamp_minute((xtime::timestamp_t)open_bo_timestamp) - xtime::SECONDS_IN_MINUTE : 0;
            if(bo_open_timestamp == 0) return intrade_bar_common::INVALID_ARGUMENT;

            /* проверяем имя валютной пары */
            switch(broker_type) {
            case INTRADE_BAR:
                if(!payout_model::IntradeBar::check_currecy_pair_name(symbol)) return intrade_bar_common::INVALID_ARGUMENT;
                break;
            case GRAND_CAPITAL:
                if(!payout_model::Grandcapital::check_currecy_pair_name(symbol)) return intrade_bar_common::INVALID_ARGUMENT;
                break;
            }

            /* находим состояние опциона */
            int bo_state = 0;
            {
                std::lock_guard<std::mutex> lock(hist_mutex);
                if(hist.find(symbol_name) == hist.end()) {
                    switch(broker_type) {
                    case INTRADE_BAR:
                        return intrade_bar_common::INVALID_ARGUMENT;
                        break;
                    case GRAND_CAPITAL:
                        return intrade_bar_common::INVALID_ARGUMENT;
                        break;
                    }
                }
                int err = hist[symbol_name]->check_protected_binary_option(
                    bo_state,
                    contract_type,
                    duration,
                    bo_open_timestamp);
                if(err != xquotes_common::OK) return intrade_bar_common::INVALID_ARGUMENT;
            }

            /* далее добавляем сделку */
            {
                std::lock_guard<std::mutex> lock(request_future_mutex);
                request_future.resize(request_future.size() + 1);
                request_future.back() = std::async(std::launch::async,[&,
                        bo_open_timestamp,
                        amount
                        duration,
                        bo_state,
                        callback] {
                    const xtime::timestamp_t bo_close_timestamp = bo_open_timestamp + duration;
                    /* ждем закрытия опциона */
                    while(bo_close_timestamp > get_server_timestamp()) {
                        std::this_thread::yield();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    };
                    /* вызываем callback */
                    Bet new_bet;
                    new_bet.amount = amount;
                    new_bet.api_bet_id = api_bet_id;
                    new_bet.bet_status = BetStatus::OPENING_ERROR;
                    new_bet.contract_type = contract_type;
                    new_bet.duration = duration;
                    new_bet.is_demo_account = is_demo_account;
                    new_bet.is_rub_currency = is_rub_currency;
                    new_bet.symbol_name = symbol;
                    new_bet.opening_timestamp = open_timestamp;
                    new_bet.closing_timestamp = open_timestamp + duration;
                    new_bet.broker_bet_id = id_deal;
                    if(callback != nullptr) callback(new_bet);
                });
            }
        }
#endif
    };
}

#endif // OPEN-BO-API-HISTORY-TESTING_HPP_INCLUDED
