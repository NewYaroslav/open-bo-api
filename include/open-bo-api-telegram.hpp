#ifndef OPEN_BO_API_TELEGRAM_HPP_INCLUDED
#define OPEN_BO_API_TELEGRAM_HPP_INCLUDED

#include <curl/curl.h>
#include <gzip/decompress.hpp>
#include <nlohmann/json.hpp>
#include <xtime.hpp>
#include <algorithm>

#define OPEN_BO_API_TELEGRAM_EMOJI_TIME u8"🕓"
#define OPEN_BO_API_TELEGRAM_EMOJI_WARNING u8"⚠️"
#define OPEN_BO_API_TELEGRAM_EMOJI_STOP u8"⛔️"
#define OPEN_BO_API_TELEGRAM_EMOJI_GOOD u8"✅"
#define OPEN_BO_API_TELEGRAM_EMOJI_BAD u8"❌"
#define OPEN_BO_API_TELEGRAM_EMOJI_PAUSE u8"⏸"
#define OPEN_BO_API_TELEGRAM_EMOJI_PLAY u8"▶️"
#define OPEN_BO_API_TELEGRAM_EMOJI_RECORD u8"🔴"
#define OPEN_BO_API_TELEGRAM_EMOJI_DOLLAR u8"💵"
#define OPEN_BO_API_TELEGRAM_EMOJI_UP u8"🔼"
#define OPEN_BO_API_TELEGRAM_EMOJI_DOWN u8"🔽"

namespace open_bo_api {

    /** \brief Класс API телеграмма
     */
    class TelegramApi {
    public:
        enum TypesErrors {
            OK = 0,
            CURL_EASY_INIT_ERROR = -1,
            NO_ANSWER = -2,
            DECOMPRESSION_ERROR = -3,
            REQUEST_RETURN_ERROR = -4,
            PARSER_ERROR = -5,
            CONTENT_ENCODING_NOT_SUPPORT = -6,
            CHAT_ID_NOT_FOUND = -7,
            UNKNOWN_ERROR = -8,
        };

        const uint32_t REPEAT_DELAY = 10000;
    private:
        std::string token;
        std::string proxy;
        std::string proxy_pwd;
        std::string sert_file;
        std::string chat_id_json_file;

        uint32_t timeout = 120;

        std::atomic<bool> is_token = ATOMIC_VAR_INIT(false);

        std::mutex chats_id_mutex;
        std::map<std::string, int64_t> chats_id;
        std::mutex file_chats_id_mutex;

        std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);

        std::atomic<bool> is_request_future_shutdown = ATOMIC_VAR_INIT(false);

        // для работы с массивом сообщений на отправление (начало)
        std::future<void> messages_future;
        std::mutex array_sent_messages_mutex;

        /** \brief Класс для хранения собщений на отправку
         */
        class OutputMessage {
        public:
            int64_t chat_id = 0;
            std::string message;
            OutputMessage() {};

            OutputMessage(const int64_t send_chat_id, const std::string &send_message) :
                chat_id(send_chat_id), message(send_message) {};
        };

        std::vector<OutputMessage> array_sent_messages;

        int add_send_message(const int64_t chat_id, const std::string &message) {
            {
                std::lock_guard<std::mutex> lock(array_sent_messages_mutex);
                array_sent_messages.push_back(OutputMessage(chat_id, message));
            }
            return OK;
        }

        int add_send_message(const std::string &chat_name, const std::string &message) {
            int64_t chat_id = 0;
            {
                std::lock_guard<std::mutex> lock(chats_id_mutex);
                auto it = chats_id.find(chat_name);
                if(it == chats_id.end()) return CHAT_ID_NOT_FOUND;
                chat_id = it->second;
            }
            return add_send_message(chat_id, message);
        }

        std::atomic<bool> is_update_chats_id_store = ATOMIC_VAR_INIT(false);
        // для работы с массивом сообщений на отправление (конец)

        /** \brief Разобрать путь на составляющие
         *
         * Данный метод парсит путь, например C:/Users\\user/Downloads разложит на
         * C: Users user и Downloads
         * \param path путь, который надо распарсить
         * \param output_list список полученных элементов
         */
        void parse_path(std::string path, std::vector<std::string> &output_list) {
            if(path.back() != '/' && path.back() != '\\')
                path += "/";
            std::size_t start_pos = 0;
            while(true) {
                std::size_t found_beg = path.find_first_of("/\\", start_pos);
                if(found_beg != std::string::npos) {
                    std::size_t len = found_beg - start_pos;
                    if(len > 0) output_list.push_back(path.substr(start_pos, len));
                    start_pos = found_beg + 1;
                } else break;
            }
        }

