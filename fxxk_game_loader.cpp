// fxxk_game_loader.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

// 沙比GameLoader.exe每次在游戏退出的时候都不会自动关掉, 导致每次更新都报错

#include <windows.h>
#include <tlhelp32.h>
#include <Shlobj.h>

#include "gsl/gsl"

#include <atlbase.h>

#include <iostream>
#include <map>
#include <vector>

// 以下部分函数从其他地方复制过来的，懒得改了，反正功能可以实现就行了

struct Info {
    uint32_t pid;
    uint32_t parent_pid;
    std::wstring file;
};

bool kill(HANDLE h) noexcept {
    if (!::TerminateProcess(h, 0)) {
        int ec = GetLastError();
        std::cerr << "TerminateProcess(), #" << ec << std::endl;
        return false;
    }
    // 等待退出
    auto result = WaitForSingleObject(h, 5000);
    return result == WAIT_OBJECT_0;
}


bool kill(std::uint32_t process_id) noexcept {
    CHandle process;
    if (HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, process_id); nullptr != h) {
        process.Attach(h);
    }
    else {
        int ec = GetLastError();
        std::cerr << "OpenProcess(), #" << ec << std::endl;
        return false;
    }

    return kill(process);
}

std::map<uint32_t, Info> getProcessInfoMap() noexcept {
    std::map<uint32_t, Info> process_info_map;
    HANDLE h_snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h_snapshot == INVALID_HANDLE_VALUE) {
        return process_info_map;
    }
    gsl::final_action close([&]() { CloseHandle(h_snapshot); });

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (!::Process32First(h_snapshot, &pe)) {
        return process_info_map;
    }

    do {
        auto& info = process_info_map[pe.th32ProcessID];
        info.file = pe.szExeFile;
        info.pid = pe.th32ProcessID;
        info.parent_pid = pe.th32ParentProcessID;
    } while (::Process32Next(h_snapshot, &pe));

    return process_info_map;
}

std::vector<CHandle> openProcess(std::wstring_view name) noexcept {
    std::vector<CHandle> handlers;
    auto process_info_map = getProcessInfoMap();
    for (auto& [pid, info] : process_info_map) {
        if (info.file != name) {
            continue;
        }
        auto handle = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, info.pid);
        if (!handle) {
            int ec = GetLastError();
            std::cerr << "OpenProcess(), #" << ec << std::endl;
            continue;
        }
        handlers.emplace_back(handle);
    }
    return handlers;
}

int main()
{
    if (!IsUserAnAdmin()) {
        std::cerr << "请以管理员权限运行" << std::endl;
        return 1;
    }

    std::cout << "检测中" << std::endl;

    while (true)
    {
        auto wegames = openProcess(L"wegame.exe");
        if (!wegames.empty()) {
            // 等待wegame退出
            std::cout << "已找到wegame进程, 等待退出" << std::endl;
            auto result = WaitForSingleObject(wegames[0], INFINITE);
            if (result != WAIT_OBJECT_0) {
                int ec = GetLastError();
                std::cerr << "WaitForSingleObject(), #" << ec << std::endl;
                break;
            }
        }


        // 到GameLoader进程, 可能是多个
        auto game_loaders = openProcess(L"GameLoader.exe");
        for (auto& game_loader : game_loaders) {
            std::cout << "已找到GameLoader进程, 准备关闭" << std::endl;
            if (!kill(game_loader)) {
                continue;
            }
            std::cout << "已关闭" << std::endl;
        }
        Sleep(3000);
    }

    getchar();
    return 0;
}
