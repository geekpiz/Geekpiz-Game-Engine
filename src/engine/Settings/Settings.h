#pragma once
#include <string>
#include <unordered_map>

namespace Settings {
    // Load settings from file (파일에서 설정 로드)
    void Load();

    // Get string value by key (키로 문자열 값 가져오기)
    std::string Get(const std::string& key);

    // Get color value as hex (색상 값을 16진수로 가져오기)
    unsigned int GetColor(const std::string& key, unsigned int defaultColor);

    // -----------------------------------------------------------------
    // Write-back API (NEW) (설정 저장용 API - 신규 추가)
    // -----------------------------------------------------------------
    // Settings used to be read-only (Load/Get/GetColor only) - nothing
    // ever wrote back to Settings/Settings.json. The Layout system needs
    // to persist small values (icon size, last layout name, etc.), so
    // this adds an in-memory Set + an explicit Save(). Save() is never
    // called automatically on every Set() - callers decide when to flush
    // to disk (e.g. on window close, or an explicit "Save Layout" menu
    // action) so we don't hit the disk every single frame/keystroke.
    // (기존에는 Settings가 읽기 전용(Load/Get/GetColor)이라 Settings.json에
    //  다시 쓰는 기능이 전혀 없었음. 레이아웃 시스템은 작은 값들(아이콘
    //  크기, 마지막 레이아웃 이름 등)을 저장해야 해서 메모리상 Set과
    //  명시적 Save()를 추가함. Set()마다 자동으로 Save()를 부르지 않고
    //  호출하는 쪽에서 언제 디스크에 실제로 쓸지 결정함 (예: 창 닫을 때,
    //  "레이아웃 저장" 메뉴를 눌렀을 때) - 매 프레임/매 타이핑마다 디스크에
    //  쓰는 걸 피하기 위함)

    // Set a string value in memory (call Save() to actually persist it) (메모리상 문자열 값 설정 - 실제 저장은 Save() 호출 필요)
    void Set(const std::string& key, const std::string& value);

    // Get/Set a float value, stored as plain text (e.g. "1.250000") (실수 값 읽기/쓰기 - 일반 텍스트로 저장됨)
    float GetFloat(const std::string& key, float defaultValue);
    void SetFloat(const std::string& key, float value);

    // Get/Set a bool value, stored as "1"/"0" (불리언 값 읽기/쓰기 - "1"/"0"으로 저장됨)
    bool GetBool(const std::string& key, bool defaultValue);
    void SetBool(const std::string& key, bool value);

    // Flush every in-memory setting back to Settings/Settings.json,
    // keeping the same "key:value per line" format Load() already
    // understands (파일 포맷을 그대로 유지하면서 메모리에 있는 모든 설정을
    // Settings/Settings.json에 실제로 씀 - Load()가 읽을 수 있는 같은 형식)
    void Save();
}