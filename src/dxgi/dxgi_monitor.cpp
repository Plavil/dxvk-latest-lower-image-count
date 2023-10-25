#include "dxgi_monitor.h"

namespace dxvk {

  DxgiMonitorInfo::DxgiMonitorInfo(IUnknown* pParent, const DxgiOptions& options)
  : m_parent(pParent)
  , m_options(options)
  , m_globalColorSpace(DefaultColorSpace()) {

  }


  DxgiMonitorInfo::~DxgiMonitorInfo() {

  }


  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::AddRef() {
    return m_parent->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::Release() {
    return m_parent->Release();
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_parent->QueryInterface(riid, ppvObject);
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData) {
    if (!hMonitor || !pData)
      return E_INVALIDARG;
    
    std::lock_guard<dxvk::mutex> lock(m_monitorMutex);
    auto result = m_monitorData.insert({ hMonitor, *pData });

    return result.second ? S_OK : E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    InitReturnPtr(ppData);

    if (!hMonitor || !ppData)
      return E_INVALIDARG;
    
    m_monitorMutex.lock();

    auto entry = m_monitorData.find(hMonitor);
    if (entry == m_monitorData.end()) {
      m_monitorMutex.unlock();
      return DXGI_ERROR_NOT_FOUND;
    }

    *ppData = &entry->second;
    return S_OK;
  }


  void STDMETHODCALLTYPE DxgiMonitorInfo::ReleaseMonitorData() {
    m_monitorMutex.unlock();
  }


  void STDMETHODCALLTYPE DxgiMonitorInfo::PuntColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace) {
    // Only allow punting if we started from sRGB.
    // That way we can go from sRGB -> HDR10 or HDR10 -> sRGB if we started in sRGB.
    // But if we started off by advertising HDR10 to the game, don't allow us to go back.
    // This mirrors the behaviour of the global Windows HDR toggle more closely.
    if (DefaultColorSpace() != DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
      return;

    m_globalColorSpace = ColorSpace;
  }


  DXGI_COLOR_SPACE_TYPE STDMETHODCALLTYPE DxgiMonitorInfo::CurrentColorSpace() const {
    return m_globalColorSpace;
  }


  DXGI_COLOR_SPACE_TYPE DxgiMonitorInfo::DefaultColorSpace() const {
    return m_options.enableHDR
      ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
      : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  }


  uint32_t GetMonitorFormatBpp(DXGI_FORMAT Format) {
    switch (Format) {
      case DXGI_FORMAT_R1_UNORM:
        return 1;

      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_R8_UINT:
      case DXGI_FORMAT_R8_SNORM:
      case DXGI_FORMAT_R8_SINT:
      case DXGI_FORMAT_A8_UNORM:
        return 8;

      case DXGI_FORMAT_R8G8_UNORM:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R8G8_SNORM:
      case DXGI_FORMAT_R8G8_SINT:
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_R16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R16_SNORM:
      case DXGI_FORMAT_R16_SINT:
      case DXGI_FORMAT_B5G6R5_UNORM:
      case DXGI_FORMAT_B5G5R5A1_UNORM:
        return 16;

      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R11G11B10_FLOAT:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R8G8B8A8_SNORM:
      case DXGI_FORMAT_R8G8B8A8_SINT:
      case DXGI_FORMAT_R16G16_FLOAT:
      case DXGI_FORMAT_R16G16_UNORM:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R16G16_SNORM:
      case DXGI_FORMAT_R16G16_SINT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_UINT:
      case DXGI_FORMAT_R32_SINT:
      case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      case DXGI_FORMAT_R8G8_B8G8_UNORM:
      case DXGI_FORMAT_G8R8_G8B8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        return 32;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R16G16B16A16_SNORM:
      case DXGI_FORMAT_R16G16B16A16_SINT:
      case DXGI_FORMAT_R32G32_FLOAT:
      case DXGI_FORMAT_R32G32_UINT:
      case DXGI_FORMAT_R32G32_SINT:
        return 64;

      case DXGI_FORMAT_R32G32B32_FLOAT:
      case DXGI_FORMAT_R32G32B32_UINT:
      case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_UINT:
      case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;
      
      default:
        Logger::warn(str::format(
          "GetMonitorFormatBpp: Unknown format: ",
          Format));
        return 32;
    }
  }
  
}