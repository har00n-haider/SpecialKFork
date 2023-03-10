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

#include <SpecialK/config.h>
#include <SpecialK/core.h>
#include <SpecialK/dxgi_interfaces.h>
#include <SpecialK/parameter.h>
#include <SpecialK/import.h>
#include <SpecialK/utility.h>
#include <SpecialK/ini.h>
#include <SpecialK/log.h>
#include <SpecialK/steam_api.h>
#include <SpecialK/nvapi.h>

#include <SpecialK/DLL_VERSION.H>
#include <SpecialK/input/input.h>
#include <SpecialK/widgets/widget.h>

#include <unordered_map>
#include <typeindex>

#include <Shlwapi.h>

#define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1

const wchar_t*       SK_VER_STR = SK_VERSION_STR_W;

iSK_INI*             dll_ini         = nullptr;
iSK_INI*             osd_ini         = nullptr;
iSK_INI*             achievement_ini = nullptr;
iSK_INI*             macro_ini       = nullptr;
sk_config_t          config;
sk::ParameterFactory g_ParameterFactory;


static std::unordered_map <std::wstring, SK_GAME_ID> games;

SK_GAME_ID
__stdcall
SK_GetCurrentGameID (void)
{
  static SK_GAME_ID current_game =
    games.count (SK_GetHostApp ()) ?
          games [SK_GetHostApp ()] :
          SK_GAME_ID::UNKNOWN_GAME;

  return current_game;
}


struct {
  struct {
    sk::ParameterBool*    show;
  } time;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
  } io;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterBool*    frametime;
    sk::ParameterBool*    advanced;
  } fps;

  struct {
    sk::ParameterBool*    show;
  } memory;

  struct {
    sk::ParameterBool*    show;
  } SLI;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
    sk::ParameterBool*    simple;
  } cpu;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterBool*    print_slowdown;
    sk::ParameterFloat*   interval;
  } gpu;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
    sk::ParameterInt*     type;
  } disk;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
  } pagefile;
} monitoring;

struct {
  struct {
    sk::ParameterFloat*   duration;
  } version_banner;


  sk::ParameterBool*      show;

  struct {
    sk::ParameterBool*    pump;
    sk::ParameterFloat*   pump_interval;
  } update_method;

  struct {
    sk::ParameterInt*     red;
    sk::ParameterInt*     green;
    sk::ParameterInt*     blue;
  } text;

  struct {
    sk::ParameterFloat*   scale;
    sk::ParameterInt*     pos_x;
    sk::ParameterInt*     pos_y;
  } viewport;

  struct {
    sk::ParameterBool*    remember;
  } state;
} osd;

struct {
  sk::ParameterFloat*     scale;
  sk::ParameterBool*      show_eula;
  sk::ParameterBool*      show_playtime;
  sk::ParameterBool*      mac_style_menu;
  sk::ParameterBool*      show_gsync_status;
  sk::ParameterBool*      show_input_apis;
  sk::ParameterBool*      disable_alpha;
  sk::ParameterBool*      antialias_lines;
  sk::ParameterBool*      antialias_contours;
} imgui;

struct {
  struct {
    sk::ParameterStringW*   sound_file;
    sk::ParameterBool*      play_sound;
    sk::ParameterBool*      take_screenshot;
    sk::ParameterBool*      fetch_friend_stats;

    struct {
      sk::ParameterBool*    show;
      sk::ParameterBool*    show_title;
      sk::ParameterBool*    animate;
      sk::ParameterStringW* origin;
      sk::ParameterFloat*   inset;
      sk::ParameterInt*     duration;
    } popup;
  } achievements;

  struct {
    sk::ParameterInt*     appid;
    sk::ParameterInt*     init_delay;
    sk::ParameterBool*    auto_pump;
    sk::ParameterStringW* notify_corner;
    sk::ParameterBool*    block_stat_callback;
    sk::ParameterBool*    filter_stat_callbacks;
    sk::ParameterBool*    load_early;
    sk::ParameterBool*    early_overlay;
    sk::ParameterBool*    force_load;
  } system;

  struct {
    sk::ParameterBool*    silent;
  } log;

  struct {
    sk::ParameterBool*    spoof_BLoggedOn;
  } drm;

  struct {
    sk::ParameterStringW* blacklist;
  } cloud;
} steam;

struct
{
  struct
  {
    sk::ParameterBool*    use_static_addresses;
    bool                  has_local_preference = false;
  } global;
} injection;

struct {
  struct {
    sk::ParameterBool*    override;
    sk::ParameterStringW* compatibility;
    sk::ParameterStringW* num_gpus;
    sk::ParameterStringW* mode;
  } sli;

  struct {
    sk::ParameterBool*    disable;
  } api;
} nvidia;

struct {
  struct {
    sk::ParameterBool*    disable;
  } adl;
} amd;

sk::ParameterBool*        enable_cegui;
sk::ParameterBool*        safe_cegui;
sk::ParameterFloat*       mem_reserve;
sk::ParameterBool*        debug_output;
sk::ParameterBool*        game_output;
sk::ParameterBool*        handle_crashes;
sk::ParameterBool*        prefer_fahrenheit;
sk::ParameterBool*        ignore_rtss_delay;
sk::ParameterInt*         init_delay;
sk::ParameterInt*         log_level;
sk::ParameterBool*        trace_libraries;
sk::ParameterBool*        strict_compliance;
sk::ParameterBool*        silent;
sk::ParameterStringW*     version;

struct {
  struct {
    sk::ParameterFloat*   target_fps;
    sk::ParameterFloat*   limiter_tolerance;
    sk::ParameterInt*     prerender_limit;
    sk::ParameterInt*     present_interval;
    sk::ParameterInt*     buffer_count;
    sk::ParameterInt*     max_delta_time;
    sk::ParameterBool*    flip_discard;
    sk::ParameterInt*     refresh_rate;
    sk::ParameterBool*    wait_for_vblank;
    sk::ParameterBool*    allow_dwm_tearing;
    sk::ParameterBool*    sleepless_window;
    sk::ParameterBool*    sleepless_render;

    struct
    {
      sk::ParameterBool*  busy_wait;
      sk::ParameterBool*  yield_once;
      sk::ParameterBool*  minimize_latency;
      sk::ParameterFloat* sleep_scale;
      sk::ParameterFloat* deadline_transition;
    } control;
  } framerate;
  struct {
    sk::ParameterInt*     adapter_override;
    sk::ParameterStringW* max_res;
    sk::ParameterStringW* min_res;
    sk::ParameterInt*     swapchain_wait;
    sk::ParameterStringW* scaling_mode;
    sk::ParameterStringW* exception_mode;
    sk::ParameterStringW* scanline_order;
    sk::ParameterStringW* rotation;
    sk::ParameterBool*    test_present;
    sk::ParameterBool*    debug_layer;
    sk::ParameterBool*    safe_fullscreen;
    sk::ParameterBool*    enhanced_depth;
    sk::ParameterBool*    deferred_isolation;
    sk::ParameterBool*    rehook_present;
    sk::ParameterInt*     alternate_hook;
  } dxgi;
  struct {
    sk::ParameterBool*    force_d3d9ex;
    sk::ParameterInt*     hook_type;
    sk::ParameterBool*    impure;
    sk::ParameterBool*    enable_texture_mods;
  } d3d9;
} render;

struct {
  sk::ParameterBool*      force_fullscreen;
  sk::ParameterBool*      force_windowed;
} display;

struct {
  struct {
    sk::ParameterBool*    precise_hash;
    sk::ParameterBool*    dump;
    sk::ParameterBool*    inject;
    sk::ParameterBool*    injection_keeps_format;
    sk::ParameterBool*    gen_mips;
    sk::ParameterBool*    cache;
    sk::ParameterStringW* res_root;
  } d3d11;
  struct {
    sk::ParameterInt*     min_evict;
    sk::ParameterInt*     max_evict;
    sk::ParameterInt*     min_size;
    sk::ParameterInt*     max_size;
    sk::ParameterInt*     min_entries;
    sk::ParameterInt*     max_entries;
    sk::ParameterBool*    ignore_non_mipped;
    sk::ParameterBool*    allow_staging;
  } cache;
    sk::ParameterStringW* res_root;
    sk::ParameterBool*    dump_on_load;
} texture;

struct {
  struct
  {
    sk::ParameterBool*    catch_alt_f4;
    sk::ParameterBool*    disabled_to_game;
  } keyboard;

  struct
  {
    sk::ParameterBool*    disabled_to_game;
  } mouse;

  struct {
    sk::ParameterBool*    manage;
    sk::ParameterBool*    keys_activate;
    sk::ParameterFloat*   timeout;
    sk::ParameterBool*    ui_capture;
    sk::ParameterBool*    hw_cursor;
    sk::ParameterBool*    no_warp_ui;
    sk::ParameterBool*    no_warp_visible;
    sk::ParameterBool*    block_invisible;
    sk::ParameterBool*    fix_synaptics;
    sk::ParameterBool*    use_relative_input;
    sk::ParameterFloat*   antiwarp_deadzone;
  } cursor;

  struct {
    sk::ParameterBool*    disable_ps4_hid;
    sk::ParameterBool*    rehook_xinput;
    sk::ParameterBool*    haptic_ui;
    sk::ParameterBool*    disable_rumble;
    sk::ParameterBool*    hook_dinput8;
    sk::ParameterBool*    hook_hid;
    sk::ParameterBool*    hook_xinput;

    struct {
      sk::ParameterInt*     ui_slot;
      sk::ParameterInt*     placeholders;
      sk::ParameterStringW* assignment;
    } xinput;

    struct {
      sk::ParameterInt*     ui_slot;
    } steam;

    sk::ParameterBool*   native_ps4;
    sk::ParameterBool*   disabled_to_game;
  } gamepad;
} input;

struct {
  sk::ParameterBool*      borderless;
  sk::ParameterBool*      center;
  struct {
    sk::ParameterStringW* x;
    sk::ParameterStringW* y;
  } offset;
  sk::ParameterBool*      background_render;
  sk::ParameterBool*      background_mute;
  sk::ParameterBool*      confine_cursor;
  sk::ParameterBool*      unconfine_cursor;
  sk::ParameterBool*      persistent_drag;
  sk::ParameterBool*      fullscreen;
  sk::ParameterStringW*   override;
  sk::ParameterBool*      fix_mouse_coords;
  sk::ParameterInt*       always_on_top;
  sk::ParameterBool*      disable_screensaver;
} window;

struct {
  sk::ParameterBool*      rehook_loadlibrary;
  sk::ParameterBool*      disable_nv_bloat;

  struct {
    sk::ParameterBool*    rehook_reset;
    sk::ParameterBool*    rehook_present;
    sk::ParameterBool*    hook_reset_vtable;
    sk::ParameterBool*    hook_present_vtable;
  } d3d9;
} compatibility;

struct {
  struct {
    sk::ParameterBool*    hook;
  }
#ifndef _WIN64
      ddraw, d3d8,
#endif
      d3d9,  d3d9ex,
      d3d11,
#ifdef _WIN64
      d3d12,
      Vulkan,
#endif
      OpenGL;

  sk::ParameterInt*       last_known;
} apis;

bool
SK_LoadConfig (std::wstring name) {
  return SK_LoadConfigEx (name);
}


SK_AppCache_Manager app_cache_mgr;


__declspec (noinline)
const wchar_t*
__stdcall
SK_GetConfigPath (void)
{
  if ((! SK_IsInjected ()) && (! config.system.central_repository))
    return SK_GetNaiveConfigPath ();

  static bool init = false;

  if (! init)
  {
    app_cache_mgr.loadAppCacheForExe (SK_GetFullyQualifiedApp ());
    init = true;
  }

  static std::wstring path =
    app_cache_mgr.getConfigPathFromAppPath (SK_GetFullyQualifiedApp ());

  return path.c_str ();
}



template <typename _Tp>
_Tp*
SK_CreateINIParameter ( const wchar_t *wszDescription,
                        iSK_INI       *pINIFile,
                        const wchar_t *wszSection,
                        const wchar_t *wszKey )
{
  assert (std::is_polymorphic <_Tp> ());

  auto ret =
    dynamic_cast <_Tp *> (
      g_ParameterFactory.create_parameter <_Tp::value_type> (
        wszDescription )
    );

  ret->register_to_ini (pINIFile, wszSection, wszKey);

  return ret;
};


