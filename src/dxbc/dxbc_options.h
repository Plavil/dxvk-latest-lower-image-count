#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  /**
   * \brief DXBC compiler options
   * 
   * Defines driver- or device-specific options,
   * which are mostly workarounds for driver bugs.
   */
  struct DxbcOptions {
    DxbcOptions() { }
    DxbcOptions(
      const Rc<DxvkDevice>& device);
      
    /// Maximum number of sample mask words.
    uint32_t maxSampleMaskWords = 1;

    /// Use Fmin/Fmax instead of Nmin/Nmax.
    bool useSimpleMinMaxClamp = false;
    
    /// Pack the depth reference value into the
    /// coordinate vector for depth-compare ops.
    bool packDrefValueIntoCoordinates = false;
  };
  
}