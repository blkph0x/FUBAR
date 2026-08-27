#include "app_window.h"

#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {

constexpr wchar_t kMainClass[] = L"AudioVoxMainWindow";
constexpr wchar_t kReplayClass[] = L"AudioVoxReplayWindow";
constexpr UINT kMessageStatus = WM_APP + 1;
constexpr UINT kMessageReplay = WM_APP + 2;
constexpr UINT_PTR kMeterTimer = 1;

template <typename T>
class LocalComPtr {
 public:
  ~LocalComPtr() { reset(); }
  T* operator->() const { return value_; }
  T** put() {
    reset();
    return &value_;
  }
  void reset() {
    if (value_) value_->Release();
    value_ = nullptr;
  }

 private:
  T* value_ = nullptr;
};

enum ControlId {
  IdDevice = 100,
  IdMode,
  IdThreshold,
  IdFrequency,
  IdPreRoll,
  IdHold,
  IdSave,
  IdMonitor,
  IdForce,
  IdOutput,
  IdBrowse,
  IdStart,
  IdStop,
  IdReplay,
  IdOpenFolder,
  IdReplayList,
  IdReplayPlay,
  IdReplayOpen
};

HWND addControl(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id = 0,
                DWORD extendedStyle = 0) {
  HWND control = CreateWindowExW(extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                                 x, y, width, height, parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               TRUE);
  return control;
}

std::wstring windowText(HWND control) {
  const int length = GetWindowTextLengthW(control);
  std::wstring value(static_cast<std::size_t>(length), L'\0');
  if (length > 0) GetWindowTextW(control, value.data(), length + 1);
  return value;
}

void setWindowNumber(HWND control, double value, int precision = 1) {
  std::wostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  SetWindowTextW(control, stream.str().c_str());
}

std::filesystem::path executableDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

int meterPosition(float db) {
  return static_cast<int>(std::clamp(db + 90.0f, 0.0f, 90.0f) * 10.0f);
}

std::wstring replayLabel(const ReplayEntry& entry) {
  const std::time_t time = std::chrono::system_clock::to_time_t(entry.started);
  std::tm local{};
  localtime_s(&local, &time);
  std::wostringstream stream;
  stream << std::put_time(&local, L"%H:%M:%S") << L"  "
         << std::fixed << std::setprecision(2) << entry.frequencyMhz << L" MHz  "
         << channelModeName(entry.mode) << L"  " << std::setprecision(1)
         << entry.durationSeconds << L" s  " << entry.peakDb << L" dB  "
         << entry.path.filename().wstring();
  return stream.str();
}

}  // namespace

AppWindow::AppWindow(AudioOptions initialOptions) : options_(std::move(initialOptions)) {}

int AppWindow::run(HINSTANCE instance, int showCommand) {
  instance_ = instance;
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
  InitCommonControlsEx(&controls);

  WNDCLASSEXW mainClass{};
  mainClass.cbSize = sizeof(mainClass);
  mainClass.lpfnWndProc = windowProc;
  mainClass.hInstance = instance_;
  mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  mainClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  mainClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  mainClass.lpszClassName = kMainClass;
  RegisterClassExW(&mainClass);

  WNDCLASSEXW replayClass{};
  replayClass.cbSize = sizeof(replayClass);
  replayClass.lpfnWndProc = replayProc;
  replayClass.hInstance = instance_;
  replayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  replayClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  replayClass.lpszClassName = kReplayClass;
  RegisterClassExW(&replayClass);

  window_ = CreateWindowExW(0, kMainClass, L"AudioVox VOX V1.0", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 780, 690, nullptr, nullptr, instance_,
                            this);
  if (!window_) return 1;
  ShowWindow(window_, showCommand);
  UpdateWindow(window_);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}