bool
SK_LoadConfigEx (std::wstring name, bool create)
{
  // Load INI File
  std::wstring full_name;
  std::wstring custom_name; // User may have custom prefs

  std::wstring osd_config, achievement_config, macro_config;

  full_name = SK_GetConfigPath () +
                name              +
                  L".ini";

  std::wstring undecorated_name = name;

  if (undecorated_name.find (L"default_") != std::wstring::npos)
  {
    undecorated_name.erase ( undecorated_name.find (L"default_"),
                             std::wstring (L"default_").length () );
  }

  custom_name = std::wstring   (SK_GetConfigPath ()) +
                  std::wstring (L"custom_")          +
                    undecorated_name                 +
                      L".ini";

  if (create)
    SK_CreateDirectories (full_name.c_str ());

  static LONG         init     = FALSE;
  static bool         empty    = true;
  static std::wstring last_try = name;

  // Allow a second load attempt using a different name
  if (last_try != name)
  {
    init     = FALSE;
    last_try = name;
  }

  osd_config =
    SK_GetDocumentsDir () + LR"(\My Mods\SpecialK\Global\osd.ini)";

  achievement_config =
    SK_GetDocumentsDir () + LR"(\My Mods\SpecialK\Global\achievements.ini)";

  macro_config =
    SK_GetDocumentsDir () + LR"(\My Mods\SpecialK\Global\macros.ini)";

  while (init < 0)
    SleepEx (15, FALSE);

  if (init == FALSE)
  {
    init = -1;

    dll_ini =
      SK_CreateINI (full_name.c_str ());

    empty = dll_ini->get_sections ().empty ();

    SK_CreateDirectories (osd_config.c_str ());

    osd_ini =
      SK_CreateINI (osd_config.c_str ());

    achievement_ini =
      SK_CreateINI (achievement_config.c_str ());

    macro_ini =
      SK_CreateINI (macro_config.c_str ());

  #define ConfigEntry(param,descrip,ini,sec,key) { (sk::iParameter **)&(param), std::type_index (typeid ((param))), (descrip), (ini), (sec), (key) }

  //
  // Create Parameters
  //
  struct param_decl_s {
    sk::iParameter** parameter_;
    std::type_index  type_;
    const wchar_t*   description_;
    iSK_INI*         ini_;
    const wchar_t*   section_;
    const wchar_t*   key_;
  } params_to_build [] =

  //// nb: If you want any hope of reading this table, turn line wrapping off.
    //
  {
    ConfigEntry (osd.version_banner.duration,            L"How long to display version info at startup, 0=disable)",   osd_ini,         L"SpecialK.VersionBanner",L"Duration"),
    ConfigEntry (osd.show,                               L"OSD Visibility",                                            osd_ini,         L"SpecialK.OSD",          L"Show"),

    ConfigEntry (osd.update_method.pump,                 L"Refresh the OSD text irrespective of frame completion",     osd_ini,         L"SpecialK.OSD",          L"AutoPump"),
    ConfigEntry (osd.update_method.pump_interval,        L"Time in seconds between OSD updates",                       osd_ini,         L"SpecialK.OSD",          L"PumpInterval"),
    ConfigEntry (osd.text.red,                           L"OSD Color (Red)",                                           osd_ini,         L"SpecialK.OSD",          L"TextColorRed"),
    ConfigEntry (osd.text.green,                         L"OSD Color (Green)",                                         osd_ini,         L"SpecialK.OSD",          L"TextColorGreen"),
    ConfigEntry (osd.text.blue,                          L"OSD Color (Blue)",                                          osd_ini,         L"SpecialK.OSD",          L"TextColorBlue"),

    ConfigEntry (osd.viewport.pos_x,                     L"OSD Position (X)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionX"),
    ConfigEntry (osd.viewport.pos_y,                     L"OSD Position (Y)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionY"),

    ConfigEntry (osd.viewport.scale,                     L"OSD Scale",                                                 osd_ini,         L"SpecialK.OSD",          L"Scale"),

    ConfigEntry (osd.state.remember,                     L"Remember status monitoring state",                          osd_ini,         L"SpecialK.OSD",          L"RememberMonitoringState"),

    ConfigEntry (monitoring.SLI.show,                    L"Show SLI Monitoring",                                       osd_ini,         L"Monitor.SLI",           L"Show"),

    // Performance Monitoring  (Global Settings)
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (monitoring.io.show,                     L"Show IO Monitoring",                                        osd_ini,         L"Monitor.IO",            L"Show"),
    ConfigEntry (monitoring.io.interval,                 L"IO Monitoring Interval",                                    osd_ini,         L"Monitor.IO",            L"Interval"),

    ConfigEntry (monitoring.disk.show,                   L"Show Disk Monitoring",                                      osd_ini,         L"Monitor.Disk",          L"Show"),
    ConfigEntry (monitoring.disk.interval,               L"Disk Monitoring Interval",                                  osd_ini,         L"Monitor.Disk",          L"Interval"),
    ConfigEntry (monitoring.disk.type,                   L"Disk Monitoring Type (0 = Physical, 1 = Logical)",          osd_ini,         L"Monitor.Disk",          L"Type"),

    ConfigEntry (monitoring.cpu.show,                    L"Show CPU Monitoring",                                       osd_ini,         L"Monitor.CPU",           L"Show"),
    ConfigEntry (monitoring.cpu.interval,                L"CPU Monitoring Interval (seconds)",                         osd_ini,         L"Monitor.CPU",           L"Interval"),
    ConfigEntry (monitoring.cpu.simple,                  L"Minimal CPU Info",                                          osd_ini,         L"Monitor.CPU",           L"Simple"),

    ConfigEntry (monitoring.gpu.show,                    L"Show GPU Monitoring",                                       osd_ini,         L"Monitor.GPU",           L"Show"),
    ConfigEntry (monitoring.gpu.interval,                L"GPU Monitoring Interval (msecs)",                           osd_ini,         L"Monitor.GPU",           L"Interval"),
    ConfigEntry (monitoring.gpu.print_slowdown,          L"Print GPU Slowdown Reason (NVIDIA GPUs)",                   osd_ini,         L"Monitor.GPU",           L"PrintSlowdown"),

    ConfigEntry (monitoring.pagefile.show,               L"Show Pagefile Monitoring",                                  osd_ini,         L"Monitor.Pagefile",      L"Show"),
    ConfigEntry (monitoring.pagefile.interval,           L"Pagefile Monitoring INterval (seconds)",                    osd_ini,         L"Monitor.Pagefile",      L"Interval"),

    ConfigEntry (monitoring.memory.show,                 L"Show Memory Monitoring",                                    osd_ini,         L"Monitor.Memory",        L"Show"),
    ConfigEntry (monitoring.fps.show,                    L"Show Framerate Monitoring",                                 osd_ini,         L"Monitor.FPS",           L"Show"),
    ConfigEntry (monitoring.fps.frametime,               L"Show Frametime in Framerate Counter",                       osd_ini,         L"Monitor.FPS",           L"DisplayFrametime"),
    ConfigEntry (monitoring.fps.advanced,                L"Show Advanced Statistics in Framerate Counter",             osd_ini,         L"Monitor.FPS",           L"AdvancedStatistics"),
    ConfigEntry (monitoring.time.show,                   L"Show System Clock",                                         osd_ini,         L"Monitor.Time",          L"Show"),

    ConfigEntry (prefer_fahrenheit,                      L"Prefer Fahrenheit Units",                                   osd_ini,         L"SpecialK.OSD",          L"PreferFahrenheit"),

    ConfigEntry (imgui.scale,                            L"ImGui Scale",                                               osd_ini,         L"ImGui.Global",          L"FontScale"),
    ConfigEntry (imgui.show_playtime,                    L"Display Playing Time in Config UI",                         osd_ini,         L"ImGui.Global",          L"ShowPlaytime"),
    ConfigEntry (imgui.show_gsync_status,                L"Show G-Sync Status on Control Panel",                       osd_ini,         L"ImGui.Global",          L"ShowGSyncStatus"),
    ConfigEntry (imgui.mac_style_menu,                   L"Use Mac-style Menu Bar",                                    osd_ini,         L"ImGui.Global",          L"UseMacStyleMenu"),
    ConfigEntry (imgui.show_input_apis,                  L"Show Input APIs currently in-use",                          osd_ini,         L"ImGui.Global",          L"ShowActiveInputAPIs"),


    // Input
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (input.keyboard.catch_alt_f4,            L"If the game does not handle Alt+F4, offer a replacement",   dll_ini,         L"Input.Keyboard",        L"CatchAltF4"),
    ConfigEntry (input.keyboard.disabled_to_game,        L"Completely stop all keyboard input from reaching the Game", dll_ini,         L"Input.Keyboard",        L"DisabledToGame"),

    ConfigEntry (input.mouse.disabled_to_game,           L"Completely stop all mouse input from reaching the Game",    dll_ini,         L"Input.Mouse",           L"DisabledToGame"),

    ConfigEntry (input.cursor.manage,                    L"Manage Cursor Visibility (due to inactivity)",              dll_ini,         L"Input.Cursor",          L"Manage"),
    ConfigEntry (input.cursor.keys_activate,             L"Keyboard Input Activates Cursor",                           dll_ini,         L"Input.Cursor",          L"KeyboardActivates"),
    ConfigEntry (input.cursor.timeout,                   L"Inactivity Timeout (in milliseconds)",                      dll_ini,         L"Input.Cursor",          L"Timeout"),
    ConfigEntry (input.cursor.ui_capture,                L"Forcefully Capture Mouse Cursor in UI Mode",                dll_ini,         L"Input.Cursor",          L"ForceCaptureInUI"),
    ConfigEntry (input.cursor.hw_cursor,                 L"Use a Hardware Cursor for Special K's UI Features",         dll_ini,         L"Input.Cursor",          L"UseHardwareCursor"),
    ConfigEntry (input.cursor.block_invisible,           L"Block Mouse Input if Hardware Cursor is Invisible",         dll_ini,         L"Input.Cursor",          L"BlockInvisibleCursorInput"),
    ConfigEntry (input.cursor.fix_synaptics,             L"Fix Synaptic Touchpad Scroll",                              dll_ini,         L"Input.Cursor",          L"FixSynapticsTouchpadScroll"),
    ConfigEntry (input.cursor.use_relative_input,        L"Use Raw Input Relative Motion if Needed",                   dll_ini,         L"Input.Cursor",          L"UseRelativeInput"),
    ConfigEntry (input.cursor.antiwarp_deadzone,         L"Percentage of Screen that the game may try to move the "
                                                         L"cursor to for mouselook.",                                  dll_ini,         L"Input.Cursor",          L"AntiwarpDeadzonePercent"),
    ConfigEntry (input.cursor.no_warp_ui,                L"Prevent Games from Warping Cursor while Config UI is Open", dll_ini,         L"Input.Cursor",          L"NoWarpUI"),
    ConfigEntry (input.cursor.no_warp_visible,           L"Prevent Games from Warping Cursor while Cursor is Visible", dll_ini,         L"Input.Cursor",          L"NoWarpVisibleGameCursor"),

    ConfigEntry (input.gamepad.disabled_to_game,         L"Disable ALL Gamepad Input (acrossall APIs)",                dll_ini,         L"Input.Gamepad",         L"DisabledToGame"),
    ConfigEntry (input.gamepad.disable_ps4_hid,          L"Disable PS4 HID Interface (prevent double-input)",          dll_ini,         L"Input.Gamepad",         L"DisablePS4HID"),
    ConfigEntry (input.gamepad.haptic_ui,                L"Give tactile feedback on gamepads when navigating the UI",  dll_ini,         L"Input.Gamepad",         L"AllowHapticUI"),
    ConfigEntry (input.gamepad.hook_dinput8,             L"Install hooks for DirectInput 8",                           dll_ini,         L"Input.Gamepad",         L"EnableDirectInput8"),
    ConfigEntry (input.gamepad.hook_hid,                 L"Install hooks for HID",                                     dll_ini,         L"Input.Gamepad",         L"EnableHID"),
    ConfigEntry (input.gamepad.native_ps4,               L"Native PS4 Mode (temporary)",                               dll_ini,         L"Input.Gamepad",         L"EnableNativePS4"),
    ConfigEntry (input.gamepad.disable_rumble,           L"Disable Rumble from ALL SOURCES (across all APIs)",         dll_ini,         L"Input.Gamepad",         L"DisableRumble"),

    ConfigEntry (input.gamepad.hook_xinput,              L"Install hooks for XInput",                                  dll_ini,         L"Input.XInput",          L"Enable"),
    ConfigEntry (input.gamepad.rehook_xinput,            L"Re-install XInput hooks if hookchain is modified",          dll_ini,         L"Input.XInput",          L"Rehook"),
    ConfigEntry (input.gamepad.xinput.ui_slot,           L"XInput Controller that owns the config UI",                 dll_ini,         L"Input.XInput",          L"UISlot"),
    ConfigEntry (input.gamepad.xinput.placeholders,      L"XInput Controller Slots to Fake Connectivity On",           dll_ini,         L"Input.XInput",          L"PlaceholderMask"),
    ConfigEntry (input.gamepad.xinput.assignment,        L"Re-Assign XInput Slots",                                    dll_ini,         L"Input.XInput",          L"SlotReassignment"),
  //DEPRECATED  (                                                                                                                       L"Input.XInput",          L"DisableRumble"),

    ConfigEntry (input.gamepad.steam.ui_slot,            L"Steam Controller that owns the config UI",                  dll_ini,         L"Input.Steam",           L"UISlot"),


    // Window Management
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (window.borderless,                      L"Borderless Window Mode",                                    dll_ini,         L"Window.System",         L"Borderless"),
    ConfigEntry (window.center,                          L"Center the Window",                                         dll_ini,         L"Window.System",         L"Center"),
    ConfigEntry (window.background_render,               L"Render While Window is in Background",                      dll_ini,         L"Window.System",         L"RenderInBackground"),
    ConfigEntry (window.background_mute,                 L"Mute While Window is in Background",                        dll_ini,         L"Window.System",         L"MuteInBackground"),
    ConfigEntry (window.offset.x,                        L"X Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"XOffset"),
    ConfigEntry (window.offset.y,                        L"Y Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"YOffset"),
    ConfigEntry (window.confine_cursor,                  L"Confine the Mouse Cursor to the Game Window",               dll_ini,         L"Window.System",         L"ConfineCursor"),
    ConfigEntry (window.unconfine_cursor,                L"Unconfine the Mouse Cursor from the Game Window",           dll_ini,         L"Window.System",         L"UnconfineCursor"),
    ConfigEntry (window.persistent_drag,                 L"Remember where the window is dragged to",                   dll_ini,         L"Window.System",         L"PersistentDragPos"),
    ConfigEntry (window.fullscreen,                      L"Make the Game Window Fill the Screen (scale to fit)",       dll_ini,         L"Window.System",         L"Fullscreen"),
    ConfigEntry (window.override,                        L"Force the Client Region to this Size in Windowed Mode",     dll_ini,         L"Window.System",         L"OverrideRes"),
    ConfigEntry (window.fix_mouse_coords,                L"Re-Compute Mouse Coordinates for Resized Windows",          dll_ini,         L"Window.System",         L"FixMouseCoords"),
    ConfigEntry (window.always_on_top,                   L"Prevent (0) or Force (1) a game's window Always-On-Top",    dll_ini,         L"Window.System",         L"AlwaysOnTop"),
    ConfigEntry (window.disable_screensaver,             L"Prevent the Windows Screensaver from activating",           dll_ini,         L"Window.System",         L"DisableScreensaver"),

    // Compatibility
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (compatibility.disable_nv_bloat,         L"Disable All NVIDIA BloatWare (GeForce Experience)",         dll_ini,         L"Compatibility.General", L"DisableBloatWare_NVIDIA"),
    ConfigEntry (compatibility.rehook_loadlibrary,       L"Rehook LoadLibrary When RTSS/Steam/ReShade hook it",        dll_ini,         L"Compatibility.General", L"RehookLoadLibrary"),

    ConfigEntry (apis.last_known,                        L"Last Known Render API",                                     dll_ini,         L"API.Hook",              L"LastKnown"),

#ifndef _WIN64
    ConfigEntry (apis.ddraw.hook,                        L"Enable DirectDraw Hooking",                                 dll_ini,         L"API.Hook",              L"ddraw"),
    ConfigEntry (apis.d3d8.hook,                         L"Enable Direct3D 8 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d8"),
#endif

    ConfigEntry (apis.d3d9.hook,                         L"Enable Direct3D 9 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d9"),
    ConfigEntry (apis.d3d9ex.hook,                       L"Enable Direct3D 9Ex Hooking",                               dll_ini,         L"API.Hook",              L"d3d9ex"),
    ConfigEntry (apis.d3d11.hook,                        L"Enable Direct3D 11 Hooking",                                dll_ini,         L"API.Hook",              L"d3d11"),

#ifdef _WIN64
    ConfigEntry (apis.d3d12.hook,                        L"Enable Direct3D 12 Hooking",                                dll_ini,         L"API.Hook",              L"d3d12"),
    ConfigEntry (apis.Vulkan.hook,                       L"Enable Vulkan Hooking",                                     dll_ini,         L"API.Hook",              L"Vulkan"),
#endif

    ConfigEntry (apis.OpenGL.hook,                       L"Enable OpenGL Hooking",                                     dll_ini,         L"API.Hook",              L"OpenGL"),


    // Misc.
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (mem_reserve,                            L"Memory Reserve Percentage",                                 dll_ini,         L"Manage.Memory",         L"ReservePercent"),


    // General Mod System Settings
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (init_delay,                             L"Initialization Delay (msecs)",                              dll_ini,         L"SpecialK.System",       L"InitDelay"),
    ConfigEntry (silent,                                 L"Log Silence",                                               dll_ini,         L"SpecialK.System",       L"Silent"),
    ConfigEntry (strict_compliance,                      L"Strict DLL Loader Compliance",                              dll_ini,         L"SpecialK.System",       L"StrictCompliant"),
    ConfigEntry (trace_libraries,                        L"Trace DLL Loading (needed for dynamic API detection)",      dll_ini,         L"SpecialK.System",       L"TraceLoadLibrary"),
    ConfigEntry (log_level,                              L"Log Verbosity (0=General, 5=Insane Debug)",                 dll_ini,         L"SpecialK.System",       L"LogLevel"),
    ConfigEntry (handle_crashes,                         L"Use Custom Crash Handler",                                  dll_ini,         L"SpecialK.System",       L"UseCrashHandler"),
    ConfigEntry (debug_output,                           L"Print Application's Debug Output in real-time",             dll_ini,         L"SpecialK.System",       L"DebugOutput"),
    ConfigEntry (game_output,                            L"Log Application's Debug Output",                            dll_ini,         L"SpecialK.System",       L"GameOutput"),
    ConfigEntry (ignore_rtss_delay,                      L"Ignore RTSS Delay Incompatibilities",                       dll_ini,         L"SpecialK.System",       L"IgnoreRTSSHookDelay"),
    ConfigEntry (enable_cegui,                           L"Enable CEGUI (lazy loading)",                               dll_ini,         L"SpecialK.System",       L"EnableCEGUI"),
    ConfigEntry (safe_cegui,                             L"Safely Initialize CEGUI",                                   dll_ini,         L"SpecialK.System",       L"SafeInitCEGUI"),
    ConfigEntry (version,                                L"The last version that wrote the config file",               dll_ini,         L"SpecialK.System",       L"Version"),


    
    ConfigEntry (display.force_fullscreen,               L"Force Fullscreen Mode",                                     dll_ini,         L"Display.Output",        L"ForceFullscreen"),
    ConfigEntry (display.force_windowed,                 L"Force Windowed Mode",                                       dll_ini,         L"Display.Output",        L"ForceWindowed"),


    // Framerate Limiter
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (render.framerate.target_fps,            L"Framerate Target (negative signed values are non-limiting)",dll_ini,         L"Render.FrameRate",      L"TargetFPS"),
    ConfigEntry (render.framerate.limiter_tolerance,     L"Limiter Tolerance",                                         dll_ini,         L"Render.FrameRate",      L"LimiterTolerance"),
    ConfigEntry (render.framerate.wait_for_vblank,       L"Limiter Will Wait for VBLANK",                              dll_ini,         L"Render.FrameRate",      L"WaitForVBLANK"),
    ConfigEntry (render.framerate.buffer_count,          L"Number of Backbuffers in the Swapchain",                    dll_ini,         L"Render.FrameRate",      L"BackBufferCount"),
    ConfigEntry (render.framerate.present_interval,      L"Presentation Interval (VSYNC)",                             dll_ini,         L"Render.FrameRate",      L"PresentationInterval"),
    ConfigEntry (render.framerate.prerender_limit,       L"Maximum Frames to Render-Ahead",                            dll_ini,         L"Render.FrameRate",      L"PreRenderLimit"),
    ConfigEntry (render.framerate.sleepless_render,      L"Sleep Free Render Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessRenderThread"),
    ConfigEntry (render.framerate.sleepless_window,      L"Sleep Free Window Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessWindowThread"),

    ConfigEntry (render.framerate.control.busy_wait,     L"Burn through the render thread's CPU time so that a game's"
                                                         L" own framerate limiter will never engage.",                 dll_ini,         L"FrameRate.Control",     L"AlwaysBusyWait"),
    ConfigEntry (render.framerate.control.yield_once,    L"Offer a portion of the render thread's CPU time and then "
                                                         L"switch to busy-wait heuristics.",                           dll_ini,         L"FrameRate.Control",     L"YieldThenSpin"),
    ConfigEntry (render.framerate.control.
                   minimize_latency,                     L"Maintain a healthy window message loop while waiting.",     dll_ini,         L"FrameRate.Control",     L"ReduceLatency"),
    ConfigEntry (render.framerate.control.sleep_scale,   L"Ratio of full-frame deadline to longest possible sleep.",   dll_ini,         L"FrameRate.Control",     L"SleepScale"),
    ConfigEntry (render.framerate.control.
                   deadline_transition,                  L"Switch to more accurate timing when deadline approaches.",  dll_ini,         L"FrameRate.Control",     L"DeadlineTransition"),

    ConfigEntry (render.framerate.refresh_rate,          L"Fullscreen Refresh Rate",                                   dll_ini,         L"Render.FrameRate",      L"RefreshRate"),
    ConfigEntry (render.framerate.allow_dwm_tearing,     L"Enable DWM Tearing (Windows 10+)",                          dll_ini,         L"Render.DXGI",           L"AllowTearingInDWM"),


    // D3D9
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (compatibility.d3d9.rehook_present,      L"Rehook D3D9 Present On Device Reset",                       dll_ini,         L"Compatibility.D3D9",    L"RehookPresent"),
    ConfigEntry (compatibility.d3d9.rehook_reset,        L"Rehook D3D9 Reset On Device Reset",                         dll_ini,         L"Compatibility.D3D9",    L"RehookReset"),
    ConfigEntry (compatibility.d3d9.hook_present_vtable, L"Use VFtable Override for Present",                          dll_ini,         L"Compatibility.D3D9",    L"UseVFTableForPresent"),
    ConfigEntry (compatibility.d3d9.hook_reset_vtable,   L"Use VFtable Override for Reset",                            dll_ini,         L"Compatibility.D3D9",    L"UseVFTableForReset"),

    ConfigEntry (render.d3d9.force_d3d9ex,               L"Force D3D9Ex Context",                                      dll_ini,         L"Render.D3D9",           L"ForceD3D9Ex"),
    ConfigEntry (render.d3d9.impure,                     L"Force PURE device off",                                     dll_ini,         L"Render.D3D9",           L"ForceImpure"),
    ConfigEntry (render.d3d9.enable_texture_mods,        L"Enable Texture Modding Support",                            dll_ini,         L"Render.D3D9",           L"EnableTextureMods"),
    ConfigEntry (render.d3d9.hook_type,                  L"Hook Technique",                                            dll_ini,         L"Render.D3D9",           L"HookType"),


    // D3D10/11/12
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (render.framerate.max_delta_time,        L"Maximum Frame Delta Time",                                  dll_ini,         L"Render.DXGI",           L"MaxDeltaTime"),
    ConfigEntry (render.framerate.flip_discard,          L"Use Flip Discard - Windows 10+",                            dll_ini,         L"Render.DXGI",           L"UseFlipDiscard"),

    ConfigEntry (render.dxgi.adapter_override,           L"Override DXGI Adapter",                                     dll_ini,         L"Render.DXGI",           L"AdapterOverride"),
    ConfigEntry (render.dxgi.max_res,                    L"Maximum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MaxRes"),
    ConfigEntry (render.dxgi.min_res,                    L"Minimum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MinRes"),

    ConfigEntry (render.dxgi.swapchain_wait,             L"Time to wait in msec. for SwapChain",                       dll_ini,         L"Render.DXGI",           L"SwapChainWait"),
    ConfigEntry (render.dxgi.scaling_mode,               L"Scaling Preference (DontCare | Centered | Stretched"
                                                         L" | Unspecified)",                                           dll_ini,         L"Render.DXGI",           L"Scaling"),
    ConfigEntry (render.dxgi.exception_mode,             L"D3D11 Exception Handling (DontCare | Raise | Ignore)",      dll_ini,         L"Render.DXGI",           L"ExceptionMode"),

    ConfigEntry (render.dxgi.debug_layer,                L"DXGI Debug Layer Support",                                  dll_ini,         L"Render.DXGI",           L"EnableDebugLayer"),
    ConfigEntry (render.dxgi.scanline_order,             L"Scanline Order (DontCare | Progressive | LowerFieldFirst |"
                                                         L" UpperFieldFirst )",                                        dll_ini,         L"Render.DXGI",           L"ScanlineOrder"),
    ConfigEntry (render.dxgi.rotation,                   L"Screen Rotation (DontCare | Identity | 90 | 180 | 270 )",   dll_ini,         L"Render.DXGI",           L"Rotation"),
    ConfigEntry (render.dxgi.test_present,               L"Test SwapChain Presentation Before Actually Presenting",    dll_ini,         L"Render.DXGI",           L"TestSwapChainPresent"),
    ConfigEntry (render.dxgi.safe_fullscreen,            L"Prevent DXGI Deadlocks in Improperly Written Games",        dll_ini,         L"Render.DXGI",           L"SafeFullscreenMode"),
    ConfigEntry (render.dxgi.enhanced_depth,             L"Use 32-bit Depth + 8-bit Stencil + 24-bit Padding",         dll_ini,         L"Render.DXGI",           L"Use64BitDepthStencil"),
    ConfigEntry (render.dxgi.deferred_isolation,         L"Isolate D3D11 Deferred Context Queues instead of Tracking"
                                                         L" in Immediate Mode.",                                       dll_ini,         L"Render.DXGI",           L"IsolateD3D11DeferredContexts"),
    ConfigEntry (render.dxgi.rehook_present,             L"Attempt to Fix Altered SwapChain Presentation Hooks",       dll_ini,         L"Render.DXGI",           L"RehookPresent"),
    ConfigEntry (render.dxgi.alternate_hook,             L"Use a Different Hook Setup Procedure (injection compat.)",  dll_ini,         L"Render.DXGI",           L"HookType"),


    ConfigEntry (texture.d3d11.cache,                    L"Cache Textures",                                            dll_ini,         L"Textures.D3D11",        L"Cache"),
    ConfigEntry (texture.d3d11.precise_hash,             L"Precise Hash Generation",                                   dll_ini,         L"Textures.D3D11",        L"PreciseHash"),

    ConfigEntry (texture.d3d11.dump,                     L"Dump Textures",                                             dll_ini,         L"Textures.D3D11",        L"Dump"),

    ConfigEntry (texture.d3d11.inject,                   L"Inject Textures",                                           dll_ini,         L"Textures.D3D11",        L"Inject"),
    ConfigEntry (texture.d3d11.res_root,                 L"Resource Root",                                             dll_ini,         L"Textures.D3D11",        L"ResourceRoot"),
    ConfigEntry (texture.d3d11.injection_keeps_format,   L"Allow image format to change during texture injection",     dll_ini,         L"Textures.D3D11",        L"InjectionKeepsFormat"),
    ConfigEntry (texture.d3d11.gen_mips,                 L"Create complete mipmap chain for textures without them",    dll_ini,         L"Textures.D3D11",        L"GenerateMipmaps"),
    ConfigEntry (texture.res_root,                       L"Resource Root",                                             dll_ini,         L"Textures.General",      L"ResourceRoot"),
    ConfigEntry (texture.dump_on_load,                   L"Dump Textures while Loading",                               dll_ini,         L"Textures.General",      L"DumpOnFirstLoad"),
    ConfigEntry (texture.cache.min_entries,              L"Minimum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MinEntries"),
    ConfigEntry (texture.cache.max_entries,              L"Maximum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MaxEntries"),
    ConfigEntry (texture.cache.min_evict,                L"Minimum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MinEvict"),
    ConfigEntry (texture.cache.max_evict,                L"Maximum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MaxEvict"),
    ConfigEntry (texture.cache.min_size,                 L"Minimum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MinSizeInMiB"),
    ConfigEntry (texture.cache.max_size,                 L"Maximum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MaxSizeInMiB"),

    ConfigEntry (texture.cache.ignore_non_mipped,        L"Ignore textures without mipmaps?",                          dll_ini,         L"Textures.Cache",        L"IgnoreNonMipmapped"),
    ConfigEntry (texture.cache.allow_staging,            L"Enable texture caching/dumping/injecting staged textures",  dll_ini,         L"Textures.Cache",        L"AllowStaging"),


    ConfigEntry (injection.global.use_static_addresses,  L"Use Cached Memory Addresses in Global\\injection.ini",      dll_ini,         L"Injection.Global",      L"UseStaticAddresses"),


    ConfigEntry (nvidia.api.disable,                     L"Disable NvAPI",                                             dll_ini,         L"NVIDIA.API",            L"Disable"),
    ConfigEntry (nvidia.sli.compatibility,               L"SLI Compatibility Bits",                                    dll_ini,         L"NVIDIA.SLI",            L"CompatibilityBits"),
    ConfigEntry (nvidia.sli.num_gpus,                    L"SLI GPU Count",                                             dll_ini,         L"NVIDIA.SLI",            L"NumberOfGPUs"),
    ConfigEntry (nvidia.sli.mode,                        L"SLI Mode",                                                  dll_ini,         L"NVIDIA.SLI",            L"Mode"),
    ConfigEntry (nvidia.sli.override,                    L"Override Driver Defaults",                                  dll_ini,         L"NVIDIA.SLI",            L"Override"),

    ConfigEntry (amd.adl.disable,                        L"Disable AMD's ADL library",                                 dll_ini,         L"AMD.ADL",               L"Disable"),

    ConfigEntry (imgui.show_eula,                        L"Show Software EULA",                                        dll_ini,         L"SpecialK.System",       L"ShowEULA"),
    ConfigEntry (imgui.disable_alpha,                    L"Disable Alpha Transparency (reduce flicker)",               dll_ini,         L"ImGui.Render",          L"DisableAlpha"),
    ConfigEntry (imgui.antialias_lines,                  L"Reduce Aliasing on (but dim) Line Edges",                   dll_ini,         L"ImGui.Render",          L"AntialiasLines"),
    ConfigEntry (imgui.antialias_contours,               L"Reduce Aliasing on (but widen) Window Borders",             dll_ini,         L"ImGui.Render",          L"AntialiasContours"),


    // The one odd-ball Steam achievement setting that can be specified per-game
    ConfigEntry (steam.achievements.sound_file,          L"Achievement Sound File",                                    dll_ini,         L"Steam.Achievements",    L"SoundFile"),

    // Steam Achievement Enhancements  (Global Settings)
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (steam.achievements.play_sound,          L"Silence is Bliss?",                                         achievement_ini, L"Steam.Achievements",    L"PlaySound"),
    ConfigEntry (steam.achievements.take_screenshot,     L"Precious Memories",                                         achievement_ini, L"Steam.Achievements",    L"TakeScreenshot"),
    ConfigEntry (steam.achievements.fetch_friend_stats,  L"Friendly Competition",                                      achievement_ini, L"Steam.Achievements",    L"FetchFriendStats"),
    ConfigEntry (steam.achievements.popup.origin,        L"Achievement Popup Position",                                achievement_ini, L"Steam.Achievements",    L"PopupOrigin"),
    ConfigEntry (steam.achievements.popup.animate,       L"Achievement Notification Animation",                        achievement_ini, L"Steam.Achievements",    L"AnimatePopup"),
    ConfigEntry (steam.achievements.popup.show_title,    L"Achievement Popup Includes Game Title?",                    achievement_ini, L"Steam.Achievements",    L"ShowPopupTitle"),
    ConfigEntry (steam.achievements.popup.inset,         L"Achievement Notification Inset X",                          achievement_ini, L"Steam.Achievements",    L"PopupInset"),
    ConfigEntry (steam.achievements.popup.duration,      L"Achievement Popup Duration (in ms)",                        achievement_ini, L"Steam.Achievements",    L"PopupDuration"),

    ConfigEntry (steam.system.notify_corner,             L"Overlay Notification Position  (non-Big Picture Mode)",     dll_ini,         L"Steam.System",          L"NotifyCorner"),
    ConfigEntry (steam.system.appid,                     L"Steam AppID",                                               dll_ini,         L"Steam.System",          L"AppID"),
    ConfigEntry (steam.system.init_delay,                L"Delay SteamAPI initialization if the game doesn't do it",   dll_ini,         L"Steam.System",          L"AutoInitDelay"),
    ConfigEntry (steam.system.auto_pump,                 L"Should we force the game to run Steam callbacks?",          dll_ini,         L"Steam.System",          L"AutoPumpCallbacks"),
    ConfigEntry (steam.system.block_stat_callback,       L"Block the User Stats Receipt Callback?",                    dll_ini,         L"Steam.System",          L"BlockUserStatsCallback"),
    ConfigEntry (steam.system.filter_stat_callbacks,     L"Filter Unrelated Data from the User Stats Receipt Callback",dll_ini,         L"Steam.System",          L"FilterExternalDataFromCallbacks"),
    ConfigEntry (steam.system.load_early,                L"Load the Steam Client DLL Early?",                          dll_ini,         L"Steam.System",          L"PreLoadSteamClient"),
    ConfigEntry (steam.system.early_overlay,             L"Load the Steam Overlay Early",                              dll_ini,         L"Steam.System",          L"PreLoadSteamOverlay"),
    ConfigEntry (steam.system.force_load,                L"Forcefully load steam_api{64}.dll",                         dll_ini,         L"Steam.System",          L"ForceLoadSteamAPI"),

    // Swashbucklers pay attention
    //////////////////////////////////////////////////////////////////////////

    ConfigEntry (steam.log.silent,                       L"Makes steam_api.log go away [DISABLES STEAMAPI FEATURES]",  dll_ini,         L"Steam.Log",             L"Silent"),
    ConfigEntry (steam.cloud.blacklist,                  L"CSV list of files to block from cloud sync.",               dll_ini,         L"Steam.Cloud",           L"FilesNotToSync"),
    ConfigEntry (steam.drm.spoof_BLoggedOn,              L"Fix For Stupid Games That Don't Know How DRM Works",        dll_ini,         L"Steam.DRMWorks",        L"SpoofBLoggedOn"),
  };


  for ( auto&& decl : params_to_build )
  {
    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterBool* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterBool> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterInt* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterInt> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterInt64* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterInt64> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterFloat* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterFloat> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterStringW* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterStringW> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }

    if ( decl.type_ == std::type_index ( typeid ( sk::ParameterVec2f* ) ) )
    {
      *decl.parameter_ =
        SK_CreateINIParameter <sk::ParameterVec2f> (decl.description_, decl.ini_, decl.section_, decl.key_);

      continue;
    }
  }

  iSK_INI::_TSectionMap& sections =
    dll_ini->get_sections ();

  auto&& sec =
    sections.begin ();

  int import = 0;

  host_executable.hLibrary     = GetModuleHandle     (nullptr);
  host_executable.product_desc = SK_GetDLLVersionStr (SK_GetModuleFullName (host_executable.hLibrary).c_str ());



  extern std::unordered_multimap <uint32_t, SK_KeyCommand> SK_KeyboardMacros;

  SK_KeyboardMacros.clear   ();



  if (GetFileAttributesW (custom_name.c_str ()) != INVALID_FILE_ATTRIBUTES)
  {
    dll_ini->import_file (custom_name.c_str ());
  }


  while (sec != sections.end ())
  {
    if (wcsstr ((*sec).first.c_str (), L"Import."))
    {
      imports [import].name =
        CharNextW (wcsstr ((*sec).first.c_str (), L"."));

      imports [import].filename =
         dynamic_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Filename")
             );
      imports [import].filename->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Filename" );

      imports [import].when =
         dynamic_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Timeframe")
             );
      imports [import].when->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"When" );

      imports [import].role =
         dynamic_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Role")
             );
      imports [import].role->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Role" );

      imports [import].architecture =
         dynamic_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Architecture")
             );
      imports [import].architecture->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Architecture" );

      imports [import].blacklist =
         dynamic_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Blacklisted Executables")
             );
      imports [import].blacklist->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Blacklist" );

      ((sk::iParameter *)imports [import].filename)->load     ();
      ((sk::iParameter *)imports [import].when)->load         ();
      ((sk::iParameter *)imports [import].role)->load         ();
      ((sk::iParameter *)imports [import].architecture)->load ();
      ((sk::iParameter *)imports [import].blacklist)->load    ();

      imports [import].hLibrary = nullptr;

      ++import;

      if (import > SK_MAX_IMPORTS)
        break;
    }



    if (wcsstr ((*sec).first.c_str (), L"Macro."))
    {
      for ( auto it : (*sec).second.keys )
      {
        SK_KeyCommand cmd;

        cmd.binding.human_readable = it.first;
        cmd.binding.parse ();

        cmd.command = it.second;

        SK_KeyboardMacros.emplace (cmd.binding.masked_code, cmd);
      }
    }



    ++sec;
  }



  //
  // Load Global Macro Table
  //
  if (macro_ini->get_sections ().empty ())
  {
    macro_ini->import ( L"[Macro.SpecialK_OSD_Toggles]\n"
                        L"Ctrl+Shift+O=OSD.Show toggle\n"
                        L"Ctrl+Shift+M=OSD.Memory.Show toggle\n"
                        L"Ctrl+Shift+T=OSD.Clock.Show toggle\n"
                        L"Ctrl+Shift+I=OSD.IOPS.Show toggle\n"
                        L"Ctrl+Shift+F=OSD.FPS.Show toggle\n"
                        L"Ctrl+Shift+G=OSD.GPU.Show toggle\n"
                        L"Ctrl+Shift+R=OSD.Shaders.Show toggle\n"
                        L"Ctrl+Alt+Shift+D=OSD.Disk.Show toggle\n"
                        L"Ctrl+Alt+Shift+P=OSD.Pagefile.Show toggle\n"
                        L"Ctrl+Alt+Shift+S=OSD.SLI.Show toggle\n"
                        L"Ctrl+Shift+C=OSD.CPU.Show toggle\n\n"
                      );
                        //L"[Macro.SpecialK_CommandConsole]\n"
                        //L"Ctrl+Shift+Tab=Console.Show toggle\n\n" );
    macro_ini->write (macro_ini->get_filename ());
  }

  auto& macro_sections =
    macro_ini->get_sections ();

  sec =
    macro_sections.begin ();

  while (sec != macro_sections.end ())
  {
    if (wcsstr ((*sec).first.c_str (), L"Macro."))
    {
      for ( auto it : (*sec).second.keys )
      {
        SK_KeyCommand cmd;

        cmd.binding.human_readable = it.first;
        cmd.binding.parse ();

        cmd.command = it.second;

        SK_KeyboardMacros.emplace (cmd.binding.masked_code, cmd);
      }
    }

    ++sec;
  }


  config.window.border_override    = true;


                                            //
  config.system.trace_load_library = true;  // Generally safe even with the
                                            //   worst third-party software;
                                            //
                                            //  NEEDED for injector API detect


  config.system.strict_compliance  = false; // Will deadlock in DLLs that call
                                            //   LoadLibrary from DllMain
                                            //
                                            //  * NVIDIA Ansel, MSI Nahimic,
                                            //      Razer *, RTSS (Sometimes)
                                            //

  extern bool SK_DXGI_FullStateCache;
              SK_DXGI_FullStateCache = config.render.dxgi.full_state_cache;


  // Default = Don't Care
  config.render.dxgi.exception_mode = -1;
  config.render.dxgi.scaling_mode   = -1;

  games.emplace ( L"Tyranny.exe",                            SK_GAME_ID::Tyranny                      );
  games.emplace ( L"SRHK.exe",                               SK_GAME_ID::Shadowrun_HongKong           );
  games.emplace ( L"TidesOfNumenera.exe",                    SK_GAME_ID::TidesOfNumenera              );
  games.emplace ( L"MassEffectAndromeda.exe",                SK_GAME_ID::MassEffect_Andromeda         );
  games.emplace ( L"MadMax.exe",                             SK_GAME_ID::MadMax                       );
  games.emplace ( L"Dreamfall Chapters.exe",                 SK_GAME_ID::Dreamfall_Chapters           );
  games.emplace ( L"TheWitness.exe",                         SK_GAME_ID::TheWitness                   );
  games.emplace ( L"Obduction-Win64-Shipping.exe",           SK_GAME_ID::Obduction                    );
  games.emplace ( L"witcher3.exe",                           SK_GAME_ID::TheWitcher3                  );
  games.emplace ( L"re7.exe",                                SK_GAME_ID::ResidentEvil7                );
  games.emplace ( L"DDDA.exe",                               SK_GAME_ID::DragonsDogma                 );
  games.emplace ( L"eqgame.exe",                             SK_GAME_ID::EverQuest                    );
  games.emplace ( L"GE2RB.exe",                              SK_GAME_ID::GodEater2RageBurst           );
  games.emplace ( L"WatchDogs2.exe",                         SK_GAME_ID::WatchDogs2                   );
  games.emplace ( L"NieRAutomata.exe",                       SK_GAME_ID::NieRAutomata                 );
  games.emplace ( L"Warframe.x64.exe",                       SK_GAME_ID::Warframe_x64                 );
  games.emplace ( L"LEGOLCUR_DX11.exe",                      SK_GAME_ID::LEGOCityUndercover           );
  games.emplace ( L"Sacred.exe",                             SK_GAME_ID::Sacred                       );
  games.emplace ( L"sacred2.exe",                            SK_GAME_ID::Sacred2                      );
  games.emplace ( L"FF9.exe",                                SK_GAME_ID::FinalFantasy9                );
  games.emplace ( L"FinchGame.exe",                          SK_GAME_ID::EdithFinch                   );
  games.emplace ( L"FFX.exe",                                SK_GAME_ID::FinalFantasyX_X2             );
  games.emplace ( L"FFX-2.exe",                              SK_GAME_ID::FinalFantasyX_X2             );
  games.emplace ( L"DP.exe",                                 SK_GAME_ID::DeadlyPremonition            );
  games.emplace ( L"GG2Game.exe",                            SK_GAME_ID::GalGun_Double_Peace          );
  games.emplace ( L"AkibaUU.exe",                            SK_GAME_ID::AKIBAs_Trip                  );
  games.emplace ( L"Ys7.exe",                                SK_GAME_ID::YS_Seven                     );
  games.emplace ( L"TOS.exe",                                SK_GAME_ID::Tales_of_Symphonia           );
  games.emplace ( L"Life is Strange - Before the Storm.exe", SK_GAME_ID::LifeIsStrange_BeforeTheStorm );
  games.emplace ( L"EoCApp.exe",                             SK_GAME_ID::DivinityOriginalSin          );
  games.emplace ( L"Hob.exe",                                SK_GAME_ID::Hob                          );
  games.emplace ( L"DukeForever.exe",                        SK_GAME_ID::DukeNukemForever             );
  games.emplace ( L"BLUE_REFLECTION.exe",                    SK_GAME_ID::BlueReflection               );
  games.emplace ( L"Zero Escape.exe",                        SK_GAME_ID::ZeroEscape                   );
  games.emplace ( L"hackGU.exe",                             SK_GAME_ID::DotHackGU                    );
  games.emplace ( L"WOFF.exe",                               SK_GAME_ID::WorldOfFinalFantasy          );
  games.emplace ( L"StarOceanTheLastHope.exe",               SK_GAME_ID::StarOcean4                   );
  games.emplace ( L"LEGOMARVEL2_DX11.exe",                   SK_GAME_ID::LEGOMarvelSuperheroes2       );
  games.emplace ( L"okami.exe",                              SK_GAME_ID::Okami                        );
  games.emplace ( L"DuckTales.exe",                          SK_GAME_ID::DuckTalesRemastered          );
  games.emplace ( L"mafia3.exe",                             SK_GAME_ID::Mafia3                       );
  games.emplace ( L"Owlboy.exe",                             SK_GAME_ID::Owlboy                       );

  //
  // Application Compatibility Overrides
  // ===================================
  //
  if (games.count (std::wstring (SK_GetHostApp ())))
  {
    switch (games [std::wstring (SK_GetHostApp ())])
    {
      case SK_GAME_ID::Tyranny:
        // Cannot auto-detect API?!
        config.apis.dxgi.d3d11.hook       = false;
        config.apis.OpenGL.hook           = false;
        config.steam.filter_stat_callback = true; // Will stop running SteamAPI when it receives
                                                  //   data it didn't ask for
        break;


      case SK_GAME_ID::Shadowrun_HongKong:
        config.compatibility.d3d9.rehook_reset = true;
        break;


      case SK_GAME_ID::TidesOfNumenera:
        // API Auto-Detect Broken (0.7.43)
        //
        //   => Auto-Detection Thinks Game is OpenGL
        //
        config.apis.d3d9.hook       = true;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d11.hook = false;
        config.apis.OpenGL.hook     = false;
        break;


      case SK_GAME_ID::MassEffect_Andromeda:
        // Disable Exception Handling Instead of Crashing at Shutdown
        config.render.dxgi.exception_mode      = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;

        // Not a Steam game :(
        config.steam.silent                    = true;

        config.system.strict_compliance        = false; // Uses NVIDIA Ansel, so this won't work!

        config.apis.d3d9.hook                  = false;
        config.apis.d3d9ex.hook                = false;
        config.apis.OpenGL.hook                = false;

        config.input.ui.capture_hidden         = false; // Mouselook is a bitch
        config.input.mouse.add_relative_motion = true;
        SK_ImGui_Cursor.prefs.no_warp.ui_open  = false;
        SK_ImGui_Cursor.prefs.no_warp.visible  = false;

        config.textures.d3d11.cache            = true;
        config.textures.cache.ignore_nonmipped = true;
        config.textures.cache.max_size         = 4096;
        break;


      case SK_GAME_ID::MadMax:
        break;


      case SK_GAME_ID::Dreamfall_Chapters:
        config.system.trace_load_library       = true;
        config.system.strict_compliance        = false;

        // Game has mouselook problems without this
        config.input.mouse.add_relative_motion = true;

        // Chances are good that we will not catch SteamAPI early enough to hook callbacks, so
        //   auto-pump.
        config.steam.auto_pump_callbacks       = true;
        config.steam.preload_client            = true;
        config.steam.filter_stat_callback      = true; // Will stop running SteamAPI when it receives
                                                    //   data it didn't ask for

        config.apis.dxgi.d3d11.hook            = true;
        config.apis.d3d9.hook                  = true;
        config.apis.d3d9ex.hook                = true;
        config.apis.OpenGL.hook                = false;
        break;


      case SK_GAME_ID::TheWitness:
        config.system.trace_load_library    = true;
        config.system.strict_compliance     = false; // Uses Ansel

        // Game has mouselook problems without this
        config.input.ui.capture_mouse       = true;
        break;


      case SK_GAME_ID::Obduction:
        config.system.trace_load_library = true;  // Need to catch SteamAPI DLL load
        config.system.strict_compliance  = false; // Cannot block threads while loading DLLs
                                                  //   (uses an incorrectly written DLL)
        break;


      case SK_GAME_ID::TheWitcher3:
        config.system.strict_compliance   = false; // Uses NVIDIA Ansel, so this won't work!
        config.steam.filter_stat_callback = true;  // Will stop running SteamAPI when it receives
                                                   //   data it didn't ask for

        config.apis.d3d9.hook             = false;
        config.apis.d3d9ex.hook           = false;
        config.apis.OpenGL.hook           = false;

        config.textures.cache.ignore_nonmipped = true; // Invalid use of immutable textures
        break;


      case SK_GAME_ID::ResidentEvil7:
        config.system.trace_load_library = true;  // Need to catch SteamAPI DLL load
        config.system.strict_compliance  = false; // Cannot block threads while loading DLLs
                                                  //   (uses an incorrectly written DLL)
        break;


      case SK_GAME_ID::DragonsDogma:
        // BROKEN (by GeForce Experience)
        //
        // TODO: Debug the EXACT cause of NVIDIA's Deadlock
        //
        config.compatibility.disable_nv_bloat = true;  // PREVENT DEADLOCK CAUSED BY NVIDIA!

        config.system.trace_load_library      = true;  // Need to catch NVIDIA Bloat DLLs
        config.system.strict_compliance       = false; // Cannot block threads while loading DLLs
                                                       //   (uses an incorrectly written DLL)

        config.steam.auto_pump_callbacks      = false;
        config.steam.preload_client           = true;

        config.apis.d3d9.hook                 = true;
        config.apis.dxgi.d3d11.hook           = false;
        config.apis.d3d9ex.hook               = false;
        config.apis.OpenGL.hook               = false;
        break;


      case SK_GAME_ID::EverQuest:
        // Fix-up rare issues during Server Select -> Game
        //config.compatibility.d3d9.rehook_reset = true;
        break;


      case SK_GAME_ID::GodEater2RageBurst:
        //Does not support XInput hot-plugging, needs Special K loving :)
        config.input.gamepad.xinput.placehold [0] = true;

        config.apis.d3d9.hook                     = true;
        config.apis.d3d9ex.hook                   = true;
        config.apis.dxgi.d3d11.hook               = false;
        config.apis.OpenGL.hook                   = false;
        break;


      case SK_GAME_ID::WatchDogs2:
        //Does not support XInput hot-plugging, needs Special K loving :)
        config.input.gamepad.xinput.placehold [0] = true;
        config.input.mouse.add_relative_motion    = true;
        break;


      case SK_GAME_ID::NieRAutomata:
        // Maximize compatibility with 3rd party injectors that corrupt hooks
        //config.render.dxgi.slow_state_cache    = false;
        //SK_DXGI_SlowStateCache                 = config.render.dxgi.slow_state_cache;
        config.render.dxgi.scaling_mode        = DXGI_MODE_SCALING_UNSPECIFIED;
        config.input.mouse.add_relative_motion = false;
        break;


      case SK_GAME_ID::Warframe_x64:
        config.apis.d3d9.hook       = false;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d11.hook = true;
        break;


      case SK_GAME_ID::LEGOCityUndercover:
        // Prevent the game from deadlocking its message pump in fullscreen
        config.render.dxgi.safe_fullscreen       = true;
        config.render.framerate.sleepless_window = true; // Fix framerate limiter
        break;


      case SK_GAME_ID::Sacred2:
        config.display.force_windowed      = true; // Fullscreen is not particularly well
                                                   //   supported in this game
      case SK_GAME_ID::Sacred:
        config.render.dxgi.safe_fullscreen = true; // dgVoodoo compat
        // Contrary to its name, this game needs this turned off ;)
        config.cegui.safe_init             = false;
        config.steam.force_load_steamapi   = true; // Not safe in all games, but it is here.
        break;


      case SK_GAME_ID::FinalFantasy9:
        // Don't auto-pump callbacks
        config.steam.auto_pump_callbacks = false;
        config.apis.OpenGL.hook          = false; // Not an OpenGL game, API auto-detect is borked
        break;


      case SK_GAME_ID::EdithFinch:
        config.render.framerate.sleepless_window = true;
        break;


      case SK_GAME_ID::FinalFantasyX_X2:
        // Don't auto-pump callbacks
        //  Excessively lenghty startup is followed by actual SteamAPI init eventually...
        config.steam.auto_pump_callbacks = false;

        //config.render.dxgi.full_state_cache    = true;
        //SK_DXGI_FullStateCache                 = config.render.dxgi.full_state_cache;
        break;

#ifndef _WIN64
      case SK_GAME_ID::DeadlyPremonition:
        config.steam.force_load_steamapi       = true;
        config.apis.d3d9.hook                  = true;
        config.apis.d3d9ex.hook                = false;
        config.apis.d3d8.hook                  = false;
        config.input.mouse.add_relative_motion = false;
        break;
#endif

#ifdef _WIN64
      case SK_GAME_ID::LifeIsStrange_BeforeTheStorm:
        config.apis.d3d9.hook       = false;
        config.apis.d3d9ex.hook     = false;
        config.apis.OpenGL.hook     = false;
        config.apis.Vulkan.hook     = false;
        config.apis.dxgi.d3d11.hook = true;
        config.apis.dxgi.d3d12.hook = false;
        break;

      case SK_GAME_ID::DivinityOriginalSin:
        config.textures.cache.ignore_nonmipped = true;
        break;
#endif

      case SK_GAME_ID::Hob:
        // Fails to start Steamworks correctly, we have to kickstart it
        config.steam.force_load_steamapi = true;
        break;

#ifndef _WIN64
      case SK_GAME_ID::DukeNukemForever:
        // The mouse cursor's coordinate space is limited to 1920x1080 even at 4K, which
        //   has the unfortunate side effect of reducing aiming precision when the game
        //     isn't using HID.
        config.window.unconfine_cursor = true; // Remap the coordinates and increase precision

        // The graphics engine doesn't import any render APIs directly aside from ddraw.dll,
        //   which is obviously not the correct API. D3D9 and only D3D9 is the correct API.
        config.apis.d3d9.hook          = true;
        config.apis.ddraw.hook         = false;
        config.apis.d3d8.hook          = false;
        config.apis.dxgi.d3d11.hook    = false;
        config.apis.OpenGL.hook        = false;
        break;
#endif

      case SK_GAME_ID::DotHackGU:
        config.cegui.safe_init         = false; // If not turned off, the game will have problems
                                                // loading its constituent DLLs
      //config.textures.d3d11.generate_mips = true;
        break;

      case SK_GAME_ID::WorldOfFinalFantasy:
      {
        config.window.borderless                 = true;
        config.window.fullscreen                 = true;
        config.window.offset.x.absolute          = -1;
        config.window.offset.y.absolute          = -1;
        config.window.center                     = false;
        config.render.framerate.buffer_count     =  3;
        config.render.framerate.target_fps       = -30.0f;
        config.render.framerate.flip_discard     = true;
        config.render.framerate.present_interval = 2;
        config.render.framerate.pre_render_limit = 2;
        config.render.framerate.sleepless_window = false;
        config.input.cursor.manage               = true;
        config.input.cursor.timeout              = 0;

        HMONITOR hMonitor =
          MonitorFromWindow ( HWND_DESKTOP,
                                MONITOR_DEFAULTTOPRIMARY );

        MONITORINFO mi   = {         };
        mi.cbSize        = sizeof (mi);
        GetMonitorInfo (hMonitor, &mi);

        config.window.res.override.x = mi.rcMonitor.right  - mi.rcMonitor.left;
        config.window.res.override.y = mi.rcMonitor.bottom - mi.rcMonitor.top;
      } break;

      case SK_GAME_ID::StarOcean4:
        // Prevent the game from layering windows always on top.
        config.window.always_on_top             = 0;
        config.window.disable_screensaver       = true;
        config.textures.d3d11.uncompressed_mips = true;
        config.textures.d3d11.cache_gen_mips    = false;
        break;

      case SK_GAME_ID::Okami:
        config.render.dxgi.deferred_isolation   = true;
        config.render.dxgi.alternate_hook       = 0;
        break;

      case SK_GAME_ID::Mafia3:
        config.steam.force_load_steamapi        = true;
        break;

      // (0.8.58 - 1/1/18) -> API autodetect doesn't like XNA games
      case SK_GAME_ID::Owlboy:
        config.apis.d3d9.hook       = true;
        config.apis.d3d9ex.hook     = true;
        config.apis.OpenGL.hook     = false;
        config.apis.dxgi.d3d11.hook = false;
        break;
    }
  }

  init = true; }


  //
  // Load Parameters
  //
  compatibility.disable_nv_bloat->load   (config.compatibility.disable_nv_bloat);
  compatibility.rehook_loadlibrary->load (config.compatibility.rehook_loadlibrary);

  osd.version_banner.duration->load      (config.version_banner.duration);
  osd.state.remember->load               (config.osd.remember_state);

  imgui.scale->load                      (config.imgui.scale);
  imgui.show_eula->load                  (config.imgui.show_eula);
  imgui.show_playtime->load              (config.steam.show_playtime);
  imgui.show_gsync_status->load          (config.apis.NvAPI.gsync_status);
  imgui.mac_style_menu->load             (config.imgui.use_mac_style_menu);
  imgui.show_input_apis->load            (config.imgui.show_input_apis);

  imgui.disable_alpha->load              (config.imgui.render.disable_alpha);
  imgui.antialias_lines->load            (config.imgui.render.antialias_lines);
  imgui.antialias_contours->load         (config.imgui.render.antialias_contours);


  if (((sk::iParameter *)monitoring.io.show)->load     () && config.osd.remember_state)
    config.io.show =     monitoring.io.show->get_value ();
                         monitoring.io.interval->load  (config.io.interval);

  monitoring.fps.show->load      (config.fps.show);
  monitoring.fps.frametime->load (config.fps.frametime);
  monitoring.fps.advanced->load  (config.fps.advanced);

  if (((sk::iParameter *)monitoring.memory.show)->load     () && config.osd.remember_state)
       config.mem.show = monitoring.memory.show->get_value ();
  mem_reserve->load (config.mem.reserve);

  if (((sk::iParameter *)monitoring.cpu.show)->load     () && config.osd.remember_state)
    config.cpu.show =    monitoring.cpu.show->get_value ();
                         monitoring.cpu.interval->load  (config.cpu.interval);
                         monitoring.cpu.simple->load    (config.cpu.simple);

  monitoring.gpu.show->load           (config.gpu.show);
  monitoring.gpu.print_slowdown->load (config.gpu.print_slowdown);
  monitoring.gpu.interval->load       (config.gpu.interval);

  if (((sk::iParameter *)monitoring.disk.show)->load     () && config.osd.remember_state)
    config.disk.show =   monitoring.disk.show->get_value ();
                         monitoring.disk.interval->load  (config.disk.interval);
                         monitoring.disk.type->load      (config.disk.type);

  //if (monitoring.pagefile.show->load () && config.osd.remember_state)
    //config.pagefile.show = monitoring.pagefile.show->get_value ();
  monitoring.pagefile.interval->load (config.pagefile.interval);

  monitoring.time.show->load (config.time.show);
  monitoring.SLI.show->load  (config.sli.show);

  apis.last_known->load ((int &)config.apis.last_known);

