#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {

  static const Sha1Hash       g_nullHash      = Sha1Hash::compute(nullptr, 0);
  static const DxvkShaderKey  g_nullShaderKey = DxvkShaderKey();

  bool DxvkStateCacheKey::eq(const DxvkStateCacheKey& key) const {
    return this->vs.eq(key.vs)
        && this->tcs.eq(key.tcs)
        && this->tes.eq(key.tes)
        && this->gs.eq(key.gs)
        && this->fs.eq(key.fs)
        && this->cs.eq(key.cs);
  }


  size_t DxvkStateCacheKey::hash() const {
    DxvkHashState hash;
    hash.add(this->vs.hash());
    hash.add(this->tcs.hash());
    hash.add(this->tes.hash());
    hash.add(this->gs.hash());
    hash.add(this->fs.hash());
    hash.add(this->cs.hash());
    return hash;
  }


  DxvkStateCache::DxvkStateCache(
          DxvkPipelineManager*  pipeManager,
          DxvkRenderPassPool*   passManager)
  : m_pipeManager(pipeManager),
    m_passManager(passManager) {
    bool newFile = !readCacheFile();

    // Open cache file for writing
    std::ios_base::openmode mode = std::ios_base::binary;

    mode |= newFile
      ? std::ios_base::trunc
      : std::ios_base::app;
    
    m_writerFile = std::ofstream(getCacheFileName(), mode);

    if (!m_writerFile) {
      // We can't write to the file, but we might still
      // use cache entries previously read from the file
      Logger::warn("DXVK: Failed to open state cache file");
    } else if (newFile) {
      Logger::warn("DXVK: Creating new state cache file");

      // Write header with the current version number
      DxvkStateCacheHeader header;

      auto data = reinterpret_cast<const char*>(&header);
      auto size = sizeof(header);

      m_writerFile.write(data, size);

      // Write all valid entries to the cache file in
      // case we're recovering a corrupted cache file
      for (auto& e : m_entries)
        writeCacheEntry(m_writerFile, e);

      m_writerFile.flush();
    }

    // Use half the available CPU cores for pipeline compilation
    uint32_t numCpuCores = dxvk::thread::hardware_concurrency();
    uint32_t numWorkers  = numCpuCores > 8
      ? numCpuCores * 3 / 4
      : numCpuCores * 1 / 2;

    if (numWorkers <  1) numWorkers =  1;
    if (numWorkers > 16) numWorkers = 16;
    
    Logger::info(str::format("DXVK: Using ", numWorkers, " compiler threads"));
    
    // Start the worker threads and the file writer
    for (uint32_t i = 0; i < numWorkers; ++i)
      m_workerThreads.emplace_back([this] () { workerFunc(); });
    
    m_writerThread = dxvk::thread([this] () { writerFunc(); });
  }
  

  DxvkStateCache::~DxvkStateCache() {
    { std::lock_guard<std::mutex> workerLock(m_workerLock);
      std::lock_guard<std::mutex> writerLock(m_writerLock);

      m_stopThreads.store(true);

      m_workerCond.notify_all();
      m_writerCond.notify_all();
    }

    for (auto& worker : m_workerThreads)
      worker.join();
    
    m_writerThread.join();
  }


  void DxvkStateCache::addGraphicsPipeline(
    const DxvkStateCacheKey&              shaders,
    const DxvkGraphicsPipelineStateInfo&  state,
    const DxvkRenderPassFormat&           format) {
    if (shaders.vs.eq(g_nullShaderKey))
      return;
    
    // Do not add an entry that is already in the cache
    auto entries = m_entryMap.equal_range(shaders);

    for (auto e = entries.first; e != entries.second; ++e) {
      const DxvkStateCacheEntry& entry = m_entries[e->second];

      if (entry.format.matches(format) && entry.gpState == state)
        return;
    }

    // Queue a job to write this pipeline to the cache
    std::unique_lock<std::mutex> lock(m_writerLock);

    m_writerQueue.push({ shaders, state,
      DxvkComputePipelineStateInfo(),
      format, g_nullHash });
    m_writerCond.notify_one();
  }


  void DxvkStateCache::addComputePipeline(
    const DxvkStateCacheKey&              shaders,
    const DxvkComputePipelineStateInfo&   state) {
    if (shaders.cs.eq(g_nullShaderKey))
      return;

    // Do not add an entry that is already in the cache
    auto entries = m_entryMap.equal_range(shaders);

    for (auto e = entries.first; e != entries.second; ++e) {
      if (m_entries[e->second].cpState == state)
        return;
    }

    // Queue a job to write this pipeline to the cache
    std::unique_lock<std::mutex> lock(m_writerLock);

    m_writerQueue.push({ shaders,
      DxvkGraphicsPipelineStateInfo(), state,
      DxvkRenderPassFormat(), g_nullHash });
    m_writerCond.notify_one();
  }


  void DxvkStateCache::registerShader(const Rc<DxvkShader>& shader) {
    DxvkShaderKey key = shader->getShaderKey();

    if (key.eq(g_nullShaderKey))
      return;
    
    // Add the shader so we can look it up by its key
    std::unique_lock<std::mutex> entryLock(m_entryLock);
    m_shaderMap.insert({ key, shader });

    // Deferred lock, don't stall workers unless we have to
    std::unique_lock<std::mutex> workerLock;

    auto pipelines = m_pipelineMap.equal_range(key);

    for (auto p = pipelines.first; p != pipelines.second; ++p) {
      WorkerItem item;

      if (!getShaderByKey(p->second.vs,  item.vs)
       || !getShaderByKey(p->second.tcs, item.tcs)
       || !getShaderByKey(p->second.tes, item.tes)
       || !getShaderByKey(p->second.gs,  item.gs)
       || !getShaderByKey(p->second.fs,  item.fs)
       || !getShaderByKey(p->second.cs,  item.cs))
        continue;
      
      if (!workerLock)
        workerLock = std::unique_lock<std::mutex>(m_workerLock);
      
      m_workerQueue.push(item);
    }

    if (workerLock)
      m_workerCond.notify_all();
  }


  DxvkShaderKey DxvkStateCache::getShaderKey(const Rc<DxvkShader>& shader) const {
    return shader != nullptr ? shader->getShaderKey() : g_nullShaderKey;
  }


  bool DxvkStateCache::getShaderByKey(
    const DxvkShaderKey&            key,
          Rc<DxvkShader>&           shader) const {
    if (key.eq(g_nullShaderKey))
      return true;
    
    auto entry = m_shaderMap.find(key);
    if (entry == m_shaderMap.end())
      return false;

    shader = entry->second;
    return true;
  }


  void DxvkStateCache::mapPipelineToEntry(
    const DxvkStateCacheKey&        key,
          size_t                    entryId) {
    m_entryMap.insert({ key, entryId });
  }

  
  void DxvkStateCache::mapShaderToPipeline(
    const DxvkShaderKey&            shader,
    const DxvkStateCacheKey&        key) {
    if (!shader.eq(g_nullShaderKey))
      m_pipelineMap.insert({ shader, key });
  }


  void DxvkStateCache::compilePipelines(const WorkerItem& item) {
    DxvkStateCacheKey key;
    key.vs  = getShaderKey(item.vs);
    key.tcs = getShaderKey(item.tcs);
    key.tes = getShaderKey(item.tes);
    key.gs  = getShaderKey(item.gs);
    key.fs  = getShaderKey(item.fs);
    key.cs  = getShaderKey(item.cs);

    if (item.cs == nullptr) {
      auto pipeline = m_pipeManager->createGraphicsPipeline(
        item.vs, item.tcs, item.tes, item.gs, item.fs);
      auto entries = m_entryMap.equal_range(key);

      for (auto e = entries.first; e != entries.second; ++e) {
        const auto& entry = m_entries[e->second];

        auto rp = m_passManager->getRenderPass(entry.format);
        pipeline->getPipelineHandle(entry.gpState, *rp);
      }
    } else {
      auto pipeline = m_pipeManager->createComputePipeline(item.cs);
      auto entries = m_entryMap.equal_range(key);

      for (auto e = entries.first; e != entries.second; ++e) {
        const auto& entry = m_entries[e->second];
        pipeline->getPipelineHandle(entry.cpState);
      }
    }
  }


  bool DxvkStateCache::readCacheFile() {
    // Open state file and just fail if it doesn't exist
    std::ifstream ifile(getCacheFileName(), std::ios_base::binary);

    if (!ifile) {
      Logger::warn("DXVK: No state cache file found");
      return false;
    }

    // The header stores the state cache version,
    // we need to regenerate it if it's outdated
    if (!readCacheHeader(ifile)) {
      Logger::warn("DXVK: State cache out of date");
      return false;
    }

    // Read actual cache entries from the file.
    // If we encounter invalid entries, we should
    // regenerate the entire state cache file.
    uint32_t numInvalidEntries = 0;

    while (ifile) {
      DxvkStateCacheEntry entry;

      if (readCacheEntry(ifile, entry)) {
        size_t entryId = m_entries.size();
        m_entries.push_back(entry);

        mapPipelineToEntry(entry.shaders, entryId);

        mapShaderToPipeline(entry.shaders.vs,  entry.shaders);
        mapShaderToPipeline(entry.shaders.tcs, entry.shaders);
        mapShaderToPipeline(entry.shaders.tes, entry.shaders);
        mapShaderToPipeline(entry.shaders.gs,  entry.shaders);
        mapShaderToPipeline(entry.shaders.fs,  entry.shaders);
        mapShaderToPipeline(entry.shaders.cs,  entry.shaders);
      } else if (ifile) {
        numInvalidEntries += 1;
      }
    }

    Logger::info(str::format(
      "DXVK: Read ", m_entries.size(),
      " valid state cache entries"));

    if (numInvalidEntries) {
      Logger::warn(str::format(
        "DXVK: Skipped ", numInvalidEntries,
        " invalid state cache entries"));
    }
    
    return !numInvalidEntries;
  }


  bool DxvkStateCache::readCacheHeader(
          std::istream&             stream) const {
    DxvkStateCacheHeader expected;
    DxvkStateCacheHeader actual;

    auto data = reinterpret_cast<char*>(&actual);
    auto size = sizeof(actual);

    if (!stream.read(data, size))
      return false;
    
    for (uint32_t i = 0; i < 4; ++i) {
      if (expected.magic[i] != actual.magic[i])
        return false;
    }
    
    return expected.version   == actual.version
        && expected.entrySize == actual.entrySize;
  }


  bool DxvkStateCache::readCacheEntry(
          std::istream&             stream, 
          DxvkStateCacheEntry&      entry) const {
    auto data = reinterpret_cast<char*>(&entry);
    auto size = sizeof(DxvkStateCacheEntry);

    if (!stream.read(data, size))
      return false;
    
    Sha1Hash expectedHash = std::exchange(entry.hash, g_nullHash);
    Sha1Hash computedHash = Sha1Hash::compute(entry);
    return expectedHash == computedHash;
  }


  void DxvkStateCache::writeCacheEntry(
          std::ostream&             stream, 
          DxvkStateCacheEntry&      entry) const {
    entry.hash = Sha1Hash::compute(entry);

    auto data = reinterpret_cast<const char*>(&entry);
    auto size = sizeof(DxvkStateCacheEntry);

    stream.write(data, size);
    stream.flush();
  }


  void DxvkStateCache::workerFunc() {
    env::setThreadName(L"dxvk-shader");

    while (!m_stopThreads.load()) {
      WorkerItem item;

      { std::unique_lock<std::mutex> lock(m_workerLock);

        m_workerCond.wait(lock, [this] () {
          return m_workerQueue.size()
              || m_stopThreads.load();
        });

        if (m_workerQueue.size() == 0)
          break;
        
        item = m_workerQueue.front();
        m_workerQueue.pop();
      }

      compilePipelines(item);
    }
  }


  void DxvkStateCache::writerFunc() {
    env::setThreadName(L"dxvk-writer");

    while (!m_stopThreads.load()) {
      DxvkStateCacheEntry entry;

      { std::unique_lock<std::mutex> lock(m_writerLock);

        m_writerCond.wait(lock, [this] () {
          return m_writerQueue.size()
              || m_stopThreads.load();
        });

        if (m_writerQueue.size() == 0)
          break;

        entry = m_writerQueue.front();
        m_writerQueue.pop();
      }

      writeCacheEntry(m_writerFile, entry);
    }
  }


  std::string DxvkStateCache::getCacheFileName() const {
    std::string path = env::getEnvVar(L"DXVK_STATE_CACHE_PATH");

    if (!path.empty() && *path.rbegin() != '/')
      path += '/';
    
    std::string exeName = env::getExeName();
    auto extp = exeName.find_last_of('.');
    
    if (extp != std::string::npos && exeName.substr(extp + 1) == "exe")
      exeName.erase(extp);
    
    path += exeName + ".dxvk-cache";
    return path;
  }

}
