#include "App.h"

#pragma comment(lib, "comctl32.lib")

#define ID_TRAY_ICON 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_HIDE 1003
#define ID_TRAY_EXIT 1004

#define COLOR_SAFE       RGB(0, 150, 0)
#define COLOR_WARNING    RGB(255, 165, 0)
#define COLOR_DANGER     RGB(220, 20, 60)
#define COLOR_BG         RGB(240, 248, 255)

#define BASE_WIDTH  800
#define BASE_HEIGHT 600

App::App(HINSTANCE hInstance, int nCmdShow)
    : m_hInstance(hInstance), m_hWnd(nullptr), m_hBtnScan(nullptr),
      m_hBtnClean(nullptr), m_hBtnIgnore(nullptr), m_hBtnBackToHome(nullptr),
      m_hStaticStatus(nullptr), m_hStaticFiles(nullptr), m_hStaticResult(nullptr),
      m_hProgressBar(nullptr), m_hFont(nullptr), m_hFontLarge(nullptr),
      m_scanState(ScanState::Idle), m_totalFilesScanned(0),
      m_foundTasks(false), m_foundDrivers(false) {
    m_appName = L"银狐木马专杀工具";

    WCHAR path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_appPath = path;

    size_t lastSlash = m_appPath.find_last_of(L"\\");
    std::wstring appDir = m_appPath.substr(0, lastSlash);

    ExtractResource(IDR_NSUDO_EXE, appDir + L"\\nsudo.exe");
    ExtractResource(IDR_KILL_BAT, appDir + L"\\kill.bat");
}

App::~App() {
    DestroyTrayIcon();
    CleanupExtractedFiles();
}

int App::Run() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"SilverFoxAntiVirusClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    m_hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        m_appName.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        BASE_WIDTH, BASE_HEIGHT,
        NULL,
        NULL,
        m_hInstance,
        this
    );

    if (m_hWnd == NULL) {
        return 0;
    }

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_NCCREATE) {
        LPCREATESTRUCT lpCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        App* pApp = reinterpret_cast<App*>(lpCreateStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pApp));
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    App* pApp = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (pApp == NULL) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_CREATE:
            return pApp->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lParam));
        case WM_DESTROY:
            return pApp->OnDestroy(hwnd);
        case WM_COMMAND:
            return pApp->OnCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
        case WM_SIZE:
            return pApp->OnSize(hwnd, (UINT)wParam, LOWORD(lParam), HIWORD(lParam));
        case WM_SYSCOMMAND:
            return pApp->OnSysCommand(hwnd, wParam, lParam);
        case WM_USER + 1:
            return pApp->OnTrayNotification(uMsg, wParam, lParam);
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT App::OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct) {
    m_hWnd = hwnd;
    CreateTrayIcon();
    SetAutoStart(TRUE);

    RECT rc;
    GetClientRect(hwnd, &rc);
    CreateMainInterface(rc.right, rc.bottom);

    return 0;
}

LRESULT App::OnDestroy(HWND hwnd) {
    DestroyTrayIcon();
    PostQuitMessage(0);
    return 0;
}