#ifndef _WIN64
  apis.ddraw.hook->load (config.apis.ddraw.hook);
  apis.d3d8.hook->load  (config.apis.d3d8.hook);
#endif


  apis.d3d9.hook->load   (config.apis.d3d9.hook);
  apis.d3d9ex.hook->load (config.apis.d3d9ex.hook);
  apis.d3d11.hook->load  (config.apis.dxgi.d3d11.hook);

#ifdef _WIN64
  apis.d3d12.hook->load (config.apis.dxgi.d3d12.hook);
#endif

  apis.OpenGL.hook->load (config.apis.OpenGL.hook);

#ifdef _WIN64
  apis.Vulkan.hook->load (config.apis.Vulkan.hook);
#endif

  if (nvidia.api.disable->load (config.apis.NvAPI.enable))
     config.apis.NvAPI.enable = (! nvidia.api.disable->get_value ());

  if (amd.adl.disable->load (config.apis.ADL.enable))
     config.apis.ADL.enable = (! amd.adl.disable->get_value ());




  // Global Injection
  //
  //   Hook Address Policy
  //
  if (injection.global.use_static_addresses->load (config.injection.global.use_static_addresses))
  {
    injection.global.has_local_preference = true;
  }

  else
  {
    auto inject_config =
      SK_GetDocumentsDir () + LR"(\My Mods\SpecialK\Global\injection.ini)";

    iSK_INI* pInjectINI =
      SK_CreateINI (inject_config.c_str ());

    auto* default_usage =
      dynamic_cast <sk::ParameterBool *> (
        g_ParameterFactory.create_parameter <bool> (L"Default Usage")
      );
    default_usage->register_to_ini (pInjectINI, L"Injection.Policy", L"DefaultStaticAddressUsage");

    // Highly Experimental, so OFF by default
    if (! default_usage->load (config.injection.global.use_static_addresses))
    {
      default_usage->store (false);

      config.injection.global.use_static_addresses = false;

      pInjectINI->write (pInjectINI->get_filename ());
    }

    if (SK_IsInjected ())
    {
      // Apply this by default
      config.render.dxgi.rehook_present = config.injection.global.use_static_addresses;
    }

    delete pInjectINI;
  }




  display.force_fullscreen->load            (config.display.force_fullscreen);
  display.force_windowed->load              (config.display.force_windowed);

  render.framerate.target_fps->load         (config.render.framerate.target_fps);
  render.framerate.limiter_tolerance->load  (config.render.framerate.limiter_tolerance);
  render.framerate.sleepless_render->load   (config.render.framerate.sleepless_render);
  render.framerate.sleepless_window->load   (config.render.framerate.sleepless_window);

  render.framerate.control.busy_wait->load  (config.render.framerate.busy_wait_limiter);
  render.framerate.control.yield_once->load (config.render.framerate.yield_once);
  render.framerate.control.
                     minimize_latency->load (config.render.framerate.min_input_latency);
  render.framerate.control.
                          sleep_scale->load (config.render.framerate.max_sleep_percent);
  render.framerate.control.
                  deadline_transition->load (config.render.framerate.sleep_deadline);

  // D3D9/11
  //

  nvidia.sli.compatibility->load            (config.nvidia.sli.compatibility);
  nvidia.sli.mode->load                     (config.nvidia.sli.mode);
  nvidia.sli.num_gpus->load                 (config.nvidia.sli.num_gpus);
  nvidia.sli.override->load                 (config.nvidia.sli.override);

  render.framerate.wait_for_vblank->load    (config.render.framerate.wait_for_vblank);
  render.framerate.buffer_count->load       (config.render.framerate.buffer_count);
  render.framerate.prerender_limit->load    (config.render.framerate.pre_render_limit);
  render.framerate.present_interval->load   (config.render.framerate.present_interval);

  if (render.framerate.refresh_rate)
  {
    render.framerate.refresh_rate->load     (config.render.framerate.refresh_rate);
  }

  // D3D9
  //
  compatibility.d3d9.rehook_present->load   (config.compatibility.d3d9.rehook_present);
  compatibility.d3d9.rehook_reset->load     (config.compatibility.d3d9.rehook_reset);

  compatibility.d3d9.hook_present_vtable->load (config.compatibility.d3d9.hook_present_vftbl);
  compatibility.d3d9.hook_reset_vtable->load   (config.compatibility.d3d9.hook_reset_vftbl);

  render.d3d9.force_d3d9ex->load        (config.render.d3d9.force_d3d9ex);
  render.d3d9.impure->load              (config.render.d3d9.force_impure);
  render.d3d9.enable_texture_mods->load (config.textures.d3d9_mod);
  render.d3d9.hook_type->load           (config.render.d3d9.hook_type);


  // DXGI
  //
  render.framerate.max_delta_time->load (config.render.framerate.max_delta_time);

  if (render.framerate.flip_discard->load (config.render.framerate.flip_discard))
  {
    if (render.framerate.allow_dwm_tearing->load (config.render.dxgi.allow_tearing))
    {
      //if (config.render.dxgi.allow_tearing) config.render.framerate.flip_discard = true;
    }

    extern bool SK_DXGI_use_factory1;
    if (config.render.framerate.flip_discard)
      SK_DXGI_use_factory1 = true;
  }

  render.dxgi.adapter_override->load (config.render.dxgi.adapter_override);

  if (((sk::iParameter *)render.dxgi.max_res)->load ())
  {
    swscanf ( render.dxgi.max_res->get_value_str ().c_str (),
                L"%lux%lu",
                &config.render.dxgi.res.max.x,
                  &config.render.dxgi.res.max.y );
  }
  if (((sk::iParameter *)render.dxgi.min_res)->load ())
  {
    swscanf ( render.dxgi.min_res->get_value_str ().c_str (),
                L"%lux%lu",
                &config.render.dxgi.res.min.x,
                  &config.render.dxgi.res.min.y );
  }

  if (((sk::iParameter *)render.dxgi.scaling_mode)->load ())
  {
    if (! _wcsicmp (
            render.dxgi.scaling_mode->get_value_str ().c_str (),
            L"Unspecified"
          )
       )
    {
      config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_UNSPECIFIED;
    }

    else if (! _wcsicmp (
                 render.dxgi.scaling_mode->get_value_str ().c_str (),
                 L"Centered"
               )
            )
    {
      config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_CENTERED;
    }

    else if (! _wcsicmp (
                 render.dxgi.scaling_mode->get_value_str ().c_str (),
                 L"Stretched"
               )
            )
    {
      config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_STRETCHED;
    }
  }

  if (((sk::iParameter *)render.dxgi.scanline_order)->load ())
  {
    if (! _wcsicmp (
            render.dxgi.scanline_order->get_value_str ().c_str (),
            L"Unspecified"
          )
       )
    {
      config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    }

    else if (! _wcsicmp (
                 render.dxgi.scanline_order->get_value_str ().c_str (),
                 L"Progressive"
               )
            )
    {
      config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    }

    else if (! _wcsicmp (
                 render.dxgi.scanline_order->get_value_str ().c_str (),
                 L"LowerFieldFirst"
               )
            )
    {
      config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    }

    else if (! _wcsicmp (
                 render.dxgi.scanline_order->get_value_str ().c_str (),
                 L"UpperFieldFirst"
               )
            )
    {
      config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
    }

    // If a user specifies Interlaced, default to Lower Field First
    else if (! _wcsicmp (
                 render.dxgi.scanline_order->get_value_str ().c_str (),
                 L"Interlaced"
               )
            )
    {
      config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    }
  }

  render.dxgi.debug_layer->load (config.render.dxgi.debug_layer);

  if (((sk::iParameter *)render.dxgi.exception_mode)->load ())
  {
    if (! _wcsicmp (
            render.dxgi.exception_mode->get_value_str ().c_str (),
            L"Raise"
          )
       )
    {
      #define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1
      config.render.dxgi.exception_mode = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;
    }

    else if (! _wcsicmp (
                 render.dxgi.exception_mode->get_value_str ().c_str (),
                 L"Ignore"
               )
            )
    {
      config.render.dxgi.exception_mode = 0;
    }
    else
      config.render.dxgi.exception_mode = -1;
  }

  render.dxgi.test_present->load       (config.render.dxgi.test_present);
  render.dxgi.swapchain_wait->load     (config.render.framerate.swapchain_wait);

  render.dxgi.safe_fullscreen->load    (config.render.dxgi.safe_fullscreen);

  render.dxgi.enhanced_depth->load     (config.render.dxgi.enhanced_depth);
  render.dxgi.deferred_isolation->load (config.render.dxgi.deferred_isolation);
  render.dxgi.rehook_present->load     (config.render.dxgi.rehook_present);
  render.dxgi.alternate_hook->load     (config.render.dxgi.alternate_hook);


  texture.d3d11.cache->load        (config.textures.d3d11.cache);
  texture.d3d11.precise_hash->load (config.textures.d3d11.precise_hash);
  texture.d3d11.dump->load         (config.textures.d3d11.dump);
  texture.d3d11.inject->load       (config.textures.d3d11.inject);
  texture.d3d11.res_root->load     (config.textures.d3d11.res_root);
        texture.res_root->load     (config.textures.d3d11.res_root);

  texture.d3d11.injection_keeps_format->load (config.textures.d3d11.injection_keeps_fmt);
                  texture.dump_on_load->load (config.textures.dump_on_load);

  texture.d3d11.gen_mips->load     (config.textures.d3d11.generate_mips);

  texture.cache.max_entries->load       (config.textures.cache.max_entries);
  texture.cache.min_entries->load       (config.textures.cache.min_entries);
  texture.cache.max_evict->load         (config.textures.cache.max_evict);
  texture.cache.min_evict->load         (config.textures.cache.min_evict);
  texture.cache.max_size->load          (config.textures.cache.max_size);
  texture.cache.min_size->load          (config.textures.cache.min_size);
  texture.cache.ignore_non_mipped->load (config.textures.cache.ignore_nonmipped);
  texture.cache.allow_staging->load     (config.textures.cache.allow_staging);

  extern void WINAPI SK_DXGI_SetPreferredAdapter (int override_id);

  if (config.render.dxgi.adapter_override != -1)
    SK_DXGI_SetPreferredAdapter (config.render.dxgi.adapter_override);

  input.keyboard.catch_alt_f4->load      (config.input.keyboard.catch_alt_f4);
  input.keyboard.disabled_to_game->load  (config.input.keyboard.disabled_to_game);

  input.mouse.disabled_to_game->load     (config.input.mouse.disabled_to_game);

  input.cursor.manage->load              (config.input.cursor.manage);
  input.cursor.keys_activate->load       (config.input.cursor.keys_activate);

                            float fTimeout;
  if (input.cursor.timeout->load (fTimeout))
    config.input.cursor.timeout = (int)(1000.0 * fTimeout);

  input.cursor.ui_capture->load         (config.input.ui.capture);
  input.cursor.hw_cursor->load          (config.input.ui.use_hw_cursor);
  input.cursor.no_warp_ui->load         (SK_ImGui_Cursor.prefs.no_warp.ui_open);
  input.cursor.no_warp_visible->load    (SK_ImGui_Cursor.prefs.no_warp.visible);
  input.cursor.block_invisible->load    (config.input.ui.capture_hidden);
  input.cursor.fix_synaptics->load      (config.input.mouse.fix_synaptics);
  input.cursor.antiwarp_deadzone->load  (config.input.mouse.antiwarp_deadzone);
  input.cursor.use_relative_input->load (config.input.mouse.add_relative_motion);

  input.gamepad.disabled_to_game->load  (config.input.gamepad.disabled_to_game);
  input.gamepad.disable_ps4_hid->load   (config.input.gamepad.disable_ps4_hid);
  input.gamepad.rehook_xinput->load     (config.input.gamepad.rehook_xinput);
  input.gamepad.hook_xinput->load       (config.input.gamepad.hook_xinput);

  // Hidden INI values; they're loaded, but never written
  input.gamepad.hook_dinput8->load      (config.input.gamepad.hook_dinput8);
  input.gamepad.hook_hid->load          (config.input.gamepad.hook_hid);
  input.gamepad.native_ps4->load        (config.input.gamepad.native_ps4);

  input.gamepad.haptic_ui->load         (config.input.gamepad.haptic_ui);

  int placeholder_mask;

  if (input.gamepad.xinput.placeholders->load (placeholder_mask))
  {
    config.input.gamepad.xinput.placehold [0] = ( placeholder_mask & 0x1 );
    config.input.gamepad.xinput.placehold [1] = ( placeholder_mask & 0x2 );
    config.input.gamepad.xinput.placehold [2] = ( placeholder_mask & 0x4 );
    config.input.gamepad.xinput.placehold [3] = ( placeholder_mask & 0x8 );
  }

  input.gamepad.disable_rumble->load (config.input.gamepad.disable_rumble);


  if (((sk::iParameter *)input.gamepad.xinput.assignment)->load ())
  {
    wchar_t* wszAssign =
      _wcsdup (input.gamepad.xinput.assignment->get_value ().c_str ());

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszAssign, L",", &wszBuf);

    if (wszTok == nullptr)
    {
      config.input.gamepad.xinput.assignment [0] = 0; config.input.gamepad.xinput.assignment [1] = 1;
      config.input.gamepad.xinput.assignment [2] = 2; config.input.gamepad.xinput.assignment [3] = 3;
    }

    int idx = 0;

    while (wszTok && idx < 4)
    {
      config.input.gamepad.xinput.assignment [idx++] =
        _wtoi (wszTok);

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }

    free (wszAssign);
  }

  input.gamepad.xinput.ui_slot->load ((int &)config.input.gamepad.xinput.ui_slot);
  input.gamepad.steam.ui_slot->load  ((int &)config.input.gamepad.steam.ui_slot);

  window.borderless->load        (config.window.borderless);

  window.center->load            (config.window.center);
  window.background_render->load (config.window.background_render);
  window.background_mute->load   (config.window.background_mute);

  std::wstring offset;

  if (window.offset.x->load (offset))
  {
    if (wcsstr (offset.c_str (), L"%"))
    {
      config.window.offset.x.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.x.percent);
      config.window.offset.x.percent /= 100.0f;
    }

    else
    {
      config.window.offset.x.percent = 0.0f;
      swscanf (offset.c_str (), L"%li", &config.window.offset.x.absolute);
    }
  }

  if (window.offset.y->load (offset))
  {
    if (wcsstr (offset.c_str (), L"%"))
    {
      config.window.offset.y.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.y.percent);
      config.window.offset.y.percent /= 100.0f;
    }

    else
    {
      config.window.offset.y.percent = 0.0f;
      swscanf (offset.c_str (), L"%li", &config.window.offset.y.absolute);
    }
  }

  window.confine_cursor->load      (config.window.confine_cursor);
  window.unconfine_cursor->load    (config.window.unconfine_cursor);
  window.persistent_drag->load     (config.window.persistent_drag);
  window.fullscreen->load          (config.window.fullscreen);
  window.fix_mouse_coords->load    (config.window.res.override.fix_mouse);
  window.always_on_top->load       (config.window.always_on_top);
  window.disable_screensaver->load (config.window.disable_screensaver);

  if (((sk::iParameter *)window.override)->load ())
  {
    swscanf ( window.override->get_value_str ().c_str (),
                L"%lux%lu",
                &config.window.res.override.x,
                  &config.window.res.override.y );
  }

  steam.achievements.play_sound->load         (config.steam.achievements.play_sound);
  steam.achievements.sound_file->load         (config.steam.achievements.sound_file);
  steam.achievements.take_screenshot->load    (config.steam.achievements.take_screenshot);
  steam.achievements.fetch_friend_stats->load (config.steam.achievements.pull_friend_stats);
  steam.achievements.popup.animate->load      (config.steam.achievements.popup.animate);
  steam.achievements.popup.show_title->load   (config.steam.achievements.popup.show_title);

  if (((sk::iParameter *)steam.achievements.popup.origin)->load ())
  {
    config.steam.achievements.popup.origin =
      SK_Steam_PopupOriginWStrToEnum (
        steam.achievements.popup.origin->get_value ().c_str ()
      );
  }

  else
  {
    config.steam.achievements.popup.origin = 3;
  }

  steam.achievements.popup.inset->load    (config.steam.achievements.popup.inset);
  steam.achievements.popup.duration->load (config.steam.achievements.popup.duration);

  if (config.steam.achievements.popup.duration == 0)
  {
    config.steam.achievements.popup.show        = false;
    config.steam.achievements.pull_friend_stats = false;
    config.steam.achievements.pull_global_stats = false;
  }


  if (((sk::iParameter *)steam.cloud.blacklist)->load ())
  {
    std::unique_ptr <wchar_t> wszCSV (
        _wcsdup (steam.cloud.blacklist->get_value ().c_str ())
    );

    wchar_t* wszBuf = nullptr;
    wchar_t* wszTok =
      std::wcstok (wszCSV.get (), L",", &wszBuf);

    while (wszTok != nullptr)
    {
      config.steam.cloud.blacklist.emplace (SK_WideCharToUTF8 (wszTok));

      wszTok =
        std::wcstok (nullptr, L",", &wszBuf);
    }
  }


  steam.log.silent->load          (config.steam.silent);
  steam.drm.spoof_BLoggedOn->load (config.steam.spoof_BLoggedOn);

  steam.system.appid->load                 (config.steam.appid);
  steam.system.init_delay->load            (config.steam.init_delay);
  steam.system.auto_pump->load             (config.steam.auto_pump_callbacks);
  steam.system.block_stat_callback->load   (config.steam.block_stat_callback);
  steam.system.filter_stat_callbacks->load (config.steam.filter_stat_callback);
  steam.system.load_early->load            (config.steam.preload_client);
  steam.system.early_overlay->load         (config.steam.preload_overlay);
  steam.system.force_load->load            (config.steam.force_load_steamapi);

  if (((sk::iParameter *)steam.system.notify_corner)->load ())
  {
    config.steam.notify_corner =
      SK_Steam_PopupOriginWStrToEnum (
        steam.system.notify_corner->get_value ().c_str ()
    );
  }

  osd.show->load                        (config.osd.show);
  osd.update_method.pump->load          (config.osd.pump);
  osd.update_method.pump_interval->load (config.osd.pump_interval);

  osd.text.red->load       (config.osd.red);
  osd.text.green->load     (config.osd.green);
  osd.text.blue->load      (config.osd.blue);

  osd.viewport.pos_x->load (config.osd.pos_x);
  osd.viewport.pos_y->load (config.osd.pos_y);
  osd.viewport.scale->load (config.osd.scale);


  init_delay->load        (config.system.init_delay);
  silent->load            (config.system.silent);
  trace_libraries->load   (config.system.trace_load_library);
  strict_compliance->load (config.system.strict_compliance);
  log_level->load         (config.system.log_level);
  prefer_fahrenheit->load (config.system.prefer_fahrenheit);
  ignore_rtss_delay->load (config.system.ignore_rtss_delay);
  handle_crashes->load    (config.system.handle_crashes);
  debug_output->load      (config.system.display_debug_out);
  game_output->load       (config.system.game_output);
  enable_cegui->load      (config.cegui.enable);
  safe_cegui->load        (config.cegui.safe_init);
  version->load           (config.system.version);




  void
  WINAPI
  SK_D3D11_SetResourceRoot (const wchar_t* root);
  SK_D3D11_SetResourceRoot (config.textures.d3d11.res_root.c_str ());


  //
  // EMERGENCY OVERRIDES
  //
  config.input.ui.use_raw_input = false;



  config.imgui.font.default.file  = "arial.ttf";
  config.imgui.font.default.size  = 18.0f;

  config.imgui.font.japanese.file = "msgothic.ttc";
  config.imgui.font.japanese.size = 18.0f;

  config.imgui.font.cyrillic.file = "arial.ttf";
  config.imgui.font.cyrillic.size = 18.0f;

  config.imgui.font.korean.file   = "malgun.ttf";
  config.imgui.font.korean.size   = 18.0f;

  config.imgui.font.chinese.file  = "msyh.ttc";
  config.imgui.font.chinese.size  = 18.0f;



  static bool scanned = false;

  if ((! scanned) && (! config.window.res.override.isZero ()))
  {
    scanned = true;

    if (games.count (std::wstring (SK_GetHostApp ())))
    {
      switch (games [std::wstring (SK_GetHostApp ())])
      {
        case SK_GAME_ID::GalGun_Double_Peace:
        case SK_GAME_ID::DuckTalesRemastered:
        {
          CreateThread (nullptr, 0, [](LPVOID) ->
          DWORD
          {
            // Wait for the image relocation to settle down, or we'll probably
            //   break the memory scanner.
            WaitForInputIdle (GetCurrentProcess (), 3333UL);

            void
            SK_ResHack_PatchGame (uint32_t w, uint32_t h);

            SK_ResHack_PatchGame (1920, 1080);

            CloseHandle (GetCurrentThread ());

            return 0;
          }, nullptr, 0x00, nullptr);
        } break;


        case SK_GAME_ID::YS_Seven:
        {
          CreateThread (nullptr, 0, [ ] (LPVOID) ->
                        DWORD
          {
            // Wait for the image relocation to settle down, or we'll probably
            //   break the memory scanner.
            WaitForInputIdle (GetCurrentProcess (), 3333UL);

            void
              SK_ResHack_PatchGame2 (uint32_t w, uint32_t h);

            SK_ResHack_PatchGame2 (1920, 1080);

            CloseHandle (GetCurrentThread ());

            return 0;
          }, nullptr, 0x00, nullptr);
        } break;


        case SK_GAME_ID::AKIBAs_Trip:
        {
          CreateThread (nullptr, 0, [](LPVOID) ->
          DWORD
          {
            // Wait for the image relocation to settle down, or we'll probably
            //   break the memory scanner.
            WaitForInputIdle (GetCurrentProcess (), 3333UL);

            void
            SK_ResHack_PatchGame (uint32_t w, uint32_t h);

            SK_ResHack_PatchGame (1920, 1080);

            CloseHandle (GetCurrentThread ());

            return 0;
          }, nullptr, 0x00, nullptr);
        } break;
      }
    }
  }



  //if ( SK_GetDLLRole () == DLL_ROLE::D3D8 ||
  //     SK_GetDLLRole () == DLL_ROLE::DDraw )
  //{
  //  config.render.dxgi.safe_fullscreen = true;
  //}



  if (empty)
    return false;

  return true;
}

