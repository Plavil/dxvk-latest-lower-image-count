#include "d3d9_core.h"

#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_format.h"

#define CHECK_ADAPTER(adapter) { if (!ValidAdapter(adapter)) { return D3DERR_INVALIDCALL; } }
#define CHECK_DEV_TYPE(ty) { if (ty != D3DDEVTYPE_HAL) { return D3DERR_INVALIDDEVICE; } }

namespace dxvk {
  Direct3D9::Direct3D9() {
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&m_factory))))
      throw DxvkError("Failed to create DXGI factory");

    UINT i = 0;
    IDXGIAdapter* adapter;
    while (m_factory->EnumAdapters(i++, &adapter) != DXGI_ERROR_NOT_FOUND) {
      m_adapters.emplace_back(Com(adapter));
    }
  }

  Direct3D9::~Direct3D9() = default;

  bool Direct3D9::ValidAdapter(UINT adapter) {
    return adapter < m_adapters.size();
  }

  D3D9Adapter& Direct3D9::GetAdapter(UINT adapter) {
    return m_adapters[adapter];
  }

  HRESULT Direct3D9::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3D9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT Direct3D9::RegisterSoftwareDevice(void*) {
    // Applications would call this if there aren't any GPUs available
    // and want to fall back to software rasterization.
    Logger::warn("Ignoring RegisterSoftwareDevice: software rasterizers are not supported");

    // Since we know we always have at least one Vulkan GPU,
    // we simply fake success.
    return D3D_OK;
  }

  // Returns the number of GPUs on the system.
  UINT Direct3D9::GetAdapterCount() {
    return m_adapters.size();
  }

  // Returns a description of the GPU.
  HRESULT Direct3D9::GetAdapterIdentifier(UINT Adapter,
    DWORD, D3DADAPTER_IDENTIFIER9* pIdentifier) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pIdentifier);

    // Note: we ignore the second parameter, Flags, since
    // checking if the driver is WHQL'd is irrelevant to Wine.

    auto& ident = *pIdentifier;

    return GetAdapter(Adapter).GetIdentifier(ident);
  }

  UINT Direct3D9::GetAdapterModeCount(UINT Adapter, D3DFORMAT) {
    if (!ValidAdapter(Adapter))
      return 0;

    return GetAdapter(Adapter).GetModeCount();
  }

  HRESULT Direct3D9::EnumAdapterModes(UINT Adapter,
    D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pMode);

    auto& mode = *pMode;

    mode.Format = Format;
    GetAdapter(Adapter).GetMode(Mode, mode);

    return S_OK;
  }

  HRESULT Direct3D9::GetAdapterDisplayMode(UINT Adapter,
    D3DDISPLAYMODE* pMode) {
    CHECK_ADAPTER(Adapter);
    CHECK_NOT_NULL(pMode);

    auto& mode = *pMode;

    // We don't really known nor care what the real screen format is,
    // since modern GPUs can handle render targets in another format.
    // WineD3D does something similar.
    mode.Format = D3DFMT_X8R8G8B8;

    // Fill in the current width / height.
    // TODO: this returns the maximum / native monitor resolution,
    // but not the current one. We should fix this.
    GetAdapter(Adapter).GetMode(0, mode);

    return D3D_OK;
  }

  HRESULT Direct3D9::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
    BOOL bWindowed) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // We don't do any checks here, since modern GPUs support pretty much
    // all the D3D9 formats. If that is not the case, we will fail later.

    // Note: Vulkan doesn't care if the app is windowed or not.

    return D3D_OK;
  }

  HRESULT Direct3D9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, DWORD Usage,
    D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // In principle, on Vulkan / D3D11 hardware (modern GPUs),
    // all of the formats and features should be supported.
    return D3D_OK;
  }

  // This function is called by the app to determine if a certain image format
  // can be used with multisampling.
  HRESULT Direct3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT SurfaceFormat, BOOL,
    D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    if (pQualityLevels) {
      // Vulkan doesn't have quality levels: we either enable AA, or don't.
      *pQualityLevels = 1;
    }

    if (MultiSampleType > 16)
      return D3DERR_INVALIDCALL;

    // D3D11-level hardware guarantees at least 8x multisampling for the formats we're interested in.
    // Only thing we need to check is that the MS count is a power-of-two.

    switch (MultiSampleType) {
    case 1:
    case 2:
    case 4:
    case 8:
      return D3D_OK;
    default:
      return D3DERR_NOTAVAILABLE;
    }
  }

  HRESULT Direct3D9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat,
    D3DFORMAT DepthStencilFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    // We don't check anything here, since modern hardware supports
    // pretty much every depth-stencil format combined with any RT format.

    return S_OK;
  }

  HRESULT Direct3D9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DevType,
    D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);

    Logger::err("CheckDeviceFormatConversion");
    throw DxvkError("not supported");
  }

  HRESULT Direct3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DevType,
    D3DCAPS9* pCaps) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);
    CHECK_NOT_NULL(pCaps);

    auto& caps = *pCaps;

    FillCaps(Adapter, caps);

    return D3D_OK;
  }

  HMONITOR Direct3D9::GetAdapterMonitor(UINT Adapter) {
    if (!ValidAdapter(Adapter))
      return nullptr;

    return GetAdapter(Adapter).GetMonitor();
  }

  HRESULT Direct3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DevType,
    HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresParams,
    IDirect3DDevice9** pReturnDevice) {
    CHECK_ADAPTER(Adapter);
    CHECK_DEV_TYPE(DevType);
    CHECK_NOT_NULL(pPresParams);
    InitReturnPtr(pReturnDevice);
    CHECK_NOT_NULL(pReturnDevice);

    // This is actually an array, if we were to support multi-GPU adapters.
    auto& pp = pPresParams[0];
    auto& device = *pReturnDevice;
    auto adapter = GetAdapter(Adapter);

    // First we check the flags.

    // TODO: No support for multi-GPU yet.
    if (BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) {
      Logger::err("Multi-GPU configurations not yet supported");
      return D3DERR_INVALIDCALL;
    }

    // TODO: support multithreaded API usage. Since D3D11 is mostly thread-safe we should be OK,
    // but the docs aren't very explicit as to what thread safe means.
    // They just say that D3D9 will "lock some global mutex more often" if the flag is set.
    if (BehaviorFlags & D3DCREATE_MULTITHREADED) {
      Logger::warn("D3D9 is not yet thread-safe");
    }

    // Ignored flags:
    // - DISABLE_PRINTSCREEN: not relevant to us.
    // - PSGP_THREADING: we multithread as we see fit.
    // - FPU_PRESERVE: on modern CPUs we needn't mess with the FPU settings.
    // - *_VERTEXPROCESSING: we always just use hardware acceleration.
    // - NOWINDOWCHANGES: we don't do anything with the focus window anyway.
    // - SCREENSAVER: not applicable.
    // - PUREDEVICE: disables emulation for vertex processing, but we didn't support emulation anyway.
    //   Would also disable some getters, but we don't really care.
    // - DISABLE_DRIVER_MANAGEMENT: we just allow DXVK to handle resources.

    // TODO: support D3D9Ex flags like PRESENTSTATS and such.

    // Now to do some checking of the presentation parameters.

    if (pp.Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) {
      Logger::warn("Lockable back buffer not supported");
    }

    // Ensure at least one window is good.
    CHECK_NOT_NULL(pp.hDeviceWindow || hFocusWindow);

    const D3DDEVICE_CREATION_PARAMETERS cp {
      Adapter,
      DevType,
      hFocusWindow,
      BehaviorFlags,
    };

    try {
      device = new D3D9Device(this, adapter, cp, pp);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }

    return D3D_OK;
  }
}
