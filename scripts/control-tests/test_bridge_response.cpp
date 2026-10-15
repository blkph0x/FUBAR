#include "../../src/sdr_town_bridge.h"
#include "../../third_party/nlohmann/json.hpp"
#include <windows.h>
#include <iostream>
#include <stdexcept>

using nlohmann::json;
int wmain(int argc, wchar_t** argv) {
  try {
    if (argc!=2 || !SetEnvironmentVariableW(L"SDRTOWN_CONTROL_DLL",argv[1]))throw std::runtime_error("fixture DLL path required");
    SdrTownBridgeConfig config; config.enabled=true;
    SdrTownBridge bridge;
    for (const auto* name : {"absent","nested","string","number","null","object","array","false",
                            "too-large","incomplete","invalid-json","expired","legacy-expired","valid"}) {
      std::string error;
      const auto response=json::parse(bridge.request(config,"GET",std::string("/v1/test/")+name,"{}",&error));
      const bool success=std::string(name)=="valid";
      if (response.at("ok")!=success || error.empty()!=success)throw std::runtime_error(std::string("invalid success contract: ")+name);
      if (std::string(name)=="too-large" && (response["code"]!="response_too_large" || response["errorCode"]!="response_too_large"))throw std::runtime_error("lost size errorCode");
      if (std::string(name)=="incomplete" && response["errorCode"]!="incomplete_response")throw std::runtime_error("lost incomplete errorCode");
      if (std::string(name)=="invalid-json" && response["errorCode"]!="invalid_json")throw std::runtime_error("lost JSON errorCode");
      if (std::string(name)=="expired" && response["code"]!="snapshot_expired")throw std::runtime_error("lost snapshot code");
      if (response.dump().size()>512)throw std::runtime_error("unbounded error");
    }
    std::cout<<"PASS: strict top-level boolean success despite legacy DLL result 0; recognized native errorCode/code preserved\n";
    return 0;
  } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