void
SK_ResHack_PatchGame ( uint32_t width,
                       uint32_t height )
{
  static unsigned int replacements = 0;

  struct
  {
    struct
    {
      uint32_t w, h;
    } pattern;

    struct
    {
      uint32_t w = config.window.res.override.x,
               h = config.window.res.override.y;
    } replacement;
  } res_mod;

  res_mod.pattern.w = width;
  res_mod.pattern.h = height;

        uint32_t* pOut;
  const void*     pPattern = &res_mod.pattern;

  pOut =
    reinterpret_cast <uint32_t *> (
      nullptr
    );


  for (int i = 0 ; i < 3; i++)
  {
    pOut =
      static_cast <uint32_t *> (
        SK_ScanAlignedEx ( pPattern, 8, nullptr, pOut, 8 )
      );


    if (pOut != nullptr)
    {
      if ( SK_InjectMemory ( pOut,
                               &res_mod.replacement.w,
                                8,
                                  PAGE_READWRITE )
         )
      {
        ++replacements;
      }

      pOut += 8;
    }

    else
    {
      dll_log.Log ( L"[GalGunHACK] ** %lu Resolution Replacements Made  ==>  "
                                         L"( %lux%lu --> %lux%lu )",
                      replacements,
                        width, height,
                          res_mod.replacement.w, res_mod.replacement.h );
      break;
    }
  }
}

