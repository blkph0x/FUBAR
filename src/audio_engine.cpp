#include "audio_engine.h"

#include "audio_safety.h"
#include "wav_writer.h"
#include "vox_gate.h"

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <propvarutil.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iomanip>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

namespace {

template <typename T>
class ComPtr {
 public:
  ComPtr() = default;
  ~ComPtr() { reset(); }
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;

  T* get() const { return value_; }
  T** put() {
    reset();
    return &value_;
  }
  T* operator->() const { return value_; }
  explicit operator bool() const { return value_ != nullptr; }
  void reset() {
    if (value_) value_->Release();
    value_ = nullptr;
  }

 private:
  T* value_ = nullptr;
};

std::wstring hresultText(const wchar_t* operation, HRESULT result) {
  std::wostringstream stream;
  stream << operation << L" failed (0x" << std::hex << std::uppercase
         << static_cast<unsigned long>(result) << L")";
  return stream.str();
}

float peakToDb(float peak) {
  if (!std::isfinite(peak) || peak <= 0.0000316228f) return -90.0f;
  return std::clamp(20.0f * std::log10(std::min(peak, 1.0f)), -90.0f, 0.0f);
}

std::int16_t floatToPcm16(float sample) {
  const float clipped = sanitizeAudioSample(sample);
  return static_cast<std::int16_t>(std::lrint(clipped * 32767.0f));
}

enum class SampleEncoding {
  Unsupported,
  Unsigned8,
  Signed16,
  Signed24,
  Signed32,
  Float32,
  Float64
};

class SampleDecoder {
 public:
  explicit SampleDecoder(const WAVEFORMATEX* format) {
    if (!format || format->nChannels == 0 || format->nBlockAlign == 0 ||
        format->nBlockAlign % format->nChannels != 0) {
      return;
    }
    channels_ = format->nChannels;
    blockAlign_ = format->nBlockAlign;
    sampleStride_ = static_cast<WORD>(format->nBlockAlign / format->nChannels);
    sampleRate_ = format->nSamplesPerSec;
    bitsPerSample_ = format->wBitsPerSample;
    bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool isPcm = format->wFormatTag == WAVE_FORMAT_PCM;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
      const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
      isFloat = extensible->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
      isPcm = extensible->SubFormat.Data1 == WAVE_FORMAT_PCM;
    }
    if (isFloat && bitsPerSample_ == 32 && sampleStride_ >= 4) {
      encoding_ = SampleEncoding::Float32;
    } else if (isFloat && bitsPerSample_ == 64 && sampleStride_ >= 8) {
      encoding_ = SampleEncoding::Float64;
    } else if (isPcm && bitsPerSample_ == 8 && sampleStride_ >= 1) {
      encoding_ = SampleEncoding::Unsigned8;
    } else if (isPcm && bitsPerSample_ == 16 && sampleStride_ >= 2) {
      encoding_ = SampleEncoding::Signed16;
    } else if (isPcm && bitsPerSample_ == 24 && sampleStride_ >= 3) {
      encoding_ = SampleEncoding::Signed24;
    } else if (isPcm && bitsPerSample_ == 32 && sampleStride_ >= 4) {
      encoding_ = SampleEncoding::Signed32;
    }
  }

  bool valid() const { return encoding_ != SampleEncoding::Unsupported; }
  WORD channels() const { return channels_; }

  std::wstring description() const {
    std::wostringstream stream;
    stream << sampleRate_ << L" Hz, " << channels_ << L" channel";
    if (channels_ != 1) stream << L"s";
    stream << L", " << bitsPerSample_ << L"-bit ";
    switch (encoding_) {
      case SampleEncoding::Float32:
      case SampleEncoding::Float64: stream << L"float"; break;
      case SampleEncoding::Unsigned8:
      case SampleEncoding::Signed16:
      case SampleEncoding::Signed24:
      case SampleEncoding::Signed32: stream << L"PCM"; break;
      case SampleEncoding::Unsupported: stream << L"unsupported format"; break;
    }
    stream << L", block align " << blockAlign_;
    return stream.str();
  }