LRESULT CALLBACK AppWindow::windowProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  AppWindow* app = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    app = static_cast<AppWindow*>(create->lpCreateParams);
    app->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  }
  return app ? app->handleMessage(window, message, wParam, lParam)
             : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK AppWindow::replayProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  AppWindow* app = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    app = static_cast<AppWindow*>(create->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  }
  return app ? app->handleReplayMessage(window, message, wParam, lParam)
             : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT AppWindow::handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      createControls();
      populateDevices();
      applyOptionsToControls();
      SetTimer(window, kMeterTimer, 75, nullptr);
      PostMessageW(window, WM_COMMAND, IdStart, 0);
      return 0;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IdStart: startEngine(); return 0;
        case IdStop: stopEngine(); return 0;
        case IdBrowse: browseOutputDirectory(); return 0;
        case IdOpenFolder: openOutputDirectory(); return 0;
        case IdReplay: showReplayWindow(); return 0;
      }
      break;

    case WM_HSCROLL:
      if (reinterpret_cast<HWND>(lParam) == thresholdSlider_) {
        const int value = static_cast<int>(SendMessageW(thresholdSlider_, TBM_GETPOS, 0, 0));
        const std::wstring label = std::to_wstring(value) + L" dB";
        SetWindowTextW(thresholdValue_, label.c_str());
      }
      return 0;

    case WM_TIMER:
      if (wParam == kMeterTimer) updateMeters();
      return 0;

    case kMessageStatus: {
      std::unique_ptr<std::wstring> status(reinterpret_cast<std::wstring*>(lParam));
      if (status) updateStatus(*status);
      return 0;
    }

    case kMessageReplay: {
      std::unique_ptr<ReplayEntry> replay(reinterpret_cast<ReplayEntry*>(lParam));
      if (replay) {
        replays_.push_back(*replay);
        refreshReplayWindow();
      }
      return 0;
    }

    case WM_CLOSE:
      saveSettings();
      stopEngine();
      DestroyWindow(window);
      return 0;

    case WM_DESTROY:
      KillTimer(window, kMeterTimer);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

