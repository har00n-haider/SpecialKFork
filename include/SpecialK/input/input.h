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

#ifndef __SK__INPUT_H__
#define __SK__INPUT_H__

#include <Windows.h>
#include <cstdint>

#define SK_LOG_INPUT_CALL { static int  calls  = 0;                   { SK_LOG0 ( (L"[!] > Call #%lu: %hs", calls++, __FUNCTION__), L"Input Mgr." ); } }
#define SK_LOG_FIRST_CALL { static bool called = false; if (! called) { SK_LOG0 ( (L"[!] > First Call: %34hs", __FUNCTION__), L"Input Mgr." ); called = true; } }

bool SK_ImGui_WantGamepadCapture  (void);
bool SK_ImGui_WantMouseCapture    (void);
bool SK_ImGui_WantMouseCaptureEx  (DWORD dwReasonMask = 0xFFFF);
bool SK_ImGui_WantKeyboardCapture (void);
bool SK_ImGui_WantTextCapture     (void);

void SK_Input_HookDI8         (void);
void SK_Input_HookHID         (void);

void SK_Input_HookXInput1_3   (void);
void SK_Input_HookXInput1_4   (void);
void SK_Input_HookXInput9_1_0 (void);

void SK_Input_PreHookXInput   (void);

void SK_Input_Init (void);


struct sk_imgui_cursor_s
{
  HCURSOR orig_img   =      nullptr;
  POINT   orig_pos   =  { 0, 0 };
  bool    orig_vis   =     false;
                       
  HCURSOR img        =      nullptr;
  POINT   pos        =  { 0, 0 };
                       
  bool    visible    =     false;
  bool    idle       =     false; // Hasn't moved
  DWORD   last_move  =  MAXDWORD;

  void    showSystemCursor (bool system = true);
  void    showImGuiCursor  (void);

  void    update           (void);

  void   LocalToScreen    (LPPOINT lpPoint);
  void   LocalToClient    (LPPOINT lpPoint);
  void   ClientToLocal    (LPPOINT lpPoint);
  void   ScreenToLocal    (LPPOINT lpPoint);

  void   activateWindow   (bool active);

  struct {
    struct {
      bool visible = true;
      bool gamepad = false; // TODO - Disable warping ifgamepad is plugged in
      bool ui_open = true;
    } no_warp;
  } prefs;
} extern SK_ImGui_Cursor;


enum class sk_input_dev_type {
  Mouse    = 1,
  Keyboard = 2,
  Gamepad  = 4,
  Other    = 8
};


struct sk_input_api_context_s
{
  volatile LONG reads [4], writes [4];

  struct {
    volatile LONG reads [4], writes [4];
  } last_frame;

  struct {
    bool keyboard, mouse, gamepad, other;
  } active;

  void markRead  (sk_input_dev_type type) { InterlockedIncrement (&last_frame.reads    [ type == sk_input_dev_type::Mouse    ? 0 :
                                                                                         type == sk_input_dev_type::Keyboard ? 1 :
                                                                                         type == sk_input_dev_type::Gamepad  ? 2 : 3 ] ); }
  void markWrite  (sk_input_dev_type type) { InterlockedIncrement (&last_frame.writes  [ type == sk_input_dev_type::Mouse    ? 0 :
                                                                                         type == sk_input_dev_type::Keyboard ? 1 :
                                                                                         type == sk_input_dev_type::Gamepad  ? 2 : 3 ] ); }

