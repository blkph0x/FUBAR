#include "sdr_public_boundary.h"
#include "../third_party/nlohmann/json.hpp"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <functional>
#include <set>

using nlohmann::json;
namespace {
bool privateKey(const std::string& key) {
  static const std::set<std::string> keys = {"outputDirectory","sstvSaveDir","recordDir",
    "recordPath","recordingDirectory","inputPath","outputPath","aptPreviewPath","sstvOutputDir","logDir","diagnosticLog",
    "token","password","authorization","centerLat","centerLon","radiusNm","latDegHome","lonDegHome"};
  return keys.count(key) != 0;
}
bool localPath(const std::string& s) {
  return s.find(":\\") != std::string::npos || (s.find(":/") != std::string::npos &&
      s.find("://") == std::string::npos) || s.rfind("\\\\",0) == 0;
}
}

bool SdrPublicBoundary::validateRequest(const std::string& text, std::string* error) {
  const auto fail = [&](const char* reason) { if(error)*error=reason; return false; };
  if(text.size()>8192) return fail("Command is too large");
  const auto j=json::parse(text,nullptr,false);
  if(!j.is_object()) return fail("JSON object required");
  if(j.contains("clientId") && !j["clientId"].is_string()) return fail("Invalid client ID");
  if(j.contains("deviceKey") && (!j["deviceKey"].is_string() || j["deviceKey"].get_ref<const std::string&>().size()>512))
    return fail("Invalid radio identity");
  std::function<bool(const json&,int)> safe=[&](const json& value,int depth){
    if(depth>12)return false;
    if(value.is_object()) for(auto it=value.begin();it!=value.end();++it) {
      if(privateKey(it.key()) || it.key()=="path" || it.key()=="observer" || it.key()=="latDeg" || it.key()=="lonDeg") return false;
      if((it.key()=="force" || it.key()=="stopP25") && it.value()!=false) return false;
      if(!safe(it.value(),depth+1)) return false;
    }
    else if(value.is_array()) for(const auto& v:value) if(!safe(v,depth+1))return false;
    return true;
  };
  return safe(j,0) || fail("Command contains a host-only setting");
}

std::string SdrPublicBoundary::response(const std::string& text) {
  auto j=json::parse(text,nullptr,false);
  if(j.is_discarded()) return R"({"ok":false,"error":"Invalid SDR Town response"})";
  std::lock_guard lock(mutex_);
  std::function<void(json&,bool)> redact=[&](json& value,bool imageList) {
    if(value.is_string() && localPath(value.get<std::string>())) { value="[host-local detail]"; return; }
    if(value.is_array()) { for(auto& v:value)redact(v,imageList); return; }
    if(!value.is_object()) return;
    if(imageList && value.contains("path") && value["path"].is_string()) {
      auto path=std::filesystem::u8path(value["path"].get<std::string>());
      std::error_code ec;
      auto canonical=std::filesystem::canonical(path,ec);
      auto ext=canonical.extension().string();
      std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(!ec && (ext==".png" || ext==".jpg" || ext==".jpeg" || ext==".webp") &&
          std::filesystem::is_regular_file(canonical,ec) && std::filesystem::file_size(canonical,ec)<=16*1024*1024 && !ec) {
        auto found=std::find_if(images_.begin(),images_.end(),[&](const auto& pair){return pair.second==canonical;});
        std::string id;
        if(found!=images_.end())id=found->first;
        else {
          GUID guid{};
          if(SUCCEEDED(CoCreateGuid(&guid))) {
            const auto* bytes=reinterpret_cast<const unsigned char*>(&guid);
            static constexpr char hex[]="0123456789abcdef";
            for(size_t i=0;i<sizeof(guid);++i){id+=hex[bytes[i]>>4];id+=hex[bytes[i]&15];}
            if(images_.size()>=256)images_.erase(images_.begin());
            images_[id]=canonical;
          }
        }
        if(!id.empty())value["imageId"]=id;
      }
    }
    for(auto it=value.begin();it!=value.end();) {
      if(privateKey(it.key()) || it.key()=="path") {it=value.erase(it);continue;}
      if(it.value().is_string() && localPath(it.value().get<std::string>())) it.value()="[host-local detail]";
      else redact(it.value(),it.key()=="images");
      ++it;
    }
  };
  redact(j,false);
  return j.dump();
}

std::filesystem::path SdrPublicBoundary::image(const std::string& id) const {
  std::lock_guard lock(mutex_);
  const auto it=images_.find(id);
  if(it==images_.end()) return {};
  std::error_code ec;
  const auto current=std::filesystem::canonical(it->second,ec);
  if(ec || current!=it->second || !std::filesystem::is_regular_file(current,ec) ||
      std::filesystem::file_size(current,ec)>16*1024*1024 || ec) return {};
  return current;
}