LRESULT App::OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
    if (id == ID_BTN_SCAN && codeNotify == BN_CLICKED) {
        StartScan();
    } else if (id == ID_BTN_CLEAN && codeNotify == BN_CLICKED) {
        m_scanState = ScanState::Processing;
        m_statusText = L"正在处理...";
        
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        DestroyAllControls();
        CreateMainInterface(rc.right, rc.bottom);
        
        size_t lastSlash = m_appPath.find_last_of(L"\\");
        std::wstring appDir = m_appPath.substr(0, lastSlash);
        std::wstring nsudoPath = appDir + L"\\nsudo.exe";
        std::wstring batPath = appDir + L"\\kill.bat";
        
        std::wstring command = L"\"" + nsudoPath + L"\" -U:T -P:E \"" + batPath + L"\"";
        
        HANDLE hJob = CreateJobObjectW(NULL, NULL);
        if (hJob) {
            JOBOBJECT_BASIC_LIMIT_INFORMATION jobInfo = {0};
            jobInfo.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob, JobObjectBasicLimitInformation, &jobInfo, sizeof(jobInfo));
        }
        
        STARTUPINFOW si = {0};
        si.cb = sizeof(STARTUPINFOW);
        PROCESS_INFORMATION pi = {0};
        
        if (CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, CREATE_NEW_PROCESS_GROUP, NULL, appDir.c_str(), &si, &pi)) {
            if (hJob) {
                AssignProcessToJobObject(hJob, pi.hProcess);
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        if (hJob) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION jobAccounting = {0};
            do {
                QueryInformationJobObject(hJob, JobObjectBasicAccountingInformation, &jobAccounting, sizeof(jobAccounting), NULL);
                if (jobAccounting.ActiveProcesses == 0) break;
                Sleep(200);
            } while (jobAccounting.ActiveProcesses > 0);
            CloseHandle(hJob);
        }
        
        m_foundTasks = false;
        m_foundDrivers = false;
        m_scanState = ScanState::Completed;
        
        GetClientRect(m_hWnd, &rc);
        DestroyAllControls();
        CreateMainInterface(rc.right, rc.bottom);
    } else if (id == ID_BTN_IGNORE && codeNotify == BN_CLICKED) {
        m_scanState = ScanState::Idle;
        m_totalFilesScanned = 0;
        m_foundTasks = false;
        m_foundDrivers = false;
        m_statusText.clear();
        
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        DestroyAllControls();
        CreateMainInterface(rc.right, rc.bottom);
    } else if (id == ID_BTN_BACK_TO_HOME && codeNotify == BN_CLICKED) {
        m_scanState = ScanState::Idle;
        m_totalFilesScanned = 0;
        m_foundTasks = false;
        m_foundDrivers = false;
        m_statusText.clear();
        
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        DestroyAllControls();
        CreateMainInterface(rc.right, rc.bottom);
    }
    return 0;
}

LRESULT App::OnSize(HWND hwnd, UINT state, int cx, int cy) {
    if (state != SIZE_MINIMIZED && cx > 0 && cy > 0) {
        DestroyAllControls();
        CreateMainInterface(cx, cy);
    }
    return 0;
}

LRESULT App::OnSysCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    switch (wParam & 0xFFF0) {
        case SC_MINIMIZE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case SC_MAXIMIZE:
        case SC_RESTORE:
            ShowWindow(hwnd, SW_SHOW);
            return DefWindowProc(hwnd, WM_SYSCOMMAND, wParam, lParam);
        default:
            return DefWindowProc(hwnd, WM_SYSCOMMAND, wParam, lParam);
    }
}

void App::DestroyAllControls() {
    if (m_hBtnScan) { DestroyWindow(m_hBtnScan); m_hBtnScan = nullptr; }
    if (m_hBtnClean) { DestroyWindow(m_hBtnClean); m_hBtnClean = nullptr; }
    if (m_hBtnIgnore) { DestroyWindow(m_hBtnIgnore); m_hBtnIgnore = nullptr; }
    if (m_hBtnBackToHome) { DestroyWindow(m_hBtnBackToHome); m_hBtnBackToHome = nullptr; }
    if (m_hStaticStatus) { DestroyWindow(m_hStaticStatus); m_hStaticStatus = nullptr; }
    if (m_hStaticFiles) { DestroyWindow(m_hStaticFiles); m_hStaticFiles = nullptr; }
    if (m_hStaticResult) { DestroyWindow(m_hStaticResult); m_hStaticResult = nullptr; }
    if (m_hProgressBar) { DestroyWindow(m_hProgressBar); m_hProgressBar = nullptr; }
    if (m_hFont) { DeleteObject(m_hFont); m_hFont = nullptr; }
    if (m_hFontLarge) { DeleteObject(m_hFontLarge); m_hFontLarge = nullptr; }
}