void
SK_ResHack_PatchGame2 ( uint32_t width,
                        uint32_t height )
{
  static unsigned int replacements = 0;

  uint32_t orig [2] = { 0x00000000,
                        0x00000000 };

  *(orig + 0) = width;
  *(orig + 1) = height;

  auto* pOut = reinterpret_cast <uint32_t *> (nullptr);
    //reinterpret_cast  <uint32_t *> (GetModuleHandle (nullptr));

  for (int i = 0 ; i < 5; i++)
  {
    pOut =
      static_cast <uint32_t *> (
        SK_ScanAlignedEx (orig, 8, nullptr, pOut, 4)
      );

    if (pOut != nullptr)
    {
      struct {
        uint32_t w = static_cast <uint32_t> (config.window.res.override.x),
                 h = static_cast <uint32_t> (config.window.res.override.y);
      } out_data;


      if ( SK_InjectMemory ( pOut,
                               &out_data.w,
                                 8,
                                   PAGE_READWRITE )
         )
      {
        ++replacements;
      }

      pOut += 8;
    }

    if (pOut == nullptr)
    {
      dll_log.Log ( L"[AkibasHACK] ** %lu Resolution Replacements Made  ==>  "
                                      L"( %lux%lu --> %lux%lu )",
                      replacements,
                        width, height,
                          config.window.res.override.x, config.window.res.override.y );
      break;
    }
  }
}

