/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#ifndef __SK__RENDER_BACKEND_H__
#define __SK__RENDER_BACKEND_H__

#include <Windows.h>
#include <../depends/include/nvapi/nvapi_lite_common.h>

#include <comdef.h>
#include <atlbase.h>

#include <cstdint>

enum class SK_RenderAPI
{
  Reserved  = 0x0001,

  // Native API Implementations
  OpenGL    = 0x0002,
  Vulkan    = 0x0004,
  D3D9      = 0x0008,
  D3D9Ex    = 0x0018,
  D3D10     = 0x0020, // Don't care
  D3D11     = 0x0040,
  D3D12     = 0x0080,

  // These aren't native, but we need the bitmask anyway
  D3D8      = 0x2000,
  DDraw     = 0x4000,
  Glide     = 0x8000,

  // Wrapped APIs (D3D12 Flavor)
  D3D11On12 = 0x00C0,

  // Wrapped APIs (D3D11 Flavor)
  D3D8On11  = 0x2040,
  DDrawOn11 = 0x4040,
  GlideOn11 = 0x8040,
};

class SK_RenderBackend_V1
{
public:
  enum class SK_RenderAPI api       = SK_RenderAPI::Reserved;
             wchar_t      name [16] = { };
};

enum
{
  SK_FRAMEBUFFER_FLAG_SRGB    = 0x1,
  SK_FRAMEBUFFER_FLAG_FLOAT   = 0x2,
  SK_FRAMEBUFFER_FLAG_RGB10A2 = 0x4,
  SK_FRAMEBUFFER_FLAG_MSAA    = 0X8
};

enum reset_stage_e
{
  Initiate = 0x0, // Fake device loss
  Respond  = 0x1, // Fake device not reset
  Clear    = 0x2  // Return status to normal
} extern trigger_reset;

enum mode_change_request_e
{
  Windowed   = 0,    // Switch from Fullscreen to Windowed
  Fullscreen = 1,    // Switch from Windowed to Fullscreen
  None       = 0xFF
} extern request_mode_change;


struct sk_hwnd_cache_s
{
  HWND    hwnd             = HWND_DESKTOP;
  HWND    parent           = HWND_DESKTOP;

  struct devcaps_s
  {
    struct
    {
      int x, y, refresh;
    } res;

    DWORD last_checked     = 0UL;
  } devcaps;

  devcaps_s&
  getDevCaps (void);

  wchar_t class_name [128] = { };
  wchar_t title      [128] = { };

  struct {
    DWORD pid, tid;
  }       owner            = { 0, 0 };

  bool    unicode          = false;

  ULONG   last_changed     = 0UL;


  sk_hwnd_cache_s (HWND wnd);

  operator const HWND& (void) const { return hwnd; };
};

#pragma pack(push)
#pragma pack(8)
class SK_RenderBackend_V2 : public SK_RenderBackend_V1
{
public:
   SK_RenderBackend_V2 (void);
  ~SK_RenderBackend_V2 (void);

  CComPtr <IUnknown>      device               = nullptr;
  CComPtr <IUnknown>      swapchain            = nullptr;
  NVDX_ObjectHandle       surface              = nullptr;
  bool                    fullscreen_exclusive = false;
  uint64_t                framebuffer_flags    = 0x00;
  int                     present_interval     = 0; // Present interval on last call to present
  static volatile LONG    frames_drawn;

  struct window_registry_s
  {
    // Most input processing happens in this HWND's message pump
    sk_hwnd_cache_s       focus                = { HWND_DESKTOP };

    // Defines the client rectangle and not much else
    sk_hwnd_cache_s       device               = { HWND_DESKTOP };

    // This Unity engine is so terrible at window management that we need
    //   to flag the game and start disabling features!
    bool                  unity                = false;

    void setFocus  (HWND hWndFocus);
    void setDevice (HWND hWndRender);

    HWND getFocus  (void);
    HWND getDevice (void);
  } windows;


  // TODO: Proper abstraction
  struct d3d11_s
  {
    //MIDL_INTERFACE ("c0bfa96c-e089-44fb-8eaf-26f8796190da")     
    CComPtr <IUnknown>    immediate_ctx        = nullptr;
  } d3d11;


  struct gsync_s
  {
    void update (void);

    BOOL                  capable      = FALSE;
    BOOL                  active       = FALSE;
    DWORD                 last_checked = 0;
  } gsync_state;


  //
  // Somewhat meaningless, given how most engines manage
  //   memory, textures and shaders...
  //
  //   This is the thread that handles SwapChain Presentation;
  //     nothing else can safely be inferred about this thread.
  //
  volatile LONG           thread       = 0;
  CRITICAL_SECTION        cs_res       = { };


  bool canEnterFullscreen    (void);

  void requestFullscreenMode (bool override = false);
  void requestWindowedMode   (bool override = false);

  float getActiveRefreshRate (void);


  void releaseOwnedResources (void);
};
#pragma pack(pop)

using SK_RenderBackend = SK_RenderBackend_V2;

SK_RenderBackend&
__stdcall
SK_GetCurrentRenderBackend (void);

void
__stdcall
SK_InitRenderBackends (void);

__declspec (dllexport)
IUnknown* __stdcall SK_Render_GetDevice (void);

__declspec (dllexport)
IUnknown* __stdcall SK_Render_GetSwapChain (void);

void SK_BootD3D8   (void);
void SK_BootDDraw  (void);
void SK_BootGlide  (void);

void SK_BootD3D9   (void);
void SK_BootDXGI   (void);
void SK_BootOpenGL (void);
void SK_BootVulkan (void);


_Return_type_success_ (nullptr)
IUnknown*
SK_COM_ValidateRelease (IUnknown** ppObj);

const wchar_t*
SK_Render_GetAPIName (SK_RenderAPI api);


__forceinline
ULONG
__stdcall
SK_GetFramesDrawn (void)
{
  return
    static_cast <ULONG> (ReadNoFence (&SK_RenderBackend::frames_drawn));
}


#endif /* __SK__RENDER_BACKEND__H__ */