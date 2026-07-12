#include "FileSystem.h"
#include <fstream>
#include <cstdlib>

// Include system headers for Unix-like systems (유닉스 계열 시스템용 시스템 헤더 포함)
#if defined(__linux__) || defined(__apple__)
#include <unistd.h>
#include <pwd.h>
#endif

namespace fs = std::filesystem;

namespace FS {

    // Get the standard application data path for each OS (각 OS별 표준 앱 데이터 경로 가져오기)
    static fs::path GetAppDataPath() {
#if defined(_WIN32)
        // Windows AppData path (윈도우 AppData 경로)
        char* appData = std::getenv("LOCALAPPDATA");
        if (appData) {
            return fs::path(appData);
        }
        return fs::path(std::getenv("USERPROFILE")) / "AppData" / "Local";
#elif defined(__apple__)
        // macOS Application Support path (맥 OS Application Support 경로)
        char* home = std::getenv("HOME");
        if (!home) {
            home = getpwuid(getuid())->pw_dir;
        }
        return fs::path(home) / "Library" / "Application Support";
#else
        // Linux config path (리눅스 config 경로)
        char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
        if (xdgConfig && xdgConfig[0] != '\0') {
            return fs::path(xdgConfig);
        }
        char* home = std::getenv("HOME");
        if (!home) {
            home = getpwuid(getuid())->pw_dir;
        }
        return fs::path(home) / ".config";
#endif
    }

    fs::path GetEngineRootPath() {
        // Enforce the root directory exclusively within Geekpiz/Game Engine (Geekpiz/Game Engine 내부로만 루트 디렉토리 제한)
        static const fs::path rootPath = GetAppDataPath() / "Geekpiz" / "Game Engine";
        return rootPath;
    }

    std::string ReadFile(const std::string& relativePath) {
        // Combine the root path with the requested relative path (루트 경로와 요청된 상대 경로 결합)
        fs::path fullPath = GetEngineRootPath() / relativePath;

        // Check if the file exists before opening (열기 전에 파일이 존재하는지 확인)
        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
            return ""; // Return empty string if file not found (파일을 찾을 수 없는 경우 빈 문자열 반환)
        }

        // Open file in binary mode to prevent newline conversions (개행 변환을 방지하기 위해 이진 모드로 파일 열기)
        std::ifstream file(fullPath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        // Read the entire file buffer into a string stream (전체 파일 버퍼를 문자열 스트림으로 읽기)
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool WriteFile(const std::string& relativePath, const std::string& content) {
        // Combine the root path with the requested relative path (루트 경로와 요청된 상대 경로 결합)
        fs::path fullPath = GetEngineRootPath() / relativePath;

        // Ensure parent directories exist before creating the file (파일을 생성하기 전에 부모 디렉토리가 존재하는지 확인)
        fs::create_directories(fullPath.parent_path());

        // Open file for writing (쓰기 모드로 파일 열기)
        std::ofstream file(fullPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false; // Return false if file creation failed (파일 생성에 실패하면 false 반환)
        }

        file << content;
        return true; // Return true on successful write (쓰기 성공 시 true 반환)
    }
}