  float read(const BYTE* data, UINT32 frame, WORD channel, bool silent) const {
    if (silent || !data || !valid()) return 0.0f;
    const WORD safeChannel = std::min<WORD>(channel, static_cast<WORD>(channels_ - 1));
    const BYTE* sample = data + static_cast<std::size_t>(frame) * blockAlign_ +
                         static_cast<std::size_t>(safeChannel) * sampleStride_;
    switch (encoding_) {
      case SampleEncoding::Unsigned8:
        return sanitizeAudioSample((static_cast<int>(*sample) - 128) / 128.0);
      case SampleEncoding::Signed16: {
        std::int16_t value = 0;
        std::memcpy(&value, sample, sizeof(value));
        return sanitizeAudioSample(value / 32768.0);
      }
      case SampleEncoding::Signed24: {
        std::int32_t value = static_cast<std::int32_t>(sample[0]) |
                             (static_cast<std::int32_t>(sample[1]) << 8) |
                             (static_cast<std::int32_t>(sample[2]) << 16);
        if (value & 0x00800000) value |= static_cast<std::int32_t>(0xff000000);
        return sanitizeAudioSample(value / 8388608.0);
      }
      case SampleEncoding::Signed32: {
        std::int32_t value = 0;
        std::memcpy(&value, sample, sizeof(value));
        return sanitizeAudioSample(value / 2147483648.0);
      }
      case SampleEncoding::Float32: {
        float value = 0.0f;
        std::memcpy(&value, sample, sizeof(value));
        return sanitizeAudioSample(value);
      }
      case SampleEncoding::Float64: {
        double value = 0.0;
        std::memcpy(&value, sample, sizeof(value));
        return sanitizeAudioSample(value);
      }
      case SampleEncoding::Unsupported: return 0.0f;
    }
    return 0.0f;
  }

 private:
  SampleEncoding encoding_ = SampleEncoding::Unsupported;
  WORD channels_ = 0;
  WORD blockAlign_ = 0;
  WORD sampleStride_ = 0;
  DWORD sampleRate_ = 0;
  WORD bitsPerSample_ = 0;
};

std::wstring friendlyName(IMMDevice* device, const std::wstring& fallback = L"Audio device") {
  if (!device) return fallback;
  ComPtr<IPropertyStore> properties;
  if (FAILED(device->OpenPropertyStore(STGM_READ, properties.put()))) return fallback;
  PROPVARIANT value;
  PropVariantInit(&value);
  std::wstring name = fallback;
  if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
      value.vt == VT_LPWSTR && value.pwszVal) {
    name = value.pwszVal;
  }
  PropVariantClear(&value);
  return name;
}

std::filesystem::path recordingPath(const AudioOptions& options,
                                    std::wstring_view suffix = {}) {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_s(&local, &time);
  std::wostringstream name;
  const auto khz = static_cast<long long>(std::llround(std::max(0.0, options.frequencyMhz) * 1000.0));
  name << L"FUBAR_" << std::put_time(&local, L"%Y%m%d_%H%M%S") << L"_" << khz << L"kHz_";
  switch (options.mode) {
    case ChannelMode::Stereo: name << L"stereo"; break;
    case ChannelMode::Left: name << L"left"; break;
    case ChannelMode::Right: name << L"right"; break;
    case ChannelMode::Mono: name << L"mono"; break;
  }
  name << suffix;
  name << L".wav";
  std::error_code error;
  std::filesystem::create_directories(options.outputDirectory, error);
  return options.outputDirectory / name.str();
}

class MonitorOutput {
 public:
  ~MonitorOutput() { close(); }

