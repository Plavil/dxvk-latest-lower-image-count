#include "log.h"

#include "../util_env.h"

#include <ctime>

namespace dxvk {
  
  Logger::Logger(const std::string& file_name)
  : m_minLevel(getMinLogLevel())
  {
    std::string path = env::getEnvVar(L"DXVK_LOG_PATH");
    std::string file = path;
    if (!file.empty() && *file.rbegin() != '/')
      file += '/';
    file += env::getExeName() + "_";
    if (!env::getEnvVar(L"DXVK_LOG_TIMESTAMP").empty()) {
      std::time_t t = std::time(nullptr);
      char mbstr[sizeof("2018-01-01_00.00.00")];
      if (std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d_%H.%M.%S", std::localtime(&t))) {
        file += std::string(mbstr) + "_";
      }
    }
      
    m_fileStream = std::ofstream(file + file_name);
  }
  
  
  Logger::~Logger() { }
  
  
  void Logger::trace(const std::string& message) {
    s_instance.log(LogLevel::Trace, message);
  }
  
  
  void Logger::debug(const std::string& message) {
    s_instance.log(LogLevel::Debug, message);
  }
  
  
  void Logger::info(const std::string& message) {
    s_instance.log(LogLevel::Info, message);
  }
  
  
  void Logger::warn(const std::string& message) {
    s_instance.log(LogLevel::Warn, message);
  }
  
  
  void Logger::err(const std::string& message) {
    s_instance.log(LogLevel::Error, message);
  }
  
  
  void Logger::log(LogLevel level, const std::string& message) {
    if (level >= m_minLevel) {
      std::lock_guard<std::mutex> lock(m_mutex);
      
      static std::array<const char*, 5> s_prefixes
        = {{ "trace: ", "debug: ", "info:  ", "warn:  ", "err:   " }};
      
      const char* prefix = s_prefixes.at(static_cast<uint32_t>(level));
      std::cerr    << prefix << message << std::endl;
      m_fileStream << prefix << message << std::endl;
      m_fileStream.flush();
    }
  }
  
  
  LogLevel Logger::getMinLogLevel() {
    const std::array<std::pair<const char*, LogLevel>, 5> logLevels = {{
      { "trace", LogLevel::Trace },
      { "debug", LogLevel::Debug },
      { "info",  LogLevel::Info  },
      { "warn",  LogLevel::Warn  },
      { "error", LogLevel::Error },
    }};
    
    const std::string logLevelStr = env::getEnvVar(L"DXVK_LOG_LEVEL");
    
    for (const auto& pair : logLevels) {
      if (logLevelStr == pair.first)
        return pair.second;
    }
    
    return LogLevel::Info;
  }
  
}
