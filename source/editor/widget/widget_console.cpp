#include "widget_console.h"
#include "../../imgui/imgui.h"
#include <malloc.h>
#include <ctype.h>
#include <stdio.h>
#include <mutex>
#include <deque>
#include "widget_console.h"
#include "../../engine/core/cvar.h"
#include "../../engine/core/log.h"
#include <regex>

// NOTE: ImguiDemo.cpp中已经写好的AppConsole Sample
//       这里直接拿过来接入我们的Log系统和CVar系统

#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf    _snprintf
#endif
#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf   _vsnprintf
#endif

using namespace engine;

struct WidgetConsoleApp
{
    char                  InputBuf[256];

    // 我们使用一个固定大小的deque存储历史Log
    std::deque<std::string> items;
    const uint32_t max_items_count = 200;

    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter       Filter;
    bool                  AutoScroll = true;
    bool                  ScrollToBottom;
    int CurPos;


    std::vector<const char*> activeCommands;

    enum class LogType : size_t
    {
        Trace = 0,
        Info = 1,
        Warn = 2,
        Error = 3,
        Other = 4
    };
    bool logVisible[5] = {  true, // Trace
                            true, // Info
                            true, // Warn
                            true, // Error
                            true  // Other
                          };
    uint32 logTypeCount[5] = {0,0,0,0,0};

    WidgetConsoleApp()
    {
        // 在这里cache住所有的 cvar command.
        auto& int32Array = CVarSystem::get()->getInt32Array();
        auto& floatArray = CVarSystem::get()->getFloatArray();
        auto& doubleArray = CVarSystem::get()->getDoubleArray();
        auto& stringArray = CVarSystem::get()->getStringArray();

        auto parseArray = [&](auto& cVarsArray)
        {
            auto TypeBit = cVarsArray.lastCVar;
            while(TypeBit > 0)
            {
                TypeBit--;
                auto storageValue = cVarsArray.getCurrentStorage(TypeBit);
                Commands.push_back(storageValue->parameter->name);
            }
        };


        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;

        parseArray(int32Array);
        parseArray(floatArray);
        parseArray(doubleArray);
        parseArray(stringArray);

        AutoScroll = true;
        ScrollToBottom = false;
    }

    ~WidgetConsoleApp()
    {
        ClearLog();
        for (int i = 0; i < History.Size; i++)
            free(History[i]);
    }

