#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include "ico.h"
#include <windows.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui-SFML.h"
#include "IconsFontaudio.h"
#include "IconsForkAwesome.h"
#include "inlcude/va-editor-256x256.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/CircleShape.hpp>

#include "open-bo-api-virtual-account.hpp"
#include "nlohmann/json.hpp"

#define PROGRAM_VERSION "1.2 beta"
#define PROGRAM_DATE "19.05.2020"
#define NDEBUG

static void HelpMarker(const char* desc);
const std::string ini_file_name("va_editor.json");

int main() {
    #ifdef NDEBUG
    HWND hwnd = GetConsoleWindow();
    ShowWindow (hwnd,SW_HIDE);
    #endif

    /* загрузим настройки */
    std::string ini_db_file_name;
    {
        using json = nlohmann::json;
        std::ifstream file_json(ini_file_name);
        if(file_json) {
            try {
                json j;
                file_json >> j;
                ini_db_file_name = j["data"]["db_file_name"];
            }
            catch(...) {}
            file_json.close();
        }
    }

    /* инициализируем окно */
    const uint32_t window_width = 640;
    const uint32_t window_height = 480;
    const uint32_t window_indent = 32;
    const char window_name[] = "Virtual Account Editor "PROGRAM_VERSION;
    sf::RenderWindow window(sf::VideoMode(window_width, window_height), window_name, sf::Style::None);
    window.setFramerateLimit(60);
    window.setIcon(va_editor_256x256_icon.width,  va_editor_256x256_icon.height,  va_editor_256x256_icon.pixel_data);

    ImGui::SFML::Init(window, false);

    /* настраиваем стиль */
    ImGui::StyleColorsDark();

    /* настраиваем язык */
    ImFontConfig font_config;
    font_config.OversampleH = 2; //or 2 is the same
    font_config.OversampleV = 2;
    font_config.PixelSnapH = 2;
    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x044F, // Cyrillic
        0,
    };

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 14.0f, NULL, ranges);

    /* добавляем иконки */
    /*
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.OversampleH = 1;
        icons_config.OversampleV = 1;
        icons_config.PixelSnapH = 1;
        icons_config.GlyphOffset.y = 3;
        icons_config.GlyphOffset.x = -2;
        io.Fonts->AddFontFromFileTTF("fonts/"FONT_ICON_FILE_NAME_FAD, 16.0f, &icons_config, icons_ranges ); //fontaudio.ttf
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.OversampleH = 1;
        icons_config.OversampleV = 1;
        icons_config.PixelSnapH = 1;
        icons_config.GlyphOffset.y = 1;
        icons_config.GlyphOffset.x = 0;
        io.Fonts->AddFontFromFileTTF("fonts/"FONT_ICON_FILE_NAME_FK, 16.0f, &icons_config, icons_ranges );
    }
    */
    ImGui::SFML::UpdateFontTexture();

    /* создаем шапку */
    sf::RectangleShape head_rectangle(sf::Vector2f(640.0f, 32.0f));
    head_rectangle.setFillColor(sf::Color(50, 50, 50));

	/* класс для работы с базой данных виртуальных аккаунтов
	 * и прочие вспомогательные переменные
	 */
	std::shared_ptr<open_bo_api::VirtualAccounts> va_ptr;
    std::map<uint64_t, open_bo_api::VirtualAccount> va_list;
    open_bo_api::VirtualAccount va_edit;
    bool is_va_select = false;


    sf::Vector2i grabbed_offset;
    sf::Clock delta_clock;
    while (window.isOpen()) {

        /* обрабатываем события */
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                window.close();
            } else
            // перетаскивание окна https://en.sfml-dev.org/forums/index.php?topic=14391.0
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    grabbed_offset = window.getPosition() - sf::Mouse::getPosition();
                }
            }
        }

        ImGui::SFML::Update(window, delta_clock.restart());