  bool nextFrame (void)
  {
    bool active_data = false;


    active.keyboard = false; active.gamepad = false;
    active.mouse    = false; active.other   = false;


    InterlockedAdd  (&reads   [0], last_frame.reads  [0]); 
    InterlockedAdd  (&writes  [0], last_frame.writes [0]);

      if (ReadAcquire (&last_frame.reads  [0]))  { active.keyboard = true; active_data = true; }
      if (ReadAcquire (&last_frame.writes [0]))  { active.keyboard = true; active_data = true; }

    InterlockedAdd  (&reads   [1], last_frame.reads  [1]);
    InterlockedAdd  (&writes  [1], last_frame.writes [1]); 

      if (ReadAcquire (&last_frame.reads  [1]))  { active.mouse    = true; active_data = true; }
      if (ReadAcquire (&last_frame.writes [1]))  { active.mouse    = true; active_data = true; }

    InterlockedAdd  (&reads   [2], last_frame.reads  [2]);
    InterlockedAdd  (&writes  [2], last_frame.writes [2]);

      if (ReadAcquire (&last_frame.reads  [2]))  { active.gamepad  = true; active_data = true; }
      if (ReadAcquire (&last_frame.writes [2]))  { active.gamepad  = true; active_data = true; }

    InterlockedAdd  (&reads   [3], last_frame.reads  [3]);
    InterlockedAdd  (&writes  [3], last_frame.writes [3]);

      if (ReadAcquire (&last_frame.reads  [2]))  { active.other    = true; active_data = true; }
      if (ReadAcquire (&last_frame.writes [2]))  { active.other    = true; active_data = true; }


    InterlockedExchange (&last_frame.reads  [0], 0);   InterlockedExchange (&last_frame.reads  [1], 0);
    InterlockedExchange (&last_frame.writes [0], 0);   InterlockedExchange (&last_frame.writes [1], 0);
    InterlockedExchange (&last_frame.reads  [2], 0);   InterlockedExchange (&last_frame.reads  [3], 0);
    InterlockedExchange (&last_frame.writes [2], 0);   InterlockedExchange (&last_frame.writes [3], 0);


    return active_data;
  }
};

extern sk_input_api_context_s SK_XInput_Backend;
extern sk_input_api_context_s SK_DI8_Backend;
extern sk_input_api_context_s SK_HID_Backend;
extern sk_input_api_context_s SK_Win32_Backend;
extern sk_input_api_context_s SK_RawInput_Backend;

extern sk_input_api_context_s SK_WinMM_Backend;
extern sk_input_api_context_s SK_Steam_Backend;



enum class SK_Input_BindFlags
{
  Invalid           =    0,

  NonExclusive      = ( 1UL << 0 ),
     Exclusive      = ( 1UL << 1 ),
  ModeAgnostic      = ( NonExclusive  |
                        Exclusive     ),
  
  ActivateOnPress   = ( 1UL << 2 ),
  ActivateOnRelease = ( 1UL << 3 ),

  Repeatable        = ( 1UL << 4 ),

  ReleaseOnActivate = ( 1UL << 5 ), // Immediately behave as though the buttons were released
  HoldOnActivate    = ( 1UL << 6 ), // Immediately behave as though the buttons are held

  Default           = ( ModeAgnostic    |
                        ActivateOnPress )
};

enum class SK_Input_ModifierKeys
{
  None        =    0,


  Left_Ctrl   = ( 1UL << 0 ),
  Right_Ctrl  = ( 1UL << 1 ),

  Left_Alt    = ( 1UL << 2 ),
  Right_Alt   = ( 1UL << 3 ),

  Left_Shift  = ( 1UL << 4 ),
  Right_Shift = ( 1UL << 5 ),


  Ctrl        = ( Left_Ctrl |
                 Right_Ctrl ),

  Alt         = ( Left_Alt |
                 Right_Alt ),

  Shift       = ( Left_Shift |
                 Right_Shift )
};


struct SK_Input_Duration
{
  uint16_t min = 0,
           max = 0;
};

struct SK_Input_KeyBinding
{
  struct key_s
  {
    SK_Input_Duration   duration   = {    };
    uint16_t            scancode   = 0x0000;
  } keys [1];

  SK_Input_ModifierKeys modifiers  = SK_Input_ModifierKeys::None;
  SK_Input_BindFlags    flags      = SK_Input_BindFlags::Default;

  uint8_t               combo_size = 1;
};

