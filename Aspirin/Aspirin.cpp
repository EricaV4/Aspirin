#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <windows.h>
#include <map>
#include <regex>
#include <execution>
#include <mutex>
#ifdef _WIN32
#include <lmcons.h>
#else
#endif

// 全局变量声明
std::string computerName = "FoxMomo";  // 初始化默认值

// 修改获取函数
void initComputerName() {
#ifdef _WIN32
    char buffer[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        computerName = buffer;
    }
#else
    char buffer[HOST_NAME_MAX] = { 0 };
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        computerName = buffer;
    }
#endif
}



namespace fs = std::filesystem;
using namespace std;

// 全局变量
vector<vector<fs::path>> search_cache;

// 前置声明
struct CommandHandler;
void register_commands(map<string, CommandHandler*>& handlers);
vector<string> split(const string& s, char delimiter);
void reset_console();
void print_helpPlus();
void print_help();
void print_momo();
void print_main();

// 命令处理器基类
struct CommandHandler {
    virtual void execute(const vector<string>& parts) = 0;
    virtual ~CommandHandler() = default;
};

// 文件夹搜索参数结构体
struct FolderSearchParams {
    string folder_name;
    char drive = '\0';
    char mode = ' ';
    bool check_file = false;
    string check_name;
    bool check_none = false;
};


int main() {

    initComputerName();  // 初始化全局变量

    SetConsoleTitleA("Aspirin_2.2");
    print_main();

    map<string, CommandHandler*> command_handlers;
    register_commands(command_handlers);

    string input;
    while (true) {
        cout << "\n> ";
        getline(cin, input);

        if (input == "exit" || input == "Exit") break;
        if (input == "reset" || input == "Reset") {
            reset_console();
            continue;
        }
        if (input == "help" || input == "Help" || input == "HELP") {
            print_help();
            continue;
        }
        if (input == "helpPlus" || input == "HelpPlus") {
            print_helpPlus();
            continue;
        }
        if (input == "momo" || input == "Momo" || input == "moMo") {
            print_momo();
            continue;
        }

        istringstream iss(input);
        string command;
        while (getline(iss, command, ';')) {
            if (command.empty()) continue;

            vector<string> parts = split(command, ' ');
            if (parts.empty()) continue;

            bool handled = false;
            for (const auto& [prefix, handler] : command_handlers) {
                if (parts[0].find(prefix) == 0) {
                    try {
                        handler->execute(parts);
                        handled = true;
                        break;
                    }
                    catch (const exception& e) {
                        cerr << "错误: " << e.what() << endl;
                    }
                }
            }

            if (!handled) {
                cerr << "未知命令: " << parts[0] << endl;
            }
        }
    }

    for (auto& [_, handler] : command_handlers) {
        delete handler;
    }
    return 0;
}
void print_momo() {
    std::cout << " 糟糕! 又被你发现惹~ \n"
        << " by Akarin \n"
        << " Hello " << computerName << "\n";  // 直接使用全局变量
}
// 增强型文件夹搜索处理器
struct FolderSearchHandler : CommandHandler {
    void execute(const vector<string>& parts) override {
        FolderSearchParams params = parse_command(parts[0]);
        vector<fs::path> results = search_folder(params);
        search_cache.push_back(results);
        cout << "找到" << results.size() << "个结果，存入缓存 0x" << search_cache.size() << endl;
    }

private:
    FolderSearchParams parse_command(const string& cmd) {
        FolderSearchParams params;
        regex pattern(R"(i_([^_]+)_([A-Za-z])(?:_([ap]+))?(?:\+([=]?)([^ ]+)|<none>)?)");
        smatch matches;

        if (regex_match(cmd, matches, pattern)) {
            params.folder_name = matches[1];
            params.drive = toupper(matches[2].str()[0]);

            if (matches[3].matched) {
                string mode_str = matches[3];
                params.mode = mode_str.back();
                if (mode_str.find('a') != string::npos) params.mode |= 0x01;
            }

            if (matches[5].matched) {
                params.check_file = (matches[4] == "=");
                params.check_name = matches[5];
                if (params.check_name == "<none>") {
                    params.check_none = true;
                    params.check_name.clear();
                }
            }
        }
        else {
            throw invalid_argument("无效的命令格式");
        }
        return params;
    }

