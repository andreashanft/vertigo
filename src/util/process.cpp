/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "process.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif !defined(__APPLE__) || !TARGET_OS_IPHONE
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace util {

Process::~Process()
{
    kill();
}


#if defined(__APPLE__) && TARGET_OS_IPHONE

bool Process::isSupported() { return false; }
bool Process::start(const std::vector<std::string> &) { return false; }
bool Process::isRunning() { return false; }
void Process::kill() {}

#elif defined(_WIN32)

namespace {

std::wstring wide(const std::string &text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(length > 0 ? length - 1 : 0, L'\0');
    if (length > 1)
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
    return result;
}

// an argument as CommandLineToArgvW reads it back
std::wstring quoted(const std::wstring &argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos)
        return argument;
    std::wstring result = L"\"";
    int backslashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            backslashes++;
            continue;
        }
        result.append(c == L'"' ? 2*backslashes + 1 : backslashes, L'\\');
        backslashes = 0;
        result += c;
    }
    result.append(2*backslashes, L'\\');
    return result + L"\"";
}

} // namespace

bool Process::isSupported() { return true; }

bool Process::start(const std::vector<std::string> &arguments)
{
    kill();
    std::wstring commandLine;
    for (const std::string &argument : arguments)
        commandLine += (commandLine.empty() ? L"" : L" ") + quoted(wide(argument));
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info = {};
    if (!CreateProcessW(nullptr, &commandLine[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info))
        return false;
    CloseHandle(info.hThread);
    m_handle = info.hProcess;
    m_exitCode = -1;
    return true;
}

bool Process::isRunning()
{
    if (!m_handle)
        return false;
    if (WaitForSingleObject(m_handle, 0) == WAIT_TIMEOUT)
        return true;
    DWORD code = DWORD(-1);
    GetExitCodeProcess(m_handle, &code);
    m_exitCode = int(code);
    CloseHandle(m_handle);
    m_handle = nullptr;
    return false;
}

void Process::kill()
{
    if (!m_handle)
        return;
    TerminateProcess(m_handle, 1);
    WaitForSingleObject(m_handle, INFINITE);
    CloseHandle(m_handle);
    m_handle = nullptr;
    m_exitCode = -1;
}

#else

bool Process::isSupported() { return true; }

bool Process::start(const std::vector<std::string> &arguments)
{
    kill();
    if (arguments.empty())
        return false;
    std::vector<char *> argv;
    for (const std::string &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid = -1;
    const int error = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0)
        return false;
    m_pid = pid;
    m_exitCode = -1;
    return true;
}

bool Process::isRunning()
{
    if (m_pid < 0)
        return false;
    int status = 0;
    const pid_t result = waitpid(m_pid, &status, WNOHANG);
    if (result == 0)
        return true;
    m_exitCode = result == m_pid && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    m_pid = -1;
    return false;
}

void Process::kill()
{
    if (m_pid < 0)
        return;
    ::kill(m_pid, SIGTERM);
    int status = 0;
    waitpid(m_pid, &status, 0);
    m_pid = -1;
    m_exitCode = -1;
}

#endif

} // namespace util
