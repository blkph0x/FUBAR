#include "audio_engine.h"

#include "wav_writer.h"

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <propvarutil.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iomanip>
#include <memory>
#include <sstream>
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
  return peak <= 0.0000316228f ? -90.0f : std::clamp(20.0f * std::log10(peak), -90.0f, 0.0f);
}

std::int16_t floatToPcm16(float sample) {
  const float clipped = std::clamp(sample, -1.0f, 1.0f);
  return static_cast<std::int16_t>(std::lrint(clipped * 32767.0f));
}

float sampleAt(const BYTE* data, UINT32 frame, WORD channel, const WAVEFORMATEX* format,
               bool silent) {
  if (silent || data == nullptr) return 0.0f;
  const WORD channels = std::max<WORD>(format->nChannels, 1);
  const std::size_t index = static_cast<std::size_t>(frame) * channels + channel;
  bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
  bool isPcm = format->wFormatTag == WAVE_FORMAT_PCM;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    isFloat = extensible->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
    isPcm = extensible->SubFormat.Data1 == WAVE_FORMAT_PCM;
  }
  if (isFloat && format->wBitsPerSample == 32) {
    return reinterpret_cast<const float*>(data)[index];
  }
  if (isPcm && format->wBitsPerSample == 16) {
    return static_cast<float>(reinterpret_cast<const std::int16_t*>(data)[index]) / 32768.0f;
  }
  if (isPcm && format->wBitsPerSample == 32) {
    return static_cast<float>(reinterpret_cast<const std::int32_t*>(data)[index] / 2147483648.0);
  }
  if (isPcm && format->wBitsPerSample == 24) {
    const BYTE* sample = data + index * 3;
    std::int32_t value = static_cast<std::int32_t>(sample[0]) |
                         (static_cast<std::int32_t>(sample[1]) << 8) |
                         (static_cast<std::int32_t>(sample[2]) << 16);
    if (value & 0x00800000) value |= static_cast<std::int32_t>(0xff000000);
    return static_cast<float>(value / 8388608.0);
  }
  return 0.0f;
}

std::filesystem::path recordingPath(const AudioOptions& options) {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_s(&local, &time);
  std::wostringstream name;
  name << L"AudioVox_" << std::put_time(&local, L"%Y%m%d_%H%M%S") << L"_";
  switch (options.mode) {
    case ChannelMode::Stereo: name << L"stereo"; break;
    case ChannelMode::Left: name << L"left"; break;
    case ChannelMode::Right: name << L"right"; break;
    case ChannelMode::Mono: name << L"mono"; break;
  }
  name << L".wav";
  return options.outputDirectory / name.str();
}

void CALLBACK waveOutCallback(HWAVEOUT output, UINT message, DWORD_PTR, DWORD_PTR first,
                              DWORD_PTR) {
  if (message != WOM_DONE || first == 0) return;
  auto* header = reinterpret_cast<WAVEHDR*>(first);
  waveOutUnprepareHeader(output, header, sizeof(WAVEHDR));
  delete[] header->lpData;
  delete header;
}

class MonitorOutput {
 public:
  ~MonitorOutput() { close(); }

  bool open(DWORD sampleRate) {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;
    const MMRESULT result = waveOutOpen(&handle_, WAVE_MAPPER, &format,
                                        reinterpret_cast<DWORD_PTR>(waveOutCallback), 0,
                                        CALLBACK_FUNCTION);
    return result == MMSYSERR_NOERROR;
  }