    vector<fs::path> search_folder(const FolderSearchParams& params) {
        vector<fs::path> results;
        const string root = string(1, params.drive) + ":\\";
        vector<fs::path> all_dirs;

        // 第一阶段：收集根目录直接子目录
        try {
            for (auto& entry : fs::directory_iterator(root)) {
                if (entry.is_directory()) {
                    all_dirs.push_back(entry.path().lexically_normal());
                }
            }
        }
        catch (...) {}

        // 第二阶段：并行递归搜索
        mutex mtx;
        for_each(
            execution::par,
            all_dirs.begin(),
            all_dirs.end(),
            [&](const fs::path& dir) {
                try {
                    for (auto& entry : fs::recursive_directory_iterator(
                        dir,
                        fs::directory_options::skip_permission_denied
                    )) {
                        try {
                            if (entry.is_directory() &&
                                entry.path().filename() == params.folder_name) {
                                if (validate_path(entry.path(), params)) {
                                    lock_guard<mutex> lock(mtx);
                                    results.push_back(entry.path().lexically_normal());
                                }
                            }
                        }
                        catch (fs::filesystem_error& e) {
                        }
                    }
                }
                catch (...) {}
            }
        );

        // 结果排序处理
        if (params.mode & 0x01) {
            sort(results.rbegin(), results.rend(), [](const auto& a, const auto& b) {
                return a.string().length() < b.string().length();
                });
        }

        if (params.mode == 'p') {
            vector<fs::path> filtered;
            for (const auto& p : results) {
                if (p.parent_path().filename() == params.folder_name) {
                    filtered.push_back(p);
                }
            }
            return filtered;
        }
        return results;
    }

    bool validate_path(const fs::path& path, const FolderSearchParams& params) {
        if (params.check_name.empty()) return true;

        bool has_target = false;
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                bool name_match = false;
                try {
                    name_match = params.check_file ?
                        (entry.is_regular_file() &&
                            entry.path().filename() == params.check_name) :
                        (entry.is_directory() &&
                            entry.path().filename() == params.check_name);
                }
                catch (...) {
                    continue;
                }

                if (name_match) {
                    has_target = true;
                    break;
                }
            }
        }
        catch (fs::filesystem_error& e) {
            cerr << "验证失败: " << e.path1().string() << endl;
        }

        return params.check_none ? !has_target : has_target;
    }
};

// 文件操作处理
struct FileOperationHandler : CommandHandler {
    enum OpType { ALL, THE, FILE };
    OpType type;

    explicit FileOperationHandler(OpType t) : type(t) {}