  bool open(DWORD sampleRate) {
    close();
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) return false;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;
    const MMRESULT result = waveOutOpen(&handle_, WAVE_MAPPER, &format,
                                        reinterpret_cast<DWORD_PTR>(event_), 0,
                                        CALLBACK_EVENT);
    if (result == MMSYSERR_NOERROR) return true;
    CloseHandle(event_);
    event_ = nullptr;
    return false;
  }

  void submit(std::span<const std::int16_t> samples) {
    releaseCompleted();
    if (!handle_ || samples.empty() || pendingHeaders_.size() >= kMaximumPendingBuffers) return;
    auto* bytes = new char[samples.size_bytes()];
    std::memcpy(bytes, samples.data(), samples.size_bytes());
    auto* header = new WAVEHDR{};
    header->lpData = bytes;
    header->dwBufferLength = static_cast<DWORD>(samples.size_bytes());
    if (waveOutPrepareHeader(handle_, header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
      delete[] bytes;
      delete header;
      return;
    }
    if (waveOutWrite(handle_, header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
      waveOutUnprepareHeader(handle_, header, sizeof(WAVEHDR));
      delete[] bytes;
      delete header;
      return;
    }
    pendingHeaders_.push_back(header);
  }

  void close() {
    if (handle_) {
      waveOutReset(handle_);
      while (!pendingHeaders_.empty()) {
        releaseCompleted();
        if (!pendingHeaders_.empty() && event_) WaitForSingleObject(event_, 25);
      }
      waveOutClose(handle_);
      handle_ = nullptr;
    }
    if (event_) {
      CloseHandle(event_);
      event_ = nullptr;
    }
  }

 private:
  void releaseCompleted() {
    auto header = pendingHeaders_.begin();
    while (header != pendingHeaders_.end()) {
      if (((*header)->dwFlags & WHDR_DONE) == 0 ||
          waveOutUnprepareHeader(handle_, *header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        ++header;
        continue;
      }
      delete[] (*header)->lpData;
      delete *header;
      header = pendingHeaders_.erase(header);
    }
  }

  static constexpr std::uint32_t kMaximumPendingBuffers = 32;
  HWAVEOUT handle_ = nullptr;
  HANDLE event_ = nullptr;
  std::vector<WAVEHDR*> pendingHeaders_;
};

}  // namespace

bool testAudioSampleDecoder() {
  auto format = [](WORD tag, WORD bits, WORD channels, WORD sampleStride) {
    WAVEFORMATEX value{};
    value.wFormatTag = tag;
    value.nChannels = channels;
    value.nSamplesPerSec = 48000;
    value.wBitsPerSample = bits;
    value.nBlockAlign = static_cast<WORD>(channels * sampleStride);
    value.nAvgBytesPerSec = value.nSamplesPerSec * value.nBlockAlign;
    return value;
  };
  auto approximatelyEqual = [](float value, float expected) {
    return std::abs(value - expected) < 0.0001f;
  };

  auto pcm8 = format(WAVE_FORMAT_PCM, 8, 1, 1);
  const std::array<BYTE, 1> pcm8Data{192};
  if (!approximatelyEqual(SampleDecoder(&pcm8).read(pcm8Data.data(), 0, 0, false), 0.5f)) {
    return false;
  }

  auto pcm16 = format(WAVE_FORMAT_PCM, 16, 1, 2);
  const std::int16_t pcm16Data = 16384;
  if (!approximatelyEqual(
          SampleDecoder(&pcm16).read(reinterpret_cast<const BYTE*>(&pcm16Data), 0, 0, false),
          0.5f)) {
    return false;
  }

  auto pcm24 = format(WAVE_FORMAT_PCM, 24, 1, 3);
  const std::array<BYTE, 3> pcm24Data{0x00, 0x00, 0x40};
  if (!approximatelyEqual(SampleDecoder(&pcm24).read(pcm24Data.data(), 0, 0, false), 0.5f)) {
    return false;
  }

  auto pcm32 = format(WAVE_FORMAT_PCM, 32, 1, 4);
  const std::int32_t pcm32Data = 1073741824;
  if (!approximatelyEqual(
          SampleDecoder(&pcm32).read(reinterpret_cast<const BYTE*>(&pcm32Data), 0, 0, false),
          0.5f)) {
    return false;
  }

  auto float32 = format(WAVE_FORMAT_IEEE_FLOAT, 32, 1, 4);
  const float float32Data = 0.25f;
  if (!approximatelyEqual(
          SampleDecoder(&float32).read(reinterpret_cast<const BYTE*>(&float32Data), 0, 0, false),
          0.25f)) {
    return false;
  }

  auto float64 = format(WAVE_FORMAT_IEEE_FLOAT, 64, 1, 8);
  const double float64Data = -0.25;
  if (!approximatelyEqual(
          SampleDecoder(&float64).read(reinterpret_cast<const BYTE*>(&float64Data), 0, 0, false),
          -0.25f)) {
    return false;
  }

  WAVEFORMATEXTENSIBLE extensible{};
  extensible.Format = format(WAVE_FORMAT_EXTENSIBLE, 32, 2, 4);
  extensible.Format.cbSize = 22;
  extensible.SubFormat.Data1 = WAVE_FORMAT_IEEE_FLOAT;
  const std::array<float, 2> extensibleData{0.75f, -0.75f};
  const SampleDecoder extensibleDecoder(&extensible.Format);
  if (!approximatelyEqual(extensibleDecoder.read(
                              reinterpret_cast<const BYTE*>(extensibleData.data()), 0, 0, false),
                          0.75f) ||
      !approximatelyEqual(extensibleDecoder.read(
                              reinterpret_cast<const BYTE*>(extensibleData.data()), 0, 1, false),
                          -0.75f)) {
    return false;
  }

  const float invalidFloat = std::numeric_limits<float>::infinity();
  if (SampleDecoder(&float32).read(reinterpret_cast<const BYTE*>(&invalidFloat), 0, 0, false) !=
      0.0f) {
    return false;
  }
  auto invalid = format(WAVE_FORMAT_PCM, 16, 2, 2);
  invalid.nBlockAlign = 3;
  return !SampleDecoder(&invalid).valid();
}

AudioEngine::AudioEngine() {
  InitializeCriticalSection(&statusLock_);
}

AudioEngine::~AudioEngine() {
  stop();
  DeleteCriticalSection(&statusLock_);
}

std::vector<AudioDeviceInfo> AudioEngine::enumerateInputDevices(std::wstring* error) {
  std::vector<AudioDeviceInfo> devices;
  const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = SUCCEEDED(comResult);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
    if (error) *error = hresultText(L"COM initialization", comResult);
    return devices;
  }

  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator),
                                    reinterpret_cast<void**>(enumerator.put()));
  if (FAILED(result)) {
    if (error) *error = hresultText(L"Audio device enumerator", result);
    if (uninitialize) CoUninitialize();
    return devices;
  }

  ComPtr<IMMDeviceCollection> collection;
  std::wstring defaultDeviceId;
  ComPtr<IMMDevice> defaultDevice;
  if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
                                                     defaultDevice.put()))) {
    LPWSTR id = nullptr;
    if (SUCCEEDED(defaultDevice->GetId(&id)) && id) {
      defaultDeviceId = id;
      CoTaskMemFree(id);
    }
  }
  result = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, collection.put());
  if (SUCCEEDED(result)) {
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT index = 0; index < count; ++index) {
      ComPtr<IMMDevice> device;
      if (FAILED(collection->Item(index, device.put()))) continue;
      LPWSTR id = nullptr;
      if (FAILED(device->GetId(&id))) continue;
      std::wstring name = L"Audio input " + std::to_wstring(index + 1);
      ComPtr<IPropertyStore> properties;
      if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put()))) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
            value.vt == VT_LPWSTR && value.pwszVal) {
          name = value.pwszVal;
        }
        PropVariantClear(&value);
      }
      devices.push_back({id, name, defaultDeviceId == id});
      CoTaskMemFree(id);
    }
  } else if (error) {
    *error = hresultText(L"Input device enumeration", result);
  }

  if (uninitialize) CoUninitialize();
  return devices;
}