  void submit(std::span<const std::int16_t> samples) {
    if (!handle_ || samples.empty()) return;
    auto* bytes = new char[samples.size_bytes()];
    std::memcpy(bytes, samples.data(), samples.size_bytes());
    auto* header = new WAVEHDR{};
    header->lpData = bytes;
    header->dwBufferLength = static_cast<DWORD>(samples.size_bytes());
    if (waveOutPrepareHeader(handle_, header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
        waveOutWrite(handle_, header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
      waveOutUnprepareHeader(handle_, header, sizeof(WAVEHDR));
      delete[] bytes;
      delete header;
    }
  }

  void close() {
    if (!handle_) return;
    waveOutReset(handle_);
    waveOutClose(handle_);
    handle_ = nullptr;
  }

 private:
  HWAVEOUT handle_ = nullptr;
};

}  // namespace

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
      devices.push_back({id, name});
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
  if (thread_) {
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
}

DWORD WINAPI AudioEngine::captureThreadEntry(LPVOID context) {
  static_cast<AudioEngine*>(context)->captureThread();
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
  const WORD inputChannels = std::max<WORD>(mixFormat->nChannels, 1);
  const WORD recordChannels = options_.mode == ChannelMode::Stereo ? 2 : 1;
  const std::size_t preRollLimit = static_cast<std::size_t>(
      std::max(0.0f, options_.preRollSeconds) * sampleRate * recordChannels);
  const std::uint64_t holdFrames = static_cast<std::uint64_t>(
      std::max(0.1f, options_.holdSeconds) * sampleRate);
  const float thresholdLinear = std::pow(10.0f, options_.thresholdDb / 20.0f);

  MonitorOutput monitor;
  if (options_.monitor && !monitor.open(sampleRate)) {
    setStatus(L"Listening started; live monitor unavailable on the selected output format");
  }

  std::deque<std::int16_t> preRoll;
  WavWriter writer;
  ReplayEntry currentEntry;
  std::uint64_t quietFrames = 0;
  std::uint64_t recordingFrames = 0;
  float recordingPeak = 0.0f;

  auto beginRecording = [&]() {
    if (writer.isOpen() || !options_.saveAudio) return;
    currentEntry = {};
    currentEntry.path = recordingPath(options_);
    currentEntry.started = std::chrono::system_clock::now();
    currentEntry.mode = options_.mode;
    currentEntry.frequencyMhz = options_.frequencyMhz;
    if (!writer.open(currentEntry.path, sampleRate, recordChannels)) {
      setStatus(L"Could not create recording file: " + currentEntry.path.wstring());
      return;
    }
    if (!preRoll.empty()) {
      std::vector<std::int16_t> buffered(preRoll.begin(), preRoll.end());
      writer.write(buffered);
      recordingFrames = buffered.size() / recordChannels;
    } else {
      recordingFrames = 0;
    }
    recordingPeak = 0.0f;
    quietFrames = 0;
    recording_ = true;
    setStatus(L"Recording");
  };

  auto finishRecording = [&]() {
    if (!writer.isOpen()) return;
    writer.close();
    currentEntry.durationSeconds = static_cast<double>(recordingFrames) / sampleRate;
    currentEntry.peakDb = peakToDb(recordingPeak);
    recording_ = false;
    setStatus(L"Listening");
    if (replayCallback_) replayCallback_(currentEntry);
    recordingFrames = 0;
    recordingPeak = 0.0f;
    quietFrames = 0;
  };

  result = client->Start();
  if (FAILED(result)) {
    setStatus(hresultText(L"Audio capture start", result));
    CoTaskMemFree(mixFormat);
    if (uninitialize) CoUninitialize();
    return;
  }

  running_ = true;
  setStatus(L"Listening");
  if (options_.forceRecord) beginRecording();

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
        const float left = sampleAt(data, frame, 0, mixFormat, silent);
        const float right = inputChannels > 1 ? sampleAt(data, frame, 1, mixFormat, silent) : left;
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

      if (options_.monitor) monitor.submit(monitorSamples);

      bool packetAlreadyBuffered = false;
      if (!writer.isOpen()) {
        for (const auto sample : routed) preRoll.push_back(sample);
        while (preRoll.size() > preRollLimit) preRoll.pop_front();
        if (options_.saveAudio && (options_.forceRecord || packetPeak >= thresholdLinear)) {
          beginRecording();
          packetAlreadyBuffered = writer.isOpen();
        }
      }

      if (writer.isOpen()) {
        if (!packetAlreadyBuffered) {
          writer.write(routed);
          recordingFrames += frameCount;
        }
        recordingPeak = std::max(recordingPeak, packetPeak);
        if (!options_.forceRecord) {
          if (packetPeak >= thresholdLinear) quietFrames = 0;
          else quietFrames += frameCount;
          if (quietFrames >= holdFrames) finishRecording();
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
  client->Stop();
  monitor.close();
  running_ = false;
  recording_ = false;
  inputLeftDb_ = inputRightDb_ = outputLeftDb_ = outputRightDb_ = -90.0f;
  setStatus(L"Idle");
  CoTaskMemFree(mixFormat);
  if (uninitialize) CoUninitialize();
}