void App::CreateMainInterface(int clientWidth, int clientHeight) {
    float scaleX = (float)clientWidth / BASE_WIDTH;
    float scaleY = (float)clientHeight / BASE_HEIGHT;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 0.6f) scale = 0.6f;
    if (scale > 2.5f) scale = 2.5f;

    int margin = (int)(30 * scale);
    int ctrlSpacing = (int)(20 * scale);
    int buttonHeight = (int)(50 * scale);
    int buttonWidth = (int)(200 * scale);
    int progressHeight = (int)(25 * scale);
    int progressWidth = (int)(400 * scale);
    int normalFontSize = (int)(14 * scale);
    int largeFontSize = (int)(20 * scale);

    m_hFont = CreateFontW(normalFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    m_hFontLarge = CreateFontW(largeFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    int centerX = clientWidth / 2;
    int startY = clientHeight / 4;

    // 扫描按钮
    m_hBtnScan = CreateWindowExW(0, L"BUTTON", L"开始扫描",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        centerX - buttonWidth / 2, startY, buttonWidth, buttonHeight,
        m_hWnd, (HMENU)ID_BTN_SCAN, m_hInstance, nullptr);
    SendMessageW(m_hBtnScan, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);

    // 进度条
    int progressY = startY + buttonHeight + ctrlSpacing;
    int actualProgressWidth = (int)(clientWidth * 0.6f);
    if (actualProgressWidth > progressWidth) actualProgressWidth = progressWidth;
    if (actualProgressWidth < 200) actualProgressWidth = 200;

    m_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        centerX - actualProgressWidth / 2, progressY, actualProgressWidth, progressHeight,
        m_hWnd, (HMENU)ID_PROGRESS_BAR, m_hInstance, nullptr);

    if (m_scanState == ScanState::Scanning) {
        SendMessage(m_hProgressBar, PBM_SETPOS, 50, 0);
    } else if (m_scanState == ScanState::Completed) {
        SendMessage(m_hProgressBar, PBM_SETPOS, 100, 0);
    }

    // 状态文本
    int statusY = progressY + progressHeight + ctrlSpacing;
    int statusHeight = (int)(30 * scale);

    std::wstring statusText = L"";
    std::wstring filesText = L"";
    std::wstring resultText = L"";

    if (m_scanState == ScanState::Idle) {
        statusText = L"点击\"开始扫描\"按钮开始检测系统安全状态";
        filesText = L"已扫描文件: 0";
        resultText = L"";
    } else if (m_scanState == ScanState::Scanning) {
        statusText = m_statusText.empty() ? L"正在扫描系统..." : m_statusText;
        filesText = L"已扫描文件: " + std::to_wstring(m_totalFilesScanned);
        resultText = L"";
    } else if (m_scanState == ScanState::Processing) {
        statusText = m_statusText.empty() ? L"正在处理..." : m_statusText;
        filesText = L"请稍候";
        resultText = L"正在执行威胁处理...";
    } else if (m_scanState == ScanState::Completed) {
        statusText = L"扫描完成";
        filesText = L"已扫描文件: " + std::to_wstring(m_totalFilesScanned);
        if (m_foundTasks || m_foundDrivers) {
            if (m_foundTasks && m_foundDrivers) {
                resultText = L"发现威胁！\n检测到银狐木马特征（Tasks目录）\n检测到可疑驱动程序（drivers目录）";
            } else if (m_foundTasks) {
                resultText = L"发现威胁！\n检测到银狐木马特征（Tasks目录）";
            } else {
                resultText = L"发现威胁！\n检测到可疑驱动程序（drivers目录）";
            }
        } else {
            statusText = L"处理完毕";
            resultText = L"威胁已处理\n系统安全";
        }
    }

    m_hStaticStatus = CreateWindowExW(0, L"STATIC", statusText.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, statusY, clientWidth - margin * 2, statusHeight,
        m_hWnd, (HMENU)ID_STATIC_STATUS, m_hInstance, nullptr);
    SendMessageW(m_hStaticStatus, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 文件计数文本
    int filesY = statusY + statusHeight + (int)(10 * scale);
    int filesHeight = (int)(25 * scale);

    m_hStaticFiles = CreateWindowExW(0, L"STATIC", filesText.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        margin, filesY, clientWidth - margin * 2, filesHeight,
        m_hWnd, (HMENU)ID_STATIC_FILES, m_hInstance, nullptr);
    SendMessageW(m_hStaticFiles, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // 结果文本
    int resultY = filesY + filesHeight + ctrlSpacing;
    int resultHeight = (int)(90 * scale);

    COLORREF resultColor = RGB(0, 0, 0);
    if (m_scanState == ScanState::Completed) {
        resultColor = (m_foundTasks || m_foundDrivers) ? COLOR_DANGER : COLOR_SAFE;
    }

    m_hStaticResult = CreateWindowExW(0, L"STATIC", resultText.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_WORDELLIPSIS | SS_NOTIFY,
        margin, resultY, clientWidth - margin * 2, resultHeight,
        m_hWnd, (HMENU)ID_STATIC_RESULT, m_hInstance, nullptr);
    SendMessageW(m_hStaticResult, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);
    
    if (m_scanState == ScanState::Completed) {
        HDC hdc = GetDC(m_hStaticResult);
        SetTextColor(hdc, resultColor);
        ReleaseDC(m_hStaticResult, hdc);
    }

    if (m_scanState == ScanState::Scanning) {
        EnableWindow(m_hBtnScan, FALSE);
        SetWindowTextW(m_hBtnScan, L"扫描中...");
    } else if (m_scanState == ScanState::Completed) {
        if (m_foundTasks || m_foundDrivers) {
            int btnY = resultY + resultHeight + ctrlSpacing;
            int btnWidthSmall = (int)(150 * scale);
            int btnSpacing = (int)(20 * scale);
            int totalBtnWidth = btnWidthSmall * 2 + btnSpacing;
            int btnStartX = centerX - totalBtnWidth / 2;

            m_hBtnClean = CreateWindowExW(0, L"BUTTON", L"立即处理",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                btnStartX, btnY, btnWidthSmall, buttonHeight,
                m_hWnd, (HMENU)ID_BTN_CLEAN, m_hInstance, nullptr);
            SendMessageW(m_hBtnClean, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);

            m_hBtnIgnore = CreateWindowExW(0, L"BUTTON", L"放过威胁",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                btnStartX + btnWidthSmall + btnSpacing, btnY, btnWidthSmall, buttonHeight,
                m_hWnd, (HMENU)ID_BTN_IGNORE, m_hInstance, nullptr);
            SendMessageW(m_hBtnIgnore, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);

            EnableWindow(m_hBtnScan, FALSE);
        } else {
            int btnY = resultY + resultHeight + ctrlSpacing;
            int btnWidthSmall = (int)(150 * scale);
            
            m_hBtnBackToHome = CreateWindowExW(0, L"BUTTON", L"回到主页",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                centerX - btnWidthSmall / 2, btnY, btnWidthSmall, buttonHeight,
                m_hWnd, (HMENU)ID_BTN_BACK_TO_HOME, m_hInstance, nullptr);
            SendMessageW(m_hBtnBackToHome, WM_SETFONT, (WPARAM)m_hFontLarge, TRUE);
            
            EnableWindow(m_hBtnScan, FALSE);
        }
    }
}

void App::StartScan() {
    if (m_scanState == ScanState::Scanning) return;

    m_scanState = ScanState::Scanning;
    m_totalFilesScanned = 0;
    m_foundTasks = false;
    m_foundDrivers = false;

    EnableScanButton(FALSE);
    SetStatusText(L"正在扫描系统...");
    SetFilesText(L"已扫描文件: 0");
    SetResultText(L"", RGB(0, 0, 0));
    SendMessage(m_hProgressBar, PBM_SETPOS, 0, 0);

    PerformScan();
}

void App::PerformScan() {
    SetStatusText(L"正在扫描系统任务目录...");

    int tasksFiles = 0;
    std::wstring tasksPath = L"C:\\Windows\\System32\\Tasks";
    tasksFiles = CountFilesInDirectory(tasksPath);
    m_totalFilesScanned += tasksFiles;

    if (tasksFiles > 0) {
        m_foundTasks = true;
    }

    SendMessage(m_hProgressBar, PBM_SETPOS, 50, 0);
    SetFilesText(L"已扫描文件: " + std::to_wstring(m_totalFilesScanned));

    SetStatusText(L"正在扫描驱动程序目录...");

    int driversFiles = 0;
    std::wstring driversPath = L"C:\\Windows\\System32\\drivers";
    driversFiles = CountFilesInDirectory(driversPath);
    m_totalFilesScanned += driversFiles;

    if (driversFiles > 0) {
        m_foundDrivers = true;
    }

    SendMessage(m_hProgressBar, PBM_SETPOS, 100, 0);
    SetFilesText(L"已扫描文件: " + std::to_wstring(m_totalFilesScanned));

    m_scanState = ScanState::Completed;

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    DestroyAllControls();
    CreateMainInterface(rc.right, rc.bottom);
}

int App::CountFilesInDirectory(const std::wstring& path) {
    int count = 0;
    std::wstring searchPath = path + L"\\*";

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                count++;
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }

    return count;
}

void App::SetScanResult(SecurityLevel level, const std::wstring& message) {
    COLORREF color;
    switch (level) {
        case SecurityLevel::Safe:
            color = COLOR_SAFE;
            break;
        case SecurityLevel::Warning:
            color = COLOR_WARNING;
            break;
        case SecurityLevel::Danger:
            color = COLOR_DANGER;
            break;
        default:
            color = RGB(0, 0, 0);
    }
    SetResultText(message, color);
}

void App::SetStatusText(const std::wstring& text) {
    m_statusText = text;
    if (m_hStaticStatus) {
        SetWindowTextW(m_hStaticStatus, text.c_str());
    }
}

void App::SetFilesText(const std::wstring& text) {
    if (m_hStaticFiles) {
        SetWindowTextW(m_hStaticFiles, text.c_str());
    }
}

void App::SetResultText(const std::wstring& text, COLORREF color) {
    if (m_hStaticResult) {
        SetWindowTextW(m_hStaticResult, text.c_str());
    }
}

void App::EnableScanButton(BOOL enable) {
    if (m_hBtnScan) {
        EnableWindow(m_hBtnScan, enable);
        SetWindowTextW(m_hBtnScan, enable ? L"开始扫描" : L"扫描中...");
    }
}

BOOL App::ExtractResource(int resourceId, const std::wstring& outputPath) {
    HRSRC hResource = FindResourceW(m_hInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (hResource == NULL) return FALSE;

    HGLOBAL hGlobal = LoadResource(m_hInstance, hResource);
    if (hGlobal == NULL) return FALSE;

    DWORD size = SizeofResource(m_hInstance, hResource);
    if (size == 0) return FALSE;

    LPVOID pData = LockResource(hGlobal);
    if (pData == NULL) return FALSE;

    HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD bytesWritten = 0;
    BOOL success = WriteFile(hFile, pData, size, &bytesWritten, NULL);
    
    CloseHandle(hFile);
    return success && (bytesWritten == size);
}

void App::CleanupExtractedFiles() {
    size_t lastSlash = m_appPath.find_last_of(L"\\");
    std::wstring appDir = m_appPath.substr(0, lastSlash);

    DeleteFileW((appDir + L"\\nsudo.exe").c_str());
    DeleteFileW((appDir + L"\\kill.bat").c_str());
}

BOOL App::CreateTrayIcon() {
    ZeroMemory(&m_nid, sizeof(NOTIFYICONDATA));
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = ID_TRAY_ICON;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_USER + 1;

    HICON hIcon = LoadIcon(NULL, IDI_SHIELD);
    if (hIcon == NULL) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    m_nid.hIcon = hIcon;

    wcscpy_s(m_nid.szTip, m_appName.c_str());

    return Shell_NotifyIcon(NIM_ADD, &m_nid);
}

void App::DestroyTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &m_nid);
    if (m_nid.hIcon) {
        DestroyIcon(m_nid.hIcon);
    }
}