//////// реализуем меню (начало) ///////////////////////////////////////////////
        const uint32_t main_menu_width = 640;
        const uint32_t main_menu_heigh = 480 - head_rectangle.getSize().y;

        ImGui::SetNextWindowPos(ImVec2(window_width/2 - main_menu_width/2, window_height/2 - main_menu_heigh/2 + head_rectangle.getSize().y/2), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(main_menu_width, main_menu_heigh), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.5);

        ImGui::Begin("##MainMenu", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        /* создаем строку для указания файла */
        static bool db_file_name_error = false;
        static std::array<char, _MAX_PATH> db_file_name = {0};
        if(ini_db_file_name.size() != 0) {
            size_t ini_db_file_name_copy_size = std::min(ini_db_file_name.size(), db_file_name.size() - 2);
            ini_db_file_name.copy(db_file_name.data(), ini_db_file_name_copy_size);
            ini_db_file_name.clear();
        }
        ImGui::InputText("Database file name", db_file_name.data(), db_file_name.size());
        ImGui::SameLine();
        if(ImGui::Button("Open")) {
            std::string str(db_file_name.data());
            if(str.length() == 0) {
                db_file_name_error = true;
            } else {
                va_ptr = std::make_shared<open_bo_api::VirtualAccounts>(str);
                if(va_ptr) va_list = va_ptr->get_virtual_accounts();
                db_file_name_error = false;
                /* сохраним настройки */
                {
                    using json = nlohmann::json;
                    json j;
                    j["data"]["db_file_name"] = str;
                    std::ofstream file_json(ini_file_name);
                    file_json << std::setw(4) << j << std::endl;
                    file_json.close();
                }
            }
        }

        /* выход из программы */
        ImGui::SameLine();
        if(ImGui::Button("Exit##Exit1")) {
            window.close();
        }

        /* вывод сообщений об ошибка */
        if(db_file_name_error) {
            ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), "Error! File name is empty");
        }
        if(va_ptr && va_ptr->check_errors()) {
            ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), "Error creating or opening a database");
        }
        ImGui::Separator();

        /* создаем две колонки минимум */
        ImGui::Columns(2, NULL, false);

        if(!va_ptr || (va_ptr && va_ptr->check_errors())) {
            /* включим режим блокировки меню
             * ВНИМАНИЕ! В КОНЦЕ МЕНЮ ЕСТЬ ВЫЗОВ POP ФУНКЦИЙ!
             */
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        /* выводим в первой колонке список аккаунтов */
        ImGui::Text("List of virtual accounts");
#if(1)
        ImVec2 va_button_size(240, 20);
        uint64_t va_list_index = 0;
        int64_t va_list_delete_id = -1;

        ImGui::BeginChild("List of virtual accounts##ListVA", ImVec2(300, 0), true);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f,0.5f));
        for(auto &it : va_list) {
            ImGui::PushID(va_list_index);
            std::string button_str("Name: ");
            button_str += it.second.holder_name;
            button_str += " ID: ";
            button_str += std::to_string(it.second.va_id);
            if(ImGui::Button(button_str.c_str(), va_button_size)) {
                va_edit = it.second;
                is_va_select = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("X##Delete")) {
                va_list_delete_id = it.second.va_id;
            }
            ImGui::PopID();
            ++va_list_index;
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        if(va_list_delete_id >= 0) {
            if(va_ptr) {
                va_ptr->delete_virtual_account(va_list_delete_id);
                va_list = va_ptr->get_virtual_accounts();
            }
        }
#else
        ImGui::Columns(IM_ARRAYSIZE(headers), "TableTextureColumns", true);
#endif

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        /* создаем новый виртуальный аккаунт */
        if(ImGui::Button("Create a new virtual account")) {
            va_edit = open_bo_api::VirtualAccount();
            va_edit.start_timestamp = xtime::get_timestamp();
            va_edit.timestamp = va_edit.start_timestamp;
            va_edit.demo = true;
            va_edit.enabled = false;
            if(va_ptr) {
                va_ptr->add_virtual_account(va_edit);
                va_list = va_ptr->get_virtual_accounts();
                is_va_select = true;
            }
        }

        if(!va_ptr || (va_ptr && va_ptr->check_errors())) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        ImGui::NextColumn();

        /* во второй колонке выводим список атрибутов виртуального аккаунта */

        if(!is_va_select) {
            /* включим режим блокировки меню
             * ВНИМАНИЕ! В КОНЦЕ МЕНЮ ЕСТЬ ВЫЗОВ POP ФУНКЦИЙ!
             */
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        /* реализуем сохранение данных */
        if(ImGui::Button("Save##Save1") && is_va_select) {
            va_edit.timestamp = xtime::get_timestamp();
            if(va_ptr) {
                va_ptr->update_virtual_account(va_edit);
                va_list = va_ptr->get_virtual_accounts();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Text("Virtual Account ID: %d", va_edit.va_id); // ID аккаунта. Менять нельзя
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        /* Будьте осторожны при использовании в std::stringкачестве входного буфера!
         * Конечно, вы всегда можете вызвать std::string::resize
         * и выделить достаточно места, но не используйте его в коде обработки строк,
         * потому что строка может содержать нули или мусор,
         * и это может привести к неприятным ошибкам.
         * Размер строки может не соответствовать ожидаемому.
         * Более того, когда ImGui изменяет std::string
         * внутренний массив символов, он не меняет свой размер,
         * что может стать еще одним источником серьезных ошибок.
         */
        std::array<char, 256> va_holder_name = {0};
        size_t va_holder_name_copy_size = std::min(va_edit.holder_name.size(), va_holder_name.size() - 2);
        va_edit.holder_name.copy(va_holder_name.data(), va_holder_name_copy_size);
        ImGui::InputText("Holder name", va_holder_name.data(), va_holder_name.size()); // Имя владельца аккаунта
        va_edit.holder_name = va_holder_name.data();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::InputDouble("Starting balance", &va_edit.start_balance, 0.01, 1.0, "%.2f");
        if(va_edit.start_balance < 0.0d) va_edit.start_balance = 0.0d;

        xtime::DateTime date_time(va_edit.start_timestamp);
        ImGui::Text("Account Creation Date: %02d.%02d.%d", date_time.day, date_time.month, date_time.year);
        ImGui::InputDouble("Balance", &va_edit.balance, 0.01, 1.0, "%.2f");

        xtime::DateTime update_date_time(va_edit.timestamp);
        ImGui::Text("Last update date of account: %02d.%02d.%d %02d:%02d",
            update_date_time.day, update_date_time.month, update_date_time.year,
            update_date_time.hour, update_date_time.minute);

        //
        {
            ImGui::BeginChild("Daily balance change##DailyBalance", ImVec2(300, 0), true);
            ImGui::Columns(4, NULL, false);
            ImGui::Text("Date");
            ImGui::NextColumn();
            ImGui::Text("Balance");
            ImGui::NextColumn();
            ImGui::Text("Gain");
            ImGui::NextColumn();
            ImGui::NextColumn();
            double last_balance = va_edit.start_balance;
            int va_daily_balance_index = 0;
            xtime::timestamp_t va_daily_balance_key_delete = 0;
            for(auto &it : va_edit.date_balance) {
                ImGui::PushID(va_daily_balance_index);
                xtime::DateTime date_time(it.first);
                ImGui::Text("%02d.%02d.%d",
                    date_time.day, date_time.month, date_time.year);
                ImGui::NextColumn();
                //ImGui::InputDouble("##DateBalance", &it.second, 0.01, 1.0, "%.2f", ImGuiInputTextFlags_NoMarkEdited);
                ImGui::Text("%.2f", it.second);
                ImGui::NextColumn();
                const double gain = last_balance == 0.0 ? 1.0 : it.second / last_balance;
                last_balance = it.second;
                ImGui::Text("%.2f", gain);
                ImGui::NextColumn();
                if(ImGui::Button("X##DeleteDateBalance")) {
                    va_daily_balance_key_delete = it.first;
                }
                ImGui::NextColumn();
                ImGui::PopID();
                ++va_daily_balance_index;
            }
            ImGui::Columns(1);
            ImGui::EndChild();

            if(va_daily_balance_key_delete != 0) {
                va_edit.date_balance.erase(va_daily_balance_key_delete);
            }

            /* добавляем дату изменения депозита */
            static bool va_daily_balance_error = false;
            static std::array<char, 256> va_daily_balance_date = {0};
            static double va_daily_balance_value = {0};
            ImGui::InputText("Daily balance (Date)", va_daily_balance_date.data(), va_daily_balance_date.size()); // Имя владельца аккаунта
            ImGui::InputDouble("Daily balance (Value)", &va_daily_balance_value, 0.01, 1.0, "%.2f");

            if(ImGui::Button("Add daily balance")) {
                std::string str(va_daily_balance_date.data());
                xtime::timestamp_t daily_balance_date = 0;
                if (str.length() == 0 ||
                    !xtime::convert_str_to_timestamp(str, daily_balance_date)) {
                    va_daily_balance_error = true;
                } else {
                    va_edit.date_balance[daily_balance_date] = va_daily_balance_value;
                    va_daily_balance_date.fill('\0');
                    va_daily_balance_value = 0;
                    va_daily_balance_error = false;
                }
            }
            if(va_daily_balance_error) {
                ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), "Error! Invalid balance date");
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        /* отображаем винрейт */
        int wins = va_edit.wins, losses = va_edit.losses;
        ImGui::InputInt("Wins: ", &wins, 1, 10);
        ImGui::InputInt("Losses: ", &losses, 1, 10);
        if(wins < 0) wins = 0;
        if(losses < 0) losses = 0;
        va_edit.wins = wins;
        va_edit.losses = losses;
        ImGui::Text("Winrate: %.5f", va_edit.get_winrate());

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        if(ImGui::CollapsingHeader("Risk management")) {
            /* настройка ширины */
            ImGui::PushItemWidth(160);

            ImGui::InputDouble("Absolute Take Profit", &va_edit.absolute_take_profit, 0.01, 1.0, "%.2f");
            if(va_edit.absolute_take_profit < 0.0d) va_edit.absolute_take_profit = 0.0d;
            ImGui::InputDouble("Absolute stop loss", &va_edit.absolute_stop_loss, 0.01, 1.0, "%.2f");
            if(va_edit.absolute_stop_loss < 0.0d) va_edit.absolute_stop_loss = 0.0;

            ImGui::InputDouble("Kelly Attenuation Factor", &va_edit.kelly_attenuation_multiplier, 0.01, 1.0, "%.5f");
            if(va_edit.kelly_attenuation_multiplier < 0.001d) va_edit.kelly_attenuation_multiplier = 0.001d;
            ImGui::InputDouble("Kelly Attenuation Limit", &va_edit.kelly_attenuation_limiter, 0.01, 1.0, "%.5f");
            if(va_edit.kelly_attenuation_limiter < 0.001d) va_edit.kelly_attenuation_limiter = 0.001d;
            if(va_edit.kelly_attenuation_limiter > 10.0d) va_edit.kelly_attenuation_limiter = 10.0d;
            ImGui::InputDouble("Payout limit", &va_edit.payout_limiter, 0.01, 1.0, "%.3f");
            if(va_edit.payout_limiter > 1.0d) va_edit.payout_limiter = 1.0d;
            if(va_edit.payout_limiter < 0.0d) va_edit.payout_limiter = 0.0d;
            ImGui::InputDouble("Winrate limit", &va_edit.winrate_limiter, 0.01, 1.0, "%.3f");
            if(va_edit.winrate_limiter > 1.0d) va_edit.winrate_limiter = 1.0d;
            if(va_edit.winrate_limiter < 0.0d) va_edit.winrate_limiter = 0.0d;
            ImGui::PopItemWidth();
        }

        if(ImGui::CollapsingHeader("List of allowed strategies")) {

            ImGui::BeginChild("List of allowed strategies##ListStrategies", ImVec2(280, 140), true);
            std::vector<std::string> list_strategies(va_edit.list_strategies.begin(), va_edit.list_strategies.end());
            int64_t list_strategies_delete_index = -1;
            for(size_t n = 0; n < list_strategies.size(); ++n) {
                ImGui::PushID(n);
                std::array<char, 256> va_strategy_name = {0};
                size_t va_strategy_name_copy_size = std::min(list_strategies[n].size(),va_strategy_name.size() - 2);
                list_strategies[n].copy(va_strategy_name.data(), va_strategy_name_copy_size);
                ImGui::InputText("##strategy_name", va_strategy_name.data(), va_strategy_name.size()); // Имя владельца аккаунта
                list_strategies[n] = va_strategy_name.data();
                ImGui::SameLine();
                if(ImGui::Button("Delete##DeleteStrategy")) {
                    list_strategies_delete_index = n;
                }
                ImGui::PopID();
                ++va_list_index;
            }
            if(list_strategies_delete_index >= 0) {
                list_strategies.erase(list_strategies.begin() + list_strategies_delete_index);
            }
            ImGui::EndChild();

            static bool strategy_name_error = false;
            static std::array<char, 256> va_strategy_name = {0};
            ImGui::InputText("Strategy name", va_strategy_name.data(), va_strategy_name.size()); // Имя владельца аккаунта
            if(ImGui::Button("Add strategy")) {
                std::string str(va_strategy_name.data());
                if(str.length() == 0) {
                    strategy_name_error = true;
                } else {
                    list_strategies.push_back(str);
                    va_strategy_name.fill('\0');
                    strategy_name_error = false;
                }
            }
            va_edit.list_strategies = std::set<std::string>(list_strategies.begin(), list_strategies.end());
            if(strategy_name_error) {
                ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), "Error! Strategy name is empty");
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        std::array<char, 1024*64> va_note = {0};
        size_t va_note_copy_size = std::min(va_edit.note.size(), va_note.size() - 2);
        va_edit.note.copy(va_note.data(), va_note_copy_size);
        ImGui::InputTextMultiline("Note", va_note.data(), va_note.size());
        va_edit.note = va_note.data();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        /* перечисляем изменения депозита */

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::Checkbox("Demo", &va_edit.demo);
        ImGui::Checkbox("Enabled", &va_edit.enabled);

        /* реализуем сохранение данных */
        if(ImGui::Button("Save##Save2") && is_va_select) {
            va_edit.timestamp = xtime::get_timestamp();
            if(va_ptr) {
                va_ptr->update_virtual_account(va_edit);
                va_list = va_ptr->get_virtual_accounts();
            }
        }

        if(!is_va_select) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        ImGui::Columns(1);
        ImGui::Separator();

        /* выход из программы */
        if(ImGui::Button("Exit##Exit2")) {
            window.close();
        }
        ImGui::End();
//////// реализуем меню (конец) ////////////////////////////////////////////////

        window.clear();

        /* реализуем перемещение окна в экране */
        bool grabbed_window = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        if(grabbed_window && (sf::Mouse::getPosition(window).y < window_indent)) {
                window.setPosition(sf::Mouse::getPosition() + grabbed_offset);
        }

        /* отрисовываем текстуры */
        //window.draw(sprite_main);
        //window.draw(sprite_main_header);
        window.draw(head_rectangle);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}

static void HelpMarker(const char* desc) {
    ImGui::TextDisabled(ICON_FK_QUESTION_CIRCLE_O);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