    void execute(const vector<string>& parts) override {
        if (parts.size() < 3 || (parts[1] != "move" && parts[1] != "i_move")) {
            throw invalid_argument("无效的命令格式");
        }

        string source_str = parts[0].substr(parts[0].find('_') + 1);
        vector<fs::path> source_paths;

        if (source_str.find("0x") == 0) {
            int index = stoi(source_str.substr(2)) - 1;
            if (index < 0 || index >= search_cache.size()) {
                throw invalid_argument("无效的缓存编号: " + source_str);
            }
            source_paths = search_cache[index];
        }
        else {
            source_paths.push_back(fs::path(source_str));
        }

        string dest_str = parts[2];
        fs::path dest_path;

        if (dest_str.find("0x") == 0) {
            int index = stoi(dest_str.substr(2)) - 1;
            if (index < 0 || index >= search_cache.size()) {
                throw invalid_argument("无效的目标缓存编号: " + dest_str);
            }
            if (search_cache[index].empty()) {
                throw invalid_argument("目标缓存为空: " + dest_str);
            }
            dest_path = search_cache[index].front();
        }
        else {
            dest_path = fs::path(dest_str);
        }

        bool overwrite = (parts[1] == "i_move");

        for (const auto& source : source_paths) {
            switch (type) {
            case ALL:
                if (!fs::is_directory(source))
                    throw invalid_argument("路径不是目录: " + source.string());
                break;
            case THE:
                if (!fs::exists(source))
                    throw invalid_argument("路径不存在: " + source.string());
                break;
            case FILE:
                if (!fs::is_regular_file(source))
                    throw invalid_argument("路径不是文件: " + source.string());
                break;
            }
        }

        for (const auto& source : source_paths) {
            if (type == ALL) {
                for (const auto& entry : fs::directory_iterator(source)) {
                    move_item(entry.path(), dest_path, overwrite);
                }
            }
            else {
                move_item(source, dest_path, overwrite);
            }
        }

        cout << "操作完成: " << source_str << " -> " << dest_path << endl;
    }

private:
    void move_item(const fs::path& src, const fs::path& dest, bool overwrite) {
        fs::path target = dest / src.filename();

        // 检查目标是否存在
        if (fs::exists(target)) {
            if (overwrite) {
                if (fs::equivalent(src, target)) {
                    cerr << "错误: 不能覆盖自身 " << src << endl;
                    return;
                }
                try {
                    fs::remove_all(target);
                }
                catch (const fs::filesystem_error& e) {
                    cerr << "无法删除目标: " << e.what() << endl;
                    return;
                }
            }
            else {
                cerr << "警告: 目标已存在，跳过 " << target << endl;
                return;
            }
        }

        try {
            fs::rename(src, target);
        }
        catch (const fs::filesystem_error& e) {
            // 跨设备移动，使用复制+删除
            try {
                if (fs::is_directory(src)) {
                    fs::copy(src, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                }
                else {
                    fs::copy(src, target, fs::copy_options::overwrite_existing);
                }
                fs::remove_all(src);
            }
            catch (const fs::filesystem_error& copy_err) {
                cerr << "移动失败: " << copy_err.what() << endl;
            }
        }
    }
};

// 缓存清除处理器
struct ClearHandler : CommandHandler {
    void execute(const vector<string>& parts) override {
        string clear_cmd = parts[0].substr(6);

        if (clear_cmd == "all") {
            search_cache.clear();
            cout << "已清除所有缓存" << endl;
        }
        else if (clear_cmd == "step") {
            if (!search_cache.empty()) {
                search_cache.pop_back();
                cout << "已清除最新缓存 0x" << search_cache.size() + 1 << endl;
            }
            else {
                throw invalid_argument("缓存已空");
            }
        }
        else if (clear_cmd.find("0x") == 0) {
            int index = stoi(clear_cmd.substr(2)) - 1;
            if (index >= 0 && index < search_cache.size()) {
                search_cache.erase(search_cache.begin() + index);
                cout << "已清除缓存 0x" << index + 1 << endl;
            }
            else {
                throw invalid_argument("无效的缓存编号: " + clear_cmd);
            }
        }
        else {
            throw invalid_argument("未知的清除指令: " + clear_cmd);
        }
    }
};

// 注册命令处理器
void register_commands(map<string, CommandHandler*>& handlers) {
    handlers["i_"] = new FolderSearchHandler();
    handlers["all_"] = new FileOperationHandler(FileOperationHandler::ALL);
    handlers["the_"] = new FileOperationHandler(FileOperationHandler::THE);
    handlers["fil_"] = new FileOperationHandler(FileOperationHandler::FILE);
    handlers["clear_"] = new ClearHandler();
}

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

void reset_console() {
#ifdef _WIN32
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD topLeft = { 0, 0 };

    if (!GetConsoleScreenBufferInfo(hStdOut, &csbi)) return;

    DWORD length = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;
    FillConsoleOutputCharacter(hStdOut, ' ', length, topLeft, &written);
    FillConsoleOutputAttribute(hStdOut, csbi.wAttributes, length, topLeft, &written);
    SetConsoleCursorPosition(hStdOut, topLeft);

    std::cout.flush();
    SetConsoleTextAttribute(hStdOut, csbi.wAttributes);
    print_main();
#else
    std::cout << "\033[2J\033[1;1H";
    std::cout << "\033[0m";
#endif
}
void print_main() {
    std::cout << "     _                    _          _         " << std::endl;
    std::cout << "    / \\     ___   _ __   (_)  _ __  (_)  _ __  " << std::endl;
    std::cout << "   / _ \\   / __| | '_ \\  | | | '__| | | | '_ \\ " << std::endl;
    std::cout << "  / ___ \\  \\__ \\ | |_) | | | | |    | | | | | |" << std::endl;
    std::cout << " /_/   \\_\\ |___/ | .__/  |_| |_|    |_| |_| |_|" << std::endl;
    std::cout << "                 |_|                           \n" << std::endl;
    cout << " 欢迎使用\"Aspirin\"IQBoost工具!" << endl;
    cout << " 输入help或helpPlus查看帮助 - 或者输入" << endl;
}

void print_help() {
    cout << "╔══════════════════ Aspirin 基础帮助 ═════════════════╗\n"
        << "║ 命令格式        说明                                ║\n"
        << "╠════════════════════╦════════════════════════════════╣\n"
        << "║ i_<名称>_<盘>[模式]║ 智能搜索 (例:i_Data_D_p)       ║\n"
        << "║ all/the/fil ...    ║ 操作目录/文件 (支持0x缓存)     ║\n"
        << "║ clear_<类型>       ║ 缓存管理 (all/step/0xN)        ║\n"
        << "║ move               ║ 移动文件(i_move为移动并覆盖)   ║\n"
        << "║ reset              ║ 重置控制台                     ║\n"
        << "║ help[Plus]         ║ 获取不同级别帮助               ║\n"
        << "╚════════════════════╩════════════════════════════════╝\n"
        << "常用示例：\n"
        << "  i_Project_C        → 深度搜索C盘Project文件夹\n"
        << "  fil_0x2 move 0x3   → 移动路径缓存2文件到路径缓存3的文件夹\n"
        << "  clear_step         → 删除最近的路径缓存\n";
}

void print_helpPlus() {
    cout << "\n╔══════════════════ Aspirin 高级手册 ═════════════════╗\n"
        << "║                     搜索命令详解                    ║\n"
        << "╠═══════════════════╦═════════════════════════════════╣\n"
        << "║ 模式参数          ║ 功能说明                        ║\n"
        << "╠═══════════════════╬═════════════════════════════════╣\n"
        << "║ _p                ║ 嵌套模式 (Parent)               ║\n"
        << "║ _a                ║ 深度优先 (Ascending depth)      ║\n"
        << "║ _ap/_pa           ║ 组合模式                        ║\n"
        << "╠═══════════════════╬═════════════════════════════════╣\n"
        << "║ 条件过滤          ║ 示例说明                        ║\n"
        << "╠═══════════════════╬═════════════════════════════════╣\n"
        << "║ +Documents        ║ 需含Documents子目录             ║\n"
        << "║ +=config.ini      ║ 需含指定文件                    ║\n"
        << "║ +<none>           ║ 无任何子项                      ║\n"
        << "╚═══════════════════╩═════════════════════════════════╝\n";
}