void AppWindow::createControls() {
  statusLabel_ = addControl(window_, L"STATIC", L"Idle", SS_CENTER | SS_CENTERIMAGE | WS_BORDER,
                            20, 18, 720, 48);

  addControl(window_, L"BUTTON", L"Audio source and VOX settings", BS_GROUPBOX, 20, 78, 720,
             258);
  addControl(window_, L"STATIC", L"Audio input device:", SS_RIGHT, 40, 108, 160, 22);
  deviceCombo_ = addControl(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 210,
                            104, 500, 240, IdDevice);
  addControl(window_, L"STATIC", L"Input channel:", SS_RIGHT, 40, 143, 160, 22);
  modeCombo_ = addControl(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 210, 139, 220, 160,
                          IdMode);
  addControl(window_, L"STATIC", L"Trigger threshold:", SS_RIGHT, 40, 178, 160, 22);
  thresholdSlider_ = addControl(window_, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 210, 170, 400,
                                35, IdThreshold);
  SendMessageW(thresholdSlider_, TBM_SETRANGE, TRUE, MAKELONG(-60, -5));
  thresholdValue_ = addControl(window_, L"STATIC", L"-35 dB", SS_CENTER, 620, 178, 80, 22);
  addControl(window_, L"STATIC", L"Radio frequency (MHz):", SS_RIGHT, 40, 215, 160, 22);
  frequencyEdit_ = addControl(window_, L"EDIT", L"268.000", WS_BORDER | ES_AUTOHSCROLL, 210,
                              211, 110, 24, IdFrequency, WS_EX_CLIENTEDGE);
  addControl(window_, L"STATIC", L"Pre-roll (sec):", SS_RIGHT, 335, 215, 100, 22);
  preRollEdit_ = addControl(window_, L"EDIT", L"1.0", WS_BORDER | ES_AUTOHSCROLL, 445, 211,
                            70, 24, IdPreRoll, WS_EX_CLIENTEDGE);
  addControl(window_, L"STATIC", L"Hold (sec):", SS_RIGHT, 525, 215, 75, 22);
  holdEdit_ = addControl(window_, L"EDIT", L"1.5", WS_BORDER | ES_AUTOHSCROLL, 610, 211, 70,
                         24, IdHold, WS_EX_CLIENTEDGE);
  saveCheck_ = addControl(window_, L"BUTTON", L"Save audio files", BS_AUTOCHECKBOX, 210, 248,
                          150, 24, IdSave);
  monitorCheck_ = addControl(window_, L"BUTTON", L"Live monitor", BS_AUTOCHECKBOX, 370, 248,
                             130, 24, IdMonitor);
  forceCheck_ = addControl(window_, L"BUTTON", L"Continuous record", BS_AUTOCHECKBOX, 510,
                           248, 160, 24, IdForce);
  addControl(window_, L"STATIC", L"Recording folder:", SS_RIGHT, 40, 292, 160, 22);
  outputEdit_ = addControl(window_, L"EDIT", L"recordings", WS_BORDER | ES_AUTOHSCROLL, 210,
                           288, 410, 25, IdOutput, WS_EX_CLIENTEDGE);
  addControl(window_, L"BUTTON", L"Browse...", BS_PUSHBUTTON, 630, 287, 80, 27, IdBrowse);

  addControl(window_, L"BUTTON", L"Live levels — input and routed output", BS_GROUPBOX, 20, 348,
             720, 190);
  addControl(window_, L"STATIC", L"Input left", SS_RIGHT, 45, 382, 100, 22);
  inputLeftMeter_ = addControl(window_, PROGRESS_CLASSW, L"", PBS_SMOOTH, 160, 380, 535, 22);
  addControl(window_, L"STATIC", L"Input right", SS_RIGHT, 45, 417, 100, 22);
  inputRightMeter_ = addControl(window_, PROGRESS_CLASSW, L"", PBS_SMOOTH, 160, 415, 535, 22);
  addControl(window_, L"STATIC", L"Output left", SS_RIGHT, 45, 466, 100, 22);
  outputLeftMeter_ = addControl(window_, PROGRESS_CLASSW, L"", PBS_SMOOTH, 160, 464, 535, 22);
  addControl(window_, L"STATIC", L"Output right", SS_RIGHT, 45, 501, 100, 22);
  outputRightMeter_ = addControl(window_, PROGRESS_CLASSW, L"", PBS_SMOOTH, 160, 499, 535, 22);
  for (HWND meter : {inputLeftMeter_, inputRightMeter_, outputLeftMeter_, outputRightMeter_}) {
    SendMessageW(meter, PBM_SETRANGE32, 0, 900);
    SendMessageW(meter, PBM_SETBARCOLOR, 0, RGB(30, 170, 70));
  }

  startButton_ = addControl(window_, L"BUTTON", L"Start / Apply", BS_DEFPUSHBUTTON, 85, 560,
                            130, 38, IdStart);
  stopButton_ = addControl(window_, L"BUTTON", L"Stop", BS_PUSHBUTTON, 230, 560, 100, 38,
                           IdStop);
  addControl(window_, L"BUTTON", L"Replay log", BS_PUSHBUTTON, 345, 560, 130, 38, IdReplay);
  addControl(window_, L"BUTTON", L"Open recordings", BS_PUSHBUTTON, 490, 560, 150, 38,
             IdOpenFolder);
  addControl(window_, L"STATIC",
             L"CLI automation: AudioVox.exe --headless --mode left --threshold-db -35",
             SS_CENTER, 40, 615, 680, 22);
}

void AppWindow::populateDevices() {
  std::wstring error;
  devices_ = AudioEngine::enumerateInputDevices(&error);
  SendMessageW(deviceCombo_, CB_RESETCONTENT, 0, 0);
  for (const auto& device : devices_) {
    SendMessageW(deviceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(device.name.c_str()));
  }
  if (devices_.empty()) {
    SendMessageW(deviceCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"No active capture devices"));
  }
  SendMessageW(deviceCombo_, CB_SETCURSEL, 0, 0);

  for (ChannelMode mode : {ChannelMode::Stereo, ChannelMode::Left, ChannelMode::Right,
                           ChannelMode::Mono}) {
    SendMessageW(modeCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(channelModeName(mode)));
  }
}