bool AudioEngine::start(const AudioOptions& options, StatusCallback statusCallback,
                        ReplayCallback replayCallback) {
  if (running_) return false;
  options_ = options;
  statusCallback_ = std::move(statusCallback);
  replayCallback_ = std::move(replayCallback);
  stopRequested_ = false;
  thread_ = CreateThread(nullptr, 0, captureThreadEntry, this, 0, nullptr);
  if (!thread_) {
    setStatus(L"Could not create the audio worker thread");
    return false;
  }
  return true;
}

void AudioEngine::stop() {
  stopRequested_ = true;
  liveHub_.endSession();
  if (thread_) {
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
}

DWORD WINAPI AudioEngine::captureThreadEntry(LPVOID context) {
  auto* engine = static_cast<AudioEngine*>(context);
  try {
    engine->captureThread();
  } catch (const std::exception& error) {
    engine->running_ = false;
    engine->recording_ = false;
    std::wstring message = L"Audio worker stopped: ";
    const std::string detail = error.what();
    message.append(detail.begin(), detail.end());
    engine->setStatus(message);
  } catch (...) {
    engine->running_ = false;
    engine->recording_ = false;
    engine->setStatus(L"Audio worker stopped after an unexpected error");
  }
  return 0;
}

bool AudioEngine::running() const { return running_; }
bool AudioEngine::recording() const { return recording_; }

LevelSnapshot AudioEngine::levels() const {
  return {inputLeftDb_, inputRightDb_, outputLeftDb_, outputRightDb_};
}

std::wstring AudioEngine::status() const {
  EnterCriticalSection(&statusLock_);
  const std::wstring copy = status_;
  LeaveCriticalSection(&statusLock_);
  return copy;
}

void AudioEngine::setStatus(const std::wstring& status) {
  EnterCriticalSection(&statusLock_);
  status_ = status;
  LeaveCriticalSection(&statusLock_);
  if (statusCallback_) statusCallback_(status);
}

void AudioEngine::captureThread() {
  const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = SUCCEEDED(comResult);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
    setStatus(hresultText(L"COM initialization", comResult));
    return;
  }

  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator),
                                    reinterpret_cast<void**>(enumerator.put()));
  if (FAILED(result)) {
    setStatus(hresultText(L"Audio device enumerator", result));
    if (uninitialize) CoUninitialize();
    return;
  }

  ComPtr<IMMDevice> device;
  if (!options_.deviceId.empty()) {
    result = enumerator->GetDevice(options_.deviceId.c_str(), device.put());
  } else {
    result = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, device.put());
  }
  if (FAILED(result)) {
    setStatus(hresultText(L"Opening input device", result));
    if (uninitialize) CoUninitialize();
    return;
  }

  ComPtr<IAudioClient> client;
  result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(client.put()));
  if (FAILED(result)) {
    setStatus(hresultText(L"WASAPI client activation", result));
    if (uninitialize) CoUninitialize();
    return;
  }

  WAVEFORMATEX* mixFormat = nullptr;
  result = client->GetMixFormat(&mixFormat);
  if (FAILED(result) || !mixFormat) {
    setStatus(hresultText(L"Reading input format", result));
    if (uninitialize) CoUninitialize();
    return;
  }

  const SampleDecoder decoder(mixFormat);
  if (!decoder.valid()) {
    setStatus(L"Unsupported input format: " + decoder.description());
    CoTaskMemFree(mixFormat);
    if (uninitialize) CoUninitialize();
    return;
  }

  result = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST,
                              10000000, 0, mixFormat, nullptr);
  if (FAILED(result)) {
    setStatus(hresultText(L"Starting shared audio capture", result));
    CoTaskMemFree(mixFormat);
    if (uninitialize) CoUninitialize();
    return;
  }

  ComPtr<IAudioCaptureClient> capture;
  result = client->GetService(__uuidof(IAudioCaptureClient),
                              reinterpret_cast<void**>(capture.put()));
  if (FAILED(result)) {
    setStatus(hresultText(L"Capture service", result));
    CoTaskMemFree(mixFormat);
    if (uninitialize) CoUninitialize();
    return;
  }

  const DWORD sampleRate = mixFormat->nSamplesPerSec;
  const WORD inputChannels = decoder.channels();
  const WORD recordChannels = options_.mode == ChannelMode::Stereo ? 2 : 1;
  const bool splitStereo = options_.splitStereoFiles && options_.mode == ChannelMode::Stereo;
  const std::size_t preRollLimit = static_cast<std::size_t>(
      std::max(0.0f, options_.preRollSeconds) * sampleRate * recordChannels);
  const std::uint64_t holdFrames = static_cast<std::uint64_t>(
      std::max(0.1f, options_.holdSeconds) * sampleRate);
  const float thresholdLinear = std::pow(10.0f, options_.thresholdDb / 20.0f);

  const std::wstring captureName = friendlyName(device.get(), options_.deviceName);
  bool monitorEnabled = options_.monitor;
  std::wstring monitorWarning;
  if (monitorEnabled && isVirtualAudioEndpoint(captureName)) {
    monitorEnabled = false;
    monitorWarning = L"LISTENING - live monitor disabled for virtual-cable capture safety";
  } else if (monitorEnabled) {
    ComPtr<IMMDevice> renderDevice;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, renderDevice.put()))) {
      const std::wstring renderName = friendlyName(renderDevice.get(), L"Default output");
      if (isVirtualCableMonitorLoop(captureName, renderName)) {
        monitorEnabled = false;
        monitorWarning = L"LISTENING - live monitor disabled to prevent a virtual-cable loop";
      }
    }
  }

  MonitorOutput monitor;
  if (monitorEnabled && !monitor.open(sampleRate)) {
    monitorEnabled = false;
    monitorWarning = L"LISTENING - live monitor unavailable for this output format";
  }

  std::deque<std::int16_t> preRoll;
  WavWriter writer;
  WavWriter rightWriter;
  ReplayEntry currentEntry;
  std::filesystem::path rightRecordingPath;
  std::uint64_t recordingFrames = 0;
  float recordingLeftPeak = 0.0f;
  float recordingRightPeak = 0.0f;
  VoxGate voxGate(holdFrames);

  auto writeSamples = [&](std::span<const std::int16_t> samples) {
    if (!splitStereo) return writer.write(samples);
    std::vector<std::int16_t> left;
    std::vector<std::int16_t> right;
    left.reserve(samples.size() / 2);
    right.reserve(samples.size() / 2);
    for (std::size_t index = 0; index + 1 < samples.size(); index += 2) {
      left.push_back(samples[index]);
      right.push_back(samples[index + 1]);
    }
    return writer.write(left) && rightWriter.write(right);
  };

  auto openRecording = [&]() {
    if (writer.isOpen() || !options_.saveAudio) return writer.isOpen();
    currentEntry = {};
    currentEntry.path = recordingPath(options_, splitStereo ? L"_left" : L"");
    rightRecordingPath = splitStereo ? recordingPath(options_, L"_right")
                                     : std::filesystem::path{};
    currentEntry.started = std::chrono::system_clock::now();
    currentEntry.mode = options_.mode;
    currentEntry.frequencyMhz = options_.frequencyMhz;
    const WORD fileChannels = splitStereo ? 1 : recordChannels;
    if (!writer.open(currentEntry.path, sampleRate, fileChannels)) {
      setStatus(L"Could not create recording file: " + currentEntry.path.wstring());
      return false;
    }
    if (splitStereo && !rightWriter.open(rightRecordingPath, sampleRate, 1)) {
      writer.close();
      setStatus(L"Could not create recording file: " + rightRecordingPath.wstring());
      return false;
    }
    recordingFrames = 0;
    recordingLeftPeak = 0.0f;
    recordingRightPeak = 0.0f;
    return true;
  };

  auto beginRecording = [&]() {
    if (!openRecording()) return false;
    if (!preRoll.empty()) {
      std::vector<std::int16_t> buffered(preRoll.begin(), preRoll.end());
      if (!writeSamples(buffered)) {
        setStatus(L"Could not write recording audio");
        return false;
      }
      recordingFrames += buffered.size() / recordChannels;
      preRoll.clear();
    }
    recording_ = true;
    setStatus(L"Recording");
    return true;
  };

  auto pauseRecording = [&]() {
    recording_ = false;
    preRoll.clear();
    setStatus(L"Paused - waiting for audio");
  };

  auto finishRecording = [&]() {
    if (!writer.isOpen()) return;
    writer.close();
    if (rightWriter.isOpen()) rightWriter.close();
    currentEntry.durationSeconds = static_cast<double>(recordingFrames) / sampleRate;
    recording_ = false;
    setStatus(L"Listening");
    if (replayCallback_) {
      if (splitStereo) {
        currentEntry.mode = ChannelMode::Left;
        currentEntry.peakDb = peakToDb(recordingLeftPeak);
        replayCallback_(currentEntry);
        ReplayEntry rightEntry = currentEntry;
        rightEntry.path = rightRecordingPath;
        rightEntry.mode = ChannelMode::Right;
        rightEntry.peakDb = peakToDb(recordingRightPeak);
        replayCallback_(rightEntry);
      } else {
        currentEntry.peakDb = peakToDb(std::max(recordingLeftPeak, recordingRightPeak));
        replayCallback_(currentEntry);
      }
    }
    recordingFrames = 0;
    recordingLeftPeak = 0.0f;
    recordingRightPeak = 0.0f;
    preRoll.clear();
  };

  result = client->Start();
  if (FAILED(result)) {
    setStatus(hresultText(L"Audio capture start", result));
    CoTaskMemFree(mixFormat);
    if (uninitialize) CoUninitialize();
    return;
  }

  running_ = true;
  liveHub_.beginSession(sampleRate);
  setStatus(L"Input format: " + decoder.description());
  setStatus(monitorWarning.empty() ? L"Listening" : monitorWarning);
  if (options_.forceRecord && beginRecording()) voxGate.activate();

  while (!stopRequested_) {
    UINT32 packetFrames = 0;
    result = capture->GetNextPacketSize(&packetFrames);
    if (FAILED(result)) {
      setStatus(hresultText(L"Reading capture packet size", result));
      break;
    }
    if (packetFrames == 0) {
      Sleep(5);
      continue;
    }

    while (packetFrames > 0 && !stopRequested_) {
      BYTE* data = nullptr;
      UINT32 frameCount = 0;
      DWORD flags = 0;
      result = capture->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr);
      if (FAILED(result)) {
        setStatus(hresultText(L"Reading audio packet", result));
        stopRequested_ = true;
        break;
      }

      const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
      std::vector<std::int16_t> routed;
      std::vector<std::int16_t> monitorSamples;
      routed.reserve(static_cast<std::size_t>(frameCount) * recordChannels);
      monitorSamples.reserve(static_cast<std::size_t>(frameCount) * 2);
      float inputLeftPeak = 0.0f;
      float inputRightPeak = 0.0f;
      float outputLeftPeak = 0.0f;
      float outputRightPeak = 0.0f;

      for (UINT32 frame = 0; frame < frameCount; ++frame) {
        const float left = decoder.read(data, frame, 0, silent);
        const float right = inputChannels > 1 ? decoder.read(data, frame, 1, silent) : left;
        inputLeftPeak = std::max(inputLeftPeak, std::abs(left));
        inputRightPeak = std::max(inputRightPeak, std::abs(right));

        float outLeft = left;
        float outRight = right;
        switch (options_.mode) {
          case ChannelMode::Stereo:
            routed.push_back(floatToPcm16(left));
            routed.push_back(floatToPcm16(right));
            break;
          case ChannelMode::Left:
            outLeft = outRight = left;
            routed.push_back(floatToPcm16(left));
            break;
          case ChannelMode::Right:
            outLeft = outRight = right;
            routed.push_back(floatToPcm16(right));
            break;
          case ChannelMode::Mono:
            outLeft = outRight = (left + right) * 0.5f;
            routed.push_back(floatToPcm16(outLeft));
            break;
        }
        outputLeftPeak = std::max(outputLeftPeak, std::abs(outLeft));
        outputRightPeak = std::max(outputRightPeak, std::abs(outRight));
        monitorSamples.push_back(floatToPcm16(outLeft));
        monitorSamples.push_back(floatToPcm16(outRight));
      }

      inputLeftDb_ = peakToDb(inputLeftPeak);
      inputRightDb_ = peakToDb(inputRightPeak);
      outputLeftDb_ = peakToDb(outputLeftPeak);
      outputRightDb_ = peakToDb(outputRightPeak);
      const float packetPeak = std::max(outputLeftPeak, outputRightPeak);

      if (monitorEnabled) monitor.submit(monitorSamples);
      liveHub_.pushInterleaved(routed.data(), routed.size(), recordChannels);

      bool packetAlreadyBuffered = false;
      if (!voxGate.active()) {
        for (const auto sample : routed) preRoll.push_back(sample);
        while (preRoll.size() > preRollLimit) preRoll.pop_front();
        if (options_.saveAudio && !options_.forceRecord) {
          const VoxAction action = voxGate.update(packetPeak >= thresholdLinear, frameCount,
                                                  writer.isOpen(), options_.appendSession);
          if (action == VoxAction::Start || action == VoxAction::Resume) {
            packetAlreadyBuffered = beginRecording();
            if (!packetAlreadyBuffered) voxGate.reset();
          }
        }
      }

      if (voxGate.active()) {
        if (!packetAlreadyBuffered) {
          if (!writeSamples(routed)) {
            setStatus(L"Could not write recording audio");
            stopRequested_ = true;
          }
          recordingFrames += frameCount;
        }
        recordingLeftPeak = std::max(recordingLeftPeak, outputLeftPeak);
        recordingRightPeak = std::max(recordingRightPeak, outputRightPeak);
        if (!options_.forceRecord) {
          const VoxAction action = voxGate.update(packetPeak >= thresholdLinear, frameCount,
                                                  writer.isOpen(), options_.appendSession);
          if (action == VoxAction::Pause) pauseRecording();
          else if (action == VoxAction::Finish) finishRecording();
        }
      }

      capture->ReleaseBuffer(frameCount);
      result = capture->GetNextPacketSize(&packetFrames);
      if (FAILED(result)) {
        stopRequested_ = true;
        break;
      }
    }
  }

  finishRecording();
  voxGate.reset();
  client->Stop();
  monitor.close();
  liveHub_.endSession();
  running_ = false;
  recording_ = false;
  inputLeftDb_ = inputRightDb_ = outputLeftDb_ = outputRightDb_ = -90.0f;
  setStatus(L"Idle");
  CoTaskMemFree(mixFormat);
  if (uninitialize) CoUninitialize();
}