        /** \brief Создать директорию
         * \param path директория, которую необходимо создать
         */
        void create_directory(std::string path) {
            std::vector<std::string> dir_list;
            parse_path(path, dir_list);
            std::string name;
            for(size_t i = 0; i < (dir_list.size() - 1); i++) {
                name += dir_list[i] + "\\";
                if(dir_list[i] == "..") continue;
                mkdir(name.c_str());
            }
        }

        /// Варианты кодирования
        enum {
            USE_CONTENT_ENCODING_GZIP = 1,          ///< Сжатие GZIP
            USE_CONTENT_ENCODING_IDENTITY = 2,      ///< Без кодирования
            USE_CONTENT_ENCODING_NOT_SUPPORED = 3,  ///< Без кодирования
        };

        /** \brief Callback-функция для обработки HTTP Header ответа
         * Данный метод нужен, чтобы определить, какой тип сжатия данных используется (или сжатие не используется)
         * Данный метод нужен для внутреннего использования
         */
        static int header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
            const char CONTENT_ENCODING_GZIP[] = "Content-Encoding: gzip";
            const char CONTENT_ENCODING_IDENTITY[] = "Content-Encoding: identity";
            const char CONTENT_ENCODING[] = "Content-Encoding:";
            size_t buffer_size = nitems * size;
            int *content_encoding = (int*)userdata;
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_GZIP) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_GZIP, sizeof(CONTENT_ENCODING_GZIP) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_GZIP;
                }
            } else
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING_IDENTITY) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING_IDENTITY, sizeof(CONTENT_ENCODING_IDENTITY) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_IDENTITY;
                }
            } else
            if(content_encoding[0] == 0 && buffer_size >= (sizeof(CONTENT_ENCODING) - 1)) {
                if(strncmp(buffer, CONTENT_ENCODING, sizeof(CONTENT_ENCODING) - 1) == 0) {
                    content_encoding[0] = USE_CONTENT_ENCODING_NOT_SUPPORED;
                }
            }
            return buffer_size;
        }

        static int writer(char *data, size_t size, size_t nmemb, std::string *buffer) {
            int result = 0;
            if (buffer != NULL) {
                buffer->append(data, size * nmemb);
                result = size * nmemb;
            }
            return result;
        }

        /** \brief Часть парсинга HTML
         * Данный метод нужен для внутреннего использования
         */
        std::size_t get_string_fragment(
                const std::string &str,
                const std::string &div_beg,
                const std::string &div_end,
                std::string &out,
                std::size_t start_pos = 0) {
            std::size_t beg_pos = str.find(div_beg, start_pos);
            if(beg_pos != std::string::npos) {
                std::size_t end_pos = str.find(div_end, beg_pos + div_beg.size());
                if(end_pos != std::string::npos) {
                    out = str.substr(beg_pos + div_beg.size(), end_pos - beg_pos - div_beg.size());
                    return end_pos;
                } else return std::string::npos;
            } else return std::string::npos;
        }

