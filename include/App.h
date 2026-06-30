#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

#include "resource.h"

#define ID_BTN_SCAN        2001
#define ID_BTN_CLEAN       2006
#define ID_BTN_IGNORE      2007
#define ID_BTN_BACK_TO_HOME 2008
#define ID_STATIC_STATUS   2002
#define ID_STATIC_FILES    2003
#define ID_STATIC_RESULT   2004
#define ID_PROGRESS_BAR    2005

enum class ScanState {
    Idle,
    Scanning,
    Completed,
    Processing
};

enum class SecurityLevel {
    Safe,
    Warning,
    Danger
};

class App {
public:
    App(HINSTANCE hInstance, int nCmdShow);
    ~App();

    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
    LRESULT OnDestroy(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
    LRESULT OnSize(HWND hwnd, UINT state, int cx, int cy);
    LRESULT OnSysCommand(HWND hwnd, WPARAM wParam, LPARAM lParam);

    BOOL CreateTrayIcon();
    void DestroyTrayIcon();
    LRESULT OnTrayNotification(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL SetAutoStart(BOOL enable);
    BOOL IsAutoStartEnabled();

    void DestroyAllControls();
    void CreateMainInterface(int clientWidth, int clientHeight);

    void StartScan();
    void PerformScan();
    int CountFilesInDirectory(const std::wstring& path);
    void SetScanResult(SecurityLevel level, const std::wstring& message);

    void SetStatusText(const std::wstring& text);
    void SetFilesText(const std::wstring& text);
    void SetResultText(const std::wstring& text, COLORREF color);
    void EnableScanButton(BOOL enable);

    BOOL ExtractResource(int resourceId, const std::wstring& outputPath);
    void CleanupExtractedFiles();

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    NOTIFYICONDATA m_nid;
    std::wstring m_appName;
    std::wstring m_appPath;

    HWND m_hBtnScan;
    HWND m_hBtnClean;
    HWND m_hBtnIgnore;
    HWND m_hBtnBackToHome;
    HWND m_hStaticStatus;
    HWND m_hStaticFiles;
    HWND m_hStaticResult;
    HWND m_hProgressBar;
    HFONT m_hFont;
    HFONT m_hFontLarge;

    ScanState m_scanState;
    int m_totalFilesScanned;
    bool m_foundTasks;
    bool m_foundDrivers;
    std::wstring m_statusText;
};