void AppWindow::applyOptionsToControls() {
  int selectedDevice = 0;
  for (std::size_t index = 0; index < devices_.size(); ++index) {
    if ((!options_.deviceId.empty() && devices_[index].id == options_.deviceId) ||
        (!options_.deviceName.empty() && devices_[index].name == options_.deviceName)) {
      selectedDevice = static_cast<int>(index);
      break;
    }
  }
  SendMessageW(deviceCombo_, CB_SETCURSEL, selectedDevice, 0);
  SendMessageW(modeCombo_, CB_SETCURSEL, static_cast<int>(options_.mode), 0);
  SendMessageW(thresholdSlider_, TBM_SETPOS, TRUE,
               static_cast<LPARAM>(std::clamp(static_cast<int>(options_.thresholdDb), -60, -5)));
  const std::wstring threshold = std::to_wstring(static_cast<int>(options_.thresholdDb)) + L" dB";
  SetWindowTextW(thresholdValue_, threshold.c_str());
  setWindowNumber(frequencyEdit_, options_.frequencyMhz, 3);
  setWindowNumber(preRollEdit_, options_.preRollSeconds, 1);
  setWindowNumber(holdEdit_, options_.holdSeconds, 1);
  SendMessageW(saveCheck_, BM_SETCHECK, options_.saveAudio ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(monitorCheck_, BM_SETCHECK, options_.monitor ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(forceCheck_, BM_SETCHECK, options_.forceRecord ? BST_CHECKED : BST_UNCHECKED, 0);
  SetWindowTextW(outputEdit_, options_.outputDirectory.wstring().c_str());
}

AudioOptions AppWindow::optionsFromControls() const {
  AudioOptions result = options_;
  const int device = static_cast<int>(SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0));
  if (device >= 0 && device < static_cast<int>(devices_.size())) {
    result.deviceId = devices_[device].id;
    result.deviceName = devices_[device].name;
  }
  const int mode = static_cast<int>(SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0));
  if (mode >= 0 && mode <= 3) result.mode = static_cast<ChannelMode>(mode);
  result.thresholdDb = static_cast<float>(SendMessageW(thresholdSlider_, TBM_GETPOS, 0, 0));
  result.saveAudio = SendMessageW(saveCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
  result.monitor = SendMessageW(monitorCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
  result.forceRecord = SendMessageW(forceCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
  result.outputDirectory = windowText(outputEdit_);
  try { result.frequencyMhz = std::stod(windowText(frequencyEdit_)); } catch (...) {}
  try { result.preRollSeconds = std::stof(windowText(preRollEdit_)); } catch (...) {}
  try { result.holdSeconds = std::stof(windowText(holdEdit_)); } catch (...) {}
  return result;
}

void AppWindow::startEngine() {
  stopEngine();
  options_ = optionsFromControls();
  if (options_.outputDirectory.is_relative()) {
    options_.outputDirectory = executableDirectory() / options_.outputDirectory;
    SetWindowTextW(outputEdit_, options_.outputDirectory.wstring().c_str());
  }
  EnableWindow(startButton_, FALSE);
  EnableWindow(stopButton_, TRUE);
  updateStatus(L"Starting...");
  engine_.start(
      options_,
      [this](const std::wstring& status) {
        if (IsWindow(window_)) {
          PostMessageW(window_, kMessageStatus, 0,
                       reinterpret_cast<LPARAM>(new std::wstring(status)));
        }
      },
      [this](const ReplayEntry& replay) {
        if (IsWindow(window_)) {
          PostMessageW(window_, kMessageReplay, 0,
                       reinterpret_cast<LPARAM>(new ReplayEntry(replay)));
        }
      });
}

void AppWindow::stopEngine() {
  engine_.stop();
  if (startButton_) EnableWindow(startButton_, TRUE);
  if (stopButton_) EnableWindow(stopButton_, FALSE);
  updateStatus(L"Idle");
}

void AppWindow::updateMeters() {
  const auto levels = engine_.levels();
  SendMessageW(inputLeftMeter_, PBM_SETPOS, meterPosition(levels.inputLeftDb), 0);
  SendMessageW(inputRightMeter_, PBM_SETPOS, meterPosition(levels.inputRightDb), 0);
  SendMessageW(outputLeftMeter_, PBM_SETPOS, meterPosition(levels.outputLeftDb), 0);
  SendMessageW(outputRightMeter_, PBM_SETPOS, meterPosition(levels.outputRightDb), 0);
  if (!engine_.running() && !engine_.status().empty() && engine_.status() != L"Idle") {
    updateStatus(engine_.status());
  }
}

void AppWindow::updateStatus(const std::wstring& status) {
  std::wstring text = status;
  if (status == L"Recording") text = L"● RECORDING — signal above threshold";
  else if (status == L"Listening") text = L"LISTENING — waiting for VOX trigger";
  SetWindowTextW(statusLabel_, text.c_str());
}

void AppWindow::browseOutputDirectory() {
  LocalComPtr<IFileDialog> dialog;
  HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(dialog.put()));
  if (FAILED(result)) return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  if (SUCCEEDED(dialog->Show(window_))) {
    LocalComPtr<IShellItem> item;
    if (SUCCEEDED(dialog->GetResult(item.put()))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        SetWindowTextW(outputEdit_, path);
        CoTaskMemFree(path);
      }
    }
  }
}

void AppWindow::openOutputDirectory() const {
  auto path = optionsFromControls().outputDirectory;
  if (path.is_relative()) path = executableDirectory() / path;
  std::error_code error;
  std::filesystem::create_directories(path, error);
  ShellExecuteW(window_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void AppWindow::showReplayWindow() {
  if (IsWindow(replayWindow_)) {
    ShowWindow(replayWindow_, SW_SHOW);
    SetForegroundWindow(replayWindow_);
    return;
  }
  replayWindow_ = CreateWindowExW(WS_EX_TOOLWINDOW, kReplayClass, L"AudioVox Replay Log",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                  780, 430, window_, nullptr, instance_, this);
}

LRESULT AppWindow::handleReplayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      replayWindow_ = window;
      replayList_ = addControl(window, L"LISTBOX", L"",
                               LBS_NOTIFY | WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 15,
                               15, 735, 300, IdReplayList, WS_EX_CLIENTEDGE);
      addControl(window, L"BUTTON", L"Play selected", BS_DEFPUSHBUTTON, 190, 335, 130, 35,
                 IdReplayPlay);
      addControl(window, L"BUTTON", L"Open file location", BS_PUSHBUTTON, 335, 335, 150, 35,
                 IdReplayOpen);
      addControl(window, L"BUTTON", L"Close", BS_PUSHBUTTON, 500, 335, 90, 35, IDCANCEL);
      refreshReplayWindow();
      return 0;
    case WM_COMMAND:
      if (LOWORD(wParam) == IdReplayPlay ||
          (LOWORD(wParam) == IdReplayList && HIWORD(wParam) == LBN_DBLCLK)) {
        playSelectedReplay();
        return 0;
      }
      if (LOWORD(wParam) == IdReplayOpen) {
        const int selected = static_cast<int>(SendMessageW(replayList_, LB_GETCURSEL, 0, 0));
        if (selected >= 0 && selected < static_cast<int>(replays_.size())) {
          const std::wstring parameters = L"/select,\"" + replays_[selected].path.wstring() + L"\"";
          ShellExecuteW(window, L"open", L"explorer.exe", parameters.c_str(), nullptr,
                        SW_SHOWNORMAL);
        }
        return 0;
      }
      if (LOWORD(wParam) == IDCANCEL) {
        DestroyWindow(window);
        return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      replayWindow_ = nullptr;
      replayList_ = nullptr;
      return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

void AppWindow::refreshReplayWindow() {
  if (!IsWindow(replayList_)) return;
  SendMessageW(replayList_, LB_RESETCONTENT, 0, 0);
  for (const auto& replay : replays_) {
    const auto label = replayLabel(replay);
    SendMessageW(replayList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
  }
  if (!replays_.empty()) {
    SendMessageW(replayList_, LB_SETCURSEL, static_cast<WPARAM>(replays_.size() - 1), 0);
  }
}

void AppWindow::playSelectedReplay() {
  if (!IsWindow(replayList_)) return;
  const int selected = static_cast<int>(SendMessageW(replayList_, LB_GETCURSEL, 0, 0));
  if (selected < 0 || selected >= static_cast<int>(replays_.size())) return;
  PlaySoundW(replays_[selected].path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void AppWindow::saveSettings() const {
  const auto ini = executableDirectory() / L"AudioVox.ini";
  const auto current = optionsFromControls();
  WritePrivateProfileStringW(L"AudioVox", L"OutputDirectory",
                             current.outputDirectory.wstring().c_str(), ini.c_str());
  WritePrivateProfileStringW(L"AudioVox", L"Frequency",
                             windowText(frequencyEdit_).c_str(), ini.c_str());
  WritePrivateProfileStringW(L"AudioVox", L"Threshold",
                             std::to_wstring(static_cast<int>(current.thresholdDb)).c_str(),
                             ini.c_str());
}

void AppWindow::loadSettings() {}