#if(0)
        int get_list_proxy(std::vector<std::string> &list_http_proxy) {
            CURL *curl;
            curl = curl_easy_init();
            std::cout << "get_list_proxy" << std::endl;
            if(!curl) return CURL_EASY_INIT_ERROR;
            /* формируем запрос */
            std::string url("https://hidemy.name/ru/proxy-list/?type=s#list");
            char error_buffer[CURL_ERROR_SIZE];
            const int TIME_OUT = 60;
            int content_encoding = 0;   // Тип кодирования сообщения
            std::string buffer;

            curl_easy_setopt(curl, CURLOPT_POST, 0);
            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CAINFO, sert_file.c_str());
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0); // отключаем заголовок в ответе
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIME_OUT);

            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_encoding);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

            struct curl_slist *http_headers = NULL;
            http_headers = curl_slist_append(http_headers, "Host: hidemy.name");
            http_headers = curl_slist_append(http_headers, "User-Agent: Mozilla/5.0 (Windows NT 6.3; Win64; x64; rv:72.0) Gecko/20100101 Firefox/72.0");
            http_headers = curl_slist_append(http_headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            http_headers = curl_slist_append(http_headers, "Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.5,en;q=0.3");
            http_headers = curl_slist_append(http_headers, "Accept-Encoding: gzip, deflate");
            //http_headers = curl_slist_append(http_headers, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
            http_headers = curl_slist_append(http_headers, "Upgrade-Insecure-Requests: 1");
            http_headers = curl_slist_append(http_headers, "Connection: keep-alive");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);

            CURLcode result;
            result = curl_easy_perform(curl);

            curl_slist_free_all(http_headers);
            http_headers = NULL;
            curl_easy_cleanup(curl);

            //std::cout << "buffer " << buffer <<  std::endl;
            std::string response;
            if(result == CURLE_OK) {
                if(content_encoding == USE_CONTENT_ENCODING_GZIP) {
                    std::cout << "USE_CONTENT_ENCODING_GZIP" << std::endl;
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    std::cout << "USE_CONTENT_ENCODING_IDENTITY" << std::endl;

                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    std::cout << "USE_CONTENT_ENCODING_NOT_SUPPORED" << std::endl;
                    return CONTENT_ENCODING_NOT_SUPPORT;
                } else {
                    std::cout << "USE_CONTENT_ENCODING_IDENTITY-2" << std::endl;
                    response = buffer;
                }

                std::ofstream o("response.txt");
                o << response << std::endl;
                o.close();

                std::size_t beg_pos = response.find("<tbody>");
                std::size_t end_pos = response.find("</tbody>");
                if(beg_pos == std::string::npos || end_pos == std::string::npos) return NO_ANSWER;
                std::string temp = response.substr(beg_pos, end_pos - beg_pos);


                // <td>118.174.211.220</td>
                size_t offset = 0; // смещение в ответе от сервера
                while(true) {
                    std::string id_address;
                    size_t new_offset = get_string_fragment(temp, "<td>", "</td>", id_address, offset);
                    if(new_offset == std::string::npos) break;
                    offset = new_offset;
                    if(std::count(id_address.begin(), id_address.end(), '.') != 4) continue;
                    std::string port;
                    new_offset = get_string_fragment(temp, "<td>", "</td>", port, offset);
                    if(new_offset == std::string::npos) break;
                    offset = new_offset;
                    list_http_proxy.push_back(id_address + ":" + port);
                    std::cout << list_http_proxy.back() << std::endl;
                }
                return OK;
            }
            std::cerr << "Error: [" << result << "] - " << error_buffer << std::endl;
            return result;
        }