bool
SK_DeleteConfig (std::wstring name)
{
  wchar_t wszFullName [ MAX_PATH + 2 ] = { };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  return (DeleteFileW (wszFullName) != FALSE);
}

void
SK_SaveConfig ( std::wstring name,
                bool         close_config )
{
  //
  // Shutting down before initialization would be damn near fatal if we didn't catch this! :)
  //
  if (dll_ini == nullptr)
    return;

  compatibility.disable_nv_bloat->store       (config.compatibility.disable_nv_bloat);
  compatibility.rehook_loadlibrary->store     (config.compatibility.rehook_loadlibrary);

  monitoring.memory.show->set_value           (config.mem.show);
  mem_reserve->store                          (config.mem.reserve);

  monitoring.fps.show->store                  (config.fps.show);
  monitoring.fps.advanced->store              (config.fps.advanced);
  monitoring.fps.frametime->store             (config.fps.frametime);

  monitoring.io.show->set_value               (config.io.show);
  monitoring.io.interval->store               (config.io.interval);

  monitoring.cpu.show->set_value              (config.cpu.show);
  monitoring.cpu.interval->store              (config.cpu.interval);
  monitoring.cpu.simple->store                (config.cpu.simple);

  monitoring.gpu.show->store                  (config.gpu.show);
  monitoring.gpu.print_slowdown->store        (config.gpu.print_slowdown);
  monitoring.gpu.interval->store              (config.gpu.interval);

  monitoring.disk.show->set_value             (config.disk.show);
  monitoring.disk.interval->store             (config.disk.interval);
  monitoring.disk.type->store                 (config.disk.type);

  monitoring.pagefile.show->set_value         (config.pagefile.show);
  monitoring.pagefile.interval->store         (config.pagefile.interval);

  if (! (nvapi_init && sk::NVAPI::nv_hardware && sk::NVAPI::CountSLIGPUs () > 1))
    config.sli.show = false;

  monitoring.SLI.show->store                  (config.sli.show);
  monitoring.time.show->store                 (config.time.show);

  osd.version_banner.duration->store          (config.version_banner.duration);
  osd.show->store                             (config.osd.show);
  osd.update_method.pump->store               (config.osd.pump);
  osd.update_method.pump_interval->store      (config.osd.pump_interval);
  osd.text.red->store                         (config.osd.red);
  osd.text.green->store                       (config.osd.green);
  osd.text.blue->store                        (config.osd.blue);
  osd.viewport.pos_x->store                   (config.osd.pos_x);
  osd.viewport.pos_y->store                   (config.osd.pos_y);
  osd.viewport.scale->store                   (config.osd.scale);
  osd.state.remember->store                   (config.osd.remember_state);

  imgui.scale->store                          (config.imgui.scale);
  imgui.show_eula->store                      (config.imgui.show_eula);
  imgui.show_playtime->store                  (config.steam.show_playtime);
  imgui.show_gsync_status->store              (config.apis.NvAPI.gsync_status);
  imgui.mac_style_menu->store                 (config.imgui.use_mac_style_menu);
  imgui.show_input_apis->store                (config.imgui.show_input_apis);
  imgui.disable_alpha->store                  (config.imgui.render.disable_alpha);
  imgui.antialias_lines->store                (config.imgui.render.antialias_lines);
  imgui.antialias_contours->store             (config.imgui.render.antialias_contours);

  apis.last_known->store                      (static_cast <int> (config.apis.last_known));

#ifndef _WIN64
  apis.ddraw.hook->store                      (config.apis.ddraw.hook);
  apis.d3d8.hook->store                       (config.apis.d3d8.hook);
#endif
  apis.d3d9.hook->store                       (config.apis.d3d9.hook);
  apis.d3d9ex.hook->store                     (config.apis.d3d9ex.hook);
  apis.d3d11.hook->store                      (config.apis.dxgi.d3d11.hook);
  apis.OpenGL.hook->store                     (config.apis.OpenGL.hook);
#ifdef _WIN64
  apis.d3d12.hook->store                      (config.apis.dxgi.d3d12.hook);
  apis.Vulkan.hook->store                     (config.apis.Vulkan.hook);
#endif

  input.keyboard.catch_alt_f4->store          (config.input.keyboard.catch_alt_f4);
  input.keyboard.disabled_to_game->store      (config.input.keyboard.disabled_to_game);

  input.mouse.disabled_to_game->store         (config.input.mouse.disabled_to_game);

  input.cursor.manage->store                  (config.input.cursor.manage);
  input.cursor.keys_activate->store           (config.input.cursor.keys_activate);
  input.cursor.timeout->store                 (static_cast <float> (config.input.cursor.timeout) / 1000.0f);
  input.cursor.ui_capture->store              (config.input.ui.capture);
  input.cursor.hw_cursor->store               (config.input.ui.use_hw_cursor);
  input.cursor.block_invisible->store         (config.input.ui.capture_hidden);
  input.cursor.no_warp_ui->store              (SK_ImGui_Cursor.prefs.no_warp.ui_open);
  input.cursor.no_warp_visible->store         (SK_ImGui_Cursor.prefs.no_warp.visible);
  input.cursor.fix_synaptics->store           (config.input.mouse.fix_synaptics);
  input.cursor.antiwarp_deadzone->store       (config.input.mouse.antiwarp_deadzone);
  input.cursor.use_relative_input->store      (config.input.mouse.add_relative_motion);

  input.gamepad.disabled_to_game->store       (config.input.gamepad.disabled_to_game);
  input.gamepad.disable_ps4_hid->store        (config.input.gamepad.disable_ps4_hid);
  input.gamepad.rehook_xinput->store          (config.input.gamepad.rehook_xinput);
  input.gamepad.haptic_ui->store              (config.input.gamepad.haptic_ui);

  int placeholder_mask = 0x0;

  placeholder_mask |= (config.input.gamepad.xinput.placehold [0] ? 0x1 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [1] ? 0x2 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [2] ? 0x4 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [3] ? 0x8 : 0x0);

  input.gamepad.xinput.placeholders->store    (placeholder_mask);
  input.gamepad.xinput.ui_slot->store         (config.input.gamepad.xinput.ui_slot);
  input.gamepad.steam.ui_slot->store          (config.input.gamepad.steam.ui_slot);

  std::wstring xinput_assign = L"";

  for (int i = 0; i < 4; i++)
  {
    xinput_assign += std::to_wstring (
      config.input.gamepad.xinput.assignment [i]
    );

    if (i != 3)
      xinput_assign += L",";
  }

  input.gamepad.xinput.assignment->store     (xinput_assign);
  input.gamepad.disable_rumble->store        (config.input.gamepad.disable_rumble);

  window.borderless->store                   (config.window.borderless);
  window.center->store                       (config.window.center);
  window.background_render->store            (config.window.background_render);
  window.background_mute->store              (config.window.background_mute);
  if (config.window.offset.x.absolute != 0)
  {
    wchar_t    wszAbsolute [16] = { };
    _swprintf (wszAbsolute, L"%li", config.window.offset.x.absolute);

    window.offset.x->store (wszAbsolute);
  }

  else
  {
       wchar_t wszPercent [16] = { };
    _swprintf (wszPercent, L"%08.6f", 100.0f * config.window.offset.x.percent);

    SK_RemoveTrailingDecimalZeros (wszPercent);

    lstrcatW (wszPercent, L"%");

    window.offset.x->store (wszPercent);
  }

  if (config.window.offset.y.absolute != 0)
  {
    wchar_t    wszAbsolute [16] = { };
    _swprintf (wszAbsolute, L"%li", config.window.offset.y.absolute);

    window.offset.y->store (wszAbsolute);
  }

  else
  {
       wchar_t wszPercent [16] = { };
    _swprintf (wszPercent, L"%08.6f", 100.0f * config.window.offset.y.percent);

    SK_RemoveTrailingDecimalZeros (wszPercent);
    lstrcatW                      (wszPercent, L"%");

    window.offset.y->store (wszPercent);
  }

  window.confine_cursor->store            (config.window.confine_cursor);
  window.unconfine_cursor->store          (config.window.unconfine_cursor);
  window.persistent_drag->store           (config.window.persistent_drag);
  window.fullscreen->store                (config.window.fullscreen);
  window.fix_mouse_coords->store          (config.window.res.override.fix_mouse);
  window.always_on_top->store             (config.window.always_on_top);
  window.disable_screensaver->store       (config.window.disable_screensaver);

  wchar_t wszFormattedRes [64] = { };

  wsprintf ( wszFormattedRes, L"%lux%lu",
               config.window.res.override.x,
                 config.window.res.override.y );

  window.override->store (wszFormattedRes);

  extern float target_fps;

  display.force_fullscreen->store             (config.display.force_fullscreen);
  display.force_windowed->store               (config.display.force_windowed);

  render.framerate.target_fps->store          (target_fps);
  render.framerate.limiter_tolerance->store   (config.render.framerate.limiter_tolerance);
  render.framerate.sleepless_render->store    (config.render.framerate.sleepless_render);
  render.framerate.sleepless_window->store    (config.render.framerate.sleepless_window);

  render.framerate.control.busy_wait->store   (config.render.framerate.busy_wait_limiter);
  render.framerate.control.yield_once->store  (config.render.framerate.yield_once);
  render.framerate.control.
                      minimize_latency->store (config.render.framerate.min_input_latency);
  render.framerate.control.
                           sleep_scale->store (config.render.framerate.max_sleep_percent);
  render.framerate.control.
                   deadline_transition->store (config.render.framerate.sleep_deadline);

  if ( SK_IsInjected () || (SK_GetDLLRole () & DLL_ROLE::DInput8) ||
      (SK_GetDLLRole () & DLL_ROLE::D3D9 || SK_GetDLLRole () & DLL_ROLE::DXGI) )
  {
    render.framerate.wait_for_vblank->store   (config.render.framerate.wait_for_vblank);
    render.framerate.prerender_limit->store   (config.render.framerate.pre_render_limit);
    render.framerate.buffer_count->store      (config.render.framerate.buffer_count);
    render.framerate.present_interval->store  (config.render.framerate.present_interval);

    if (render.framerate.refresh_rate != nullptr)
      render.framerate.refresh_rate->store    (config.render.framerate.refresh_rate);

    // SLI only works in Direct3D
    nvidia.sli.compatibility->store           (config.nvidia.sli.compatibility);
    nvidia.sli.mode->store                    (config.nvidia.sli.mode);
    nvidia.sli.num_gpus->store                (config.nvidia.sli.num_gpus);
    nvidia.sli.override->store                (config.nvidia.sli.override);

    if (  SK_IsInjected ()                       ||
        ( SK_GetDLLRole () & DLL_ROLE::DInput8 ) ||
        ( SK_GetDLLRole () & DLL_ROLE::DXGI    ) )
    {
      render.framerate.max_delta_time->store      (config.render.framerate.max_delta_time);
      render.framerate.flip_discard->store        (config.render.framerate.flip_discard);
      render.framerate.allow_dwm_tearing->store   (config.render.dxgi.allow_tearing);

      texture.d3d11.cache->store                  (config.textures.d3d11.cache);
      texture.d3d11.precise_hash->store           (config.textures.d3d11.precise_hash);
      texture.d3d11.dump->store                   (config.textures.d3d11.dump);
      texture.d3d11.inject->store                 (config.textures.d3d11.inject);
      texture.d3d11.injection_keeps_format->store (config.textures.d3d11.injection_keeps_fmt);
      texture.d3d11.gen_mips->store               (config.textures.d3d11.generate_mips);
      texture.d3d11.res_root->store               (config.textures.d3d11.res_root);

      texture.cache.max_entries->store            (config.textures.cache.max_entries);
      texture.cache.min_entries->store            (config.textures.cache.min_entries);
      texture.cache.max_evict->store              (config.textures.cache.max_evict);
      texture.cache.min_evict->store              (config.textures.cache.min_evict);
      texture.cache.max_size->store               (config.textures.cache.max_size);
      texture.cache.min_size->store               (config.textures.cache.min_size);

      texture.cache.ignore_non_mipped->store      (config.textures.cache.ignore_nonmipped);
      texture.cache.allow_staging->store          (config.textures.cache.allow_staging);

      wsprintf ( wszFormattedRes, L"%lux%lu",
                   config.render.dxgi.res.max.x,
                     config.render.dxgi.res.max.y );

      render.dxgi.max_res->store (wszFormattedRes);

      wsprintf ( wszFormattedRes, L"%lux%lu",
                   config.render.dxgi.res.min.x,
                     config.render.dxgi.res.min.y );

      render.dxgi.min_res->store (wszFormattedRes);

      render.dxgi.swapchain_wait->store (config.render.framerate.swapchain_wait);

      switch (config.render.dxgi.scaling_mode)
      {
        case DXGI_MODE_SCALING_UNSPECIFIED:
          render.dxgi.scaling_mode->store (L"Unspecified");
          break;
        case DXGI_MODE_SCALING_CENTERED:
          render.dxgi.scaling_mode->store (L"Centered");
          break;
        case DXGI_MODE_SCALING_STRETCHED:
          render.dxgi.scaling_mode->store (L"Stretched");
          break;
        default:
          render.dxgi.scaling_mode->store (L"DontCare");
          break;
      }

      switch (config.render.dxgi.scanline_order)
      {
        case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:
          render.dxgi.scanline_order->store (L"Unspecified");
          break;
        case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
          render.dxgi.scanline_order->store (L"Progressive");
          break;
        case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
          render.dxgi.scanline_order->store (L"LowerFieldFirst");
          break;
        case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
          render.dxgi.scanline_order->store (L"UpperFieldFirst");
          break;
        default:
          render.dxgi.scanline_order->store (L"DontCare");
          break;
      }

      switch (config.render.dxgi.exception_mode)
      {
        case D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR:
          render.dxgi.exception_mode->store (L"Raise");
          break;
        case 0:
          render.dxgi.exception_mode->store (L"Ignore");
          break;
        default:
          render.dxgi.exception_mode->store (L"DontCare");
          break;
      }

      render.dxgi.debug_layer->store        (config.render.dxgi.debug_layer);
      render.dxgi.safe_fullscreen->store    (config.render.dxgi.safe_fullscreen);
      render.dxgi.enhanced_depth->store     (config.render.dxgi.enhanced_depth);
      render.dxgi.deferred_isolation->store (config.render.dxgi.deferred_isolation);
      render.dxgi.rehook_present->store     (config.render.dxgi.rehook_present);
      render.dxgi.alternate_hook->store     (config.render.dxgi.alternate_hook);
    }

    if ( SK_IsInjected () || ( SK_GetDLLRole () & DLL_ROLE::D3D9    ) ||
                             ( SK_GetDLLRole () & DLL_ROLE::DInput8 ) )
    {
      render.d3d9.force_d3d9ex->store        (config.render.d3d9.force_d3d9ex);
      render.d3d9.hook_type->store           (config.render.d3d9.hook_type);
      render.d3d9.enable_texture_mods->store (config.textures.d3d9_mod);
    }
  }

  texture.res_root->store                      (config.textures.d3d11.res_root);
  texture.dump_on_load->store                  (config.textures.dump_on_load);

  steam.achievements.sound_file->store         (config.steam.achievements.sound_file);
  steam.achievements.play_sound->store         (config.steam.achievements.play_sound);
  steam.achievements.take_screenshot->store    (config.steam.achievements.take_screenshot);
  steam.achievements.fetch_friend_stats->store (config.steam.achievements.pull_friend_stats);
  steam.achievements.popup.origin->store       (
    SK_Steam_PopupOriginToWStr (config.steam.achievements.popup.origin)
  );
  steam.achievements.popup.inset->store        (config.steam.achievements.popup.inset);

  if (! config.steam.achievements.popup.show)
    config.steam.achievements.popup.duration = 0;

  steam.achievements.popup.duration->store     (config.steam.achievements.popup.duration);
  steam.achievements.popup.animate->store      (config.steam.achievements.popup.animate);
  steam.achievements.popup.show_title->store   (config.steam.achievements.popup.show_title);

  if (config.steam.appid == 0)
  {
    if (SK::SteamAPI::AppID () != 0 &&
        SK::SteamAPI::AppID () != 1)
    {
      config.steam.appid = SK::SteamAPI::AppID ();
    }
  }

  steam.system.appid->store                 (config.steam.appid);
  steam.system.init_delay->store            (config.steam.init_delay);
  steam.system.auto_pump->store             (config.steam.auto_pump_callbacks);
  steam.system.block_stat_callback->store   (config.steam.block_stat_callback);
  steam.system.filter_stat_callbacks->store (config.steam.filter_stat_callback);
  steam.system.load_early->store            (config.steam.preload_client);
  steam.system.early_overlay->store         (config.steam.preload_overlay);
  steam.system.force_load->store            (config.steam.force_load_steamapi);
  steam.system.notify_corner->store         (
    SK_Steam_PopupOriginToWStr (config.steam.notify_corner)
  );

  steam.log.silent->store                   (config.steam.silent);
  steam.drm.spoof_BLoggedOn->store          (config.steam.spoof_BLoggedOn);

  init_delay->store                         (config.system.init_delay);
  silent->store                             (config.system.silent);
  log_level->store                          (config.system.log_level);
  prefer_fahrenheit->store                  (config.system.prefer_fahrenheit);


  if (SK_IsInjected () && injection.global.has_local_preference)
  {
    injection.global.use_static_addresses->store (config.injection.global.use_static_addresses);
  }

  nvidia.api.disable->store                 (! config.apis.NvAPI.enable);
  amd.adl.disable->store                    (! config.apis.ADL.enable);

  ignore_rtss_delay->store                  (config.system.ignore_rtss_delay);


  // Don't store this setting at shutdown  (it may have been turned off automatically)
  if (__SK_DLL_Ending == false)
  {
    handle_crashes->store                (config.system.handle_crashes);
  }

  game_output->store                     (config.system.game_output);

  // Only add this to the INI file if it differs from default
  if (config.system.display_debug_out != debug_output->get_value ())
  {
    debug_output->store                  (config.system.display_debug_out);
  }

  enable_cegui->store                    (config.cegui.enable);
  safe_cegui->store                      (config.cegui.safe_init);
  trace_libraries->store                 (config.system.trace_load_library);
  strict_compliance->store               (config.system.strict_compliance);
  version->store                         (SK_VER_STR);

  if (! (nvapi_init && sk::NVAPI::nv_hardware))
    dll_ini->remove_section (L"NVIDIA.SLI");



  wchar_t wszFullName [ MAX_PATH + 2 ] = { };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  SK_ImGui_Widgets.SaveConfig ();


  dll_ini->write ( wszFullName );
  osd_ini->write ( std::wstring ( SK_GetDocumentsDir () +
                     LR"(\My Mods\SpecialK\Global\osd.ini)"
                   ).c_str () );
  achievement_ini->write ( std::wstring ( SK_GetDocumentsDir () +
                     LR"(\My Mods\SpecialK\Global\achievements.ini)"
                   ).c_str () );

  macro_ini->write ( std::wstring ( SK_GetDocumentsDir () +
                     LR"(\My Mods\SpecialK\Global\macros.ini)"
                   ).c_str () );


  if (close_config)
  {
    if (dll_ini != nullptr)
    {
      delete dll_ini;
             dll_ini = nullptr;
    }

    if (osd_ini != nullptr)
    {
      delete osd_ini;
             osd_ini = nullptr;
    }

    if (achievement_ini != nullptr)
    {
      delete achievement_ini;
             achievement_ini = nullptr;
    }

    if (macro_ini != nullptr)
    {
      delete macro_ini;
             macro_ini = nullptr;
    }
  }
}

