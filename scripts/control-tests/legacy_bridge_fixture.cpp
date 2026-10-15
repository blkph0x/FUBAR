#include <algorithm>
#include <cstring>
#include <string>

// Simulate the legacy ABI claiming success on an embedded "ok":true substring.
extern "C" __declspec(dllexport) int SdrTownControl_Request(
    const void*, const char*, const char* path, const char*, char* output, std::size_t size) {
  const std::string name(path);
  const char* value = R"({"ok":true})";
  if (name.ends_with("/absent")) value=R"({"state":{}})";
  else if (name.ends_with("/nested")) value=R"({"state":{"ok":true}})";
  else if (name.ends_with("/string")) value=R"({"ok":"true"})";
  else if (name.ends_with("/number")) value=R"({"ok":1})";
  else if (name.ends_with("/null")) value=R"({"ok":null})";
  else if (name.ends_with("/object")) value=R"({"ok":{"ok":true}})";
  else if (name.ends_with("/array")) value=R"({"ok":[true]})";
  else if (name.ends_with("/false")) value=R"({"ok":false,"state":{"ok":true}})";
  else if (name.ends_with("/too-large")) value=R"({"ok":false,"errorCode":"response_too_large","error":"Too large"})";
  else if (name.ends_with("/incomplete")) value=R"({"ok":false,"errorCode":"incomplete_response","error":"Incomplete"})";
  else if (name.ends_with("/invalid-json")) value=R"({"ok":false,"errorCode":"invalid_json","error":"Invalid JSON"})";
  else if (name.ends_with("/expired")) value=R"({"ok":false,"code":"snapshot_expired","error":"Snapshot expired"})";
  else if (name.ends_with("/legacy-expired")) value=R"({"ok":false,"status":409,"error":"Snapshot expired"})";
  if (size) { const auto count=std::min(std::strlen(value),size-1);std::memcpy(output,value,count);output[count]='\0'; }
  return 0;
}
extern "C" __declspec(dllexport) int SdrTownControl_Health(const void*, char*, std::size_t) { return 0; }
extern "C" __declspec(dllexport) int SdrTownControl_Status(const void*, char*, std::size_t) { return 0; }
extern "C" __declspec(dllexport) int SdrTownControl_Tune(const void*, const void*, char*, std::size_t) { return 0; }
extern "C" __declspec(dllexport) int SdrTownControl_SetRfGain(const void*, double, char*, std::size_t) { return 0; }
extern "C" __declspec(dllexport) int SdrTownControl_StartP25Control(const void*, double, int, char*, std::size_t) { return 0; }