LRESULT App::OnTrayNotification(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (wParam != ID_TRAY_ICON) {
        return 0;
    }

    switch (lParam) {
        case WM_LBUTTONUP:
            ShowWindow(m_hWnd, SW_SHOW);
            SetForegroundWindow(m_hWnd);
            break;
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SHOW, L"显示窗口");
            AppendMenu(hMenu, MF_STRING, ID_TRAY_HIDE, L"隐藏窗口");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

            SetForegroundWindow(m_hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
            DestroyMenu(hMenu);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_TRAY_SHOW:
                    ShowWindow(m_hWnd, SW_SHOW);
                    SetForegroundWindow(m_hWnd);
                    break;
                case ID_TRAY_HIDE:
                    ShowWindow(m_hWnd, SW_HIDE);
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(m_hWnd);
                    break;
            }
            break;
        }
    }

    return 0;
}

BOOL App::SetAutoStart(BOOL enable) {
    HKEY hKey = NULL;
    const wchar_t* subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    if (enable) {
        result = RegSetValueEx(hKey, m_appName.c_str(), 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(m_appPath.c_str()),
                               (m_appPath.length() + 1) * sizeof(WCHAR));
    } else {
        result = RegDeleteValue(hKey, m_appName.c_str());
    }

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

BOOL App::IsAutoStartEnabled() {
    HKEY hKey = NULL;
    const wchar_t* subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    WCHAR path[MAX_PATH] = {0};
    DWORD size = MAX_PATH * sizeof(WCHAR);
    result = RegQueryValueEx(hKey, m_appName.c_str(), NULL, NULL,
                             reinterpret_cast<BYTE*>(path), &size);

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}