const wchar_t*
__stdcall
SK_GetVersionStr (void)
{
  return SK_VER_STR;
}


#include <unordered_map>

std::unordered_map <std::wstring, BYTE> humanKeyNameToVirtKeyCode;
std::unordered_map <BYTE, std::wstring> virtKeyCodeToHumanKeyName;

#include <queue>

#define SK_MakeKeyMask(vKey,ctrl,shift,alt) \
  (UINT)((vKey) | (((ctrl) != 0) <<  9) |   \
                  (((shift)!= 0) << 10) |   \
                  (((alt)  != 0) << 11))

void
SK_Keybind::update (void)
{
  human_readable = L"";

  std::wstring key_name = virtKeyCodeToHumanKeyName [(BYTE)(vKey & 0xFF)];

  if (! key_name.length ())
    return;

  std::queue <std::wstring> words;

  if (ctrl)
    words.push (L"Ctrl");

  if (alt)
    words.push (L"Alt");

  if (shift)
    words.push (L"Shift");

  words.push (key_name);

  while (! words.empty ())
  {
    human_readable += words.front ();
    words.pop ();

    if (! words.empty ())
      human_readable += L"+";
  }

  masked_code = SK_MakeKeyMask (vKey & 0xFF, ctrl, shift, alt);
}

void
SK_Keybind::parse (void)
{
  vKey = 0x00;

  static bool init = false;

  if (! init)
  {
    init = true;

    for (int i = 0; i < 0xFF; i++)
    {
      wchar_t name [32] = { };

      switch (i)
      {
        case VK_F1:     wcscat (name, L"F1");           break;
        case VK_F2:     wcscat (name, L"F2");           break;
        case VK_F3:     wcscat (name, L"F3");           break;
        case VK_F4:     wcscat (name, L"F4");           break;
        case VK_F5:     wcscat (name, L"F5");           break;
        case VK_F6:     wcscat (name, L"F6");           break;
        case VK_F7:     wcscat (name, L"F7");           break;
        case VK_F8:     wcscat (name, L"F8");           break;
        case VK_F9:     wcscat (name, L"F9");           break;
        case VK_F10:    wcscat (name, L"F10");          break;
        case VK_F11:    wcscat (name, L"F11");          break;
        case VK_F12:    wcscat (name, L"F12");          break;
        case VK_F13:    wcscat (name, L"F13");          break;
        case VK_F14:    wcscat (name, L"F14");          break;
        case VK_F15:    wcscat (name, L"F15");          break;
        case VK_F16:    wcscat (name, L"F16");          break;
        case VK_F17:    wcscat (name, L"F17");          break;
        case VK_F18:    wcscat (name, L"F18");          break;
        case VK_F19:    wcscat (name, L"F19");          break;
        case VK_F20:    wcscat (name, L"F20");          break;
        case VK_F21:    wcscat (name, L"F21");          break;
        case VK_F22:    wcscat (name, L"F22");          break;
        case VK_F23:    wcscat (name, L"F23");          break;
        case VK_F24:    wcscat (name, L"F24");          break;
        case VK_PRINT:  wcscat (name, L"Print Screen"); break;
        case VK_SCROLL: wcscat (name, L"Scroll Lock");  break;
        case VK_PAUSE:  wcscat (name, L"Pause Break");  break;

        default:
        {
          unsigned int scanCode =
            ( MapVirtualKey (i, 0) & 0xFF );

                        BYTE buf [256] = { };
          unsigned short int temp      =  0;

          bool asc = (i <= 32);

          if (! asc && i != VK_DIVIDE)
             asc = ToAscii ( i, scanCode, buf, &temp, 1 );

          scanCode            <<= 16;
          scanCode   |= ( 0x1 <<  25  );

          if (! asc)
            scanCode |= ( 0x1 << 24   );

          GetKeyNameText ( scanCode,
                             name,
                               32 );
        } break;
      }


      if ( i != VK_CONTROL  && i != VK_MENU     &&
           i != VK_SHIFT    && i != VK_OEM_PLUS && i != VK_OEM_MINUS &&
           i != VK_LSHIFT   && i != VK_RSHIFT   &&
           i != VK_LCONTROL && i != VK_RCONTROL &&
           i != VK_LMENU    && i != VK_RMENU )
      {

        humanKeyNameToVirtKeyCode.emplace (name, (BYTE)i);
        virtKeyCodeToHumanKeyName.emplace ((BYTE)i, name);
      }
    }

    humanKeyNameToVirtKeyCode.emplace (L"Plus",        (BYTE)VK_OEM_PLUS);
    humanKeyNameToVirtKeyCode.emplace (L"Minus",       (BYTE)VK_OEM_MINUS);
    humanKeyNameToVirtKeyCode.emplace (L"Ctrl",        (BYTE)VK_CONTROL);
    humanKeyNameToVirtKeyCode.emplace (L"Alt",         (BYTE)VK_MENU);
    humanKeyNameToVirtKeyCode.emplace (L"Shift",       (BYTE)VK_SHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Left Shift",  (BYTE)VK_LSHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Right Shift", (BYTE)VK_RSHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Left Alt",    (BYTE)VK_LMENU);
    humanKeyNameToVirtKeyCode.emplace (L"Right Alt",   (BYTE)VK_RMENU);
    humanKeyNameToVirtKeyCode.emplace (L"Left Ctrl",   (BYTE)VK_LCONTROL);
    humanKeyNameToVirtKeyCode.emplace (L"Right Ctrl",  (BYTE)VK_RCONTROL);

    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_CONTROL,   L"Ctrl");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_MENU,      L"Alt");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_SHIFT,     L"Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_OEM_PLUS,  L"Plus");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_OEM_MINUS, L"Minus");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LSHIFT,    L"Left Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RSHIFT,    L"Right Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LMENU,     L"Left Alt");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RMENU,     L"Right Alt");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LCONTROL,  L"Left Ctrl");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RCONTROL,  L"Right Ctrl");

    init = true;
  }

  wchar_t wszKeyBind [128] = { };

  lstrcatW (wszKeyBind, human_readable.c_str ());

  wchar_t* wszBuf = nullptr;
  wchar_t* wszTok = std::wcstok (wszKeyBind, L"+", &wszBuf);

  ctrl  = false;
  alt   = false;
  shift = false;

  if (wszTok == nullptr)
  {
    vKey = humanKeyNameToVirtKeyCode [wszKeyBind];
  }

  while (wszTok)
  {
    BYTE vKey_ = humanKeyNameToVirtKeyCode [wszTok];

    if (vKey_ == VK_CONTROL)
      ctrl  = true;
    else if (vKey_ == VK_SHIFT)
      shift = true;
    else if (vKey_ == VK_MENU)
      alt   = true;
    else
      vKey = vKey_;

    wszTok = std::wcstok (nullptr, L"+", &wszBuf);
  }

  masked_code = SK_MakeKeyMask (vKey & 0xFF, ctrl, shift, alt);
}