struct SK_Input_PadBinding
{
  struct button_s
  {
    SK_Input_Duration duration  = {};
    uint8_t           button    = 00;
  } buttons [1];

  SK_Input_BindFlags flags      = SK_Input_BindFlags::Default;

  uint8_t            combo_size = 1;
};


class SK_Input_KeyBindFactory
{

};

class SK_Input_PadBindFactory
{

};


bool
SK_ImGui_HandlesMessage (LPMSG lpMsg, bool remove, bool peek);


using keybd_event_pfn = void (WINAPI *)(
  _In_ BYTE      bVk,
  _In_ BYTE      bScan,
  _In_ DWORD     dwFlags,
  _In_ ULONG_PTR dwExtraInfo
);

using SetCursor_pfn = HCURSOR (WINAPI *)(HCURSOR hCursor);

extern keybd_event_pfn keybd_event_Original;
extern SetCursor_pfn   SetCursor_Original;

#include <cstdint>

extern
void
SK_ImGui_CenterCursorOnWindow (void);

//////////////////////////////////////////////////////////////
//
// HIDClass (Usermode)
//
//////////////////////////////////////////////////////////////

#include <Hidsdi.h>
#include <Windows.h>
#include <winnt.h>
#include <hidusage.h>
#include <Hidpi.h>

using HidP_GetCaps_pfn = NTSTATUS (__stdcall *)(
  _In_  PHIDP_PREPARSED_DATA PreparsedData,
  _Out_ PHIDP_CAPS           Capabilities
);

using HidD_GetPreparsedData_pfn = BOOLEAN (__stdcall *)(
  _In_  HANDLE                HidDeviceObject,
  _Out_ PHIDP_PREPARSED_DATA *PreparsedData
);

using HidD_FreePreparsedData_pfn = BOOLEAN (__stdcall *)(
  _In_ PHIDP_PREPARSED_DATA PreparsedData
);

using HidP_GetData_pfn = NTSTATUS (__stdcall *)(
  _In_    HIDP_REPORT_TYPE     ReportType,
  _Out_   PHIDP_DATA           DataList,
  _Inout_ PULONG               DataLength,
  _In_    PHIDP_PREPARSED_DATA PreparsedData,
  _In_    PCHAR                Report,
  _In_    ULONG                ReportLength
);

using HidD_GetFeature_pfn = BOOLEAN (__stdcall *)(
  _In_  HANDLE HidDeviceObject,
  _Out_ PVOID  ReportBuffer,
  _In_  ULONG  ReportBufferLength
);

extern HidP_GetCaps_pfn           HidP_GetCaps_Original          ;
extern HidD_GetPreparsedData_pfn  HidD_GetPreparsedData_Original ;
extern HidD_FreePreparsedData_pfn HidD_FreePreparsedData_Original;
extern HidD_GetFeature_pfn        HidD_GetFeature_Original       ;
extern HidP_GetData_pfn           HidP_GetData_Original          ;
extern SetCursor_pfn              SetCursor_Original             ;



// Temporarily override game's preferences for input device window message generation
bool
SK_RawInput_EnableLegacyMouse (bool enable);

// Restore the game's original setting
void
SK_RawInput_RestoreLegacyMouse (void);

// Temporarily override game's preferences for input device window message generation
bool
SK_RawInput_EnableLegacyKeyboard (bool enable);

// Restore the game's original setting
void
SK_RawInput_RestoreLegacyKeyboard (void);


using GetRegisteredRawInputDevices_pfn = UINT (WINAPI *)(
  _Out_opt_ PRAWINPUTDEVICE pRawInputDevices,
  _Inout_   PUINT           puiNumDevices,
  _In_      UINT            cbSize
);

extern GetRegisteredRawInputDevices_pfn GetRegisteredRawInputDevices_Original;

extern LONG  SK_RawInput_MouseX;
extern LONG  SK_RawInput_MouseY;
extern POINT SK_RawInput_Mouse;

#endif /* __SK__INPUT_H__ */
