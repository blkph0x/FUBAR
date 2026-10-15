#include "../../src/web_server.h"
#include "../../third_party/nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using nlohmann::json;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

struct ControlAdmissionTest {
  static json action(CaptureWebServer& server, const char* name, const std::string& ticket,
                     std::uint64_t now, int* status = nullptr) {
    return json::parse(server.sdrTownControlSessionActionJson(json{{"action",name},
      {"admissionTicket",ticket},{"name","Test operator"}}.dump(), status, now));
  }
  static void run() {
    SdrTownBridgeConfig config; config.enabled = true; config.allowTune = true;
    CaptureWebServer server; server.setSdrTownControlConfig(config);
    auto owner = action(server,"take","",1000);
    const std::string ticket = owner["admissionTicket"];
    require(ticket.size()==32 && owner["session"]["canControl"]==true,"server ticket owner");
    std::vector<std::string> waiting;
    for(int i=0;i<128;++i) {
      int status = 0;
      auto result=json::parse(server.sdrTownControlSessionActionJson(json{{"action","take"},
        {"clientId","caller-"+std::to_string(i)}}.dump(),&status,1000));
      if(result["ok"]==true)waiting.push_back(result["admissionTicket"]);
      else require(status==429,"flood returns 429");
      require(result.dump().size()<1800,"bounded status");
      require(result["session"]["queue"].size()<=8,"bounded queue display");
      require(result.dump().find(ticket)==std::string::npos,"owner ticket privacy");
    }
    require(waiting.size()==32 && server.sdrControlQueue_.size()==32,"queue cap 32");
    auto duplicate=action(server,"take",waiting[0],1001);
    require(duplicate["ok"]==true && server.sdrControlQueue_.size()==32,"duplicate ticket idempotence");
    auto active=action(server,"status",ticket,1001);
    require(active["session"]["canControl"]==true,"anonymous flood must not consume active budget");
    action(server,"status",waiting[0],90000);
    action(server,"status",ticket,92000);
    require(server.sdrControlQueue_.size()==1,"stale queue pruned while owner active");
    require(action(server,"extend",ticket,92001)["ok"]==true,"first extension");
    require(action(server,"extend",ticket,92002)["ok"]==true,"second extension");
    require(action(server,"extend",ticket,92003)["ok"]==false,"third extension rejected");
    const auto deadline=server.sdrControlLeaseUntilTick_;
    require(deadline==661000,"lease does not restart when extended");
    for(std::uint64_t now=150000;now<deadline;now+=60000)action(server,"status",waiting[0],now);
    auto expired=action(server,"status",ticket,deadline);
    require(expired["code"]=="invalid_ticket","expired ticket invalidated");
    require(action(server,"status",waiting[0],deadline)["session"]["canControl"]==true,"FIFO promotion");
    require(server.sdrControlExtensionsUsed_==0,"new lease extension count reset");
    action(server,"release",waiting[0],deadline+1);
    require(server.sdrControlActiveClient_.empty(),"owner release");
    require(action(server,"take",ticket,deadline+2)["ok"]==false,"old ticket cannot regain authority");

    CaptureWebServer concurrent; concurrent.setSdrTownControlConfig(config);
    std::vector<std::thread> threads;
    for(int i=0;i<80;++i)threads.emplace_back([&]{action(concurrent,"take","",1000);});
    for(auto& thread:threads)thread.join();
    require(concurrent.sdrControlQueue_.size()==32 && !concurrent.sdrControlActiveClient_.empty(),"concurrent bounded admission");
    int status=0;
    for(int i=0;i<20;++i)action(concurrent,"status",concurrent.sdrControlActiveClient_,1001,&status);
    require(status==429,"per-ticket request budget");

    CaptureWebServer churn; churn.setSdrTownControlConfig(config);
    for(int i=0;i<33;++i) {
      const std::string issued=action(churn,"take","",1000)["admissionTicket"];
      action(churn,"release",issued,1000);
    }
    require(action(churn,"take","",1000,&status)["code"]=="admission_limited" && status==429,"ticket churn admission budget");
    require(action(churn,"take","",2000)["ok"]==true,"admission budget refill");

    CaptureWebServer auth; auth.setSdrTownControlConfig(config);
    const auto now=GetTickCount64();
    const std::string authTicket=action(auth,"take","",now)["admissionTicket"];
    std::string error;
    require(auth.sdrTownControlCommandAllowed(json{{"admissionTicket",authTicket}}.dump(),&error,"tune"),"active owner command");
    require(!auth.sdrTownControlCommandAllowed(json{{"clientId",authTicket}}.dump(),&error,"tune"),"caller id is not authority");
    require(!auth.sdrTownControlCommandAllowed(json{{"admissionTicket",std::string(32,'a')}}.dump(),&error,"tune"),"forged ticket denied");
    for(int i=0;i<20;++i)auth.sdrTownControlCommandAllowed(json{{"admissionTicket",authTicket}}.dump(),&error,"tune");
    require(json::parse(error)["code"]=="rate_limited","command request budget");
    config.allowTune=false; auth.setSdrTownControlConfig(config);
    require(!auth.sdrTownControlCommandAllowed(json{{"admissionTicket",authTicket}}.dump(),&error,"tune"),"admin capability gate");
    config.enabled=false; auth.setSdrTownControlConfig(config);
    require(auth.sdrControlActiveClient_.empty() && auth.sdrControlQueue_.empty(),"disable revokes admission");
    CaptureWebServer restarted; config.enabled=true; restarted.setSdrTownControlConfig(config);
    require(action(restarted,"status",authTicket,now)["code"]=="invalid_ticket","restart invalidates tickets");
    std::cout << "PASS: 128 IDs, concurrent 80 admissions, bounds, rates, expiry, two extensions, promotion, auth, privacy, restart\n";
  }
};
int main() {
  try { ControlAdmissionTest::run(); return 0; }
  catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