#include <SpecialK/utility.h>


bool
SK_AppCache_Manager::loadAppCacheForExe (const wchar_t* wszExe)
{
  std::wstring naive_name =
    SK_GetNaiveConfigPath ();

  wchar_t* wszPath =
    StrStrIW (wszExe, LR"(SteamApps\common\)");

  if (wszPath != nullptr)
  {
    wchar_t* wszRelPath =
      _wcsdup (CharNextW (StrStrIW (CharNextW (StrStrIW (wszPath, L"\\")), L"\\")));

    PathRemoveFileSpecW (wszRelPath);

    std::wstring wstr_appcache =
     SK_FormatStringW ( LR"(%s\..\AppCache\%s\SpecialK.AppCache)",
                          naive_name.c_str (),
                            wszRelPath );

    SK_CreateDirectories (wstr_appcache.c_str ());

    app_cache_db =
      SK_CreateINI (wstr_appcache.c_str ());

    app_cache_db->write (app_cache_db->get_filename ());

    free (wszRelPath);
  }

  if (app_cache_db != nullptr)
    return true;

  return false;
}

uint32_t
SK_AppCache_Manager::getAppIDFromPath (const wchar_t* wszPath) const
{
  if (app_cache_db == nullptr)
    return 0;

  iSK_INISection&
    fwd_map =
      app_cache_db->get_section (L"AppID_Cache.FwdMap");

  wchar_t* wszSteamApps =
    StrStrIW (wszPath, LR"(SteamApps\common\)");

  if (wszSteamApps != nullptr)
  {
    wchar_t* wszRelPath =
      CharNextW (StrStrIW (CharNextW (StrStrIW (wszSteamApps, L"\\")), L"\\"));

    if (fwd_map.contains_key (wszRelPath))
    {
      return _wtoi (fwd_map.get_value (wszRelPath).c_str ());
    }
  }

  return 0;
}

std::wstring
SK_AppCache_Manager::getAppNameFromID (uint32_t uiAppID) const
{
  if (app_cache_db == nullptr)
    return L"";

  iSK_INISection&
    name_map =
      app_cache_db->get_section (L"AppID_Cache.Names");

  if (name_map.contains_key   (SK_FormatStringW (L"%u", uiAppID).c_str ()))
  {
    return name_map.get_value (SK_FormatStringW (L"%u", uiAppID).c_str ());
  }

  return L"";
}

std::wstring
SK_AppCache_Manager::getAppNameFromPath (const wchar_t* wszPath) const
{
  uint32_t uiAppID = getAppIDFromPath (wszPath);

  if (uiAppID != 0)
  {
    return getAppNameFromID (uiAppID);
  }

  return L"";
}

bool
SK_AppCache_Manager::addAppToCache ( const wchar_t* wszFullPath,
                                     const wchar_t*,
                                     const wchar_t* wszAppName,
                                           uint32_t uiAppID )
{
  if (! app_cache_db)
    return false;

  if (! StrStrIW (wszFullPath, LR"(SteamApps\common\)"))
      return false;

  iSK_INISection& rev_map =
    app_cache_db->get_section (L"AppID_Cache.RevMap");
  iSK_INISection& fwd_map =
    app_cache_db->get_section (L"AppID_Cache.FwdMap");
  iSK_INISection& name_map =
    app_cache_db->get_section (L"AppID_Cache.Names");


  wchar_t* wszRelativePath = _wcsdup (wszFullPath);

  wchar_t* wszRelPath =
    CharNextW (StrStrIW (CharNextW (StrStrIW (StrStrIW (wszRelativePath, LR"(SteamApps\common\)"), L"\\")), L"\\"));

  if (fwd_map.contains_key (wszRelPath))
    fwd_map.get_value (wszRelPath) = SK_FormatStringW   (L"%u", uiAppID).c_str ();
  else
    fwd_map.add_key_value (wszRelPath, SK_FormatStringW (L"%u", uiAppID).c_str ());


  if (rev_map.contains_key (SK_FormatStringW  (L"%u", uiAppID).c_str ()))
    rev_map.get_value (SK_FormatStringW       (L"%u", uiAppID).c_str ()) = wszRelPath;
  else
    rev_map.add_key_value (SK_FormatStringW   (L"%u", uiAppID).c_str (), wszRelPath);


  if (name_map.contains_key (SK_FormatStringW (L"%u", uiAppID).c_str ()))
    name_map.get_value (SK_FormatStringW      (L"%u", uiAppID).c_str ()) = wszAppName;
  else
    name_map.add_key_value (SK_FormatStringW  (L"%u", uiAppID).c_str (), wszAppName);


  app_cache_db->write (app_cache_db->get_filename ());


  free (wszRelativePath);

  return true;
}

std::wstring
SK_AppCache_Manager::getConfigPathFromAppPath (const wchar_t* wszPath) const
{
  return getConfigPathForAppID (getAppIDFromPath (wszPath));
}

#include <unordered_set>

std::wstring
SK_AppCache_Manager::getConfigPathForAppID (uint32_t uiAppID) const
{
  // If no AppCache (probably not a Steam game), or opting-out of central repo,
  //   then don't parse crap and just use the traditional path.
  if ( app_cache_db == nullptr || (! config.system.central_repository) )
    return SK_GetNaiveConfigPath ();

  std::wstring path = SK_GetNaiveConfigPath (       );
  std::wstring name ( getAppNameFromID      (uiAppID) );

  // Non-trivial name = custom path, remove the old-style <program.exe>
  if (! name.empty ())
  {
    std::wstring original_dir (path.c_str ());

    size_t       pos                     = 0;
    std::wstring host_app (SK_GetHostApp ());

    if ((pos = path.find (SK_GetHostApp (), pos)) != std::wstring::npos)
      path.replace (pos, host_app.length (), L"\0");

    name.erase ( std::remove_if ( name.begin (),
                                  name.end   (),

                                    [](wchar_t tval)
                                    {
                                      static
                                      const std::unordered_set <wchar_t>
                                        invalid_file_char =
                                        {
                                          L'\\', L'/', L':',
                                          L'*',  L'?', L'\"',
                                          L'<',  L'>', L'|'
                                        };

                                      if (invalid_file_char.count (tval) > 0)
                                        return true;

                                      return false;
                                    }
                                ),

                     name.end ()
               );

    // Truncating a std::wstring by inserting L'\0' actually does nothing,
    //   so construct path by treating name as a C-String.
    path += name.c_str ();
    path += L"\\";

    SK_StripTrailingSlashesW (path.data ());

    DWORD dwAttribs = 
      GetFileAttributesW (original_dir.c_str ());

    if ( GetFileAttributesW (path.c_str ()) == INVALID_FILE_ATTRIBUTES &&
                                  dwAttribs != INVALID_FILE_ATTRIBUTES && 
                                ( dwAttribs & FILE_ATTRIBUTE_DIRECTORY ) )
    {
      MoveFileExW ( original_dir.c_str (),
                      path.c_str       (),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED );
    }
  }

  return path.c_str ();
}

bool
SK_AppCache_Manager::saveAppCache (bool close)
{
  if (app_cache_db != nullptr)
  {
    app_cache_db->write (app_cache_db->get_filename ());

    if (close)
    {
      delete app_cache_db;
      app_cache_db = nullptr;
    }

    return true;
  }

  return false;
}

int
SK_AppCache_Manager::migrateProfileData (LPVOID)
{
  // TODO
  return 0;
}


__declspec (dllexport)
SK_RenderAPI
__stdcall
SK_Render_GetAPIHookMask (void)
{
  int mask = 0;

#ifndef _WIN64
  if (config.apis.d3d8.hook)       mask |= static_cast <int> (SK_RenderAPI::D3D8);
  if (config.apis.ddraw.hook)      mask |= static_cast <int> (SK_RenderAPI::DDraw);
#endif
  if (config.apis.d3d9.hook)       mask |= static_cast <int> (SK_RenderAPI::D3D9);
  if (config.apis.d3d9ex.hook)     mask |= static_cast <int> (SK_RenderAPI::D3D9Ex);
  if (config.apis.dxgi.d3d11.hook) mask |= static_cast <int> (SK_RenderAPI::D3D11);
  if (config.apis.OpenGL.hook)     mask |= static_cast <int> (SK_RenderAPI::OpenGL);
#ifdef _WIN64
  if (config.apis.Vulkan.hook)     mask |= static_cast <int> (SK_RenderAPI::Vulkan);
  if (config.apis.dxgi.d3d12.hook) mask |= static_cast <int> (SK_RenderAPI::D3D12);
#endif

  return static_cast <SK_RenderAPI> (mask);
}