#endif

        /** \brief Отправить запрос
         *
         * \param is_post_request Тип запроса
         * \param method_name Имя метода
         * \param request_body Тело запроса
         * \param response Ответ
         * \return Код ошибки
         */
        int do_request(
                const bool is_post_request,
                const std::string &bot_proxy,
                const std::string &bot_proxy_pwd,
                const std::string &bot_token,
                const std::string &method_name,
                const std::string &request_body,
                std::string &response) {
            CURL *curl;
            curl = curl_easy_init();
            if(!curl) return CURL_EASY_INIT_ERROR;
            /* формируем запрос */
            std::string url("https://api.telegram.org/bot");
            url += token;
            url += "/";
            url += method_name;

            char error_buffer[CURL_ERROR_SIZE];
            int content_encoding = 0;   // Тип кодирования сообщения

            std::string buffer;

            if(is_post_request) curl_easy_setopt(curl, CURLOPT_POST, 1L);
            else curl_easy_setopt(curl, CURLOPT_POST, 0);

            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
            //curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            if(bot_proxy.size() != 0) {
                curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
                curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
                //curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1);
                curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);
                curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
                if(bot_proxy_pwd.size() != 0) {
                    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, bot_proxy_pwd.c_str());
                }
            }

            curl_easy_setopt(curl, CURLOPT_CAINFO, sert_file.c_str());
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0); // отключаем заголовок в ответе
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_encoding);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

            struct curl_slist *http_headers = NULL;
            http_headers = curl_slist_append(http_headers, "Host: api.telegram.org");
            http_headers = curl_slist_append(http_headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; rv:68.0) Gecko/20100101 Firefox/68.0");
            http_headers = curl_slist_append(http_headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
            http_headers = curl_slist_append(http_headers, "Accept-Language: en-US,en;q=0.5");
            http_headers = curl_slist_append(http_headers, "Accept-Encoding: gzip, deflate");
            //http_headers = curl_slist_append(http_headers, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
            http_headers = curl_slist_append(http_headers, "Upgrade-Insecure-Requests: 1");
            http_headers = curl_slist_append(http_headers, "Connection: keep-alive");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);

            if(is_post_request) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());

            CURLcode result;
            result = curl_easy_perform(curl);

            curl_slist_free_all(http_headers);
            http_headers = NULL;

            curl_easy_cleanup(curl);
            if(result == CURLE_OK) {
                if(content_encoding == USE_CONTENT_ENCODING_GZIP) {
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding == USE_CONTENT_ENCODING_IDENTITY) {
                    response = buffer;
                } else
                if(content_encoding == USE_CONTENT_ENCODING_NOT_SUPPORED) {
                    return CONTENT_ENCODING_NOT_SUPPORT;
                } else {
                    response = buffer;
                }
                return OK;
            }
            std::cerr << "open_bo_api::TelegramApi::do_request curl error: [" << result << "] - " << error_buffer << std::endl;
            return result;
        }

        int do_get_updates() {
            std::string response;
            int err = do_request(true, proxy, proxy_pwd, token, "getUpdates", "", response);
            if(err != OK) return err;
            //std::cout << response << std::endl;
            try {
                json j = json::parse(response);
                if(j["ok"] != true) return REQUEST_RETURN_ERROR;
                const size_t result_size = j["result"].size();
                //std::cout << "result_size " << result_size << std::endl;
                for(size_t i = 0; i < result_size; ++i) {
                    int64_t chat_id = j["result"][i]["message"]["chat"]["id"];
                    //std::cout << chat_id << std::endl;
                    if(j["result"][i]["message"]["chat"]["type"] == "private") {
                        std::string username = j["result"][i]["message"]["chat"]["username"];
                        {
                            std::lock_guard<std::mutex> lock(chats_id_mutex);
                            chats_id[username] = chat_id;
                        }
                        //std::cout << username << " id: " << chat_id << std::endl;
                    } else
                    if(j["result"][i]["message"]["chat"]["type"] == "group") {
                        std::string title = j["result"][i]["message"]["chat"]["title"];
                        {
                            std::lock_guard<std::mutex> lock(chats_id_mutex);
                            chats_id[title] = chat_id;
                        }
                        //std::cout << title << " id: " << chat_id << std::endl;
                    }
                }
            }
            catch (json::parse_error &e) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch (std::exception e) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch(...) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error" << std::endl;
                return PARSER_ERROR;
            }

            std::lock_guard<std::mutex> lock(file_chats_id_mutex);
            create_directory(chat_id_json_file);
            std::ofstream file(chat_id_json_file);
            if(!file) return UNKNOWN_ERROR;
            try {
                json j_chat_id;
                {
                    std::lock_guard<std::mutex> lock(chats_id_mutex);
                    j_chat_id["chats_id"] = chats_id;
                }
                file << std::setw(4) << j_chat_id << std::endl;
            }
            catch (json::parse_error &e) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch (std::exception e) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch(...) {
                std::cerr << "open_bo_api::TelegramApi::do_get_updates(), json parser error" << std::endl;
                return PARSER_ERROR;
            }
            file.close();
            return OK;
        }

        int do_send_message(const int64_t chat_id, const std::string &message) {
            std::string response;
            std::string method_name("sendMessage?chat_id=");
            method_name += std::to_string(chat_id);
            method_name += "&text=";
            method_name += message;
            int err = do_request(true, proxy, proxy_pwd, token, method_name, "", response);
            if(err != OK) return err;
            try {
                json j = json::parse(response);
                if(j["ok"] != true) return REQUEST_RETURN_ERROR;
            }
            catch (json::parse_error &e) {
                std::cerr << "open_bo_api::TelegramApi::do_send_message(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch (std::exception e) {
                std::cerr << "open_bo_api::TelegramApi::do_send_message(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch(...) {
                std::cerr << "open_bo_api::TelegramApi::do_send_message(), json parser error" << std::endl;
                return PARSER_ERROR;
            }
            return OK;
        }


        int do_send_message(const std::string &chat_name, const std::string &message) {
            int64_t chat_id = 0;
            {
                std::lock_guard<std::mutex> lock(chats_id_mutex);
                auto it = chats_id.find(chat_name);
                if(it == chats_id.end()) return CHAT_ID_NOT_FOUND;
                chat_id = it->second;
            }
            return do_send_message(chat_id, message);
        }

        int load_chat_id() {
            std::lock_guard<std::mutex> lock(file_chats_id_mutex);
            std::ifstream file(chat_id_json_file);
            if(!file) return UNKNOWN_ERROR;
            try {
                json j;
                file >> j;
                std::map<std::string,int64_t> temp = j["chats_id"];
                std::map<std::string,int64_t> temp2 = chats_id;
                chats_id.clear();
                std::merge(temp.begin(), temp.end(), temp2.begin(), temp2.end(), std::inserter(chats_id, chats_id.begin()));
            }
            catch (json::parse_error &e) {
                std::cerr << "open_bo_api::TelegramApi::load_chat_id(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch (std::exception e) {
                std::cerr << "open_bo_api::TelegramApi::load_chat_id(), json parser error: " << std::string(e.what()) << std::endl;
                return PARSER_ERROR;
            }
            catch(...) {
                std::cerr << "open_bo_api::TelegramApi::load_chat_id(), json parser error" << std::endl;
                return PARSER_ERROR;
            }
            file.close();
            return OK;
        }

    public:

        TelegramApi(
                const std::string &bot_token,
                const std::string &bot_proxy,
                const std::string &bot_proxy_pwd,
                const std::string &bot_sert_file = "curl-ca-bundle.crt",
                const std::string &bot_chat_id_json_file = "save_chat_id.json",
                const uint32_t bot_timeout = 120) :
                token(bot_token), proxy(bot_proxy), proxy_pwd(bot_proxy_pwd),
                sert_file(bot_sert_file), chat_id_json_file(bot_chat_id_json_file), timeout(bot_timeout)  {
            if(bot_token.size() != 0) is_token = true;

            if(curl_global_init(CURL_GLOBAL_ALL) !=0) {
                is_error = true;
                return;
            }
            if(!is_token) return;
            load_chat_id();
            messages_future = std::async(std::launch::async,[&] {
                while(!is_request_future_shutdown) {
                    if(is_update_chats_id_store) {
                        int err = OK;
                        const size_t attempts = 10;
                        for(size_t n = 0; n < attempts; ++n) {
                            if(is_request_future_shutdown) break;
                            if((err = do_get_updates()) == OK) {
                                is_error = false;
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(REPEAT_DELAY));
                        }
                        is_update_chats_id_store = false;
                        if(err == OK) continue;
                        is_error = true;
                    }

                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    std::vector<OutputMessage> output_messages;
                    {
                        std::lock_guard<std::mutex> lock(array_sent_messages_mutex);
                        if(array_sent_messages.size() == 0) continue;
                        output_messages = array_sent_messages;
                        array_sent_messages.clear();
                    }

                    std::this_thread::yield();

                    for(size_t i = 0; i < output_messages.size(); ++i) {
                        int err = OK;
                        const size_t attempts = 10;
                        for(size_t n = 0; n < attempts; ++n) {
                            if(is_request_future_shutdown) break;
                            if((err = do_send_message(output_messages[i].chat_id, output_messages[i].message)) == OK) {
                                is_error = false;
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(REPEAT_DELAY));
                        }
                        if(err == OK) continue;
                        is_error = true;
                        break;
                    }
                    std::this_thread::yield();
                }
            });
        };

        ~TelegramApi() {
            if(!is_token) return;
            is_request_future_shutdown = true;
            if(messages_future.valid()) {
                try {
                    messages_future.wait();
                    messages_future.get();
                }
                catch(const std::exception &e) {
                    std::cerr << "open_bo_api::~TelegramApi() error, what: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "open_bo_api::~TelegramApi() error" << std::endl;
                }
            }
        }


        /** \brief Обновить список чатов
         * \param is_async Флаг выполнения метода в асинхронном режиме
         * \return Вернет true в случае успешного завершения (не справедливо для асинхронного метода)
         */
        bool update_chats_id_store(const bool is_async = true) {
            if(!is_token) return false;
            if(!is_async) {
                int err = OK;
                const size_t attempts = 10;
                for(size_t n = 0; n < attempts; ++n) {
                    if((err = do_get_updates()) == OK) return true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(REPEAT_DELAY));
                }
                return true;
            }
            is_update_chats_id_store = true;
            return true;
        }

        /** \brief Отправить сообщение
         * \param chat Чат. Указать или уникальный номер чата (int64_t) или имя пользователя/чата в виде строки std::string
         * \param message Текс сообщения
         * \param is_async Флаг выполнения метода в асинхронном режиме
         * \return Вернет true в случае успешного завершения (не справедливо для асинхронного метода)
         */
        template<class CHAT_TYPE>
        bool send_message(const CHAT_TYPE chat, const std::string &message, const bool is_async = true) {
            if(!is_token) return false;
            if(!is_async) {
                int err = OK;
                const size_t attempts = 10;
                for(size_t n = 0; n < attempts; ++n) {
                    if((err = do_send_message(chat, message)) == OK) return true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(REPEAT_DELAY));
                }
                is_error = true;
                return false;
            }
            if(add_send_message(chat, message) != OK) return false;
            return true;
        }

        /** \brief Ждать, пока не отправятся все сообщения в очереди
         */
        void wait() {
            if(!is_token) return;
            while(!is_request_future_shutdown) {
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                {
                    std::lock_guard<std::mutex> lock(array_sent_messages_mutex);
                    if(array_sent_messages.size() != 0) continue;
                }
                break;
            }
        }

    };
};

#endif // OPEN_BO_API_TELEGRAM_HPP_INCLUDED
