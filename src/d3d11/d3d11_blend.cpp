#include "d3d11_blend.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11BlendState::D3D11BlendState(
          D3D11Device*        device,
    const D3D11_BLEND_DESC1&  desc)
  : m_device(device), m_desc(desc) {
    // If Independent Blend is disabled, we must ignore the
    // blend modes for render target 1 to 7. In Vulkan, all
    // blend modes need to be identical in that case.
    for (uint32_t i = 0; i < m_blendModes.size(); i++) {
      m_blendModes.at(i) = DecodeBlendMode(
        desc.IndependentBlendEnable
          ? desc.RenderTarget[i]
          : desc.RenderTarget[0]);
    }
    
    // Multisample state is part of the blend state in D3D11
    m_msState.sampleMask            = 0; // Set during bind
    m_msState.enableAlphaToCoverage = desc.AlphaToCoverageEnable;
    m_msState.enableAlphaToOne      = VK_FALSE;
    
    // Vulkan only supports a global logic op for the blend
    // state, which might be problematic in some cases.
    if (desc.IndependentBlendEnable && desc.RenderTarget[0].LogicOpEnable)
      Logger::warn("D3D11: Per-target logic ops not supported");
    
    m_loState.enableLogicOp         = desc.RenderTarget[0].LogicOpEnable;
    m_loState.logicOp               = DecodeLogicOp(desc.RenderTarget[0].LogicOp);
  }
  
  
  D3D11BlendState::~D3D11BlendState() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11BlendState::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11BlendState)
     || riid == __uuidof(ID3D11BlendState1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11BlendState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = ref(m_device);
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDesc(D3D11_BLEND_DESC* pDesc) {
    pDesc->AlphaToCoverageEnable  = m_desc.AlphaToCoverageEnable;
    pDesc->IndependentBlendEnable = m_desc.IndependentBlendEnable;
    
    for (uint32_t i = 0; i < 8; i++) {
      pDesc->RenderTarget[i].BlendEnable           = m_desc.RenderTarget[i].BlendEnable;
      pDesc->RenderTarget[i].SrcBlend              = m_desc.RenderTarget[i].SrcBlend;
      pDesc->RenderTarget[i].DestBlend             = m_desc.RenderTarget[i].DestBlend;
      pDesc->RenderTarget[i].BlendOp               = m_desc.RenderTarget[i].BlendOp;
      pDesc->RenderTarget[i].SrcBlendAlpha         = m_desc.RenderTarget[i].SrcBlendAlpha;
      pDesc->RenderTarget[i].DestBlendAlpha        = m_desc.RenderTarget[i].DestBlendAlpha;
      pDesc->RenderTarget[i].BlendOpAlpha          = m_desc.RenderTarget[i].BlendOpAlpha;
      pDesc->RenderTarget[i].RenderTargetWriteMask = m_desc.RenderTarget[i].RenderTargetWriteMask;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11BlendState::GetDesc1(D3D11_BLEND_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  void D3D11BlendState::BindToContext(
    const Rc<DxvkContext>&  ctx,
          uint32_t          sampleMask) const {
    // We handled Independent Blend during object creation
    // already, so if it is disabled, all elements in the
    // blend mode array will be identical
    for (uint32_t i = 0; i < m_blendModes.size(); i++)
      ctx->setBlendMode(i, m_blendModes.at(i));
    
    // The sample mask is dynamic state in D3D11
    DxvkMultisampleState msState = m_msState;
    msState.sampleMask = sampleMask;
    ctx->setMultisampleState(msState);
    
    // Set up logic op state as well
    ctx->setLogicOpState(m_loState);
  }
  
  
  D3D11_BLEND_DESC1 D3D11BlendState::DefaultDesc() {
    D3D11_BLEND_DESC1 dstDesc;
    dstDesc.AlphaToCoverageEnable  = FALSE;
    dstDesc.IndependentBlendEnable = FALSE;
    
    // 1-7 must be ignored if IndependentBlendEnable is disabled so
    // technically this is not needed, but since this structure is
    // going to be copied around we'll initialize it nonetheless
    for (uint32_t i = 0; i < 8; i++) {
      dstDesc.RenderTarget[i].BlendEnable           = FALSE;
      dstDesc.RenderTarget[i].LogicOpEnable         = FALSE;
      dstDesc.RenderTarget[i].SrcBlend              = D3D11_BLEND_ONE;
      dstDesc.RenderTarget[i].DestBlend             = D3D11_BLEND_ZERO;
      dstDesc.RenderTarget[i].BlendOp               = D3D11_BLEND_OP_ADD;
      dstDesc.RenderTarget[i].SrcBlendAlpha         = D3D11_BLEND_ONE;
      dstDesc.RenderTarget[i].DestBlendAlpha        = D3D11_BLEND_ZERO;
      dstDesc.RenderTarget[i].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
      dstDesc.RenderTarget[i].LogicOp               = D3D11_LOGIC_OP_NOOP;
      dstDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    
    return dstDesc;
  }
  
  
  D3D11_BLEND_DESC1 D3D11BlendState::PromoteDesc(const D3D11_BLEND_DESC* pSrcDesc) {
    D3D11_BLEND_DESC1 dstDesc;
    dstDesc.AlphaToCoverageEnable  = pSrcDesc->AlphaToCoverageEnable;
    dstDesc.IndependentBlendEnable = pSrcDesc->IndependentBlendEnable;
    
    for (uint32_t i = 0; i < 8; i++) {
      dstDesc.RenderTarget[i].BlendEnable           = pSrcDesc->RenderTarget[i].BlendEnable;
      dstDesc.RenderTarget[i].LogicOpEnable         = FALSE;
      dstDesc.RenderTarget[i].SrcBlend              = pSrcDesc->RenderTarget[i].SrcBlend;
      dstDesc.RenderTarget[i].DestBlend             = pSrcDesc->RenderTarget[i].DestBlend;
      dstDesc.RenderTarget[i].BlendOp               = pSrcDesc->RenderTarget[i].BlendOp;
      dstDesc.RenderTarget[i].SrcBlendAlpha         = pSrcDesc->RenderTarget[i].SrcBlendAlpha;
      dstDesc.RenderTarget[i].DestBlendAlpha        = pSrcDesc->RenderTarget[i].DestBlendAlpha;
      dstDesc.RenderTarget[i].BlendOpAlpha          = pSrcDesc->RenderTarget[i].BlendOpAlpha;
      dstDesc.RenderTarget[i].LogicOp               = D3D11_LOGIC_OP_NOOP;
      dstDesc.RenderTarget[i].RenderTargetWriteMask = pSrcDesc->RenderTarget[i].RenderTargetWriteMask;
    }
    
    return dstDesc;
  }
  
  
  HRESULT D3D11BlendState::NormalizeDesc(D3D11_BLEND_DESC1* pDesc) {
    const D3D11_BLEND_DESC1 defaultDesc = DefaultDesc();
    if (pDesc->AlphaToCoverageEnable != 0) {
      pDesc->AlphaToCoverageEnable = 1;
    }

    if (pDesc->IndependentBlendEnable != 0) {
      pDesc->IndependentBlendEnable = 1;
    }

    D3D11_RENDER_TARGET_BLEND_DESC1* rt = &pDesc->RenderTarget[0];
    if (rt->BlendEnable != 0) {
      rt->BlendEnable = 1;
      //Can not have Blend enabled the same time as LogicOp
      if (rt->LogicOpEnable == 1) {
        Logger::err(str::format("D3D11BlendState: Logic Op must be disabled if Blend is enabled: "));
        return E_INVALIDARG;
      }
        
      if (!ValidBlendOp(rt->BlendOp)
       || !ValidBlendOp(rt->BlendOpAlpha)) {
        Logger::err(str::format(
          "D3D11BlendState: Invalid blend Op: ",
          "\n BlendOp: ", rt->BlendOp,
          "\n BlendOpAlpha: ", rt->BlendOpAlpha ));
        return E_INVALIDARG;
      }
        
      if (!ValidBlend(rt->SrcBlend)
       || !ValidBlendAlpha(rt->SrcBlendAlpha)
       || !ValidBlend(rt->DestBlend)
       || !ValidBlendAlpha(rt->DestBlendAlpha)) {
        Logger::err(str::format(
          "D3D11BlendState: Invalid Blend: ",
          "\n SrcBlend: ", rt->SrcBlend,
          "\n DestBlend: ", rt->DestBlend,
          "\n SrcBlendAlpha: ", rt->SrcBlendAlpha,
          "\n DestBlendAlpha: ", rt->DestBlendAlpha));
        return E_INVALIDARG;
      }
    }

    if (rt->LogicOpEnable != 0) {
      rt->LogicOpEnable = 1;

      if (rt->BlendEnable == 1) {
        Logger::err(str::format("D3D11BlendState: Blending must be disabled if LogicOp is enabled: "));
        return E_INVALIDARG;
      }
       
      if (pDesc->IndependentBlendEnable == 1) {
        Logger::err(str::format("D3D11BlendState: IndependentBlendEnable must be disabled if LogicOp is enabled: "));
        return E_INVALIDARG;
      }
        

      if (!ValidLogicOp(rt->LogicOp)) {
        Logger::err(str::format("D3D11BlendState: Invalid LogicOp: ", rt->LogicOp));
        return E_INVALIDARG;
      }
        
    }
    if (rt->BlendEnable == 0) {
      rt->SrcBlend = defaultDesc.RenderTarget[0].SrcBlend;
      rt->DestBlend = defaultDesc.RenderTarget[0].DestBlend;
      rt->BlendOp = defaultDesc.RenderTarget[0].BlendOp;
      rt->SrcBlendAlpha = defaultDesc.RenderTarget[0].SrcBlendAlpha;
      rt->DestBlendAlpha = defaultDesc.RenderTarget[0].DestBlendAlpha;
      rt->BlendOpAlpha = defaultDesc.RenderTarget[0].BlendOpAlpha;
    }

    if (rt->LogicOpEnable == 0) {
      rt->LogicOp = defaultDesc.RenderTarget[0].LogicOp;
    }

    if (rt->RenderTargetWriteMask > D3D11_COLOR_WRITE_ENABLE_ALL) {
      Logger::err(str::format("D3D11BlendState: Invalid RenderTargetWriteMask: ", rt->RenderTargetWriteMask));
      return E_INVALIDARG;
    }
      


    //for the rest of the rendertargets
    if (pDesc->IndependentBlendEnable) {
      for (int i = 1; i < 8; i++) {
        rt = &pDesc->RenderTarget[i];
       
        //If the independent blend is enabled and
        //blend is enabled on rendertargets they
        //must use the same blend operations
        //as rendertarget[0]
        if (rt->BlendEnable != 0) {
          rt->BlendEnable = 1;
          rt->SrcBlend = pDesc->RenderTarget[0].SrcBlend;
          rt->DestBlend = pDesc->RenderTarget[0].DestBlend;
          rt->BlendOp = pDesc->RenderTarget[0].BlendOp;
          rt->SrcBlendAlpha = pDesc->RenderTarget[0].SrcBlendAlpha;
          rt->DestBlendAlpha = pDesc->RenderTarget[0].DestBlendAlpha;
          rt->BlendOpAlpha = pDesc->RenderTarget[0].BlendOpAlpha;
        } else {
          rt->SrcBlend = defaultDesc.RenderTarget[0].SrcBlend;
          rt->DestBlend = defaultDesc.RenderTarget[0].DestBlend;
          rt->BlendOp = defaultDesc.RenderTarget[0].BlendOp;
          rt->SrcBlendAlpha = defaultDesc.RenderTarget[0].SrcBlendAlpha;
          rt->DestBlendAlpha = defaultDesc.RenderTarget[0].DestBlendAlpha;
          rt->BlendOpAlpha = defaultDesc.RenderTarget[0].BlendOpAlpha;
        }

        if (rt->RenderTargetWriteMask > D3D11_COLOR_WRITE_ENABLE_ALL) {
          Logger::err(str::format("D3D11BlendState: Invalid RenderTargetWriteMask: ", rt->RenderTargetWriteMask));
          return E_INVALIDARG;
        }

      }
    }
    if (!pDesc->IndependentBlendEnable) {
      for (int i = 1; i < 8; i++) {
        rt = &pDesc->RenderTarget[i];
        //copy the default values over
        pDesc->RenderTarget[i] = defaultDesc.RenderTarget[0];
        
        //RenderTargetMask is the same as the first rendertarget if independent
        //blend is disabled
        rt->RenderTargetWriteMask = pDesc->RenderTarget[0].RenderTargetWriteMask;
        //logic operations must be the same as the first render target if enabled
        //in the first rendertarget
        if (pDesc->RenderTarget[0].LogicOpEnable == 1) {
          rt->LogicOpEnable = 1;
          rt->LogicOp = pDesc->RenderTarget[0].LogicOp;
        }
      }

    }
    return S_OK;
  }
  
  
  DxvkBlendMode D3D11BlendState::DecodeBlendMode(
    const D3D11_RENDER_TARGET_BLEND_DESC1& BlendDesc) {
    DxvkBlendMode mode;
    mode.enableBlending   = BlendDesc.BlendEnable;
    mode.colorSrcFactor   = DecodeBlendFactor(BlendDesc.SrcBlend, false);
    mode.colorDstFactor   = DecodeBlendFactor(BlendDesc.DestBlend, false);
    mode.colorBlendOp     = DecodeBlendOp(BlendDesc.BlendOp);
    mode.alphaSrcFactor   = DecodeBlendFactor(BlendDesc.SrcBlendAlpha, true);
    mode.alphaDstFactor   = DecodeBlendFactor(BlendDesc.DestBlendAlpha, true);
    mode.alphaBlendOp     = DecodeBlendOp(BlendDesc.BlendOpAlpha);
    mode.writeMask        = BlendDesc.RenderTargetWriteMask;
    return mode;
  }
  
  
  VkBlendFactor D3D11BlendState::DecodeBlendFactor(D3D11_BLEND BlendFactor, bool IsAlpha) {
    switch (BlendFactor) {
      case D3D11_BLEND_ZERO:              return VK_BLEND_FACTOR_ZERO;
      case D3D11_BLEND_ONE:               return VK_BLEND_FACTOR_ONE;
      case D3D11_BLEND_SRC_COLOR:         return VK_BLEND_FACTOR_SRC_COLOR;
      case D3D11_BLEND_INV_SRC_COLOR:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      case D3D11_BLEND_SRC_ALPHA:         return VK_BLEND_FACTOR_SRC_ALPHA;
      case D3D11_BLEND_INV_SRC_ALPHA:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      case D3D11_BLEND_DEST_ALPHA:        return VK_BLEND_FACTOR_DST_ALPHA;
      case D3D11_BLEND_INV_DEST_ALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
      case D3D11_BLEND_DEST_COLOR:        return VK_BLEND_FACTOR_DST_COLOR;
      case D3D11_BLEND_INV_DEST_COLOR:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
      case D3D11_BLEND_SRC_ALPHA_SAT:     return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
      case D3D11_BLEND_BLEND_FACTOR:      return IsAlpha ? VK_BLEND_FACTOR_CONSTANT_ALPHA : VK_BLEND_FACTOR_CONSTANT_COLOR;
      case D3D11_BLEND_INV_BLEND_FACTOR:  return IsAlpha ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
      case D3D11_BLEND_SRC1_COLOR:        return VK_BLEND_FACTOR_SRC1_COLOR;
      case D3D11_BLEND_INV_SRC1_COLOR:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
      case D3D11_BLEND_SRC1_ALPHA:        return VK_BLEND_FACTOR_SRC1_ALPHA;
      case D3D11_BLEND_INV_SRC1_ALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }
    
    if (BlendFactor != 0)  // prevent log spamming when apps use ZeroMemory
      Logger::err(str::format("D3D11: Invalid blend factor: ", BlendFactor));
    return VK_BLEND_FACTOR_ZERO;
  }
  
  
  VkBlendOp D3D11BlendState::DecodeBlendOp(D3D11_BLEND_OP BlendOp) {
    switch (BlendOp) {
      case D3D11_BLEND_OP_ADD:            return VK_BLEND_OP_ADD;
      case D3D11_BLEND_OP_SUBTRACT:       return VK_BLEND_OP_SUBTRACT;
      case D3D11_BLEND_OP_REV_SUBTRACT:   return VK_BLEND_OP_REVERSE_SUBTRACT;
      case D3D11_BLEND_OP_MIN:            return VK_BLEND_OP_MIN;
      case D3D11_BLEND_OP_MAX:            return VK_BLEND_OP_MAX;
    }
    
    if (BlendOp != 0)  // prevent log spamming when apps use ZeroMemory
      Logger::err(str::format("D3D11: Invalid blend op: ", BlendOp));
    return VK_BLEND_OP_ADD;
  }
  
  
  VkLogicOp D3D11BlendState::DecodeLogicOp(D3D11_LOGIC_OP LogicOp) {
    switch (LogicOp) {
      case D3D11_LOGIC_OP_CLEAR:          return VK_LOGIC_OP_CLEAR;
      case D3D11_LOGIC_OP_SET:            return VK_LOGIC_OP_SET;
      case D3D11_LOGIC_OP_COPY:           return VK_LOGIC_OP_COPY;
      case D3D11_LOGIC_OP_COPY_INVERTED:  return VK_LOGIC_OP_COPY_INVERTED;
      case D3D11_LOGIC_OP_NOOP:           return VK_LOGIC_OP_NO_OP;
      case D3D11_LOGIC_OP_INVERT:         return VK_LOGIC_OP_INVERT;
      case D3D11_LOGIC_OP_AND:            return VK_LOGIC_OP_AND;
      case D3D11_LOGIC_OP_NAND:           return VK_LOGIC_OP_NAND;
      case D3D11_LOGIC_OP_OR:             return VK_LOGIC_OP_OR;
      case D3D11_LOGIC_OP_NOR:            return VK_LOGIC_OP_NOR;
      case D3D11_LOGIC_OP_XOR:            return VK_LOGIC_OP_XOR;
      case D3D11_LOGIC_OP_EQUIV:          return VK_LOGIC_OP_EQUIVALENT;
      case D3D11_LOGIC_OP_AND_REVERSE:    return VK_LOGIC_OP_AND_REVERSE;
      case D3D11_LOGIC_OP_AND_INVERTED:   return VK_LOGIC_OP_AND_INVERTED;
      case D3D11_LOGIC_OP_OR_REVERSE:     return VK_LOGIC_OP_OR_REVERSE;
      case D3D11_LOGIC_OP_OR_INVERTED:    return VK_LOGIC_OP_OR_INVERTED;
    }
    
    if (LogicOp != 0)
      Logger::err(str::format("D3D11: Invalid logic op: ", LogicOp));
    return VK_LOGIC_OP_NO_OP;
  }

  bool D3D11BlendState::ValidBlend(D3D11_BLEND blend) {
    if (blend < D3D11_BLEND_ZERO
     || blend > D3D11_BLEND_INV_SRC1_ALPHA)
      return false;
    return true;
  }

  bool D3D11BlendState::ValidBlendAlpha(D3D11_BLEND blendAlpha) {
    //can't be color operations
    if (blendAlpha == D3D11_BLEND_SRC_COLOR
     || blendAlpha == D3D11_BLEND_INV_SRC_COLOR
     || blendAlpha == D3D11_BLEND_DEST_COLOR
     || blendAlpha == D3D11_BLEND_INV_DEST_COLOR
     || blendAlpha == D3D11_BLEND_SRC1_COLOR
     || blendAlpha == D3D11_BLEND_INV_SRC1_COLOR
     || blendAlpha > D3D11_BLEND_INV_SRC1_ALPHA
     || blendAlpha < D3D11_BLEND_ZERO)
      return false;
    return true;
  }

  bool D3D11BlendState::ValidBlendOp(D3D11_BLEND_OP blendOp) {
    if (blendOp < D3D11_BLEND_OP_ADD
      || blendOp > D3D11_BLEND_OP_MAX)
      return false;
    return true;
  }

  bool D3D11BlendState::ValidLogicOp(D3D11_LOGIC_OP logicOp) {
    if (logicOp < D3D11_LOGIC_OP_CLEAR
     || logicOp > D3D11_LOGIC_OP_OR_INVERTED)
      return false;
    return true;
  }
  
}
