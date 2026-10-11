#pragma once
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

// Only Town-reported decoder images, never user-supplied filesystem paths.
class SdrPublicBoundary {
 public:
  static bool validateRequest(const std::string& body, std::string* error);
  std::string response(const std::string& json);
  std::filesystem::path image(const std::string& id) const;
 private:
  mutable std::mutex mutex_;
  std::map<std::string, std::filesystem::path> images_;
};