    // Portable helpers
    static int   Stricmp(const char* s1, const char* s2)  
    {
        int d; 
        while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) 
        { 
            s1++; 
            s2++; 
        }
        return d; 
    }

    static int   Strnicmp(const char* s1, const char* s2, int n) 
    {
        int d = 0; 
        while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) 
        { 
            s1++; 
            s2++;
            n--; 
        } 
        return d; 
    }

    static char* Strdup(const char* s)  { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
    static void  Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void    ClearLog()
    {
        items.clear();
        for(auto& i : logTypeCount)
        {
            i = 0;
        }
    }

    void AddLog(std::string info,engine::ELogType)
    {
        items.push_back(info);
        if (static_cast<uint32_t>(items.size()) > max_items_count)
        {
            items.pop_front();
        }

        if(info.find("[trace]") != std::string::npos)
        {
            logTypeCount[size_t(LogType::Trace)] ++;
        }
        else if(info.find("[info]") != std::string::npos)
        {
            logTypeCount[size_t(LogType::Info)] ++;
        }
        else if(info.find("[warn]") != std::string::npos)
        {
            logTypeCount[size_t(LogType::Warn)] ++;
        }
        else if(info.find("[error]") != std::string::npos)
        {
            logTypeCount[size_t(LogType::Error)] ++;
        }
        else 
        {
            logTypeCount[size_t(LogType::Other)] ++;
        }
    }

    void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf)-1] = 0;
        va_end(args);

        AddLog(std::string(Strdup(buf)),engine::ELogType::info);
    }

    void    Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        // As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
        // So e.g. IsItemHovered() will return true when hovering the title bar.
        // Here we create a context menu only available from the title bar.
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Console"))
                *p_open = false;
            ImGui::EndPopup();
        }

        const auto button_log_type_visibility_toggle = [this](const uint32 icon, LogType index,std::string Name)
        {
            bool& visibility = logVisible[(size_t)index];
            
            ImGui::Checkbox((Name + ":").c_str(),&visibility);
            ImGui::SameLine();
            if(logTypeCount[(size_t)index] <= 99)
            {
                ImGui::Text("%d", logTypeCount[(size_t)index]);
            }
            else
            {
                ImGui::Text("99+");
            }
        };

        // Log category visibility buttons
        button_log_type_visibility_toggle(0,LogType::Trace,u8"讯息");
        ImGui::SameLine();
        button_log_type_visibility_toggle(0,LogType::Info,u8"通知");
        ImGui::SameLine();
        button_log_type_visibility_toggle(0,LogType::Warn,u8"警告");
        ImGui::SameLine();
        button_log_type_visibility_toggle(0,LogType::Error,u8"错误");
        ImGui::SameLine();
        button_log_type_visibility_toggle(0,LogType::Other,u8"其他");
        
        // Options menu
        //if (ImGui::BeginPopup("Options"))
        //{
        //    ImGui::Checkbox("Auto-scroll", &AutoScroll);
        //    ImGui::EndPopup();
        //}

        //// Options, Filter
        //if (ImGui::Button("Options"))
        //    ImGui::OpenPopup("Options");
        ImGui::Separator();
        Filter.Draw(u8"关键词过滤",180);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"清除"))           
        { 
            ClearLog(); 
        }

        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton(u8"复制");
        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable(u8"清除")) ClearLog();
            ImGui::EndPopup();
        }

        // Display every line as a separate entry so we can change their color or add custom widgets.
        // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
        // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
        // to only process visible items. The clipper will automatically measure the height of your first item and then
        // "seek" to display only items in the visible area.
        // To use the clipper we can replace your standard loop:
        //      for (int i = 0; i < Items.Size; i++)
        //   With:
        //      ImGuiListClipper clipper;
        //      clipper.Begin(Items.Size);
        //      while (clipper.Step())
        //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        // - That your items are evenly spaced (same height)
        // - That you have cheap random access to your elements (you can access them given their index,
        //   without processing all the ones before)
        // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
        // We would need random-access on the post-filtered list.
        // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
        // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
        // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
        // to improve this example code!
        // If your items are of variable height:
        // - Split them into same height items would be simpler and facilitate random-seeking into your list.
        // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
        if (copy_to_clipboard)
            ImGui::LogToClipboard();
        for (int i = 0; i < items.size(); i++)
        {
            const char* item = items[i].c_str();
            if (!Filter.PassFilter(item))
                continue;

            // Normally you would store more information in your item than just a string.
            // (e.g. make Items[] an array of structure, store color/type etc.)
            ImVec4 color;
            bool has_color = false;
            if (strstr(item, "[error]"))  
            { 
                if(!logVisible[size_t(LogType::Error)])
                    continue;
                color = ImVec4(1.0f, 0.1f, 0.1f, 1.0f); 
                has_color = true; 
            }
            else if(strstr(item, "[fatal]"))
            {
                if(!logVisible[size_t(LogType::Other)])
                    continue;

                color = ImVec4(1.0f, 0.1f, 0.1f, 1.0f); 
                has_color = true; 
            }
            else if(strstr(item,"[warn]"))
            {
                if(!logVisible[size_t(LogType::Warn)])
                    continue;

                color = ImVec4(1.0f, 1.0f, 0.1f, 1.0f); 
                has_color = true; 
            }
            else if(strstr(item,"[trace]"))
            {
                if(!logVisible[size_t(LogType::Trace)])
                    continue;

                color = ImVec4(0.1f, 1.0f, 0.1f, 1.0f); 
                has_color = true; 
            }
            else if(strstr(item,"[info]"))
            {
                if(!logVisible[size_t(LogType::Info)])
                    continue;
            }
            else if (strncmp(item, "# ", 2) == 0) 
            { 
                if(!logVisible[size_t(LogType::Other)])
                    continue;

                color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); 
                has_color = true; 
            }
            else if (strncmp(item, u8"命令帮助：", 5) == 0) 
            { 
                if(!logVisible[size_t(LogType::Other)])
                    continue;

                color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f); 
                has_color = true; 
            }
            else
            {
                if(!logVisible[size_t(LogType::Other)])
                    continue;
            }

            if (has_color)
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item);
            if (has_color)
                ImGui::PopStyleColor();
        }
        if (copy_to_clipboard)
            ImGui::LogFinish();

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        
        auto TipPos =ImGui::GetWindowPos();
        
        TipPos.x += ImGui::GetStyle().WindowPadding.x;
        const float WindowHeight = ImGui::GetWindowHeight();
        const auto ItemSize = ImGui::GetTextLineHeightWithSpacing();
        TipPos.y = TipPos.y + WindowHeight - (activeCommands.size() + 2.5f) * ItemSize;
        if (ImGui::InputText(u8" 输入命令", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
        {
            char* s = InputBuf;
            Strtrim(s);
            if(s[0])
            {
                ExecCommand(s);
                activeCommands.clear();
            }
                
            strcpy(s, "");
            reclaim_focus = true;  
        }

        if (activeCommands.size() > 0 && ImGui::IsWindowFocused())
        {
            ImGui::BeginTooltipWithPos(TipPos);
            for(auto& info : activeCommands)
            {
                ImGui::TextUnformatted(info);
            }
            ImGui::EndTooltipWithPos();
        }
        

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

        

        ImGui::End();
    }

    void ExecCommand(const char* command_line)
    {
        AddLog("# %s\n", command_line);

        // Insert into history. First find match and delete it so it can be pushed to the back.
        // This isn't trying to be smart or optimal.
        HistoryPos = -1;
        for (int i = History.Size - 1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

        std::string cmd_info = command_line;
        std::regex ws_re("\\s+"); // whitespace

        std::vector<std::string> tokens(
            std::sregex_token_iterator(cmd_info.begin(),cmd_info.end(),ws_re, -1), 
            std::sregex_token_iterator()
        );

        auto cVar = CVarSystem::get()->getCVar(tokens[0].c_str());
        if( cVar != nullptr)
        {
            if(tokens.size() == 1)
            {
                int32_t arrayIndex = cVar->arrayIndex;
                uint32_t flag = cVar->flag;
                std::string val;
                if(cVar->type==CVarType::Int32)
                {
                    val = std::to_string(CVarSystem::get()->getInt32Array().getCurrent(arrayIndex));
                }
                else if(cVar->type==CVarType::Double)
                {
                    val = std::to_string(CVarSystem::get()->getDoubleArray().getCurrent(arrayIndex));
                }
                else if(cVar->type==CVarType::Float)
                {
                    val = std::to_string(CVarSystem::get()->getFloatArray().getCurrent(arrayIndex));
                }
                else if(cVar->type==CVarType::String)
                {
                    val = CVarSystem::get()->getStringArray().getCurrent(arrayIndex);
                }

                {
					std::string outHelp = u8"命令帮助："+std::string(cVar->description);
					AddLog(outHelp.c_str());
					AddLog((u8"当前值："+tokens[0]+" "+val).c_str());
                }
            }
            else if(tokens.size()==2)
            {
                int32_t arrayIndex = cVar->arrayIndex;
                uint32_t flag = cVar->flag;

                if((flag&InitOnce)||(flag&ReadOnly))
                {
                    AddLog(u8"'%s'是一个只读的命令，不能在命令控制台修改它！\n", tokens[0].c_str());
                }
                else if(cVar->type==CVarType::Int32)
                {
                    CVarSystem::get()->getInt32Array().setCurrent((int32_t)std::stoi(tokens[1]),cVar->arrayIndex);
                }
                else if(cVar->type==CVarType::Double)
                {
                    CVarSystem::get()->getDoubleArray().setCurrent((double)std::stod(tokens[1]),cVar->arrayIndex);
                }
                else if(cVar->type==CVarType::Float)
                {
                    CVarSystem::get()->getFloatArray().setCurrent((float)std::stof(tokens[1]),cVar->arrayIndex);
                }
                else if(cVar->type==CVarType::String)
                {
                    CVarSystem::get()->getStringArray().setCurrent(tokens[1],cVar->arrayIndex);
                }
            }
            else
            {
                AddLog(u8"错误的命令参数，所有CVar的参数都只有1个，但是这次输入了'%d'个。\n", tokens.size() - 1);
            }
        }
        else
        {
            AddLog(u8"未知的命令：'%s'！\n", command_line);
        }

        // On command input, we scroll to bottom even if AutoScroll==false
        ScrollToBottom = true;
    }

    // In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        WidgetConsoleApp* console = (WidgetConsoleApp*)data->UserData;
        return console->TextEditCallback(data);
    }

    int     TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
        
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }

            // Build a list of candidates
            ImVector<const char*> candidates;
            for(int i = 0; i<Commands.Size; i++)
            {
                if(Strnicmp(Commands[i],word_start,(int)(word_end-word_start))==0)
                {
                    candidates.push_back(Commands[i]);
                }
                else
                {
                    std::string lowerCommand = Commands[i];
                    std::transform(lowerCommand.begin(), lowerCommand.end(), lowerCommand.begin(), ::tolower);

                    if(Strnicmp(lowerCommand.c_str(),word_start,(int)(word_end-word_start))==0)
                    {
                        candidates.push_back(Commands[i]);
                    }
                }
            }


            if (candidates.Size == 0)
            {
                // No match
                // AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);

            }
            else if (candidates.Size == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");


            }
            else
            {
                // Multiple matches. Complete as much as we can..
                // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }
            }

            break;
        }

        case ImGuiInputTextFlags_CallbackEdit:
        {
            CurPos = data->CursorPos;
            activeCommands.clear();
            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }

            // Build a list of candidates
            ImVector<const char*> candidates;
            for(int i = 0; i<Commands.Size; i++)
            {
                int size = (int)(word_end-word_start);

                if(size > 0 && Strnicmp(Commands[i],word_start,size)==0)
                {
                    candidates.push_back(Commands[i]);
                }
                else
                {
                    std::string lowerCommand = Commands[i];
                    std::transform(lowerCommand.begin(), lowerCommand.end(), lowerCommand.begin(), ::tolower);

                    if(size > 0 && Strnicmp(lowerCommand.c_str(),word_start,size)==0)
                    {
                        candidates.push_back(Commands[i]);
                    }
                }
            }

            if (candidates.Size >= 1)
            {
                for(int i = 0; i<candidates.Size; i++)
                {
                    activeCommands.push_back(candidates[i]);
                }
            }

            break;
        }
        case ImGuiInputTextFlags_CallbackHistory:
        {
            // Example of HISTORY
            const int prev_history_pos = HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (HistoryPos == -1)
                    HistoryPos = History.Size - 1;
                else if (HistoryPos > 0)
                    HistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (HistoryPos != -1)
                    if (++HistoryPos >= History.Size)
                        HistoryPos = -1;
            }

            // A better implementation would preserve the data on the current input line along with cursor position.
            if (prev_history_pos != HistoryPos)
            {
                const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
        }
        return 0;
    }
};

WidgetConsole::WidgetConsole(engine::Ref<engine::Engine> engine)
    : Widget(engine)
{
    app = new WidgetConsoleApp();
    m_title = u8"命令控制台";

    // 在初始化后立即注册
    id = engine::Logger::getInstance()->getLogCache()->pushCallback([&](std::string info,engine::ELogType type){
        app->AddLog(info,type);
    });
}

void WidgetConsole::onVisibleTick(size_t)
{
    app->Draw(m_title.c_str(),&m_visible);
}

WidgetConsole::~WidgetConsole()
{
    engine::Logger::getInstance()->getLogCache()->popCallback(id);

    delete app;
}