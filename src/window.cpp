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

#include <SpecialK/stdafx.h>
#include <imgui/backends/imgui_d3d11.h>

#include <future>


#define __SK_SUBSYSTEM__ L"Window Mgr"

// For SK_D3D9_TriggerReset .... (should probably be in SK_RenderBackend_v{2|3+})
#include <SpecialK/render/d3d9/d3d9_backend.h>

// WS_SYSMENU keeps the window's icon unchanged
#define SK_BORDERLESS    ( WS_VISIBLE | WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | \
                           WS_CLIPCHILDREN | WS_CLIPSIBLINGS )
#define SK_BORDERLESS_EX ( WS_EX_APPWINDOW )

#define SK_LOG_LEVEL_UNTESTED

#define SK_WINDOW_LOG_CALL0() if (config.system.log_level >= 0) SK_LOG_CALL ("Window Mgr")
#define SK_WINDOW_LOG_CALL1() if (config.system.log_level >= 1) SK_LOG_CALL ("Window Mgr")
#define SK_WINDOW_LOG_CALL2() if (config.system.log_level >= 2) SK_LOG_CALL ("Window Mgr")
#define SK_WINDOW_LOG_CALL3() if (config.system.log_level >= 3) SK_LOG_CALL ("Window Mgr")
#define SK_WINDOW_LOG_CALL4() if (config.system.log_level >= 3) SK_LOG_CALL ("Window Mgr")

#ifdef SK_LOG_LEVEL_UNTESTED
# define SK_WINDOW_LOG_CALL_UNTESTED() SK_LOG_CALL ("Window Mgr");
#else
# define SK_WINDOW_LOG_CALL_UNTESTED() { }
#endif


BOOL
WINAPI
SetWindowPlacement_Detour (
  _In_       HWND             hWnd,
  _In_ const WINDOWPLACEMENT *lpwndpl
);

BOOL
WINAPI
SetWindowPos_Detour(
  _In_     HWND hWnd,
  _In_opt_ HWND hWndInsertAfter,
  _In_     int  X,
  _In_     int  Y,
  _In_     int  cx,
  _In_     int  cy,
  _In_     UINT uFlags);

ClipCursor_pfn          ClipCursor_Original          = nullptr;
GetCursorPos_pfn        GetCursorPos_Original        = nullptr;
SetCursorPos_pfn        SetCursorPos_Original        = nullptr;
SendInput_pfn           SendInput_Original           = nullptr;
mouse_event_pfn         mouse_event_Original         = nullptr;

SetWindowPos_pfn        SetWindowPos_Original        = nullptr;
SetWindowPlacement_pfn  SetWindowPlacement_Original  = nullptr;
MoveWindow_pfn          MoveWindow_Original          = nullptr;
SetWindowLong_pfn       SetWindowLongW_Original      = nullptr;
SetWindowLong_pfn       SetWindowLongA_Original      = nullptr;
GetWindowLong_pfn       GetWindowLongW_Original      = nullptr;
GetWindowLong_pfn       GetWindowLongA_Original      = nullptr;
SetWindowLongPtr_pfn    SetWindowLongPtrW_Original   = nullptr;
SetWindowLongPtr_pfn    SetWindowLongPtrA_Original   = nullptr;
GetWindowLongPtr_pfn    GetWindowLongPtrW_Original   = nullptr;
GetWindowLongPtr_pfn    GetWindowLongPtrA_Original   = nullptr;
AdjustWindowRect_pfn    AdjustWindowRect_Original    = nullptr;
AdjustWindowRectEx_pfn  AdjustWindowRectEx_Original  = nullptr;

GetSystemMetrics_pfn    GetSystemMetrics_Original    = nullptr;

GetWindowRect_pfn       GetWindowRect_Original       = nullptr;
GetClientRect_pfn       GetClientRect_Original       = nullptr;


bool
SK_Window_IsTopMost (void);

void
SK_Window_SetTopMost (bool bTop, bool bBringToTop = false);

bool
SK_EarlyDispatchMessage (MSG *lpMsg, bool remove, bool peek = false);


BOOL
SK_Window_IsUnicode (HWND hWnd)
{
  struct cache_entry_s {
    explicit cache_entry_s (HWND hWnd)
    {
      unicode = IsWindowUnicode (hWnd);
      hwnd    = hWnd;
    };

    BOOL test_and_set (HWND hWnd)
    {
      if (hWnd == hwnd)
        return unicode;

      cache_entry_s new_record (hWnd);
      cache_entry_s old_record = *this;
                                 *this =
                    new_record;

      return unicode;
    }

    BOOL unicode;
    HWND hwnd;
  } static cache (hWnd);

  return
    cache.test_and_set (hWnd);
}

BOOL
SK_IsChild (HWND hWndParent, HWND hWnd)
{
  static Concurrency::concurrent_unordered_map
    < HWND, HWND > parents_;

  if (parents_.count (hWnd))
  {
    if (parents_ [hWnd] == hWndParent)
      return TRUE;

    return FALSE;
  }

  parents_ [hWnd] =
    GetParent (hWnd);

  return FALSE;
}




struct sk_window_message_dispatch_s {
public:
  concurrency::concurrent_unordered_map <HWND, bool> active_windows;
  concurrency::concurrent_unordered_map <HWND, bool> moving_windows;
};

SK_LazyGlobal <sk_window_message_dispatch_s> wm_dispatch;

bool override_window_rects = false;

// TODO: Refactor that structure above into this class ;)
//
//  Also, combine a few of the input manager settings with window manager
class SK_WindowManager : public SK_IVariableListener
{
public:
  static constexpr bool StyleHasBorder (DWORD_PTR style)
  {
    return ( ( style == 0x0            ) ||
             ( style  &  WS_BORDER     ) ||
             ( style  &  WS_THICKFRAME ) ||
             ( style  &  WS_DLGFRAME   )    );
  }

  bool OnVarChange (SK_IVariable* var, void* val) override
  {
    if (var == top_most_)
    {
      if (val != nullptr)
      {
        const bool orig = SK_Window_IsTopMost ();
        const bool set  =
          *static_cast <bool *> (val);

        if (set != orig)
        {
          SK_Window_SetTopMost (set, set);
        }

        *static_cast <bool *> (var->getValuePointer ()) =
          SK_Window_IsTopMost ();
      }

      return true;
    }

    if (var == confine_cursor_)
    {
      if (val != nullptr)
      {
        config.window.confine_cursor =
          *static_cast <bool *> (val);

        if (! config.window.confine_cursor)
        {
          if (! config.window.unconfine_cursor)
          {
            SK_ClipCursor   (&game_window.cursor_clip);
            SK_AdjustWindow ();
          }

          else
            SK_ClipCursor (nullptr);
        }

        else
        {
          SK_GetWindowRect (game_window.hWnd, &game_window.actual.window);
          SK_ClipCursor    (&game_window.actual.window);
        }
      }

      return true;
    }

    if (var == unconfine_cursor_)
    {
      if (val != nullptr)
      {
        config.window.unconfine_cursor =
          *static_cast <bool *> (val);

        if (config.window.unconfine_cursor)
          SK_ClipCursor (nullptr);

        else if (config.window.confine_cursor)
        {
          SK_GetWindowRect (game_window.hWnd, &game_window.actual.window);
          SK_ClipCursor    (&game_window.actual.window);
        }

        else
        {
          SK_ClipCursor   (&game_window.cursor_clip);
          SK_AdjustWindow ();
        }
      }

      return true;
    }

    if ( var == center_window_ || var == x_offset_   || var == y_offset_   ||
         var == borderless_    || var == x_override_ || var == y_override_ ||
         var == fullscreen_    || var == x_off_pct_  || var == y_off_pct_ )
    {
      if (val != nullptr)
      {
        if (var == center_window_)
          config.window.center =
          *static_cast <bool *> (val);

        else if (var == x_offset_)
        {
          if ( *static_cast <int *> (val) >= -4096 &&
               *static_cast <int *> (val) <=  4096    )
          {
            config.window.offset.x.absolute = *static_cast <signed int *> (val);
            config.window.offset.x.percent  = 0.0f;
          }
        }

        else if (var == y_offset_)
        {
          if ( *static_cast <int *> (val) >= -4096 &&
               *static_cast <int *> (val) <=  4096    )
          {
            config.window.offset.y.absolute = *static_cast <signed int *> (val);
            config.window.offset.y.percent  = 0.0f;
          }
        }

        else if (var == x_off_pct_)
        {
          if ( *static_cast <float *> (val) > -1.0f &&
               *static_cast <float *> (val) <  1.0f    )
          {
            config.window.offset.x.absolute = 0;
            config.window.offset.x.percent = *static_cast <float *> (val);
          }
        }

        else if (var == y_off_pct_)
        {
          if ( *static_cast <float *> (val) > -1.0f &&
               *static_cast <float *> (val) <  1.0f    )
          {
            config.window.offset.y.absolute = 0;
            config.window.offset.y.percent  = *static_cast <float *> (val);
          }
        }

        else if ( var == borderless_ &&
                 *static_cast <bool *> (val) != config.window.borderless )
        {
          config.window.borderless = *static_cast <bool *> (val);

          SK_AdjustBorder ();
        }

        else if (var == fullscreen_)
        {
          if ( config.window.fullscreen != *static_cast <bool *> (val) &&
             ( config.window.borderless || (config.window.fullscreen) ) )
          {
            config.window.fullscreen = *static_cast <bool *> (val);

            static RECT last_known_client = { };
            static RECT last_known_window = { };

            RECT client            = { };

            SK_GetClientRect (game_window.hWnd, &client);

            HMONITOR hMonitor =
              MonitorFromWindow ( game_window.hWnd,
                                 MONITOR_DEFAULTTONEAREST );

            MONITORINFO mi  = { };
            mi.cbSize       =  sizeof (mi);
            GetMonitorInfo (hMonitor, &mi);

            if ( (client.right  - client.left != mi.rcMonitor.right  - mi.rcMonitor.left ) &&
                 (client.bottom - client.top  != mi.rcMonitor.bottom - mi.rcMonitor.top  ) )
            {
              SK_GetClientRect (game_window.hWnd, &last_known_client);
              SK_GetWindowRect (game_window.hWnd, &last_known_window);
            }

            const unsigned int orig_x = config.window.res.override.x,
                               orig_y = config.window.res.override.y;

            static unsigned int persist_x = orig_x, persist_y = orig_y;

            // Restore Window Dimensions
            if (! config.window.fullscreen)
            {
              config.window.res.override.x = last_known_client.right  - last_known_client.left;
              config.window.res.override.y = last_known_client.bottom - last_known_client.top;

              // Trigger re-calc on adjust
              game_window.game.client   = RECT { last_known_client.left,  last_known_client.top,
                                                 last_known_client.right, last_known_client.bottom };
              game_window.game.window   = RECT { last_known_window.left,  last_known_window.top,
                                                 last_known_window.right, last_known_window.bottom };

              game_window.actual.client = RECT { last_known_client.left,  last_known_client.top,
                                                 last_known_client.right, last_known_client.bottom };
              game_window.actual.window = RECT { last_known_window.left,  last_known_window.top,
                                                 last_known_window.right, last_known_window.bottom };

              if (! config.window.unconfine_cursor)
              {
                if (! wm_dispatch->moving_windows.count (game_window.hWnd))
                {
                  const RECT clip =
                    game_window.actual.window;

                  SK_ClipCursor (&clip);
                }
              }

              else
                SK_ClipCursor (nullptr);
            }

            // Go Fullscreen (Stretch Window to Fill)
            else
            {
              config.window.res.override.x = mi.rcMonitor.right  - mi.rcMonitor.left;
              config.window.res.override.y = mi.rcMonitor.bottom - mi.rcMonitor.top;

              game_window.actual.client =
                RECT { 0,                                    0,
                       mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top };

              game_window.actual.window =
                RECT { mi.rcMonitor.left,  mi.rcMonitor.top,
                       mi.rcMonitor.right, mi.rcMonitor.bottom };
            }

            SK_AdjustWindow ();

            if (! config.window.fullscreen)
            {
              // XXX: This is being clobbered by another thread, needs redesign...
              config.window.res.override.x = persist_x;
              config.window.res.override.y = persist_y;
            }

            else
            {
              persist_x = orig_x;
              persist_y = orig_y;

              config.window.res.override.x = 0;
              config.window.res.override.y = 0;
            }

            return true;
          }
        }

        else if (var == x_override_ || var == y_override_)
        {
          if ((! config.window.borderless) || config.window.fullscreen)
            return false;

          if (var == x_override_)
          {
            config.window.res.override.x =
              *static_cast <unsigned int *> (val);

            // We cannot allow one variable to remain 0 while the other becomes
            //   non-zero, so just make the window a square temporarily.
            if (config.window.res.override.y == 0)
                config.window.res.override.y = config.window.res.override.x;
          }

          else if (var == y_override_)
          {
            config.window.res.override.y = *static_cast <unsigned int *> (val);

            // We cannot allow one variable to remain 0 while the other becomes
            //   non-zero, so just make the window a square temporarily.
            if (config.window.res.override.x == 0)
                config.window.res.override.x = config.window.res.override.y;
          }

          // We have to override BOTH variables to 0 at the same time, or the window
          //   will poof! :P
          if (*static_cast <unsigned int *> (val) == 0)
          {
            config.window.res.override.x = 0;
            config.window.res.override.y = 0;
          }
        }

        snprintf ( override_res, 32, "%lux%lu",
                  config.window.res.override.x,
                  config.window.res.override.y );

        if (var == borderless_ && (! config.window.borderless))
        {
          if (config.window.fullscreen)
          {
            // Bring the game OUT of fullscreen mode, that's only for borderless
            SK_GetCommandProcessor ()->ProcessCommandLine ("Window.Fullscreen 0");
          }

          else
          {
            SK_AdjustWindow ();
            return true;
          }
        }

#if 1
        SK_AdjustWindow ();
#else
        if (var != center_window_)
        {
          static char szCmd [64] = { };
          snprintf (szCmd, 63, "Window.Center %d", config.window.center);

          DeferCommand (szCmd);
        }

        if (var != borderless_)
        {
          static char szCmd [64] = { };
          snprintf (szCmd, 63, "Window.Borderless %d", config.window.borderless);

          DeferCommand (szCmd);
        }
#endif

        return true;
      }
    }

    else if ( var == background_mute_ )
    {
      if (val != nullptr)
      {
        config.window.background_mute = *static_cast <bool *> (val);

        if (config.window.background_mute && (! game_window.active))
        {
          muteGame (true);
        }

        else if (!config.window.background_mute)
        {
          muteGame (false);
        }

        return true;
      }
    }

    else if (var == res_override_)
    {
      unsigned int x = 65535;
      unsigned int y = 65535;

      if (val != nullptr)
      {
        char    szTemp [31] = { };
        strcat (szTemp, *static_cast <char **> (val));
        sscanf (szTemp, "%ux%u", &x, &y);
      }

      if ((x > 320 && x < 16384 && y > 240 && y < 16384) || (x == 0 && y == 0))
      {
        config.window.res.override.x = x;
        config.window.res.override.y = y;

        SK_AdjustWindow ();

        auto *pszRes =
          static_cast <char *> (((SK_IVarStub <char *> *)var)->getValuePointer ());

        snprintf (pszRes, 31, "%ux%u", x, y);

        return true;
      }

      auto *pszRes =
        static_cast <char *> (((SK_IVarStub <char *> *)var)->getValuePointer ());

      snprintf (pszRes, 31, "INVALID");

      return false;
    }

    return false;
  }

  virtual ~SK_WindowManager (void) = default;

  SK_WindowManager (void)
  {
    SK_ICommandProcessor* cmd =
      SK_GetCommandProcessor ();

    if (! cmd)
    {
      SK_ReleaseAssert (! "Could not create or get the Special K Command Processor!");
      return;
    }

    unconfine_cursor_ =
      SK_CreateVar (SK_IVariable::Boolean,&config.window.unconfine_cursor,this);

    cmd->AddVariable (
      "Window.UnconfineCursor",
      unconfine_cursor_
    );

    confine_cursor_ =
      SK_CreateVar (SK_IVariable::Boolean,&config.window.confine_cursor,this);

    cmd->AddVariable (
      "Window.ConfineCursor",
      confine_cursor_
    );

    static bool dontcare;
    top_most_ =
      SK_CreateVar (SK_IVariable::Boolean, &dontcare, this);

    cmd->AddVariable (
      "Window.TopMost",
      top_most_
    );

    borderless_ =
      SK_CreateVar (SK_IVariable::Boolean, &config.window.borderless,      this);

    background_mute_ =
      SK_CreateVar (SK_IVariable::Boolean, &config.window.background_mute, this);

    center_window_ =
      SK_CreateVar (SK_IVariable::Boolean, &config.window.center,          this);

    fullscreen_ =
      SK_CreateVar (SK_IVariable::Boolean, &config.window.fullscreen,      this);

    x_override_ =
      SK_CreateVar (SK_IVariable::Int,     &config.window.res.override.x,  this);

    y_override_ =
      SK_CreateVar (SK_IVariable::Int,     &config.window.res.override.y,  this);

    res_override_ =
      SK_CreateVar (SK_IVariable::String,  override_res,                   this);

    // Don't need to listen for this event, actually.
    fix_mouse_ =
      SK_CreateVar (SK_IVariable::Boolean, &config.window.res.override.fix_mouse);

    x_offset_ =
      SK_CreateVar (SK_IVariable::Int,     &config.window.offset.x.absolute, this);

    y_offset_ =
      SK_CreateVar (SK_IVariable::Int,     &config.window.offset.y.absolute, this);

    x_off_pct_ =
      SK_CreateVar (SK_IVariable::Float,   &config.window.offset.x.percent,  this);

    y_off_pct_ =
      SK_CreateVar (SK_IVariable::Float,   &config.window.offset.y.percent,  this);

    static_rects_ =
      SK_CreateVar (SK_IVariable::Boolean, &override_window_rects);

    cmd->AddVariable (
      "Window.Borderless",
      borderless_
    );

    cmd->AddVariable (
      "Window.BackgroundMute",
      background_mute_
    );

    cmd->AddVariable (
      "Window.Center",
      center_window_
    );

    cmd->AddVariable (
      "Window.Fullscreen",
      fullscreen_
    );

    cmd->AddVariable (
      "Window.OverrideX",
      x_override_
    );

    cmd->AddVariable (
      "Window.OverrideY",
      y_override_
    );

    cmd->AddVariable (
      "Window.OverrideRes",
      res_override_
    );

    snprintf ( override_res, 32, "%lux%lu",
              config.window.res.override.x,
              config.window.res.override.y );

    cmd->AddVariable (
      "Window.OverrideMouse",
      fix_mouse_
    );

    cmd->AddVariable (
      "Window.XOffset",
      x_offset_
    );

    cmd->AddVariable (
      "Window.YOffset",
      y_offset_
    );

    cmd->AddVariable (
      "Window.ScaledXOffset",
      x_off_pct_
    );

    cmd->AddVariable (
      "Window.ScaledYOffset",
      y_off_pct_
    );

    cmd->AddVariable (
      "Window.StaticRects",
      static_rects_ );
  };

  static SK_WindowManager* getInstance (void)
  {
    if (pWindowManager == nullptr)
        pWindowManager = std::make_unique <SK_WindowManager> ();

    return
      pWindowManager.get ();
  }

  void muteGame (bool bMute)
  {
    SK_SetGameMute (bMute);
  }

  void finishFullscreen (bool fullscreen)
  {
    state_.fullscreen.actual = fullscreen;
  }

  void finishBorderless (bool borderless)
  {
    state_.borderless.actual = borderless;
  }

  bool makeFullscreen (bool fullscreen)
  {
    state_.fullscreen.set (fullscreen);
    return state_.fullscreen.get ();
  }

  bool makeBorderless (bool borderless)
  {
    state_.borderless.set (borderless);
    return state_.borderless.get ();
  }

  bool isChanging (void)
  {
    return state_.changing ();
  }

protected:
  struct state_s {
    struct value_s {
      bool set (bool val) { requested = val; return get (); };
      bool get (void)     { return actual;                  };

      bool requested = false;
      bool actual    = false;
    } borderless,
      fullscreen;

    bool changing (void)
    {
      return (borderless.requested != borderless.actual ||
              fullscreen.requested != fullscreen.actual);
    };
  } state_;

  SK_IVariable* borderless_       = nullptr;
  SK_IVariable* background_mute_  = nullptr;
  SK_IVariable* confine_cursor_   = nullptr;
  SK_IVariable* top_most_         = nullptr;
  SK_IVariable* unconfine_cursor_ = nullptr;
  SK_IVariable* center_window_    = nullptr;
  SK_IVariable* fullscreen_       = nullptr;
  SK_IVariable* x_override_       = nullptr;
  SK_IVariable* y_override_       = nullptr;
  SK_IVariable* res_override_     = nullptr; // Set X and Y at the same time
  SK_IVariable* fix_mouse_        = nullptr;
  SK_IVariable* x_offset_         = nullptr;
  SK_IVariable* y_offset_         = nullptr;

  SK_IVariable* x_off_pct_        = nullptr;
  SK_IVariable* y_off_pct_        = nullptr;

  SK_IVariable* static_rects_     = nullptr; // Fake the game into thinking the
                                             //   client rectangle has not changed.
                                             //
                                             // This solves display scaling problems in Fallout 4 and
                                             //   Skyrim...

  char override_res [32] = { };

private:
  static std::unique_ptr <SK_WindowManager> pWindowManager;
};

const int PreventAlwaysOnTop = 0;
const int        AlwaysOnTop = 1;

void
ActivateWindow (HWND hWnd, bool active = false)
{
  SK_ASSERT_NOT_THREADSAFE ();

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  const bool state_changed =
    (wm_dispatch->active_windows [hWnd] != active && hWnd == SK_GetGameWindow ());


  if (hWnd == SK_GetGameWindow ())
  {
    game_window.active = active;

    if (! config.window.background_render)
    {
         SK_XInput_Enable (active ? TRUE : FALSE);
    }

    else SK_XInput_Enable (TRUE);
  }



  if (state_changed)
  {
    if (config.window.always_on_top == PreventAlwaysOnTop)
    {
      if ((! active) && SK_Window_IsTopMost ())
        SK_DeferCommand ("Window.TopMost false");
    }

    else if (config.window.always_on_top == AlwaysOnTop)
    {
      if ((! active) && (! SK_Window_IsTopMost ()))
        SK_DeferCommand ("Window.TopMost true");
    }

    SK_Console::getInstance ()->reset ();

    if (config.window.background_mute)
      SK_WindowManager::getInstance ()->muteGame ((! active));

    // Keep Unity games from crashing at startup when forced into FULLSCREEN
    //
    //  ... also prevents a game from staying topmost when you Alt+Tab
    //

    if ( active && config.display.force_fullscreen &&
        ( static_cast <int> (rb.api)                &
          static_cast <int> (SK_RenderAPI::D3D9     )
        )                                           )
    {
      SetWindowLongPtrW    (game_window.hWnd, GWL_EXSTYLE,
       ( GetWindowLongPtrW (game_window.hWnd, GWL_EXSTYLE) &
                         ~(WS_EX_TOPMOST | WS_EX_NOACTIVATE)
       )                                 | WS_EX_APPWINDOW );

      SK_D3D9_TriggerReset (false);
    }
  }


  if (active && state_changed)
  {
    // Engage fullscreen
    if ( config.display.force_fullscreen )
    {
      rb.requestFullscreenMode (true);
    }


    if ((! rb.fullscreen_exclusive) && config.window.background_render)
    {
      if (! game_window.cursor_visible)
      {
        //while (ShowCursor (FALSE) >= 0)
        //  ;
      }

      if (! wm_dispatch->moving_windows.count (game_window.hWnd))
        SK_ClipCursor (&game_window.cursor_clip);
    }
  }

  else if ((! active) && state_changed)
  {
    if ((! rb.fullscreen_exclusive) && config.window.background_render)
    {
      game_window.cursor_visible =
        ShowCursor (TRUE) >= 1;
        ShowCursor (FALSE);

      //while (ShowCursor (TRUE) < 0)
      //  ;

      SK_ClipCursor (nullptr);
    }
  }


  if (config.window.confine_cursor && state_changed)
  {
    if (active)
    {
      SK_LOG4 ( ( L"Confining Mouse Cursor" ),
                  L"Window Mgr" );

      if (! wm_dispatch->moving_windows.count (game_window.hWnd))
      {
        ////// XXX: Is this really necessary? State should be consistent unless we missed
        //////        an event --- Write unit test?
        SK_GetWindowRect (game_window.hWnd, &game_window.actual.window);
        SK_ClipCursor    (&game_window.actual.window);
      }
    }

    else
    {
      SK_LOG4 ( ( L"Unconfining Mouse Cursor" ),
                  L"Window Mgr" );

      SK_ClipCursor (nullptr);
    }
  }

  if (config.window.unconfine_cursor && state_changed)
  {
    SK_LOG4 ( ( L"Unconfining Mouse Cursor" ),
                L"Window Mgr" );

    SK_ClipCursor (nullptr);
  }

  if (state_changed && hWnd == game_window.hWnd)
    SK_ImGui_Cursor.activateWindow (active);

  wm_dispatch->active_windows [hWnd] = active;
};


LRESULT
CALLBACK
SK_DetourWindowProc ( _In_  HWND   hWnd,
                      _In_  UINT   uMsg,
                      _In_  WPARAM wParam,
                      _In_  LPARAM lParam );

// Fix for Trails of Cold Steel
LRESULT
WINAPI
SK_COMPAT_SafeCallProc (sk_window_s* pWin, HWND hWnd_, UINT Msg, WPARAM wParam, LPARAM lParam);

sk_window_s       game_window;

BOOL
CALLBACK
SK_EnumWindows (HWND hWnd, LPARAM lParam)
{
  window_t& win =
    *reinterpret_cast <window_t *> (lParam);

  DWORD proc_id = 0;

  GetWindowThreadProcessId (hWnd, &proc_id);

  if (win.proc_id == proc_id)
  {
    //if ( GetWindow (hWnd, GW_OWNER) == (HWND)nullptr )
    {
      DWORD dwStyle   = 0,
            dwStyleEx = 0;

      switch (IsWindowUnicode (hWnd))
      {
        case TRUE:
          dwStyle   = SK_GetWindowLongW (hWnd, GWL_STYLE);
          dwStyleEx = SK_GetWindowLongW (hWnd, GWL_EXSTYLE);
          break;

        default:
        case FALSE:
          dwStyle   = SK_GetWindowLongA (hWnd, GWL_STYLE);
          dwStyleEx = SK_GetWindowLongA (hWnd, GWL_EXSTYLE);
          break;
      }

      bool    SKIM                   = false;
      wchar_t wszName [MAX_PATH + 2] = { };

      if (RealGetWindowClassW (hWnd, wszName, MAX_PATH))
      {
        if (StrStrIW (wszName, L"SKIM") != nullptr)
                                 SKIM    = true;
      }

      if ( (dwStyle & (WS_VISIBLE | WS_MINIMIZEBOX)) || SKIM )
      {
        win.root = hWnd;
        return FALSE;
      }
    }
  }

  return TRUE;
}

window_t
SK_FindRootWindow (DWORD proc_id)
{
  window_t win = { };

  win.proc_id  = proc_id;
  win.root     = nullptr;

  EnumWindows (SK_EnumWindows, (LPARAM)&win);

  return win;
}

std::unique_ptr <SK_WindowManager> SK_WindowManager::pWindowManager = nullptr;

void
SK_SetWindowResX (LONG x)
{
  //game_window.game.client.right = game_window.game.client.left + x;
  game_window.render_x = x;
}

void
SK_SetWindowResY (LONG y)
{
  //game_window.game.client.bottom = game_window.game.client.top + y;
  game_window.render_y = y;
}

LPRECT
SK_GetGameRect (void)
{
  return &game_window.game.client;
}

//
// Given raw coordinates directly from Windows, transform them into a coordinate space
//   that looks familiar to the game ;)
//
void
SK_CalcCursorPos (LPPOINT lpPoint)
{
  if (! game_window.needsCoordTransform ())
    return;

  struct {
    float width  = 64.0f,
          height = 64.0f;
  } in, out;

  lpPoint->x -= ( game_window.actual.window.left );
  lpPoint->y -= ( game_window.actual.window.top  );

  in.width    = static_cast <float> ( game_window.actual.client.right  -
                                      game_window.actual.client.left   );
  in.height   = static_cast <float> ( game_window.actual.client.bottom -
                                      game_window.actual.client.top    );

  out.width   = static_cast <float> ( game_window.game.client.right    -
                                      game_window.game.client.left     );
  out.height  = static_cast <float> ( game_window.game.client.bottom   -
                                           game_window.game.client.top      );

  float x     = 2.0f * (static_cast <float> (lpPoint->x) / in.width ) - 1.0f;
  float y     = 2.0f * (static_cast <float> (lpPoint->y) / in.height) - 1.0f;

  lpPoint->x  = static_cast <LONG> ( (x * out.width  + out.width) / 2.0f +
                                        game_window.coord_remap.offset.x );

  lpPoint->y  = static_cast <LONG> ( (y * out.height + out.height) / 2.0f +
                                        game_window.coord_remap.offset.y );
}

//
// Given coordinates in the game's original coordinate space, produce coordinates
//   in the global coordinate space.
//
void
SK_ReverseCursorPos (LPPOINT lpPoint)
{
  if (! game_window.needsCoordTransform ())
    return;

  struct {
    float width  = 64.0f,
          height = 64.0f;
  } in, out;

  lpPoint->x -= ( static_cast <LONG> ( game_window.coord_remap.offset.x +
                                       game_window.game.window.left     +
                                       game_window.game.client.left ) );
  lpPoint->y -= ( static_cast <LONG> ( game_window.coord_remap.offset.y +
                                       game_window.game.window.top      +
                                       game_window.game.client.top ) );

  in.width   = static_cast <float> ( game_window.game.client.right  -
                                     game_window.game.client.left   );
  in.height  = static_cast <float> ( game_window.game.client.bottom -
                                     game_window.game.client.top    );

  out.width  = static_cast <float> ( game_window.actual.client.right -
                                     game_window.actual.client.left  );
  out.height = static_cast <float> ( game_window.actual.client.bottom -
                                     game_window.actual.client.top    );

  float x    = 2.0f * (static_cast <float> (lpPoint->x) / in.width ) - 1.0f;
  float y    = 2.0f * (static_cast <float> (lpPoint->y) / in.height) - 1.0f;

  lpPoint->x = static_cast <LONG> ( (x * out.width  + out.width) / 2.0f +
                                   game_window.coord_remap.offset.x );

  lpPoint->y = static_cast <LONG> ( (y * out.height + out.height) / 2.0f +
                                   game_window.coord_remap.offset.y );
}

ULONG   game_mouselook = 0;
int     game_x, game_y;

#if 0
BOOL
WINAPI
GetCursorPos_Detour (LPPOINT lpPoint)
{
  BOOL ret = GetCursorPos_Original (lpPoint);

  // Correct the cursor position for Aspect Ratio
  if (game_window.needsCoordTransform ())
  {
    SK_CalcCursorPos (lpPoint);
  }

  return ret;
}
#endif

bool
SK_ExpandSmallClipCursor (RECT *lpRect)
{
  // Don't do this until we've initialized the window
  if (game_window.WndProc_Original == nullptr)
    return false;


  RECT client, window;

  SK_GetClientRect (SK_GetGameWindow (), &client);
  SK_GetWindowRect (SK_GetGameWindow (), &window);

  if (        (lpRect->right  - lpRect->left) <
      0.75f * (window.right  -  window.left) ||
              (lpRect->bottom - lpRect->top)  <
      0.75f * (window.bottom -  window.top) )
  {
    static DWORD dwLastReported = 0;
           DWORD dwNow          = timeGetTime ();

    // Throttle this message
    if (dwLastReported < dwNow - 30000UL)
    {
      SK_LOG0 ( (L"Game is using an abnormally small mouse clipping rect, it"
                 L" would adversely affect UI support and has been ignored." ),
                 L"Input Mgr." );
      dwLastReported = dwNow;
    }

    return true;
  }

  return false;
}

BOOL
WINAPI
ClipCursor_Detour (const RECT *lpRect)
{
  SK_LOG_FIRST_CALL

    if (lpRect != nullptr)
    {
      if (SK_ExpandSmallClipCursor ((RECT *)lpRect))
      {
        // Generally this only matters if background rendering is enabled;
        //   flat-out ignore cursor clip rects if the window's not even active.
        if (game_window.active)
        {
          return
            SK_ClipCursor ( config.window.unconfine_cursor ? nullptr :
                           &game_window.actual.window );
        }
      }

      game_window.cursor_clip = *lpRect;
    }
    else
      return SK_ClipCursor (nullptr);


  // Don't let the game unclip the cursor, but DO remember the
  //   coordinates that it wants.
  if (config.window.confine_cursor && lpRect != &game_window.cursor_clip)
    return TRUE;


  //
  // If the game uses mouse clipping and we are running in borderless,
  //   we need to re-adjust the window coordinates.
  //
  if (game_window.needsCoordTransform ())
  {
    POINT top_left     = { std::numeric_limits <LONG>::max (), std::numeric_limits <LONG>::max () };
    POINT bottom_right = { std::numeric_limits <LONG>::min (), std::numeric_limits <LONG>::min () };

    top_left.y = game_window.actual.window.top;
    top_left.x = game_window.actual.window.left;

    bottom_right.y = game_window.actual.window.bottom;
    bottom_right.x = game_window.actual.window.right;

    SK_ReverseCursorPos (&top_left);
    SK_ReverseCursorPos (&bottom_right);

    game_window.cursor_clip.bottom = bottom_right.y;
    game_window.cursor_clip.top    = top_left.y;

    game_window.cursor_clip.left   = top_left.x;
    game_window.cursor_clip.right  = bottom_right.x;
  }


  // Prevent the game from clipping the cursor
  if (config.window.unconfine_cursor)
    return SK_ClipCursor (nullptr);


  if (game_window.active && (! wm_dispatch->moving_windows.count (game_window.hWnd)))
  {
    return
      SK_ClipCursor (&game_window.cursor_clip);
  }

  else
  {
    return
      SK_ClipCursor (nullptr);
  }
}

void
SK_CenterWindowAtMouse (BOOL remember_pos)
{
  // This is too much trouble to bother with...
  if ( config.window.center || ( config.window.borderless &&
                                 config.window.fullscreen ) )
  {
    return;
  }

  static volatile LONG               __busy = FALSE;
  if (! InterlockedCompareExchange (&__busy, TRUE, FALSE))
  {
    SK_Thread_Create (
    [](LPVOID user) ->
    DWORD
    {
      SetCurrentThreadDescription (L"[SK] Window Center Thread");

      BOOL  remember_pos = (user != nullptr);
      POINT mouse        = { 0, 0 };

      SK_GetCursorPos (&mouse);

      struct {
        struct {
          float percent  = 0.0f;
          int   absolute =   0L;
        } x,y;
      } offsets;

      if (! remember_pos)
      {
        offsets.x.absolute = config.window.offset.x.absolute;
        offsets.y.absolute = config.window.offset.y.absolute;

        offsets.x.percent = config.window.offset.x.percent;
        offsets.y.percent = config.window.offset.y.percent;
      }

      config.window.offset.x.absolute = mouse.x;
      config.window.offset.y.absolute = mouse.y;

      config.window.offset.x.absolute -=
        (        game_window.actual.window.right  -
                 game_window.actual.window.left ) / 2;
      config.window.offset.y.absolute -=
        (        game_window.actual.window.bottom -
                 game_window.actual.window.top  ) / 2;

      if (config.window.offset.x.absolute <= 0)
        config.window.offset.x.absolute = 1;  // 1 = Flush with Left

      if (config.window.offset.y.absolute <= 0)
        config.window.offset.y.absolute = 1;  // 1 = Flush with Top

      config.window.offset.x.percent = 0.0f;
      config.window.offset.y.percent = 0.0f;

      SK_AdjustWindow ();

      if (! remember_pos)
      {
        config.window.offset.x.absolute = offsets.x.absolute;
        config.window.offset.y.absolute = offsets.y.absolute;

        config.window.offset.x.percent  = offsets.x.percent;
        config.window.offset.y.percent  = offsets.y.percent;
      }

      InterlockedExchange (&__busy, FALSE);

      SK_Thread_CloseSelf ();

      return 0;
      // Don't dereference this, it's actually a boolean
    }, reinterpret_cast <LPVOID>   (
            static_cast <uintptr_t> (remember_pos)
                                   ) );
  }
}

BOOL
WINAPI
SK_MoveWindow (
  _In_ HWND hWnd,
  _In_ int  X,
  _In_ int  Y,
  _In_ int  nWidth,
  _In_ int  nHeight,
  _In_ BOOL bRedraw )
{
  if (MoveWindow_Original != nullptr)
    return MoveWindow_Original (hWnd, X, Y, nWidth, nHeight, bRedraw);

  return
    MoveWindow (hWnd, X, Y, nWidth, nHeight, bRedraw);
}

BOOL
WINAPI
MoveWindow_Detour (
  _In_ HWND hWnd,
  _In_ int  X,
  _In_ int  Y,
  _In_ int  nWidth,
  _In_ int  nHeight,
  _In_ BOOL bRedraw )
{
  SK_LOG_FIRST_CALL

  return
    SK_MoveWindow ( hWnd,
                      X, Y,
                        nWidth, nHeight,
                          bRedraw );
}

BOOL
WINAPI
SK_SetWindowPlacement (
  _In_       HWND             hWnd,
  _In_ const WINDOWPLACEMENT *lpwndpl)
{
  if (SetWindowPlacement_Original != nullptr)
    return SetWindowPlacement_Original (hWnd, lpwndpl);

  return
    SetWindowPlacement (hWnd, lpwndpl);
}

BOOL
WINAPI
SetWindowPlacement_Detour(
  _In_       HWND             hWnd,
  _In_ const WINDOWPLACEMENT *lpwndpl)
{
  SK_WINDOW_LOG_CALL_UNTESTED ();

  return
    SK_SetWindowPlacement ( hWnd, lpwndpl );
}

BOOL
WINAPI
SetWindowPos_Detour(
  _In_     HWND hWnd,
  _In_opt_ HWND hWndInsertAfter,
  _In_     int  X,
  _In_     int  Y,
  _In_     int  cx,
  _In_     int  cy,
  _In_     UINT uFlags)
{
  SK_LOG_FIRST_CALL

    if (hWnd == game_window.hWnd)
      SK_WINDOW_LOG_CALL1 ();

  if (config.display.force_windowed)
  {
    if (hWndInsertAfter == HWND_TOPMOST)
        hWndInsertAfter  = HWND_TOP;
  }

  ////uFlags |= SWP_ASYNCWINDOWPOS;

  BOOL bRet =
    SK_SetWindowPos ( hWnd,
                      hWndInsertAfter,
                         X,   Y,
                         cx, cy,
                                 uFlags );

  return bRet;
}

BOOL
WINAPI
SK_GetWindowRect (HWND hWnd, LPRECT rect)
{
  if (GetWindowRect_Original != nullptr)
    return GetWindowRect_Original (hWnd, rect);

  return
    GetWindowRect (hWnd, rect);
}

BOOL
WINAPI
SK_GetClientRect (HWND hWnd, LPRECT rect)
{
  if (GetClientRect_Original != nullptr)
    return GetClientRect_Original (hWnd, rect);

  return
    GetClientRect (hWnd, rect);
}

static
BOOL
WINAPI
GetClientRect_Detour (
  _In_  HWND   hWnd,
  _Out_ LPRECT lpRect )
{
  SK_LOG_FIRST_CALL

    if ( config.window.borderless &&
         hWnd == game_window.hWnd && config.window.fullscreen )
    {
      //lpRect->left = 0; lpRect->right  = config.window.res.override.x;
      //lpRect->top  = 0; lpRect->bottom = config.window.res.override.y;

      HMONITOR hMonitor =
        MonitorFromWindow ( game_window.hWnd,
                           MONITOR_DEFAULTTOPRIMARY );

      MONITORINFO mi   = {         };
      mi.cbSize        = sizeof (mi);
      GetMonitorInfo (hMonitor, &mi);

      lpRect->left  = 0;
      lpRect->right = mi.rcMonitor.right - mi.rcMonitor.left;

      lpRect->top    = 0;
      lpRect->bottom = mi.rcMonitor.bottom - mi.rcMonitor.top;

      //    *lpRect = game_window.game.client;

      return TRUE;
    }

    else
    {
      return
        SK_GetClientRect (hWnd, lpRect);
    }
}

static
BOOL
WINAPI
GetWindowRect_Detour (
  _In_   HWND  hWnd,
  _Out_ LPRECT lpRect )
{
  //if (SK_GetCurrentGameID () == SK_GAME_ID::MonsterHunterWorld)
  //{
  //  if (hWnd == 0)
  //      hWnd  = GetDesktopWindow ();
  //}

  SK_LOG_FIRST_CALL

    if ( config.window.borderless &&
         hWnd == game_window.hWnd && config.window.fullscreen )
    {
      //lpRect->left = 0; lpRect->right  = config.window.res.override.x;
      //lpRect->top  = 0; lpRect->bottom = config.window.res.override.y;

      HMONITOR hMonitor =
        MonitorFromWindow ( game_window.hWnd,
                           MONITOR_DEFAULTTOPRIMARY );

      MONITORINFO mi   = {         };
      mi.cbSize        = sizeof (mi);
      GetMonitorInfo (hMonitor, &mi);

      lpRect->left  = mi.rcMonitor.left;
      lpRect->right = mi.rcMonitor.right;

      lpRect->top    = mi.rcMonitor.top;
      lpRect->bottom = mi.rcMonitor.bottom;

      //*lpRect = game_window.game.window;

      return TRUE;
    }

    else
    {
      return
        SK_GetWindowRect (hWnd, lpRect);
    }
}

bool
WINAPI
SK_IsRectZero (_In_ const RECT *lpRect)
{
  return ( lpRect->left   == lpRect->right &&
           lpRect->bottom == lpRect->top );
}

bool
WINAPI
SK_IsRectInfinite (_In_ const tagRECT *lpRect)
{
  return ( lpRect->left   == LONG_MIN && lpRect->top   == LONG_MIN &&
           lpRect->bottom == LONG_MAX && lpRect->right == LONG_MAX );
}

bool
WINAPI
SK_IsRectFullscreen (_In_ const tagRECT *lpRect)
{
  const int virtual_x = SK_GetSystemMetrics (SM_CXVIRTUALSCREEN);
  const int virtual_y = SK_GetSystemMetrics (SM_CYVIRTUALSCREEN);

  return ( lpRect->left  == 0         && lpRect->top    == 0 &&
           lpRect->right == virtual_x && lpRect->bottom == virtual_y );
}

bool
WINAPI
SK_IsClipRectFinite (_In_ const tagRECT *lpRect)
{
  return (! ( SK_IsRectZero       (lpRect) ||
              SK_IsRectInfinite   (lpRect) ||
              SK_IsRectFullscreen (lpRect) ) );
}

BOOL
WINAPI
SK_AdjustWindowRect (
  _Inout_ LPRECT lpRect,
  _In_    DWORD  dwStyle,
  _In_    BOOL   bMenu )
{
  if (AdjustWindowRect_Original != nullptr)
    return AdjustWindowRect_Original (lpRect, dwStyle, bMenu);

  return
    AdjustWindowRect (lpRect, dwStyle, bMenu);
}

BOOL
WINAPI
AdjustWindowRect_Detour (
  _Inout_ LPRECT lpRect,
  _In_    DWORD  dwStyle,
  _In_    BOOL   bMenu  )
{
  SK_LOG1 ( ( L"AdjustWindowRect ( "
              L"{%4li,%4li / %4li,%4li}, 0x%04X, %li ) - %s",
              lpRect->left,  lpRect->top,
              lpRect->right, lpRect->bottom,
                dwStyle,
                bMenu,
                  SK_SummarizeCaller ().c_str () ),
              L"Window Mgr" );

  if (SK_GetCurrentGameID () == SK_GAME_ID::ZeroEscape)
    return TRUE;

  if (SK_GetCurrentGameID () == SK_GAME_ID::DotHackGU)
    return TRUE;

  // Override if forcing Fullscreen Borderless
  //
  if (config.window.fullscreen && config.window.borderless && (! bMenu))
  {
    HMONITOR hMonitor =
      MonitorFromWindow ( game_window.hWnd,
                         MONITOR_DEFAULTTONEAREST );

    MONITORINFO mi   = {         };
    mi.cbSize        = sizeof (mi);
    GetMonitorInfo (hMonitor, &mi);

    lpRect->left   = mi.rcMonitor.left;
    lpRect->right  = mi.rcMonitor.right;

    lpRect->top    = mi.rcMonitor.top;
    lpRect->bottom = mi.rcMonitor.bottom;

    return TRUE;
  }

  return
    SK_AdjustWindowRect (lpRect, dwStyle, bMenu);
}

void SK_SetWindowStyle   (DWORD_PTR dwStyle_ptr,   SetWindowLongPtr_pfn pDispatchFunc = nullptr);
void SK_SetWindowStyleEx (DWORD_PTR dwStyleEx_ptr, SetWindowLongPtr_pfn pDispatchFunc = nullptr);

BOOL
WINAPI
SK_AdjustWindowRectEx (
  _Inout_ LPRECT lpRect,
  _In_    DWORD  dwStyle,
  _In_    BOOL   bMenu,
  _In_    DWORD  dwExStyle )
{
  if (AdjustWindowRectEx_Original != nullptr)
    return AdjustWindowRectEx_Original (lpRect, dwStyle, bMenu, dwExStyle);

  return
    AdjustWindowRectEx (lpRect, dwStyle, bMenu, dwExStyle);
}

BOOL
WINAPI
AdjustWindowRectEx_Detour (
  _Inout_ LPRECT lpRect,
  _In_    DWORD  dwStyle,
  _In_    BOOL   bMenu,
  _In_    DWORD  dwExStyle )
{
  SK_LOG1 ( ( L"AdjustWindowRectEx ( "
              L"{%4li,%4li / %4li,%4li}, 0x%04X, %li, 0x%04X ) - %s",
           lpRect->left,       lpRect->top,
           lpRect->right, lpRect->bottom,
           dwStyle, bMenu,
           dwExStyle,
           SK_SummarizeCaller ().c_str () ),
           L"Window Mgr" );

  if (SK_GetCurrentGameID () == SK_GAME_ID::ZeroEscape)
    return TRUE;

  if (SK_GetCurrentGameID () == SK_GAME_ID::DotHackGU)
    return TRUE;

  // Override if forcing Fullscreen Borderless
  //
  if (config.window.fullscreen && config.window.borderless && (! bMenu))
  {
    HMONITOR hMonitor =
      MonitorFromWindow ( game_window.hWnd,
                         MONITOR_DEFAULTTONEAREST );

    MONITORINFO mi   = {         };
    mi.cbSize        = sizeof (mi);
    GetMonitorInfo (hMonitor, &mi);

    lpRect->left  = mi.rcMonitor.left;
    lpRect->right = mi.rcMonitor.right;

    lpRect->top    = mi.rcMonitor.top;
    lpRect->bottom = mi.rcMonitor.bottom;

    return TRUE;
  }

  return
    SK_AdjustWindowRectEx (lpRect, dwStyle, bMenu, dwExStyle);
}

// Convenience function since there are so damn many
//   variants of these functions that need to be hooked.
__forceinline
LONG
WINAPI
SetWindowLong_Marshall (
  _In_ SetWindowLong_pfn pOrigFunc,
  _In_ HWND              hWnd,
  _In_ int               nIndex,
  _In_ LONG              dwNewLong )
{
  if (! pOrigFunc) return 0;

  // Override window styles
  if (hWnd == game_window.hWnd)
  {
    switch (nIndex)
    {
      case GWL_STYLE:
      {
        game_window.game.style =
          static_cast <ULONG_PTR> (dwNewLong);

        if (config.window.borderless)
        {
          game_window.actual.style =
            SK_BORDERLESS;
        }

        else
        {
          game_window.actual.style =
            game_window.game.style;
        }

        if ( SK_WindowManager::StyleHasBorder (
              game_window.game.style          )
           )
        {
          game_window.border_style =
            game_window.game.style;
        }

        SK_SetWindowStyle ( game_window.actual.style,
         reinterpret_cast < SetWindowLongPtr_pfn >
                              ( pOrigFunc )
                          );

        return
          static_cast <LONG> (game_window.actual.style);
      }

      case GWL_EXSTYLE:
      {
        game_window.game.style_ex =
          static_cast <ULONG_PTR> (dwNewLong);

        if (config.window.borderless)
        {
          game_window.actual.style_ex =
            SK_BORDERLESS_EX;
        }

        else
        {
          game_window.actual.style_ex =
            game_window.game.style;
        }

        if ( SK_WindowManager::StyleHasBorder (
               game_window.actual.style
           )
          )
        {
          game_window.border_style_ex =
            game_window.game.style_ex;
        }

        SK_SetWindowStyleEx (                                          game_window.actual.style_ex,
                              reinterpret_cast <SetWindowLongPtr_pfn> (pOrigFunc) );

        return
          gsl::narrow_cast <ULONG> (
            game_window.actual.style_ex
          );
      }

      case GWLP_WNDPROC:
      {
        if (game_window.hooked)
        {
          //game_window.WndProc_Original = reinterpret_cast <WNDPROC> (dwNewLong);
          //return dwNewLong;
        }
      } break;
    }
  }

  return
    pOrigFunc (hWnd, nIndex, dwNewLong);
}

LONG
WINAPI
SK_SetWindowLongA (
  _In_ HWND hWnd,
  _In_ int  nIndex,
  _In_ LONG dwNewLong )
{
  return
    static_cast <LONG> (
      SK_SetWindowLongPtrA (hWnd, nIndex, dwNewLong)
    );
}

DECLSPEC_NOINLINE
LONG
WINAPI
SetWindowLongA_Detour (
  _In_ HWND hWnd,
  _In_ int  nIndex,
  _In_ LONG dwNewLong
)
{
  SK_LOG_FIRST_CALL

  return
    SetWindowLong_Marshall (
      SK_SetWindowLongA,
        hWnd,
          nIndex,
            dwNewLong
    );
}

LONG
WINAPI
SK_SetWindowLongW (
  _In_ HWND hWnd,
  _In_ int  nIndex,
  _In_ LONG dwNewLong )
{
  return
    static_cast <LONG> (
      SK_SetWindowLongPtrW (hWnd, nIndex, dwNewLong)
    );
}

DECLSPEC_NOINLINE
LONG
WINAPI
SetWindowLongW_Detour (
  _In_ HWND hWnd,
  _In_ int  nIndex,
  _In_ LONG dwNewLong
)
{
  SK_LOG_FIRST_CALL

  return
    SetWindowLong_Marshall (
      SK_SetWindowLongW,
        hWnd,
          nIndex,
            dwNewLong
    );
}

#if 0
// Convenience function since there are so damn many
//   variants of these functions that need to be hooked.
__forceinline
LONG
WINAPI
SetClassLong_Marshall (
  _In_ SetClassLong_pfn pOrigFunc,
  _In_ HWND             hWnd,
  _In_ int              nIndex,
  _In_ LONG             dwNewLong )
{
  return
    pOrigFunc (hWnd, nIndex,dwNewLong);
}

DECLSPEC_NOINLINE
LONG
WINAPI
SetClassLongA_Detour (
  _In_ HWND hWnd,
  _In_ int  nIndex,
  _In_ LONG dwNewLong
)
{
  SK_LOG_FIRST_CALL

    return SetClassLong_Marshall (
      SetClassLongA_Original,
        hWnd,
          nIndex,
            dwNewLong
    );
}

DECLSPEC_NOINLINE
LONG
WINAPI
SetClassLongPtrA_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong
)
{
  SK_LOG_FIRST_CALL

    return SetClassLong_Marshall (
      SetClassLongPtrA_Original,
      hWnd,
      nIndex,
      dwNewLong
    );
}

DECLSPEC_NOINLINE
LONG
WINAPI
SetClassLongPtrW_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong
)
{
  SK_LOG_FIRST_CALL

    return SetClassLong_Marshall (
      SetClassLongPtrW_Original,
      hWnd,
      nIndex,
      dwNewLong
    );
}
#endif

// Convenience function since there are so damn many
//   variants of these functions that need to be hooked.
__forceinline
LONG
WINAPI
GetWindowLong_Marshall (
  _In_ GetWindowLong_pfn pOrigFunc,
  _In_ HWND              hWnd,
  _In_ int               nIndex
)
{
  if (hWnd == game_window.hWnd)
  {
    switch (nIndex)
    {
      case GWL_STYLE:
        return static_cast <LONG> (game_window.game.style);
      case GWL_EXSTYLE:
        return static_cast <LONG> (game_window.game.style_ex);
    }
  }


  if (pOrigFunc != nullptr)
  {
    return
      pOrigFunc (hWnd, nIndex);
  }

  return 0;
}

LONG
WINAPI
SK_GetWindowLongA (
  _In_ HWND hWnd,
  _In_ int  nIndex)
{
  if (GetWindowLongA_Original != nullptr)
    return GetWindowLongA_Original (hWnd, nIndex);

  return
    GetWindowLongA (hWnd, nIndex);
}

DECLSPEC_NOINLINE
LONG
WINAPI
GetWindowLongA_Detour (
  _In_ HWND hWnd,
  _In_ int  nIndex
)
{
  SK_LOG_FIRST_CALL

    return GetWindowLong_Marshall (
        SK_GetWindowLongA,
          hWnd,
            nIndex
    );
}

LONG
WINAPI
SK_GetWindowLongW (
  _In_ HWND hWnd,
  _In_ int  nIndex)
{
  if (GetWindowLongW_Original != nullptr)
    return GetWindowLongW_Original (hWnd, nIndex);

  return
    GetWindowLongW (hWnd, nIndex);
}

DECLSPEC_NOINLINE
LONG
WINAPI
GetWindowLongW_Detour (
  _In_ HWND hWnd,
  _In_ int  nIndex
)
{
  SK_LOG_FIRST_CALL

  return GetWindowLong_Marshall (
      SK_GetWindowLongW,
        hWnd,
          nIndex
  );
}

// Convenience function since there are so damn many
//   variants of these functions that need to be hooked.
__forceinline
LONG_PTR
WINAPI
SetWindowLongPtr_Marshall (
  _In_ SetWindowLongPtr_pfn pOrigFunc,
  _In_ HWND                 hWnd,
  _In_ int                  nIndex,
  _In_ LONG_PTR             dwNewLong
)
{
  //dll_log->Log (L"SetWindowLongPtr: Idx=%li, Val=%X -- Caller: %s", nIndex, dwNewLong, SK_GetCallerName ().c_str ());

  // Override window styles
  if (hWnd == game_window.hWnd)
  {
    switch (nIndex)
    {
      case GWL_STYLE:
      {
        game_window.game.style =
          dwNewLong;

        if (config.window.borderless)
        {
          game_window.actual.style =
            SK_BORDERLESS;
        }

        else
        {
          game_window.actual.style =
            game_window.game.style;
        }

        if ( SK_WindowManager::StyleHasBorder (
               game_window.game.style
             )
           )
        {
          game_window.border_style =
            game_window.game.style;
        }

        if (pOrigFunc != nullptr)
          SK_SetWindowStyle ( game_window.actual.style,
                                pOrigFunc );

        return
          game_window.actual.style;
      }

      case GWL_EXSTYLE:
      {
        game_window.game.style_ex =
          dwNewLong;

        if (config.window.borderless)
        {
          game_window.actual.style_ex =
            SK_BORDERLESS_EX;
        }

        else
        {
          game_window.actual.style_ex =
            game_window.game.style;
        }

        if ( SK_WindowManager::StyleHasBorder (
               game_window.actual.style
             )
           )
        {
          game_window.border_style_ex =
            game_window.game.style_ex;
        }

        if (pOrigFunc != nullptr)
          SK_SetWindowStyleEx (game_window.actual.style_ex, pOrigFunc);

        return
          game_window.actual.style_ex;
      }

      case GWLP_WNDPROC:
      {
        if (game_window.hooked)
        {
          //game_window.WndProc_Original = reinterpret_cast <WNDPROC> (dwNewLong);
          //return dwNewLong;
        }
      } break;
    }
  }

  return pOrigFunc != nullptr ?
         pOrigFunc (hWnd, nIndex, dwNewLong) : 0;
}

LONG_PTR
WINAPI
SK_SetWindowLongPtrA (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong )
{
#if 0
        DWORD dwPid;
  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWnd, &dwPid);

  auto _Impl = [&](HWND hWnd, int nIndex, LONG_PTR dwNewLong) ->
  ULONG_PTR
  {
    if (SetWindowLongPtrA_Original != nullptr)
      return SetWindowLongPtrA_Original (hWnd, nIndex, dwNewLong);
    else
      return SetWindowLongPtrA          (hWnd, nIndex, dwNewLong);
  };

  if (dwOrigThreadId == SK_GetCurrentThreadId () || nIndex != GWL_EXSTYLE)
  {
    return
      _Impl (hWnd, nIndex, dwNewLong);
  }

  std::future <LONG_PTR> resultOfAsync =
                            std::async (
                    std::launch::async,
  [&](HWND hWnd, int nIndex, LONG_PTR dwNewLong) ->
  LONG_PTR
  {
    return
      _Impl (hWnd, nIndex, dwNewLong);
  }, hWnd, nIndex, dwNewLong);

  return
    dwNewLong;
#else  //if (SetWindowLongPtrA_Original != nullptr)
  //  return SetWindowLongPtrA_Original (hWnd, nIndex, dwNewLong);
  //
  //return SetWindowLongPtrA (hWnd, nIndex, dwNewLong);
        DWORD dwPid;
  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWnd, &dwPid);

  if (dwOrigThreadId == SK_GetCurrentThreadId () || nIndex != GWL_EXSTYLE)
  {
    if (SetWindowLongPtrA_Original != nullptr)
      return SetWindowLongPtrA_Original (hWnd, nIndex, dwNewLong);
    else
      return SetWindowLongPtrA          (hWnd, nIndex, dwNewLong);
  }

  struct args_s {
    HWND     hWnd;
    int      nIndex;
    LONG_PTR dwNewLong;
  };

  struct dispatch_queue_s {
    concurrency::concurrent_queue <args_s>
           queued;
    HANDLE hSignal;
  };

  static SK_LazyGlobal <dispatch_queue_s> work;

  SK_RunOnce ( work->hSignal =
                 SK_CreateEvent (nullptr, FALSE, FALSE, nullptr) );

  work->queued.push (
    args_s { hWnd, nIndex, dwNewLong }
  );

  SK_RunOnce (
    SK_Thread_Create ([](LPVOID lpUser) ->
    DWORD
    {
      dispatch_queue_s *dispatch =
        (dispatch_queue_s *)lpUser;

      HANDLE hSignals [] = {
        __SK_DLL_TeardownEvent,
             dispatch->hSignal
      };

      DWORD dwWait = 0;

      while ( (dwWait = WaitForMultipleObjects (
                          2, hSignals, FALSE, INFINITE )
              ) != WAIT_OBJECT_0 )
      {
        args_s args;

        while (dispatch->queued.try_pop (args))
        {
          if (SetWindowLongPtrA_Original != nullptr)
              SetWindowLongPtrA_Original (args.hWnd, args.nIndex, args.dwNewLong);
          else
              SetWindowLongPtrA          (args.hWnd, args.nIndex, args.dwNewLong);
        }
      }

      CloseHandle (dispatch->hSignal);

      SK_Thread_CloseSelf ();

      return 0;
    }, (LPVOID)work.getPtr () )
  );

  SetEvent (work->hSignal);

  return
    dwNewLong;
#endif
}

DECLSPEC_NOINLINE
LONG_PTR
WINAPI
SetWindowLongPtrA_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong
)
{
  SK_LOG_FIRST_CALL

  return SetWindowLongPtr_Marshall (
    SK_SetWindowLongPtrA,
      hWnd,
        nIndex,
          dwNewLong
  );
}

LONG_PTR
WINAPI
SK_SetWindowLongPtrW (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong )
{
#if 0
        DWORD dwPid;
  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWnd, &dwPid);

  auto _Impl = [&](HWND hWnd, int nIndex, LONG_PTR dwNewLong) ->
  ULONG_PTR
  {
    if (SetWindowLongPtrW_Original != nullptr)
      return SetWindowLongPtrW_Original (hWnd, nIndex, dwNewLong);
    else
      return SetWindowLongPtrW          (hWnd, nIndex, dwNewLong);
  };

  if (dwOrigThreadId == SK_GetCurrentThreadId () || nIndex != GWL_EXSTYLE)
  {
    return
      _Impl (hWnd, nIndex, dwNewLong);
  }

  std::future <LONG_PTR> resultOfAsync =
                            std::async (
                    std::launch::async,
  [&](HWND hWnd, int nIndex, LONG_PTR dwNewLong) ->
  LONG_PTR
  {
    return
      _Impl (hWnd, nIndex, dwNewLong);
  }, hWnd, nIndex, dwNewLong);

  return
    dwNewLong;
#else
        DWORD dwPid;
  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWnd, &dwPid);

  if (dwOrigThreadId == SK_GetCurrentThreadId () || nIndex != GWL_EXSTYLE)
  {
    if (SetWindowLongPtrW_Original != nullptr)
      return SetWindowLongPtrW_Original (hWnd, nIndex, dwNewLong);
    else
      return SetWindowLongPtrW          (hWnd, nIndex, dwNewLong);
  }

  struct args_s {
    HWND     hWnd;
    int      nIndex;
    LONG_PTR dwNewLong;
  };

  struct dispatch_queue_s {
    concurrency::concurrent_queue <args_s>
           queued;
    HANDLE hSignal;
  };

  static SK_LazyGlobal <dispatch_queue_s> work;

  SK_RunOnce ( work->hSignal =
                 SK_CreateEvent (nullptr, FALSE, FALSE, nullptr) );

  work->queued.push (
    args_s { hWnd, nIndex, dwNewLong }
  );

  SK_RunOnce (
    SK_Thread_Create ([](LPVOID lpUser) ->
    DWORD
    {
      dispatch_queue_s *dispatch =
        (dispatch_queue_s *)lpUser;

      HANDLE hSignals [] = {
        __SK_DLL_TeardownEvent,
             dispatch->hSignal
      };

      DWORD dwWait = 0;

      while ( (dwWait = WaitForMultipleObjects (
                          2, hSignals, FALSE, INFINITE )
              ) != WAIT_OBJECT_0 )
      {
        args_s args;

        while (dispatch->queued.try_pop (args))
        {
          if (SetWindowLongPtrW_Original != nullptr)
              SetWindowLongPtrW_Original (args.hWnd, args.nIndex, args.dwNewLong);
          else
              SetWindowLongPtrW          (args.hWnd, args.nIndex, args.dwNewLong);
        }
      }

      CloseHandle (dispatch->hSignal);

      SK_Thread_CloseSelf ();

      return 0;
    }, (LPVOID)work.getPtr () )
  );

  SetEvent (work->hSignal);

  return
    dwNewLong;
#endif
}

DECLSPEC_NOINLINE
LONG_PTR
WINAPI
SetWindowLongPtrW_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex,
  _In_ LONG_PTR dwNewLong
)
{
  SK_LOG_FIRST_CALL

  return SetWindowLongPtr_Marshall (
    SK_SetWindowLongPtrW,
      hWnd,
        nIndex,
          dwNewLong
  );
}

// Convenience function since there are so damn many
//   variants of these functions that need to be hooked.
__forceinline
LONG_PTR
WINAPI
GetWindowLongPtr_Marshall (
  _In_ GetWindowLongPtr_pfn pOrigFunc,
  _In_ HWND                 hWnd,
  _In_ int                  nIndex
)
{
  if (hWnd == game_window.hWnd)
  {
    switch (nIndex)
    {
      case GWL_STYLE:
        return game_window.game.style;
      case GWL_EXSTYLE:
        return game_window.game.style_ex;
    }
  }

  if (pOrigFunc != nullptr)
    return
      pOrigFunc (hWnd, nIndex);
  else
    return -1;
}

LONG_PTR
WINAPI
SK_GetWindowLongPtrA (
  _In_ HWND     hWnd,
  _In_ int      nIndex   )
{
  if (GetWindowLongPtrA_Original != nullptr)
    return GetWindowLongPtrA_Original (hWnd, nIndex);

  return GetWindowLongPtrA (hWnd, nIndex);
}

DECLSPEC_NOINLINE
LONG_PTR
WINAPI
GetWindowLongPtrA_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex
)
{
  SK_LOG_FIRST_CALL

  return GetWindowLongPtr_Marshall (
      SK_GetWindowLongPtrA,
           hWnd,
             nIndex
  );
}

LONG_PTR
WINAPI
SK_GetWindowLongPtrW   (
  _In_ HWND     hWnd,
  _In_ int      nIndex )
{
  if (GetWindowLongPtrW_Original != nullptr)
    return GetWindowLongPtrW_Original (hWnd, nIndex);

  return GetWindowLongPtrW (hWnd, nIndex);
}

DECLSPEC_NOINLINE
LONG_PTR
WINAPI
GetWindowLongPtrW_Detour (
  _In_ HWND     hWnd,
  _In_ int      nIndex
)
{
  SK_LOG_FIRST_CALL

  return GetWindowLongPtr_Marshall (
      SK_GetWindowLongPtrW,
           hWnd,
             nIndex
  );
}

void
SK_SetWindowStyle (DWORD_PTR dwStyle_ptr, SetWindowLongPtr_pfn pDispatchFunc)
{
  // Ensure that the border style is sane
  if (dwStyle_ptr == game_window.border_style)
  {
    game_window.border_style |= WS_CAPTION     | WS_SYSMENU | WS_POPUP |
                                WS_MINIMIZEBOX | WS_VISIBLE;
    dwStyle_ptr               = game_window.border_style;
  }

  // Clear the high-bits
  DWORD_PTR dwStyle =
           (dwStyle_ptr & 0xFFFFFFFF);

  // Minimal sane set of extended window styles for sane rendering
  dwStyle |= (WS_VISIBLE | WS_SYSMENU) | WS_POPUP | WS_MINIMIZEBOX;
  dwStyle &= (~WS_DISABLED);

  game_window.actual.style = dwStyle;

  if (pDispatchFunc == nullptr)
    pDispatchFunc = game_window.SetWindowLongPtr;


  pDispatchFunc ( game_window.hWnd,
                    GWL_STYLE,
                      game_window.actual.style );
}

void
SK_SetWindowStyleEx ( DWORD_PTR            dwStyleEx_ptr,
                      SetWindowLongPtr_pfn pDispatchFunc )
{
  // Clear the high-bits
  DWORD_PTR dwStyleEx =
           (dwStyleEx_ptr & 0xFFFFFFFF);

  // Minimal sane set of extended window styles for sane rendering
  dwStyleEx |=   WS_EX_APPWINDOW;
  dwStyleEx &= ~(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYOUTRTL |
                 WS_EX_RIGHT      | WS_EX_RTLREADING);

  game_window.actual.style_ex = (DWORD_PTR)dwStyleEx;

  if (pDispatchFunc == nullptr)
      pDispatchFunc = game_window.SetWindowLongPtr;


  pDispatchFunc ( game_window.hWnd,
                    GWL_EXSTYLE,
                      game_window.actual.style_ex );
}

RECT
SK_ComputeClientSize (void)
{
  const bool use_override =
    (! config.window.res.override.isZero ());

  if (use_override)
  {
    return RECT { 0L, 0L,
                    gsl::narrow_cast <LONG> (config.window.res.override.x),
                    gsl::narrow_cast <LONG> (config.window.res.override.y)
    };
  }

  RECT ret =
    game_window.actual.client;

  SK_GetClientRect ( game_window.hWnd, &ret );

  ret = { 0, 0,
          ret.right  - ret.left,
          ret.bottom - ret.top };

  return ret;
}

POINT
SK_ComputeClientOrigin (void)
{
  RECT window = { },
       client = { };

  SK_GetWindowRect ( game_window.hWnd, &window );
  SK_GetClientRect ( game_window.hWnd, &client );

  return
    POINT { window.left + client.left,
            window.top  + client.top };
}

bool
SK_IsRectTooBigForDesktop (const RECT& wndRect)
{
  HMONITOR hMonitor =
    MonitorFromWindow ( game_window.hWnd,
                       MONITOR_DEFAULTTONEAREST );

  MONITORINFO mi   = {         };
  mi.cbSize        = sizeof (mi);
  GetMonitorInfo (hMonitor, &mi);

  int win_width      = wndRect.right       - wndRect.left;
  int win_height     = wndRect.bottom      - wndRect.top;

  if (! config.window.res.override.isZero ())
  {
    win_width  = config.window.res.override.x;
    win_height = config.window.res.override.y;
  }

  const int desktop_width  = mi.rcWork.right     - mi.rcWork.left;
  const int desktop_height = mi.rcWork.bottom    - mi.rcWork.top;

  const int mon_width      = mi.rcMonitor.right  - mi.rcMonitor.left;
  const int mon_height     = mi.rcMonitor.bottom - mi.rcMonitor.top;

  if (win_width == mon_width && win_height == mon_height)
    return true;

  if (win_width > desktop_width && win_height > desktop_height)
    return true;

  return false;
}

bool
SK_Window_HasBorder (HWND hWnd)
{
  return
    SK_WindowManager::StyleHasBorder (
      (DWORD_PTR)SK_GetWindowLongW ( hWnd, GWL_STYLE )
    );
}

bool
SK_Window_IsFullscreen (HWND hWnd)
{
  HMONITOR hMonitor =
    MonitorFromWindow ( hWnd,
                          MONITOR_DEFAULTTONEAREST );

  RECT                     rc_window = { };
  SK_GetWindowRect (hWnd, &rc_window);

  auto ResolutionMatches = [](RECT& rect, HMONITOR hMonitor) ->
  bool
  {
    MONITORINFO mon_info        = { };
                mon_info.cbSize = sizeof (MONITORINFO);

    GetMonitorInfoW (
      hMonitor,
        &mon_info
    );

    return
       ( rect.left   == mon_info.rcMonitor.left   &&
         rect.right  == mon_info.rcMonitor.right  &&
         rect.top    == mon_info.rcMonitor.top    &&
         rect.bottom == mon_info.rcMonitor.bottom    );
  };

  return
    ResolutionMatches (rc_window, hMonitor);
}

void
SK_AdjustBorder (void)
{
  SK_WINDOW_LOG_CALL3 ();

  if (game_window.GetWindowLongPtr == nullptr)
    return;

  game_window.actual.style    =
    game_window.GetWindowLongPtr ( game_window.hWnd, GWL_STYLE   );
  game_window.actual.style_ex =
    game_window.GetWindowLongPtr ( game_window.hWnd, GWL_EXSTYLE );

  const bool has_border =
    SK_WindowManager::StyleHasBorder (
      game_window.actual.style
    );

  // If these are opposite, we can skip a whole bunch of
  //   pointless work!
  if (has_border != config.window.borderless)
    return;

  game_window.actual.style =
    config.window.borderless ?
    (ULONG_PTR)SK_BORDERLESS :
    game_window.border_style;

  game_window.actual.style_ex =
    config.window.borderless    ?
    (ULONG_PTR)SK_BORDERLESS_EX :
    game_window.border_style_ex;

  RECT                                 orig_client = { };
  SK_GetClientRect (game_window.hWnd, &orig_client);

  const RECT new_client =
    SK_ComputeClientSize ();

  RECT  new_window = new_client;
  POINT origin     { new_client.left, new_client.top };//SK_ComputeClientOrigin ();

  ClientToScreen (game_window.hWnd, &origin);

  SK_SetWindowStyle   ( game_window.actual.style    );
  SK_SetWindowStyleEx ( game_window.actual.style_ex );

  if ( SK_AdjustWindowRectEx ( &new_window,
         static_cast <DWORD>  (game_window.actual.style),
         FALSE,
         static_cast <DWORD>  (game_window.actual.style_ex) )
     )
  {
    const bool had_border = has_border;

    const int origin_x = had_border ? origin.x + orig_client.left : origin.x;
    const int origin_y = had_border ? origin.y + orig_client.top  : origin.y;

    SK_SetWindowPos ( game_window.hWnd,
                      SK_HWND_TOP,
                      origin_x, origin_y,
                      new_window.right  - new_window.left,
                      new_window.bottom - new_window.top,
                      SWP_NOZORDER     | SWP_NOREPOSITION   |
                      SWP_FRAMECHANGED | SWP_NOSENDCHANGING |
                      SWP_SHOWWINDOW  );

    GetWindowRect (game_window.hWnd, &game_window.actual.window);
    GetClientRect (game_window.hWnd, &game_window.actual.client);
  }

  game_window.game.window = game_window.actual.window;
  game_window.game.client = game_window.actual.client;
}


void
SK_Window_RepositionIfNeeded (void)
{
  if (! (config.window.borderless || config.window.center))
    return;

  static volatile LONG               __busy = FALSE;
  if (! InterlockedCompareExchange (&__busy, TRUE, FALSE))
  {
    SK_Thread_Create (
      [](LPVOID) ->
      DWORD
    {
      SetCurrentThreadDescription (L"[SK] Window Reposition Thread");

      GetWindowRect (game_window.hWnd, &game_window.actual.window);
      GetClientRect (game_window.hWnd, &game_window.actual.client);

      ////
      ////// Force a sane set of window styles initially
      SK_RunOnce ( SK_SetWindowStyle      (
                     SK_GetWindowLongPtrW (game_window.hWnd, GWL_STYLE)
                                          )                           );
      SK_RunOnce ( SK_SetWindowStyleEx    (
                     SK_GetWindowLongPtrW (game_window.hWnd, GWL_EXSTYLE)
                                          )                             );

      if (config.window.borderless)
      {
        SK_AdjustBorder ();

        // XXX: Why can't the call to SK_AdjustWindow (...) suffice?
        if (config.window.fullscreen)
        {
          gsl::not_null <SK_ICommandProcessor *> cp (
            SK_GetCommandProcessor ()
          );

          cp->ProcessCommandLine ("Window.Fullscreen 1");
        }
      }

      else if (config.window.center)
        SK_AdjustWindow ();

      InterlockedExchange (&__busy, FALSE);

      SK_Thread_CloseSelf ();

      return 0;
    });
  }
}


// KNOWN ISSUES: 1. Do not move window using title bar while override res is enabled
//
//
void
SK_AdjustWindow (void)
{
  SK_WINDOW_LOG_CALL3 ();

  if (game_window.GetWindowLongPtr == nullptr)
    return;

  if (! game_window.active)
    return;

  HMONITOR hMonitor =
    MonitorFromWindow ( game_window.hWnd,
                       MONITOR_DEFAULTTONEAREST );

  MONITORINFO mi   = {         };
  mi.cbSize        = sizeof (mi);
  GetMonitorInfo (hMonitor, &mi);

  if (config.window.borderless && config.window.fullscreen)
  {
    SK_LOG4 ( (L" > SK_AdjustWindow (Fullscreen)"),
             L"Window Mgr" );

    SK_SetWindowPos ( game_window.hWnd,
                      SK_HWND_TOP,
                      mi.rcMonitor.left,
                      mi.rcMonitor.top,
                      mi.rcMonitor.right  - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top,
                      SWP_NOSENDCHANGING | SWP_NOZORDER       |
                      SWP_NOREPOSITION   | SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW );

    SK_LOG1 ( ( L"FULLSCREEN => {Left: %li, Top: %li} - (WxH: %lix%li)",
                    mi.rcMonitor.left,    mi.rcMonitor.top,
                    mi.rcMonitor.right  - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top
              ), L"Border Mgr" );

    // Must set this or the mouse cursor clip rect will be wrong
    game_window.actual.window = mi.rcMonitor;
    game_window.actual.client = mi.rcMonitor;
  }

  else
  {
    SK_LOG4 ( (L" > SK_AdjustWindow (Windowed)"),
             L"Window Mgr" );

    // Adjust the desktop resolution to make room for window decorations
    //   if the game window were maximized.
    if (! config.window.borderless) {
      SK_AdjustWindowRect (
        &mi.rcWork,
        static_cast <LONG> (game_window.actual.style),
        FALSE
      );
    }

    // Monitor Workspace
          LONG mon_width     = mi.rcWork.right     - mi.rcWork.left;
          LONG mon_height    = mi.rcWork.bottom    - mi.rcWork.top;

    // Monitor's ENTIRE coordinate space (includes taskbar)
    const LONG real_width    = mi.rcMonitor.right  - mi.rcMonitor.left;
    const LONG real_height   = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // The Game's _Requested_ Client Rectangle
          LONG render_width  = game_window.game.client.right  - game_window.game.client.left;
          LONG render_height = game_window.game.client.bottom - game_window.game.client.top;

    // The Game's _Requested_ Window Rectangle (including borders)
    const LONG full_width     = game_window.game.window.right  - game_window.game.window.left;
    const LONG full_height    = game_window.game.window.bottom - game_window.game.window.top;

    if ((! config.window.res.override.isZero ()))
    {
      render_width  = config.window.res.override.x;
      render_height = config.window.res.override.y;
    }

    else {
      const int  render_width_before  = game_window.game.client.right  - game_window.game.client.left;
      const int  render_height_before = game_window.game.client.bottom - game_window.game.client.top;

      const RECT client_before =
        game_window.game.client;

      SK_GetClientRect (game_window.hWnd, &game_window.game.client);
      SK_GetWindowRect (game_window.hWnd, &game_window.game.window);

      render_width  = game_window.game.client.right  - game_window.game.client.left;
      render_height = game_window.game.client.bottom - game_window.game.client.top;

      if ( render_width  != render_width_before  ||
          render_height != render_height_before )
      {
        SK_AdjustWindow ();
        return;
      }
    }

    // No adjustment if the window is full-width
    if (render_width == real_width)
    {
      mon_width       = real_width;
      mi.rcWork.right = mi.rcMonitor.right;
      mi.rcWork.left  = mi.rcMonitor.left;
    }

    // No adjustment if the window is full-height
    if (render_height == real_height)
    {
      mon_height       = real_height;
      mi.rcWork.top    = mi.rcMonitor.top;
      mi.rcWork.bottom = mi.rcMonitor.bottom;
    }

    const LONG win_width  = std::min (mon_width,  render_width);
    const LONG win_height = std::min (mon_height, render_height);

    // Eliminate relative epsilon from screen percentage offset coords;
    //   otherwise we may accidentally move the window.
    bool nomove = config.window.offset.isZero       ();
    bool nosize = config.window.res.override.isZero ();

    static SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    // We will offset coordinates later; move the window to the top-left
    //   origin first.
    if (config.window.center && (! ( (config.window.fullscreen &&
        config.window.borderless)  ||
        rb.fullscreen_exclusive  ) ) )
    {
      game_window.actual.window.left   = 0;
      game_window.actual.window.top    = 0;
      game_window.actual.window.right  = win_width;
      game_window.actual.window.bottom = win_height;
      nomove                           = false; // Centering requires moving ;)
    }

    else
    {
      game_window.actual.window.left   = game_window.game.window.left;
      game_window.actual.window.top    = game_window.game.window.top;
    }

    const int x_offset = ( config.window.offset.x.percent != 0.0f ?
             (int)( config.window.offset.x.percent *
                           (mi.rcWork.right - mi.rcWork.left) ) :
                           config.window.offset.x.absolute );

    const int y_offset = ( config.window.offset.y.percent != 0.0f ?
             (int)( config.window.offset.y.percent *
                           (mi.rcWork.bottom - mi.rcWork.top) ) :
                           config.window.offset.y.absolute );

    if (x_offset > 0)
      game_window.actual.window.left = mi.rcWork.left  + x_offset - 1;
    else if (x_offset < 0)
      game_window.actual.window.right = mi.rcWork.right + x_offset + 1;


    if (y_offset > 0)
      game_window.actual.window.top    = mi.rcWork.top    + y_offset - 1;
    else if (y_offset < 0)
      game_window.actual.window.bottom = mi.rcWork.bottom + y_offset + 1;

    if (config.window.center && (! ( (config.window.fullscreen &&
        config.window.borderless)  ||
        rb.fullscreen_exclusive  ) ) )
    {
      SK_LOG4 ( ( L"Center --> (%li,%li)",
               mi.rcWork.right - mi.rcWork.left,
               mi.rcWork.bottom - mi.rcWork.top ),
               L"Window Mgr" );

      if (x_offset < 0)
      {
        game_window.actual.window.left  -= (win_width / 2);
        game_window.actual.window.right -= (win_width / 2);
      }

      if (y_offset < 0)
      {
        game_window.actual.window.top    -= (win_height / 2);
        game_window.actual.window.bottom -= (win_height / 2);
      }

      game_window.actual.window.left += std::max (0L, (mon_width  - win_width)  / 2);
      game_window.actual.window.top  += std::max (0L, (mon_height - win_height) / 2);
    }


    if (x_offset >= 0)
      game_window.actual.window.right = game_window.actual.window.left  + win_width;
    else
      game_window.actual.window.left  = game_window.actual.window.right - win_width;


    if (y_offset >= 0)
      game_window.actual.window.bottom = game_window.actual.window.top    + win_height;
    else
      game_window.actual.window.top    = game_window.actual.window.bottom - win_height;


    // Adjust the window to fit if it's not fullscreen
    if ( (full_height < mon_height ) ||
         (full_width  < mon_width  ) )
    {
      SK_AdjustWindowRect (
        &game_window.actual.window,
        static_cast <LONG> (game_window.actual.style),
        FALSE
      );

      //
      // Compensate for scenarios where the window is partially offscreen
      //
      int push_right = 0;

      if (game_window.actual.window.left < mi.rcWork.left)
        push_right = mi.rcWork.left - game_window.actual.window.left;
      else if (game_window.actual.window.right > mi.rcWork.right)
        push_right = (mi.rcWork.right - game_window.actual.window.right);

      game_window.actual.window.left  += push_right;
      game_window.actual.window.right += push_right;

      int push_down = 0;

      if ((game_window.actual.window.top - game_window.actual.client.top) < 0)
        push_down = 0 - (game_window.actual.window.top - game_window.actual.client.top);
      else if (game_window.actual.window.bottom + game_window.actual.client.top > mi.rcWork.bottom)
        push_down = mi.rcWork.bottom - (game_window.actual.window.bottom + game_window.actual.client.top);

      game_window.actual.window.top    += push_down;
      game_window.actual.window.bottom += push_down;
    }

    else
    {
#if 0
      game_window.actual.window.left   = 0;
      game_window.actual.window.top    = 0;
      game_window.actual.window.right  = real_width;
      game_window.actual.window.bottom = real_height;

      game_window.actual.client = game_window.actual.window;

      game_window.actual.style    = SK_BORDERLESS;
      game_window.actual.style_ex = SK_BORDERLESS_EX;

      SK_SetWindowStyle   ( game_window.actual.style    );
      SK_SetWindowStyleEx ( game_window.actual.style_ex );
#endif
    }


    if (game_window.actual.window.right  - game_window.actual.window.left > 0 &&
        game_window.actual.window.bottom - game_window.actual.window.top  > 0 )
    {
      SK_SetWindowPos ( game_window.hWnd,
                        SK_HWND_TOP,
                        game_window.actual.window.left,
                        game_window.actual.window.top,
                        game_window.actual.window.right  - game_window.actual.window.left,
                        game_window.actual.window.bottom - game_window.actual.window.top,
                        SWP_NOSENDCHANGING | SWP_NOZORDER   |
                        SWP_NOREPOSITION   | SWP_SHOWWINDOW |
                        (nomove ? SWP_NOMOVE : 0x00) |
                        (nosize ? SWP_NOSIZE : 0x00) );
    }

    SK_GetWindowRect (game_window.hWnd, &game_window.actual.window);
    SK_GetClientRect (game_window.hWnd, &game_window.actual.client);

    SK_GetWindowRect (game_window.hWnd, &game_window.game.window);
    SK_GetClientRect (game_window.hWnd, &game_window.game.client);



    wchar_t wszBorderDesc [128] = { };

    const bool has_border =
      SK_WindowManager::StyleHasBorder (game_window.actual.style);

    // Summarize the border
    if (SK_WindowManager::StyleHasBorder (game_window.actual.style))
    {
      _swprintf ( wszBorderDesc,
                 L"(Frame = %lipx x %lipx, Title = %lipx)",
                 2 * SK_GetSystemMetrics (SM_CXDLGFRAME),
                 2 * SK_GetSystemMetrics (SM_CYDLGFRAME),
                     SK_GetSystemMetrics (SM_CYCAPTION) );
    }

    SK_LOG1 ( ( L"WINDOW => {Left: %li, Top: %li} - (WxH: %lix%li) - { Border: %s }",
                game_window.actual.window.left,    game_window.actual.window.top,
                game_window.actual.window.right  - game_window.actual.window.left,
                game_window.actual.window.bottom - game_window.actual.window.top,
             (! has_border) ?
                    L"None" :
                    wszBorderDesc
              ),    L"Border Mgr" );
  }


  // Post-Process Results:
  //
  //  If window is moved as a result of this function, we need to:
  //  ------------------------------------------------------------
  //   Re-compute certain game-defined metrics (which may differ):
  //   ===========================================================
  //    1. Client Rectangle
  //    2. Clip Rectangle (mouse confinement)
  //    3. Which monitor the game is on (XXX: that whole thing needs consideration)
  //
  //   Decide how to translate mouse events so that if the game does not know the
  //     window was moved, it still processes mouse input in a sane way.
  //

  auto DescribeRect = [&](RECT* lpRect)
  {
    SK_LOG2_EX ( false, ( L" => {Left: %li, Top: %li} - {WxH: %lix%li)\n",
                          lpRect->left,    lpRect->top,
                          lpRect->right  - lpRect->left,
                          lpRect->bottom - lpRect->top ) );
  };

  // 1. Client Rectangle
  ////RECT client;
  ////GetClientRect_Original (game_window.hWnd, &client);


  // 2. Re-Compute Clip Rectangle
  const bool unconfine = config.window.unconfine_cursor;
  const bool   confine = config.window.confine_cursor;

  // This logic is questionable -- we probably need to transform the clip rect,
  //                                 but not apply it when unconfine is true.
  if (! unconfine)
  {
    RECT            clip = {};
    GetClipCursor (&clip);

    auto DescribeClipRect = [&](const wchar_t* wszName, RECT* lpRect)
    {
      SK_LOG2_EX ( true, ( L"[Cursor Mgr] Clip Rect (%s)", wszName ) );
      DescribeRect (lpRect);
    };

    if (confine || SK_IsClipRectFinite (&game_window.cursor_clip) || SK_IsClipRectFinite (&clip))
    {
      DescribeClipRect (L"Game", &game_window.cursor_clip);
      DescribeClipRect (L" IN ", &clip);

      if (confine)
      {
        clip.left   = game_window.actual.window.left + game_window.actual.client.left;
        clip.top    = game_window.actual.window.top  + game_window.actual.client.top;
        clip.right  = game_window.actual.window.left + game_window.actual.client.right;
        clip.bottom = game_window.actual.window.top  + game_window.actual.client.bottom;
      }

      // TODO: Transform clip rectangle using existing rectangle
      //
      else
      {
        SK_LOG2 ( ( L"Need to transform original clip rect..." ),
                    L"Cursor Mgr" );

        clip.left   = game_window.actual.window.left + game_window.actual.client.left;
        clip.top    = game_window.actual.window.top  + game_window.actual.client.top;
        clip.right  = game_window.actual.window.left + game_window.actual.client.right;
        clip.bottom = game_window.actual.window.top  + game_window.actual.client.bottom;
      }

      DescribeClipRect (L"OUT ", &clip);

      if (! wm_dispatch->moving_windows.count (game_window.hWnd))
        SK_ClipCursor (&clip);
      else
        SK_ClipCursor (nullptr);
    }
  }

  // Unconfine Cursor
  else
    SK_ClipCursor (nullptr);
}

static int
WINAPI
GetSystemMetrics_Detour (_In_ int nIndex)
{
  SK_LOG_FIRST_CALL

  const int nRet =
    SK_GetSystemMetrics (nIndex);

  SK_LOG4 ( ( L"GetSystemMetrics (%4li) : %-5li - %s",
           nIndex, nRet,
           SK_SummarizeCaller ().c_str () ),
           L"Resolution" );


  if (SK_GetCurrentGameID () == SK_GAME_ID::DotHackGU)
  {
    if (nIndex == SM_CYCAPTION)      return 0;
    if (nIndex == SM_CYMENU)         return 0;
    if (nIndex == SM_CXBORDER)       return 0;
    if (nIndex == SM_CYBORDER)       return 0;
    if (nIndex == SM_CXDLGFRAME)     return 0;
    if (nIndex == SM_CYDLGFRAME)     return 0;
    if (nIndex == SM_CXFRAME)        return 0;
    if (nIndex == SM_CYFRAME)        return 0;
    if (nIndex == SM_CXPADDEDBORDER) return 0;
  }

#if 0
  if (config.window.borderless)
  {
    if (nIndex == SM_CYCAPTION)
      return 0;
    if (nIndex == SM_CYMENU)
      return 0;
    if (nIndex == SM_CXBORDER)
      return 0;
    if (nIndex == SM_CYBORDER)
      return 0;
    if (nIndex == SM_CXDLGFRAME)
      return 0;
    if (nIndex == SM_CYDLGFRAME)
      return 0;
    if (nIndex == SM_CXFRAME)
      return 0;
    if (nIndex == SM_CYFRAME)
      return 0;
    if (nIndex == SM_CXPADDEDBORDER)
      return 0;
  }
#endif

  //dll_log->Log ( L"[Resolution] GetSystemMetrics (%lu) : %lu",
  //nIndex, nRet );

  return nRet;
}

int
WINAPI
SK_GetSystemMetrics (_In_ int nIndex)
{
  if (GetSystemMetrics_Original != nullptr)
    return GetSystemMetrics_Original (nIndex);

  return
    GetSystemMetrics (nIndex);

  //SK_LOG_FIRST_CALL
}



using TranslateMessage_pfn =
BOOL (WINAPI *)(
  _In_ const MSG *lpMsg
);

using NtUserDispatchMessage_pfn =
LRESULT (NTAPI *)(
  _In_ const MSG *lpmsg
);

using NtUserGetMessage_pfn =
BOOL (NTAPI *)( LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax );

using NtUserPeekMessage_pfn =
BOOL (NTAPI *)(
  _Out_    LPMSG lpMsg,
  _In_opt_ HWND  hWnd,
  _In_     UINT  wMsgFilterMin,
  _In_     UINT  wMsgFilterMax,
  _In_     UINT  wRemoveMsg
  );

TranslateMessage_pfn TranslateMessage_Original = nullptr;

NtUserPeekMessage_pfn     NtUserPeekMessage     = nullptr;
NtUserGetMessage_pfn      NtUserGetMessage      = nullptr;
//NtUserDispatchMessage_pfn NtUserDispatchMessage = nullptr;

NtUserPeekMessage_pfn     PeekMessageA_Original     = nullptr;
NtUserGetMessage_pfn      GetMessageA_Original      = nullptr;
NtUserDispatchMessage_pfn DispatchMessageA_Original = nullptr;

NtUserPeekMessage_pfn     PeekMessageW_Original     = nullptr;
NtUserGetMessage_pfn      GetMessageW_Original      = nullptr;
NtUserDispatchMessage_pfn DispatchMessageW_Original = nullptr;

BOOL
WINAPI
TranslateMessage_Detour (_In_ const MSG *lpMsg)
{
  if (SK_ImGui_WantTextCapture ())
  {
    switch (lpMsg->message)
    {
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
      case WM_IME_KEYDOWN:

        // This shouldn't happen considering this function is supposed
        //   to generate this message, but better to be on the safe side.
      case WM_CHAR:
      case WM_MENUCHAR:
      {
        return TRUE;
      } break;
    }
  }

  return TranslateMessage_Original (lpMsg);
}

LRESULT
WINAPI
ImGui_WndProcHandler ( HWND hWnd, UINT   msg,
                       WPARAM wParam,
                       LPARAM lParam );

bool
SK_EarlyDispatchMessage (MSG *lpMsg, bool remove, bool peek)
{
  //LRESULT lRet = 0;
  if (      lpMsg       != nullptr         && lpMsg->message   <= 0xFFFF             &&
           (lpMsg->hwnd != SK_HWND_DESKTOP || game_window.hWnd == SK_HWND_DESKTOP)   &&
             SK_ImGui_HandlesMessage (lpMsg, remove, peek)                       )
  {
    if ( lpMsg->message == WM_INPUT )
    {
      DefWindowProcW (lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    }

    //ImGui_WndProcHandler (lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);

    if (remove)
      lpMsg->message = WM_NULL;

    return true;
  }

  return false;
}


BOOL
WINAPI
PeekMessageA_Detour (
  _Out_    LPMSG lpMsg,
  _In_opt_ HWND  hWnd,
  _In_     UINT  wMsgFilterMin,
  _In_     UINT  wMsgFilterMax,
  _In_     UINT  wRemoveMsg )
{
  SK_LOG_FIRST_CALL

  auto PeekFunc = NtUserPeekMessage != nullptr ?
                  NtUserPeekMessage :
                        PeekMessageA_Original;

  if ( PeekFunc == nullptr )
  {
    // A nasty kludge to fix the Steam overlay
    static NtUserPeekMessage_pfn
           early_PeekMessageA =
      reinterpret_cast <NtUserPeekMessage_pfn> (
             SK_GetProcAddress      (
               SK_GetModuleHandle (   L"user32"  ),
                                       "PeekMessageA"
                                    )          );

    PeekFunc =
      early_PeekMessageA;

    if (PeekFunc != nullptr)
    {
      return
        PeekFunc ( lpMsg,
                        hWnd,
                             wMsgFilterMin,
                             wMsgFilterMax,
                                           wRemoveMsg );
    }

    return 0;
  }


  if (config.render.dxgi.safe_fullscreen)
    wRemoveMsg |= PM_REMOVE;

  if ( PeekFunc ( lpMsg,
                       hWnd,
                            wMsgFilterMin,
                            wMsgFilterMax,
                                          wRemoveMsg )
     )
  {
    if ( (wRemoveMsg & PM_REMOVE) ==
                       PM_REMOVE )
    {
      SK_EarlyDispatchMessage (lpMsg, true, true);
    }

    return TRUE;
  }

  return FALSE;
}

BOOL
WINAPI
PeekMessageW_Detour (
  _Out_    LPMSG lpMsg,
  _In_opt_ HWND  hWnd,
  _In_     UINT  wMsgFilterMin,
  _In_     UINT  wMsgFilterMax,
  _In_     UINT  wRemoveMsg )
{
  SK_LOG_FIRST_CALL

  auto PeekFunc = NtUserPeekMessage != nullptr ?
                  NtUserPeekMessage :
                        PeekMessageW_Original;

  if ( PeekFunc == nullptr )
  {
    // A nasty kludge to fix the Steam overlay
    static NtUserPeekMessage_pfn
           early_PeekMessageW =
      reinterpret_cast <NtUserPeekMessage_pfn> (
             SK_GetProcAddress      (
               SK_GetModuleHandle (   L"user32"  ),
                                       "PeekMessageW"
                                    )          );

    NtUserPeekMessage_pfn early =
      early_PeekMessageW;

    PeekFunc = early != nullptr ?
               early : PeekFunc;


    if (PeekFunc != nullptr)
    {
      return
        PeekFunc ( lpMsg,
                        hWnd,
                             wMsgFilterMin,
                             wMsgFilterMax,
                                           wRemoveMsg );
    }

    return 0;
  }


  if (config.render.dxgi.safe_fullscreen)
    wRemoveMsg |= PM_REMOVE;

  if ( PeekFunc ( lpMsg,
                       hWnd,
                            wMsgFilterMin,
                            wMsgFilterMax,
                                          wRemoveMsg )
     )
  {
    if ( (wRemoveMsg & PM_REMOVE) ==
                       PM_REMOVE )
    {
      SK_EarlyDispatchMessage (lpMsg, true, true);
    }

    return TRUE;
  }

  return FALSE;
}


BOOL
WINAPI
GetMessageA_Detour (LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
  SK_LOG_FIRST_CALL

  auto GetFunc = NtUserGetMessage != nullptr ?
                 NtUserGetMessage :
                       GetMessageA_Original;

  if ( GetFunc == nullptr )
  {
    // A nasty kludge to fix the Steam overlay
    static NtUserGetMessage_pfn
           early_GetMessageA =
      reinterpret_cast <NtUserGetMessage_pfn> (
             SK_GetProcAddress      (
               SK_GetModuleHandle (   L"user32"  ),
                                       "GetMessageA"
                                    )          );

    SK_ReleaseAssert ( early_GetMessageA != nullptr &&
                             GetFunc     != nullptr );

    GetFunc =
      early_GetMessageA;

    if (GetFunc != nullptr)
    {
      return
        GetFunc ( lpMsg,
                    hWnd,
                      wMsgFilterMin,
                      wMsgFilterMax );
    }

    return 0;
  }

  const BOOL bRet =
    GetFunc ( lpMsg,
                hWnd,
                  wMsgFilterMin,
                  wMsgFilterMax );

  if ( bRet == TRUE )
  {
    if ( SK_EarlyDispatchMessage ( lpMsg, false, false ) )
    {
      DefWindowProcA ( lpMsg->hwnd,   lpMsg->message,
                       lpMsg->wParam, lpMsg->lParam   );

      lpMsg->message = WM_NULL;
    }
  }

  return
    bRet;
}

BOOL
WINAPI
SK_GetMessageW (LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
  if (GetMessageW_Original != nullptr)
    return GetMessageW_Original (lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);

  return GetMessageW (lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

LRESULT
WINAPI
SK_DispatchMessageW (_In_ const MSG *lpMsg)
{
  if (DispatchMessageW_Original != nullptr)
    return DispatchMessageW_Original (lpMsg);

  return DispatchMessageW (lpMsg);
}

BOOL
WINAPI
GetMessageW_Detour (LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
  SK_LOG_FIRST_CALL

  auto GetFunc = NtUserGetMessage != nullptr ?
                 NtUserGetMessage :
                       GetMessageW_Original;

  if ( GetFunc == nullptr )
  {
    // A nasty kludge to fix the Steam overlay
    static NtUserGetMessage_pfn
           early_GetMessageW =
      reinterpret_cast <NtUserGetMessage_pfn> (
             SK_GetProcAddress      (
               SK_GetModuleHandle (   L"user32"  ),
                                       "GetMessageW"
                                    )          );

    SK_ReleaseAssert ( early_GetMessageW != nullptr &&
                             GetFunc     != nullptr );

    GetFunc =
      early_GetMessageW;

    if (GetFunc)
    {
      return
        GetFunc ( lpMsg,
                    hWnd,
                      wMsgFilterMin,
                      wMsgFilterMax );
    }

    return 0;
  }

  const BOOL bRet =
    GetFunc ( lpMsg,
                hWnd,
                  wMsgFilterMin,
                  wMsgFilterMax );

  if ( bRet )
  {
    if ( SK_EarlyDispatchMessage ( lpMsg, false, false ) )
    {
      DefWindowProcW ( lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam );

      lpMsg->message = WM_NULL;
    }
  }

  return
    bRet;
}


LRESULT
WINAPI
DispatchMessageA_Detour (_In_ const MSG* lpMsg)
{
  SK_LOG_FIRST_CALL

  MSG orig_msg = *lpMsg; //-V821
  MSG      msg = *lpMsg;

  //if (          lpMsg->hwnd == game_window.hWnd  ||
  //    SK_IsChild (lpMsg->hwnd,   game_window.hWnd) ||
  //    SK_IsChild (game_window.hWnd, lpMsg->hwnd))
  {
    if ( SK_EarlyDispatchMessage ( &msg, true ) )
    {
      auto DefWindowProc = DefWindowProcA;

      return
        DefWindowProc ( orig_msg.hwnd,   orig_msg.message,
                        orig_msg.wParam, orig_msg.lParam  );
    }
  }

  return
    DispatchMessageA_Original (&msg);
}

LRESULT
WINAPI
DispatchMessageW_Detour (_In_ const MSG* lpMsg)
{
  SK_LOG_FIRST_CALL

  MSG orig_msg = *lpMsg; //-V821
  MSG      msg = *lpMsg;

  //if (          lpMsg->hwnd == game_window.hWnd  ||
  //  SK_IsChild (lpMsg->hwnd,   game_window.hWnd) ||
  //  SK_IsChild (game_window.hWnd, lpMsg->hwnd))

  if ( SK_EarlyDispatchMessage ( &msg, true ) )
  {
    auto DefWindowProc = DefWindowProcW;

    return
      DefWindowProc ( orig_msg.hwnd,   orig_msg.message,
                      orig_msg.wParam, orig_msg.lParam  );
  }

  return
    DispatchMessageW_Original (&msg);
}


typedef HWND (WINAPI *GetActiveWindow_pfn)(void);
                      GetActiveWindow_pfn
                      GetActiveWindow_Original = nullptr;

typedef HWND (WINAPI *SetActiveWindow_pfn)(HWND);
                      SetActiveWindow_pfn
                      SetActiveWindow_Original = nullptr;

HWND
WINAPI
SK_GetActiveWindow (SK_TLS *pTLS)
{
  if (pTLS == nullptr)
      pTLS  = SK_TLS_Bottom ();

  if (pTLS != nullptr)
  {
    if (pTLS->win32->active == 0)
    {
      pTLS->win32->active =
        GetActiveWindow_Original ();
    }

    return
      pTLS->win32->active;
  }

  return
    GetActiveWindow_Original ();
}

HWND
WINAPI
GetActiveWindow_Detour (void)
{
  SK_LOG_FIRST_CALL

  SK_TLS *pTLS =
        SK_TLS_Bottom ();

  if (pTLS != nullptr)
  {
  //   since we're making a round-trip anyway.
    // Take this opportunity to update any stale data
    pTLS->win32->active = 0;
  }

  return
    SK_GetActiveWindow (pTLS);
}

HWND
WINAPI
SK_SetActiveWindow (HWND hWnd)
{
  HWND hWndRet = nullptr;

  if (SetActiveWindow_Original != nullptr)
  {
    hWndRet =
      SetActiveWindow_Original (hWnd);
  }

  else
  {
    hWndRet =
      SetActiveWindow (hWnd);
  }

  if (hWndRet != nullptr)
  {
    SK_TLS* pTLS =
      SK_TLS_Bottom ();

    pTLS->win32->active      = hWnd;
    pTLS->win32->last_active = hWndRet;
  }

  return hWndRet;
}

HWND
WINAPI
SetActiveWindow_Detour (HWND hWnd)
{
  SK_LOG_FIRST_CALL

  return
    SK_SetActiveWindow (hWnd);
}


typedef HWND (WINAPI *GetForegroundWindow_pfn)(void);
                      GetForegroundWindow_pfn
                      GetForegroundWindow_Original = nullptr;

HWND
WINAPI
SK_GetForegroundWindow (void)
{
  if (GetForegroundWindow_Original != nullptr)
    return GetForegroundWindow_Original ();

  return
    GetForegroundWindow ();
}

HWND
WINAPI
GetForegroundWindow_Detour (void)
{
  SK_LOG_FIRST_CALL


  if (! SK_GetCurrentRenderBackend ().fullscreen_exclusive)
  {
    if ( config.window.background_render ||
         config.window.treat_fg_as_active )
    {
      return game_window.hWnd;
    }
  }

  return
    SK_GetForegroundWindow ();
}


void
RealizeForegroundWindow_Impl (HWND hWndForeground)
{
  DWORD dwPid          = 0;
  HWND  hWndOrig       =
    GetForegroundWindow ();

  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWndOrig, &dwPid);

  const bool show_hide =
            IsWindow (hWndOrig) &&
    hWndForeground != hWndOrig  && dwPid == GetCurrentProcessId ();

  IsGUIThread       (TRUE);
  AttachThreadInput (dwThreadId, dwOrigThreadId, true );

  if ( show_hide && game_window.hWnd     == SK_HWND_DESKTOP &&
                                hWndOrig != hWndForeground )
  {
    WINDOWINFO wi        = {                 };
               wi.cbSize = sizeof (WINDOWINFO);

    GetWindowInfo (hWndOrig, &wi);

    // We only need to do this for fullscreen exclusive mode
    if ( gsl::narrow_cast <LONG> (wi.cxWindowBorders) ==
         gsl::narrow_cast <LONG> (wi.cyWindowBorders) &&
         gsl::narrow_cast <LONG> (wi.rcClient.left)   ==
         gsl::narrow_cast <LONG> (wi.rcClient.bottom) &&
         gsl::narrow_cast <LONG> (wi.cxWindowBorders) ==
         gsl::narrow_cast <LONG> (wi.rcClient.bottom) &&
         gsl::narrow_cast <LONG> (wi.cxWindowBorders) == 0L )
    {
      SK_SetWindowPos ( hWndOrig, SK_HWND_DESKTOP, 0, 0, 0, 0,
                        SWP_HIDEWINDOW     | SWP_NOACTIVATE   |
                        SWP_NOSIZE         | SWP_NOMOVE       |
                        SWP_NOSENDCHANGING | SWP_NOREPOSITION );
    }
  }

  SetForegroundWindow (hWndForeground);
  SK_SetWindowPos     (hWndForeground, SK_HWND_TOPMOST,
                       0, 0,
                       0, 0, SWP_NOSIZE   | SWP_NOMOVE     |
                       SWP_SHOWWINDOW     | SWP_NOACTIVATE |
                       SWP_NOSENDCHANGING | SWP_NOCOPYBITS );

  SetForegroundWindow (hWndForeground);
  SK_SetWindowPos     (hWndForeground, SK_HWND_NOTOPMOST,
                       0, 0,
                       0, 0, SWP_NOSIZE | SWP_NOMOVE     |
                       SWP_SHOWWINDOW   | SWP_NOACTIVATE |
                       SWP_DEFERERASE   | SWP_NOSENDCHANGING );

  SetForegroundWindow (hWndForeground);

  AttachThreadInput   (dwOrigThreadId, dwThreadId, false);

  SK_SetActiveWindow  (hWndForeground);
  BringWindowToTop    (hWndForeground);
  SetFocus            (hWndForeground);
}

HWND
WINAPI
SK_RealizeForegroundWindow (HWND hWndForeground)
{
  //
  // TODO: When COM created the stupid nightmare that requires this, the
  //         idea was that you would use SendMessage to a Top-Level Window
  //           and that window could issue many of these commands in C#
  //             software without deadlocking.
  //
  //      UNITY BLOWS !!
  //
  HWND hWndOrig =
    GetForegroundWindow ();

        DWORD dwPid;
  const DWORD dwThreadId     =
    SK_Thread_GetCurrentId  ();
  const DWORD dwOrigThreadId =
    GetWindowThreadProcessId (hWndForeground, &dwPid);

#if 0
  if (dwOrigThreadId != SK_GetCurrentThreadId ())
  {
    std::future <HWND> resultOfAsync =
                              std::async (
                      std::launch::async,
    [&](HWND hWndForeground) ->
    HWND
    {
      RealizeForegroundWindow_Impl (hWndForeground);

      return hWndForeground;
    }, hWndForeground);

    return
      hWndForeground;
  }
#else
  if (dwOrigThreadId != SK_GetCurrentThreadId ())
  {
    SK_Thread_Create ([](LPVOID lpHwnd) ->
    DWORD
    {
      RealizeForegroundWindow_Impl ((HWND)lpHwnd);

      SK_Thread_CloseSelf ();

      return 0;
    }, (LPVOID)hWndForeground);

    //
    // Rather than spawn this same thread over and over, you need to design
    //   a window that can handle WM_USER messages and dispatch this crap.
    //
    //  --> Same goes for SetWindowLong, which is not thread-safe in Unity.
    //
  }
#endif
  else
  {
    RealizeForegroundWindow_Impl (hWndForeground);
  }

  return hWndOrig;
}

__declspec (noinline)
LRESULT
CALLBACK
SK_DetourWindowProc2 ( _In_  HWND   hWnd,
                       _In_  UINT   uMsg,
                       _In_  WPARAM wParam,
                       _In_  LPARAM lParam )
{
  UNREFERENCED_PARAMETER (hWnd);
  UNREFERENCED_PARAMETER (uMsg);
  UNREFERENCED_PARAMETER (wParam);
  UNREFERENCED_PARAMETER (lParam);

  return 1;
}

__declspec (noinline)
LRESULT
CALLBACK
SK_DetourWindowProc ( _In_  HWND   hWnd,
                      _In_  UINT   uMsg,
                      _In_  WPARAM wParam,
                      _In_  LPARAM lParam )
{
  if (uMsg == WM_NULL)
    return DefWindowProcW (hWnd, uMsg, wParam, lParam);


  // If we are forcing a shutdown, then route any messages through the
  //   default Win32 handler.
  if ( ReadAcquire (&__SK_DLL_Ending) && ( uMsg != WM_SYSCOMMAND &&
                                           uMsg != WM_CLOSE      &&
                                           uMsg != WM_QUIT )        )
  {
    if (IsWindow (game_window.hWnd))
    {
      return
        SK_COMPAT_SafeCallProc ( &game_window, hWnd,
                                               uMsg, wParam,
                                                     lParam );
    }

    return
      game_window.DefWindowProc ( hWnd,
                                  uMsg, wParam,
                                        lParam );
  }


  static bool last_active = game_window.active;

  const bool console_visible =
    SK_Console::getInstance ()->isVisible ();


  static auto& rb =
    SK_GetCurrentRenderBackend ();

  static bool first_run = true;

  if (first_run)
  {
    first_run = false;

    if (GetFocus () == game_window.hWnd || GetForegroundWindow () == game_window.hWnd)
      ActivateWindow  (game_window.hWnd, true);

    // Start unmuted (in case the game crashed in the background)
    if (config.window.background_mute)
      SK_SetGameMute (false);


    //if (game_window.hWnd != nullptr && game_window.hWnd != hWndRender && hWnd != game_window.hWnd)
    //{
    //  if (hWndRender != nullptr)
    //  {
    //    dll_log->Log ( L"[Window Mgr] New HWND detected in the window proc. used"
    //                   L" for rendering... (Old=%p, New=%p)",
    //                     game_window.hWnd, hWnd );
    //  }
    //}

    rb.windows.setFocus (hWnd);

    DWORD dwFocus,
          dwForeground;

    HWND  hWndFocus      =    GetFocus            (),
          hWndForeground = SK_GetForegroundWindow ();

    if (! ( game_window.hWnd == hWndFocus   ||
            game_window.hWnd == hWndForeground ) )
    {
      GetWindowThreadProcessId (hWndFocus,      &dwFocus);
      GetWindowThreadProcessId (hWndForeground, &dwForeground);

      game_window.active       = ( dwFocus      == GetCurrentProcessId () ||
                                   dwForeground == GetCurrentProcessId () );
    }
    else
      game_window.active = true;

    game_window.game.style   = game_window.GetWindowLongPtr (game_window.hWnd, GWL_STYLE);
    game_window.actual.style = game_window.GetWindowLongPtr (game_window.hWnd, GWL_STYLE);
    game_window.unicode      =          SK_Window_IsUnicode (game_window.hWnd)   != FALSE;

    SK_GetWindowRect (game_window.hWnd, &game_window.game.window  );
    SK_GetClientRect (game_window.hWnd, &game_window.game.client  );
    SK_GetWindowRect (game_window.hWnd, &game_window.actual.window);
    SK_GetClientRect (game_window.hWnd, &game_window.actual.client);

    SK_InitWindow (hWnd, false);
  }


  extern bool SK_ImGui_WantExit;


  switch (uMsg)
  {
    case WM_QUIT:
    case WM_CLOSE:
    {
      if (hWnd == game_window.hWnd)
      {
        if ( SK_GetCurrentGameID () == SK_GAME_ID::Yakuza0 ||
             SK_GetCurrentGameID () == SK_GAME_ID::YakuzaKiwami2 )
        {
          SK_SelfDestruct     (   );
          SK_TerminateProcess (0x0);
        }

        if (SK_GetCurrentGameID () == SK_GAME_ID::Okami)
        {
          if (config.input.keyboard.catch_alt_f4)
          {
            SK_ImGui_WantExit = true;
          }

          else
          {
            ExitProcess (0x00);
          }

          return 0; // Alt+F4 is broken, use Special K's function instead.
        }
      }
    } break;



    case WM_SETCURSOR:
    {
      if (hWnd == game_window.hWnd)
      {
        SK_ImGui_Cursor.update ();

        if ( SK_ImGui_WantMouseCapture () &&
               ImGui::IsWindowHovered (ImGuiHoveredFlags_AnyWindow) )
        {
          return 0;
        }
      }
    } break;

    case WM_SYSCOMMAND:
      if (hWnd == game_window.hWnd)
      {
        if (ImGui_WndProcHandler (hWnd, uMsg, wParam, lParam))
        {
          return 0;
        }
      } break;



    case WM_SETFOCUS:
      ActivateWindow (hWnd, true);
      //if ((! rb.fullscreen_exclusive) && config.window.background_render)
      //{
      //  // Blocking this message helps with many games that mute audio in the background
      //  return
      //    game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
      //}
      break;

    case WM_KILLFOCUS:
      ActivateWindow     (hWnd, false);

      if (hWnd == game_window.hWnd)
      {
        rb.fullscreen_exclusive = false;

        if ((! rb.fullscreen_exclusive) && config.window.background_render)
        {
          // Blocking this message helps with many games that mute audio in the background
          return
            game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
        }
      }
      break;

    // Ignore (and physically remove) this event from the message queue if background_render = true
    case WM_MOUSEACTIVATE:
    {
      if ( reinterpret_cast <HWND> (wParam) == game_window.hWnd )
      {
        ActivateWindow (hWnd, true);

        if ((! rb.fullscreen_exclusive) && config.window.background_render)
        {
          SK_LOG2 ( ( L"WM_MOUSEACTIVATE ==> Activate and Eat" ),
                   L"Window Mgr" );
          return MA_ACTIVATEANDEAT;
        }
      }

      else
      {
        ActivateWindow (hWnd, false);

        // Game window was deactivated, but the game doesn't need to know this!
        //   in fact, it needs to be told the opposite.
        if ((! rb.fullscreen_exclusive) && config.window.background_render)
        {
          SK_LOG2 ( ( L"WM_MOUSEACTIVATE (Other Window) ==> Activate" ),
                   L"Window Mgr" );
          return MA_ACTIVATE;
        }
      }


      if (GetFocus () == hWnd) ActivateWindow (hWnd, true);
    } break;

    // Allow the game to run in the background
    case WM_ACTIVATEAPP:
    case WM_ACTIVATE:
    case WM_NCACTIVATE:
    {
      if (uMsg == WM_NCACTIVATE || uMsg == WM_ACTIVATEAPP)
      {
        if (uMsg != WM_NCACTIVATE)
        {
          if (wParam != FALSE)
          {
            if (last_active == false)
              SK_LOG3 ( ( L"Application Activated (Non-Client)" ),
                          L"Window Mgr" );

            ActivateWindow (hWnd, true);
          }

          else
          {
            if (last_active == true)
              SK_LOG3 ( ( L"Application Deactivated (Non-Client)" ),
                          L"Window Mgr" );

            ActivateWindow (hWnd, false);

            // We must fully consume one of these messages or audio will stop playing
            //   when the game loses focus, so do not simply pass this through to the
            //     default window procedure.
            if ( (! rb.fullscreen_exclusive) &&
                    config.window.background_render
                )
            {
              SK_COMPAT_SafeCallProc (&game_window,
                hWnd,
                  uMsg,
                    TRUE,
                      reinterpret_cast <LPARAM> (hWnd) );

              return
                game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
            }
          }
        }

        else// if (wParam == FALSE)
        {
          if (last_active == true)
            SK_LOG3 ( ( L"Application Deactivated (Non-Client)" ),
                        L"Window Mgr" );

          ActivateWindow (hWnd, false);

          // We must fully consume one of these messages or audio will stop playing
          //   when the game loses focus, so do not simply pass this through to the
          //     default window procedure.
          if ( (! rb.fullscreen_exclusive) &&
                  config.window.background_render
             )
          {
            SK_COMPAT_SafeCallProc (&game_window,
              hWnd,
                uMsg,
                  TRUE,
                    reinterpret_cast <LPARAM> (hWnd) );

            return
              game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
          }
        }
      }

      else //if (uMsg == WM_ACTIVATE)
      {
        const wchar_t* source   = L"UNKKNOWN";
        bool           activate = false;

        switch (LOWORD (wParam))
        {
          case WA_ACTIVE:
          case WA_CLICKACTIVE:
          default: // Unknown
          {
            activate = reinterpret_cast <HWND> (lParam) != game_window.hWnd;
            source   = LOWORD (wParam) == 1 ? L"(WM_ACTIVATE [ WA_ACTIVE ])" :
              L"(WM_ACTIVATE [ WA_CLICKACTIVE ])";
            ActivateWindow ((HWND)lParam, true);
          } break;

          case WA_INACTIVE:
          {
            activate = ( lParam                  == 0                ) ||
              ( reinterpret_cast <HWND> (lParam) == game_window.hWnd );
            source   = L"(WM_ACTIVATE [ WA_INACTIVE ])";
            ActivateWindow ((HWND)lParam, false);
          } break;
        }

        if (last_active)
        {
          if (! activate)
          {
            SK_LOG2 ( ( L"Application Deactivated %s", source ),
                        L"Window Mgr" );
          }
        }

        else
        {
          if (activate)
          {
            SK_LOG2 ( ( L"Application Activated %s", source ),
                        L"Window Mgr" );
          }
        }


        if ( SK_GetForegroundWindow () == game_window.hWnd ||
                GetFocus            () == game_window.hWnd )
                          ActivateWindow (game_window.hWnd, true);

        if ((! rb.fullscreen_exclusive) && config.window.background_render)
        {
          return 1;
        }
      }
    } break;

    case WM_NCCALCSIZE:
      break;


    case WM_SHOWWINDOW:
    {
      if (hWnd == SK_GetGameWindow ())
      {
        if (wParam == TRUE)
        {
          ActivateWindow   (hWnd, true);
          SK_XInput_Enable (TRUE);

          if ( config.window.background_render &&
               (! rb.fullscreen_exclusive)        )
          {
            return
              game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
          }
        }

        // The one unconditional event where gamepad input no longer makes
        //   sense (the game window is completely covered).
        if (wParam == FALSE && lParam == SW_OTHERZOOM)
        {
          ActivateWindow   (hWnd, false);
          SK_XInput_Enable (FALSE);

          if ( config.window.background_render &&
               (! rb.fullscreen_exclusive)        )
          {
            return 0;
          }
        }
      }
    } break;
  }

  bool handled = false;



  // Synaptics Touchpad Compat Hack:
  // -------------------------------
  //
  //  PROBLEM:    Driver only generates window messages for mousewheel, it does
  //                not activate RawInput, DirectInput or HID like a real mouse
  //
  //  WORKAROUND: Generate a full-blown input event using SendInput (...); be
  //                aware that this event will generate ANOTHER WM_MOUSEWHEEL.
  //
  //    ** MUST handle recursive behavior caused by this fix-up **
  //
  static bool recursive_wheel = false;

  // Dual purpose: This also catches any WM_MOUSEWHEEL messages that Synaptics
  //                 issued through CallWindowProc (...) rather than
  //                   SendMessage (...) / PostMessage (...) -- UGH.
  //
  //      >> We need to process those for ImGui <<
  //
  if (uMsg == WM_MOUSEWHEEL && (! recursive_wheel))
  {        handled =
             ImGui_WndProcHandler (hWnd, uMsg, wParam, lParam);

    if ((! handled) && config.input.mouse.fix_synaptics)
    {
      INPUT input        = { };

      input.type         = INPUT_MOUSE;
      input.mi.dwFlags   = MOUSEEVENTF_WHEEL;
      input.mi.mouseData = GET_WHEEL_DELTA_WPARAM (wParam);

      recursive_wheel    = true;

      SK_SendInput (1, &input, sizeof (INPUT));
    }
  }

  // In-lieu of a proper fence, this solves the recursion problem.
  //
  //   There's no guarantee the message we are ignoring is the one we
  //     generated, but one misplaced message won't kill anything.
  //
  else if (recursive_wheel && uMsg == WM_MOUSEWHEEL)
    recursive_wheel = false;



  //
  // Squelch input messages that managed to get into the loop without triggering
  //   filtering logic in the GetMessage (...), PeekMessage (...) and
  //     DispatchMessage (...) hooks.
  //
  //   [ Mostly for EverQuest ]
  //
  if (hWnd == game_window.hWnd)
  {
    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST && SK_ImGui_WantMouseCapture ())
    {
      game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
      return 0;
    }

    if (uMsg >= WM_KEYFIRST   && uMsg <= WM_KEYLAST   && SK_ImGui_WantKeyboardCapture ())
    {
      if (uMsg != WM_KEYUP && uMsg != WM_SYSKEYUP)
      {
        game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
        return 0;
      }
    }
  }

  //
  // DO NOT HOOK THIS FUNCTION outside of SpecialK plug-ins, the ABI is not guaranteed
  //
  if (SK_DetourWindowProc2 (hWnd, uMsg, wParam, lParam))
  {
    // Block keyboard input to the game while the console is visible
    if (console_visible)
    {
      if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
        return game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);

      if (uMsg >= WM_KEYFIRST   && uMsg <= WM_KEYLAST)
        return game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);

      // Block RAW Input
      if (uMsg == WM_INPUT)
        return game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
    }
  }

  else
  {
    return
      game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
  }


  // Filter this out for fullscreen override safety
  //if (uMsg == WM_DISPLAYCHANGE)    return game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);
  //if (uMsg == WM_WINDOWPOSCHANGED) return game_window.DefWindowProc (hWnd, uMsg, wParam, lParam);


  const LRESULT lRet =
    SK_COMPAT_SafeCallProc (&game_window, hWnd, uMsg, wParam, lParam);


  // Post-Process the game's result to fix any non-compliant behaviors
  //


  if (hWnd == game_window.hWnd)
  {
    // Fix for Skyrim SE beeping when Alt is pressed.
    if (uMsg == WM_MENUCHAR && (! HIWORD (lRet)))
      return MAKEWPARAM (0, MNC_CLOSE);
  }


  return lRet;
}

bool
SK_Window_IsTopMost (void)
{
  WINDOWINFO winfo        = { };
  winfo.cbSize = sizeof (WINDOWINFO);

  GetWindowInfo (game_window.hWnd, &winfo);

  return (winfo.dwExStyle & WS_EX_TOPMOST);
}

void
SK_Window_SetTopMost (bool bTop, bool bBringToTop)
{
  auto                       SetWindowLongFn =
    ( game_window.unicode ? &SetWindowLongW  :
                            &SetWindowLongA  );

  auto                      GetWindowLongFn =
   ( game_window.unicode ? &GetWindowLongW  :
                           &GetWindowLongA  );

  HWND      hWndOrder = nullptr;
  DWORD_PTR dwStyleEx =
    GetWindowLongFn (game_window.hWnd, GWL_EXSTYLE);

  if (bTop)
  {
    dwStyleEx |=   WS_EX_TOPMOST;
    dwStyleEx &= ~(WS_EX_NOACTIVATE);
    hWndOrder  =   HWND_TOPMOST;
  }

  else
  {
    dwStyleEx &= ~(WS_EX_TOPMOST | WS_EX_NOACTIVATE);
    hWndOrder  =  HWND_NOTOPMOST;
  }

  SetWindowLongFn ( game_window.hWnd, GWL_EXSTYLE, (LONG)dwStyleEx );
  SK_SetWindowPos ( game_window.hWnd,
                   hWndOrder,
                   0, 0, 0, 0,
                   SWP_NOACTIVATE | SWP_NOMOVE         |
                   SWP_NOSIZE     | SWP_NOSENDCHANGING |
                   SWP_ASYNCWINDOWPOS );

  if (bBringToTop)
    BringWindowToTop (game_window.hWnd);
}

void
SK_InitWindow (HWND hWnd, bool fullscreen_exclusive)
{
  if (game_window.GetWindowLongPtr == nullptr)
  {
    return SK_InstallWindowHook (hWnd);
  }


  SK_GetWindowRect (hWnd, &game_window.game.window);
  SK_GetClientRect (hWnd, &game_window.game.client);

  SK_GetWindowRect (hWnd, &game_window.actual.window);
  SK_GetClientRect (hWnd, &game_window.actual.client);

  SK_GetCursorPos  (      &game_window.cursor_pos);



  game_window.actual.style =
    game_window.GetWindowLongPtr ( hWnd, GWL_STYLE );

  game_window.actual.style_ex =
    game_window.GetWindowLongPtr ( hWnd, GWL_EXSTYLE );

  bool has_border  = SK_WindowManager::StyleHasBorder (
    game_window.actual.style
  );

  if (has_border)
  {
    game_window.border_style    = game_window.actual.style;
    game_window.border_style_ex = game_window.actual.style_ex;
  }

  game_window.game.style    = game_window.actual.style;
  game_window.game.style_ex = game_window.actual.style_ex;


  static auto& rb =
    SK_GetCurrentRenderBackend ();

  auto& windows =
    rb.windows;

  windows.setFocus  (hWnd);
  windows.setDevice (
    windows.device ?
    (HWND)windows.device : hWnd);


  if (! fullscreen_exclusive)
  {
    // Next, adjust the border and/or window location if the user
    //   wants an override
    SK_Window_RepositionIfNeeded ();
  }
}

void
SK_InstallWindowHook (HWND hWnd)
{
  wchar_t wszClass [128] = { };

  RealGetWindowClassW (hWnd, wszClass, 127);

  const bool dummy_window =
    StrStrIW (wszClass, L"Special K Dummy Window Class") ||
    StrStrIW (wszClass, L"RTSSWndClass");

  if (dummy_window)
    return;



  if (SK_IsAddressExecutable (game_window.WndProc_Original, true))
    return;

  static volatile LONG               __installed =      FALSE;
  if (! InterlockedCompareExchange (&__installed, TRUE, FALSE))
  {
    // Win32u is faster on systems that dispatch system calls through it
    //
    static sk_import_test_s win32u_test [] = { { "win32u.dll", false } };
    static bool             tested         =                   false;

    if (! tested)
    {
      SK_TestImports (SK_GetModuleHandle (L"user32"), win32u_test,
                                              sizeof (win32u_test) /
                                              sizeof (sk_import_test_s));
      tested = true;
    }


    // When ImGui is active, we will do character translation internally
    //   for any Raw Input or Win32 keydown event -- disable Windows'
    //     translation from WM_KEYDOWN to WM_CHAR while we are doing this.
    //
    SK_CreateDLLHook2 (      L"user32",
                              "TranslateMessage",
                               TranslateMessage_Detour,
      static_cast_p2p <void> (&TranslateMessage_Original) );


    ////NtUserPeekMessage =
    ////  reinterpret_cast <NtUserPeekMessage_pfn> (
    ////    SK_GetProcAddress ( SK_GetModuleHandle (L"win32u"),
    ////                          "NtUserPeekMessage" )
    ////  );

    SK_CreateDLLHook2 (      L"user32",
                              "PeekMessageA",
                               PeekMessageA_Detour,
      static_cast_p2p <void> (&PeekMessageA_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "PeekMessageW",
                               PeekMessageW_Detour,
      static_cast_p2p <void> (&PeekMessageW_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "GetMessageA",
                               GetMessageA_Detour,
      static_cast_p2p <void> (&GetMessageA_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "GetMessageW",
                               GetMessageW_Detour,
      static_cast_p2p <void> (&GetMessageW_Original) );


    // Hook as few of these as possible, disrupting the message pump
    //   when we already have the game's main window hooked is redundant.
    //
    //   ** PeekMessage is hooked because The Witness pulls mouse click events
    //        out of the pump without passing them through its window procedure.
    //
    //SK_CreateUser32Hook (     "GetMessageA",
    //                           GetMessageA_Detour,
    //  static_cast_p2p <void> (&GetMessageA_Original) );
    //SK_CreateUser32Hook (     "GetMessageW",
    //                           GetMessageW_Detour,
    //  static_cast_p2p <void> (&GetMessageW_Original) );

    ////SK_CreateUser32Hook (     "DispatchMessageA",
    ////                           DispatchMessageA_Detour,
    ////  static_cast_p2p <void> (&DispatchMessageA_Original) );
    ////SK_CreateUser32Hook (     "DispatchMessageW",
    ////                           DispatchMessageW_Detour,
    ////  static_cast_p2p <void> (&DispatchMessageW_Original) );

    game_window.WndProc_Original = nullptr;
  }


  if (! SK_GetFramesDrawn ())
    return;


  if (config.render.framerate.enable_mmcss)
    SK_DWM_EnableMMCSS (config.render.framerate.enable_mmcss);


  DWORD                            dwWindowPid = 0;
  GetWindowThreadProcessId (hWnd, &dwWindowPid);

  //
  // If our attempted target belongs to a different process,
  //   it's a trap!
  //
  //     >> Or at least, do not trap said processes input.
  //
  if (dwWindowPid != GetCurrentProcessId ())
    return;


  game_window.unicode =
    SK_Window_IsUnicode (hWnd) != FALSE;


  if (game_window.unicode)
  {
    game_window.GetWindowLongPtr = SK_RunLHIfBitness ( 64, SK_GetWindowLongPtrW,
                  reinterpret_cast <GetWindowLongPtr_pfn> (SK_GetWindowLongW)    );

    game_window.SetWindowLongPtr = SK_SetWindowLongPtrW;
    game_window.DefWindowProc    = (DefWindowProc_pfn)
                    GetProcAddress (SK_GetModuleHandle (L"user32"),
                                    "DefWindowProcW");
    game_window.CallWindowProc   = (CallWindowProc_pfn)
                    GetProcAddress (SK_GetModuleHandle (L"user32"),
                                    "CallWindowProcW" );
  }

  else
  {
    game_window.GetWindowLongPtr = SK_RunLHIfBitness ( 64, SK_GetWindowLongPtrA,
                  reinterpret_cast <GetWindowLongPtr_pfn> (SK_GetWindowLongA)    );

    game_window.SetWindowLongPtr = SK_SetWindowLongPtrA;
    game_window.DefWindowProc    = (DefWindowProc_pfn)
                    GetProcAddress (SK_GetModuleHandle (L"user32"),
                                    "DefWindowProcA");
    game_window.CallWindowProc   = (CallWindowProc_pfn)
                    GetProcAddress (SK_GetModuleHandle (L"user32"),
                                    "CallWindowProcA" );
  }



  WNDPROC class_proc = game_window.unicode   ? (WNDPROC)
    GetClassLongPtrW  ( hWnd, GCLP_WNDPROC ) : (WNDPROC)
    GetClassLongPtrA  ( hWnd, GCLP_WNDPROC );

  WNDPROC wnd_proc =
    (WNDPROC)(game_window.GetWindowLongPtr (hWnd, GWLP_WNDPROC));


  void SK_MakeWindowHook (   WNDPROC,  WNDPROC, HWND);
       SK_MakeWindowHook (class_proc, wnd_proc, hWnd);


  if ( game_window.WndProc_Original != nullptr )
  {
          bool  has_raw_mouse = false;
          UINT  count         = 0;

    const DWORD dwLast        = GetLastError ();

    SK_GetRegisteredRawInputDevices (
      nullptr, &count, sizeof (RAWINPUTDEVICE)
    );

    if (count > 0)
    {
      SK_TLS *pTLS =
        SK_TLS_Bottom ();

      auto* pDevs =
        pTLS->raw_input->allocateDevices (
          static_cast <size_t> (count) + 1
        );

      SK_GetRegisteredRawInputDevices ( pDevs,
                                       &count,
                                       sizeof (RAWINPUTDEVICE) );

      for (UINT i = 0 ; i < count ; i++)
      {
       if ( ( pDevs [i].usUsage     == HID_USAGE_GENERIC_MOUSE ||
              pDevs [i].usUsage     == HID_USAGE_GENERIC_POINTER ) &&
            ( pDevs [i].usUsagePage == HID_USAGE_PAGE_GENERIC  ||
              pDevs [i].usUsagePage == HID_USAGE_PAGE_GAME       ) )
        {
          has_raw_mouse = true; // Technically this could be just about any pointing device
                                //   including a touchscreen (on purpose).
          break;
        }
      }
    }

    // If no pointing device is setup, go ahead and register our own mouse;
    //   it won't interfere with the game's operation until the ImGui overlay
    //     requires a mouse... at which point third-party software will flip out :)
    ///if (! has_raw_mouse)
    ///{
    ///  RAWINPUTDEVICE rid;
    ///  rid.hwndTarget  = hWnd;
    ///  rid.usUsage     = HID_USAGE_GENERIC_MOUSE;
    ///  rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    ///  rid.dwFlags     = RIDEV_DEVNOTIFY;
    ///
    ///  SK_RegisterRawInputDevices (&rid, 1, sizeof RAWINPUTDEVICE);
    ///}

    SetLastError (dwLast);

    static SK_RenderBackend& rb =
      SK_GetCurrentRenderBackend ();

    rb.windows.setFocus  (hWnd);
    rb.windows.setDevice (rb.windows.device ?
                           game_window.hWnd : hWnd);

    SK_InitWindow (hWnd);


    // Fix the cursor clipping rect if needed so that we can use Special K's
    //   menu system.
    RECT                 cursor_clip = { };
    if ( GetClipCursor (&cursor_clip) )
    {
      // Run it through the hooked function, which will transform certain
      //   attributes as needed.
      ClipCursor (&cursor_clip);
    }


    gsl::not_null <SK_ICommandProcessor*> cmd (
      SK_GetCommandProcessor ()
    );

    cmd->AddVariable ("Cursor.Manage",           SK_CreateVar (SK_IVariable::Boolean, (bool *)&config.input.cursor.manage));
    cmd->AddVariable ("Cursor.Timeout",          SK_CreateVar (SK_IVariable::Int,     (int  *)&config.input.cursor.timeout));
    cmd->AddVariable ("Cursor.KeysActivate",     SK_CreateVar (SK_IVariable::Boolean, (bool *)&config.input.cursor.keys_activate));

    cmd->AddVariable ("Window.BackgroundRender", SK_CreateVar (SK_IVariable::Boolean, (bool *)&config.window.background_render));

    cmd->AddVariable ("ImGui.Visible",           SK_CreateVar (SK_IVariable::Boolean, (bool *)&SK_ImGui_Visible));
  }
}

HWND
__stdcall
SK_GetGameWindow (void)
{
  ////SK_WINDOW_LOG_CALL3 ();

  return game_window.hWnd;
}

bool
__stdcall
SK_IsGameWindowActive (void)
{
                                                       DWORD dwProcId;
  if (GetWindowThreadProcessId (SK_GetForegroundWindow (),  &dwProcId)) {
                              if ( GetCurrentProcessId () == dwProcId)
                              {
                                return true;
                              }                                         }

  return
    game_window.active;
}


void
SK_MakeWindowHook (WNDPROC class_proc, WNDPROC wnd_proc, HWND hWnd)
{
  wchar_t wszClassName [128] = { };
  wchar_t wszTitle     [128] = { };

  InternalGetWindowText ( hWnd, wszTitle,     127 );
  RealGetWindowClassW   ( hWnd, wszClassName, 127 );

  dll_log->Log ( L"[Window Mgr] Hooking the Window Procedure for "
                 L"%s Window Class ('%s' - \"%s\" * %x)",
                 game_window.unicode ? L"Unicode" : L"ANSI",
                 wszClassName, wszTitle, hWnd );

  dll_log->Log ( L"[Window Mgr]  $ ClassProc:  %s",
                SK_MakePrettyAddress (class_proc).c_str () );
  dll_log->Log ( L"[Window Mgr]  $ WndProc:    %s",
     ( class_proc != wnd_proc ?
                SK_MakePrettyAddress (wnd_proc).c_str   () :
                L"Same" ) );

  WNDPROC target_proc = nullptr;
  bool    ignore_proc = false;

  // OpenGL games almost always have a window proc. located in OpenGL32.dll; we don't want that.
  if      (SK_GetModuleFromAddr (class_proc) == GetModuleHandle (nullptr)) target_proc = class_proc;
  else if (SK_GetModuleFromAddr (wnd_proc)   == GetModuleHandle (nullptr)) target_proc = wnd_proc;
  else if (SK_GetModuleFromAddr (class_proc) != INVALID_HANDLE_VALUE)      target_proc = class_proc;
  else                                                                     ignore_proc = true;// target_proc = wnd_proc;


  // In case we cannot hook the target, try the other...
  WNDPROC
    alt_proc = ( target_proc == class_proc ? wnd_proc :
                                           class_proc );

  const bool
    hook_func = (! config.window.dont_hook_wndproc) &&
                (! ignore_proc);


  if (hook_func)
  {
    if ( MH_OK ==
         MH_CreateHook (
                                            target_proc,
                                    SK_DetourWindowProc,
           static_cast_p2p <void> (&game_window.WndProc_Original)
         )
       )
    {
      MH_EnableHook (target_proc);

      dll_log->Log (L"[Window Mgr]  >> Hooked %s.", ( target_proc == class_proc ? L"ClassProc" :
                    L"WndProc" ) );

      game_window.hooked = true;
    }


    // Target didn't work, go with the other one...
    else if ( MH_OK ==
               MH_CreateHook (
                 alt_proc,
                   SK_DetourWindowProc,
      static_cast_p2p <void> (&game_window.WndProc_Original)
                             )
            )
    {
      MH_EnableHook (alt_proc);

      dll_log->Log (L"[Window Mgr]  >> Hooked %s.", ( alt_proc == class_proc ? L"ClassProc" :
                    L"WndProc" ) );

      game_window.hooked = true;
    }

    // Now we're boned!  (Actually, fallback to SetClass/WindowLongPtr :P)
    else
    {
      game_window.WndProc_Original = nullptr;
    }

    //if (game_window.WndProc_Original != nullptr)
    //  SK_ApplyQueuedHooks ();
  }


  if ((! hook_func) || game_window.WndProc_Original == nullptr)
  {
    target_proc = wnd_proc;

    dll_log->Log ( L"[Window Mgr]  >> Hooking was impossible; installing new "
                   L"%s procedure instead (this may be undone "
                   L"by other software).", target_proc == class_proc ?
                   L"Class" : L"Window"   );

    game_window.WndProc_Original = target_proc;

    if (target_proc == class_proc)
    {
      if (game_window.unicode)
        SetClassLongPtrW ( (HWND)hWnd, GCLP_WNDPROC, (LONG_PTR)SK_DetourWindowProc );
      else
        SetClassLongPtrA ( (HWND)hWnd, GCLP_WNDPROC, (LONG_PTR)SK_DetourWindowProc );

      game_window.hooked = false;
    }

    else
    {
      //game_window.SetWindowLongPtr ( hWnd,
      game_window.unicode ?
        SK_SetWindowLongPtrW ( (HWND)hWnd,
                                     GWLP_WNDPROC,
        reinterpret_cast <LONG_PTR> (SK_DetourWindowProc) ) :
        SK_SetWindowLongPtrA ( (HWND)hWnd,
                                     GWLP_WNDPROC,
        reinterpret_cast <LONG_PTR> (SK_DetourWindowProc) );

      game_window.hooked = false;
    }
  }

  game_window.hWnd   = hWnd;
  game_window.active = true;

  // Kiss of death for sane window management
  if (! _wcsicmp (wszClassName, L"UnityWndClass"))
    SK_GetCurrentRenderBackend ().windows.unity = true;
}



/* size of a form name string */
#define CCHFORMNAME 32

using EnumDisplaySettingsA_pfn = BOOL (WINAPI *) (
  _In_opt_ LPCSTR    lpszDeviceName,
  _In_     DWORD      iModeNum,
  _Inout_  DEVMODEA *lpDevMode
  );
EnumDisplaySettingsA_pfn EnumDisplaySettingsA_Original = nullptr;

using EnumDisplaySettingsW_pfn = BOOL (WINAPI *) (
  _In_opt_ LPWSTR    lpszDeviceName,
  _In_     DWORD      iModeNum,
  _Inout_  DEVMODEW *lpDevMode
  );
EnumDisplaySettingsW_pfn EnumDisplaySettingsW_Original = nullptr;


// SAL notation in Win32 API docs is wrong
using ChangeDisplaySettingsA_pfn = LONG (WINAPI *)(
  _In_opt_ DEVMODEA *lpDevMode,
  _In_     DWORD     dwFlags
  );
ChangeDisplaySettingsA_pfn ChangeDisplaySettingsA_Original = nullptr;

// SAL notation in Win32 API docs is wrong
using ChangeDisplaySettingsW_pfn = LONG (WINAPI *)(
  _In_opt_ DEVMODEW *lpDevMode,
  _In_     DWORD     dwFlags
  );
ChangeDisplaySettingsW_pfn ChangeDisplaySettingsW_Original = nullptr;

using ChangeDisplaySettingsExA_pfn = LONG (WINAPI *)(
  _In_ LPCSTR    lpszDeviceName,
  _In_ DEVMODEA *lpDevMode,
  HWND      hwnd,
  _In_ DWORD     dwflags,
  _In_ LPVOID    lParam
  );
ChangeDisplaySettingsExA_pfn ChangeDisplaySettingsExA_Original = nullptr;

using ChangeDisplaySettingsExW_pfn = LONG (WINAPI *)(
  _In_ LPCWSTR   lpszDeviceName,
  _In_ DEVMODEW *lpDevMode,
  HWND      hwnd,
  _In_ DWORD     dwflags,
  _In_ LPVOID    lParam
  );
ChangeDisplaySettingsExW_pfn ChangeDisplaySettingsExW_Original = nullptr;

LONG
WINAPI
ChangeDisplaySettingsExA_Detour (
  _In_ LPCSTR    lpszDeviceName,
  _In_ DEVMODEA *lpDevMode,
  HWND      hWnd,
  _In_ DWORD     dwFlags,
  _In_ LPVOID    lParam )
{
  SK_LOG_FIRST_CALL

  static bool called = false;

  DEVMODEA dev_mode        = { };
           dev_mode.dmSize = sizeof (DEVMODEA);

  if (! config.window.res.override.isZero ())
  {
    if (lpDevMode != nullptr)
    {
      lpDevMode->dmPelsWidth  = config.window.res.override.x;
      lpDevMode->dmPelsHeight = config.window.res.override.y;
    }
  }

  EnumDisplaySettingsA_Original (lpszDeviceName, 0, &dev_mode);

  if (dwFlags != CDS_TEST)
  {
    if (called)
      ChangeDisplaySettingsExA_Original (lpszDeviceName, lpDevMode, hWnd, CDS_RESET, lParam);

    called = true;

    return
      ChangeDisplaySettingsExA_Original (lpszDeviceName, lpDevMode, hWnd, CDS_FULLSCREEN, lParam);
  }

  else
  {
    return
      ChangeDisplaySettingsExA_Original (lpszDeviceName, lpDevMode, hWnd, dwFlags, lParam);
  }
}


LONG
WINAPI
ChangeDisplaySettingsA_Detour (
  _In_opt_ DEVMODEA *lpDevMode,
  _In_     DWORD     dwFlags )
{
  SK_LOG_FIRST_CALL

  return
    ChangeDisplaySettingsExA_Detour (nullptr, lpDevMode, nullptr, dwFlags, nullptr);
}

LONG
WINAPI
ChangeDisplaySettingsExW_Detour (
  _In_ LPWSTR    lpszDeviceName,
  _In_ DEVMODEW *lpDevMode,
       HWND      hWnd,
  _In_ DWORD     dwFlags,
  _In_ LPVOID    lParam)
{
  SK_LOG_FIRST_CALL

  static bool called = false;

  DEVMODEW dev_mode        = { };
           dev_mode.dmSize = sizeof (DEVMODEW);

  if (! config.window.res.override.isZero ())
  {
    if (lpDevMode != nullptr)
    {
      lpDevMode->dmPelsWidth  = config.window.res.override.x;
      lpDevMode->dmPelsHeight = config.window.res.override.y;
    }
  }

  EnumDisplaySettingsW_Original (lpszDeviceName, 0, &dev_mode);

  if (dwFlags != CDS_TEST)
  {
    if (called)
      ChangeDisplaySettingsExW_Original (lpszDeviceName, lpDevMode, hWnd, CDS_RESET, lParam);

    called = true;

    return
      ChangeDisplaySettingsExW_Original (lpszDeviceName, lpDevMode, hWnd, CDS_FULLSCREEN, lParam);
  }

  else
  {
    return
      ChangeDisplaySettingsExW_Original (lpszDeviceName, lpDevMode, hWnd, dwFlags, lParam);
  }
}

LONG
WINAPI
ChangeDisplaySettingsW_Detour (
  _In_opt_ DEVMODEW *lpDevMode,
  _In_     DWORD     dwFlags )
{
  SK_LOG_FIRST_CALL

  return
    ChangeDisplaySettingsExW_Detour (nullptr, lpDevMode, nullptr, dwFlags, nullptr);
}




void
SK_HookWinAPI (void)
{
  static volatile LONG hooked = FALSE;

  if (! InterlockedCompareExchange (&hooked, TRUE, FALSE))
  {
    // Initialize the Window Manager
    SK_WindowManager::getInstance ();

#ifdef _DEBUG
    int phys_w         = GetDeviceCaps (GetDC (GetDesktopWindow ()), PHYSICALWIDTH);
    int phys_h         = GetDeviceCaps (GetDC (GetDesktopWindow ()), PHYSICALHEIGHT);
    int phys_off_x     = GetDeviceCaps (GetDC (GetDesktopWindow ()), PHYSICALOFFSETX);
    int phys_off_y     = GetDeviceCaps (GetDC (GetDesktopWindow ()), PHYSICALOFFSETY);
    int scale_factor_x = GetDeviceCaps (GetDC (GetDesktopWindow ()), SCALINGFACTORX);
    int scale_factor_y = GetDeviceCaps (GetDC (GetDesktopWindow ()), SCALINGFACTORY);

    SK_LOG0 ( ( L" Physical Coordinates  ::  { %lux%lu) + <%lu,%lu> * {%lu:%lu}",
             phys_w, phys_h,
             phys_off_x, phys_off_y,
             scale_factor_x, scale_factor_y ),
             L"Window Sys" );
#endif

#if 1
        SK_CreateDLLHook2 (  L"user32",
                              "SetWindowPos",
                               SetWindowPos_Detour,
      static_cast_p2p <void> (&SetWindowPos_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "SetWindowPlacement",
                               SetWindowPlacement_Detour,
      static_cast_p2p <void> (&SetWindowPlacement_Original) );


    SK_CreateDLLHook2 (      L"user32",
                              "MoveWindow",
                               MoveWindow_Detour,
      static_cast_p2p <void> (&MoveWindow_Original) );
#else
    SetWindowPos_Original =
      (SetWindowPos_pfn)
      GetProcAddress ( GetModuleHandleW (L"user32"),
                      "SetWindowPos" );
    SetWindowPlacement_Original =
      (SetWindowPlacement_pfn)
      GetProcAddress ( GetModuleHandleW (L"user32"),
                      "SetWindowPlacement" );

    MoveWindow_Original =
      (MoveWindow_pfn)
      GetProcAddress ( GetModuleHandleW (L"user32"),
                      "MoveWindow" );
#endif
    SK_CreateDLLHook2 (      L"user32",
                              "ClipCursor",
                               ClipCursor_Detour,
      static_cast_p2p <void> (&ClipCursor_Original));

    SK_CreateDLLHook2 (       L"user32",
                       "SetWindowLongA",
                       SetWindowLongA_Detour,
                       static_cast_p2p <void> (&SetWindowLongA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "SetWindowLongW",
                       SetWindowLongW_Detour,
                       static_cast_p2p <void> (&SetWindowLongW_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "GetWindowLongA",
                       GetWindowLongA_Detour,
                       static_cast_p2p <void> (&GetWindowLongA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "GetWindowLongW",
                       GetWindowLongW_Detour,
                       static_cast_p2p <void> (&GetWindowLongW_Original) );
#ifdef _WIN64
    SK_CreateDLLHook2 (       L"user32",
                       "SetWindowLongPtrA",
                       SetWindowLongPtrA_Detour,
                       static_cast_p2p <void> (&SetWindowLongPtrA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "SetWindowLongPtrW",
                       SetWindowLongPtrW_Detour,
                       static_cast_p2p <void> (&SetWindowLongPtrW_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "GetWindowLongPtrA",
                       GetWindowLongPtrA_Detour,
                       static_cast_p2p <void> (&GetWindowLongPtrA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "GetWindowLongPtrW",
                       GetWindowLongPtrW_Detour,
                       static_cast_p2p <void> (&GetWindowLongPtrW_Original) );
#else

    // 32-bit Windows does not have these functions; just route them
    //   through the normal function since pointers and DWORDS have
    //     compatible size.
    SetWindowLongPtrA_Original = SetWindowLongA_Original;
    SetWindowLongPtrW_Original = SetWindowLongW_Original;
    GetWindowLongPtrW_Original = GetWindowLongW_Original;
    GetWindowLongPtrA_Original = GetWindowLongA_Original;
#endif

#if 0
    SK_CreateDLLHook2 (      L"user32",
                              "GetWindowRect",
                               GetWindowRect_Detour,
      static_cast_p2p <void> (&GetWindowRect_Original) );
#else
    GetWindowRect_Original =
      reinterpret_cast <GetWindowRect_pfn> (
        SK_GetProcAddress ( L"user32", "GetWindowRect" )
      );
#endif

#if 0
    SK_CreateDLLHook2 (      L"user32",
                              "GetClientRect",
                               GetClientRect_Detour,
      static_cast_p2p <void> (&GetClientRect_Original) );
#else
    GetClientRect_Original =
      reinterpret_cast <GetClientRect_pfn> (
        GetProcAddress ( SK_GetModuleHandleW (L"user32"),
        "GetClientRect" )
        );
#endif

    SK_CreateDLLHook2 (      L"user32",
                              "AdjustWindowRect",
                               AdjustWindowRect_Detour,
      static_cast_p2p <void> (&AdjustWindowRect_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "AdjustWindowRectEx",
                               AdjustWindowRectEx_Detour,
      static_cast_p2p <void> (&AdjustWindowRectEx_Original) );

    SK_CreateDLLHook2 (      L"user32",
                              "GetSystemMetrics",
                               GetSystemMetrics_Detour,
      static_cast_p2p <void> (&GetSystemMetrics_Original) );


    SK_CreateDLLHook2 (       L"user32",
                               "GetForegroundWindow",
                                GetForegroundWindow_Detour,
       static_cast_p2p <void> (&GetForegroundWindow_Original) );

    SK_CreateDLLHook2 (       L"user32",
                               "GetActiveWindow",
                                GetActiveWindow_Detour,
       static_cast_p2p <void> (&GetActiveWindow_Original) );

    SK_CreateDLLHook2 (       L"user32",
                               "SetActiveWindow",
                                SetActiveWindow_Detour,
       static_cast_p2p <void> (&SetActiveWindow_Original) );


#if 0
    SK_CreateDLLHook2 (      L"user32",
                              "ChangeDisplaySettingsA",
                               ChangeDisplaySettingsA_Detour,
      static_cast_p2p <void> (&ChangeDisplaySettingsA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "ChangeDisplaySettingsW",
                       ChangeDisplaySettingsW_Detour,
                       static_cast_p2p <void> (&ChangeDisplaySettingsW_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "ChangeDisplaySettingsExA",
                       ChangeDisplaySettingsExA_Detour,
                       static_cast_p2p <void> (&ChangeDisplaySettingsExA_Original) );

    SK_CreateDLLHook2 (       L"user32",
                       "ChangeDisplaySettingsExW",
                       ChangeDisplaySettingsExW_Detour,
                       static_cast_p2p <void> (&ChangeDisplaySettingsExW_Original) );
#else
    ChangeDisplaySettingsA_Original =
      (ChangeDisplaySettingsA_pfn)SK_GetProcAddress
      ( L"user32", "ChangeDisplaySettingsA" );
    ChangeDisplaySettingsExA_Original =
      (ChangeDisplaySettingsExA_pfn)SK_GetProcAddress
      ( L"user32", "ChangeDisplaySettingsExA" );
    ChangeDisplaySettingsW_Original =
      (ChangeDisplaySettingsW_pfn)SK_GetProcAddress
      ( L"user32", "ChangeDisplaySettingsW" );
    ChangeDisplaySettingsExW_Original =
      (ChangeDisplaySettingsExW_pfn)SK_GetProcAddress
      ( L"user32", "ChangeDisplaySettingsExW" );
#endif


    EnumDisplaySettingsA_Original =
      (EnumDisplaySettingsA_pfn) GetProcAddress (
        SK_GetModuleHandle (L"user32"),
        "EnumDisplaySettingsA" );
    EnumDisplaySettingsW_Original =
      (EnumDisplaySettingsW_pfn) GetProcAddress (
        SK_GetModuleHandle (L"user32"),
        "EnumDisplaySettingsW" );

    InterlockedIncrement (&hooked);
  }

  else
    SK_Thread_SpinUntilAtomicMin (&hooked, 2);
}


bool
sk_window_s::needsCoordTransform (void)
{
  if (! config.window.res.override.fix_mouse)
    return false;

  bool dynamic_window =
    (config.window.borderless /*&& config.window.fullscreen*/);

  if (! dynamic_window)
    return false;

  return (! coord_remap.identical);
}

void
sk_window_s::updateDims (void)
{
  if (config.window.borderless && config.window.fullscreen)
  {
    HMONITOR hMonitor =
      MonitorFromWindow ( hWnd,
                            MONITOR_DEFAULTTONEAREST );

    MONITORINFO     mi        = { 0 };
                    mi.cbSize = sizeof (mi);
    GetMonitorInfo (
      hMonitor,    &mi);

    actual.window = mi.rcMonitor;

    actual.client.left   = 0;
    actual.client.right  = actual.window.right - actual.window.left;
    actual.client.top    = 0;
    actual.client.bottom = actual.window.bottom - actual.window.top;
  }

  long game_width   = (game.client.right   - game.client.left);
  long window_width = (actual.client.right - actual.client.left);

  long game_height   = (game.client.bottom   - game.client.top);
  long window_height = (actual.client.bottom - actual.client.top);

  bool resized =
    (game_width != window_width || game_height != window_height);

  bool moved =
    ( game.window.left != actual.window.left ||
      game.window.top  != actual.window.top );

  if (resized || moved) {
    coord_remap.identical = false;

    coord_remap.scale.x =
      (float)window_width  / (float)game_width;
    coord_remap.scale.y =
      (float)window_height / (float)game_height;

    coord_remap.offset.x =
      (float)(actual.window.left + actual.client.left) -
      (float)(game.window.left   + game.client.left);
    coord_remap.offset.y =
      (float)(actual.window.top + actual.client.top) -
      (float)(game.window.top   + game.client.top);
  }

  else {
    coord_remap.identical = true;

    coord_remap.offset.x  = 0.0f;
    coord_remap.offset.y  = 0.0f;

    coord_remap.scale.x   = 0.0f;
    coord_remap.scale.y   = 0.0f;
  }
}


LRESULT
CALLBACK
sk_window_s::DefProc (
  _In_ UINT   Msg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
)
{
  return
    DefWindowProc (hWnd, Msg, wParam, lParam);
}

LRESULT
WINAPI
sk_window_s::CallProc (
  _In_ HWND    hWnd_,
  _In_ UINT    Msg,
  _In_ WPARAM  wParam,
  _In_ LPARAM  lParam )
{
  // We subclassed the window instead of hooking the game's window proc
  if (! hooked)
    return CallWindowProc (WndProc_Original, hWnd_, Msg, wParam, lParam);
  else
    return WndProc_Original (hWnd_, Msg, wParam, lParam);
}


LRESULT
WINAPI
SK_COMPAT_SafeCallProc (sk_window_s* pWin, HWND hWnd_, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  if (pWin == nullptr)
    return 1;

  LRESULT ret = 1;

  auto orig_se =
  SK_SEH_ApplyTranslator (
    SK_FilteringStructuredExceptionTranslator (
      EXCEPTION_ACCESS_VIOLATION
    )
  );
  try {
    ret =
      pWin->CallProc (hWnd_, Msg, wParam, lParam);
  }
  catch (const SK_SEH_IgnoredException&)
  {
    assert (false);
  }
  SK_SEH_RemoveTranslator (orig_se);

  return ret;
}

BOOL
SK_Win32_IsGUIThread ( DWORD    dwTid,
                       SK_TLS **ppTLS )
{
  UNREFERENCED_PARAMETER (ppTLS);

  static volatile LONG64 last_result = 0x0;
                  LONG64 test_result =
                    ReadAcquire64 (&last_result);

  if ((DWORD)(test_result & 0x00000000FFFFFFFFULL) == dwTid)
  {
    return (test_result >> 32) > 0 ? TRUE : FALSE;
  }

  BOOL bGUI =
    IsGUIThread (FALSE);

  test_result = (bGUI ? (1ull << 32) : 0)
    | (LONG64)(dwTid & 0xFFFFFFFFUL);

  InterlockedExchange64 (&last_result, test_result);

  return bGUI;
}





HRESULT
WINAPI
SK_DWM_EnableMMCSS (BOOL enable)
{
  typedef HRESULT (WINAPI *DwmEnableMMCSS_pfn)(BOOL);
  static                   DwmEnableMMCSS_pfn
                           DwmEnableMMCSS =
         reinterpret_cast <DwmEnableMMCSS_pfn> (
     SK_GetProcAddress ( L"dwmapi.dll",
                          "DwmEnableMMCSS" )   );

  return
    ( DwmEnableMMCSS != nullptr ) ?
      DwmEnableMMCSS (  enable  ) : E_NOTIMPL;
}














BOOL
WINAPI
SK_ClipCursor (const RECT *lpRect)
{
  if (ClipCursor_Original != nullptr)
    return ClipCursor_Original (lpRect);

  return
    ClipCursor (lpRect);
}

BOOL
WINAPI
SK_GetCursorPos (LPPOINT lpPoint)
{
  if (GetCursorPos_Original != nullptr)
    return GetCursorPos_Original (lpPoint);

  return
    GetCursorPos (lpPoint);
}

BOOL
WINAPI
SK_SetCursorPos (int X, int Y)
{
  if (SetCursorPos_Original != nullptr)
    return SetCursorPos_Original (X, Y);

  return
    SetCursorPos (X, Y);
}

UINT
WINAPI
SK_SendInput (UINT nInputs, LPINPUT pInputs, int cbSize)
{
  if (SendInput_Original != nullptr)
    return SendInput_Original (nInputs, pInputs, cbSize);

  return
    SendInput (nInputs, pInputs, cbSize);
}

BOOL
WINAPI
SK_SetWindowPos ( HWND hWnd,
                  HWND hWndInsertAfter,
                  int  X,
                  int  Y,
                  int  cx,
                  int  cy,
                  UINT uFlags )
{
  if (SetWindowPos_Original != nullptr)
  {
    return
      SetWindowPos_Original (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
  }

  return
    SetWindowPos (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}


void
SKX_Window_EstablishRoot (void)
{
  HWND  hWndActive     = GetActiveWindow     ();
  HWND  hWndFocus      = GetFocus            ();
  HWND  hWndForeground = GetForegroundWindow ();

  HWND  hWndTarget     = SK_GetCurrentRenderBackend ().windows.device;
  DWORD dwWindowPid    = 0;

  if (hWndTarget == 0)
  {
    if (SK_Win32_IsGUIThread ())
    {
      GetWindowThreadProcessId (hWndActive, &dwWindowPid);

      if (dwWindowPid == GetCurrentProcessId ())
      {
        hWndTarget = hWndActive;
      }
    }

    if (hWndTarget == 0)
    {
      GetWindowThreadProcessId (hWndFocus, &dwWindowPid);

      if (dwWindowPid == GetCurrentProcessId ())
      {
        hWndTarget = hWndFocus;
      }

      else
      {
        GetWindowThreadProcessId (hWndForeground, &dwWindowPid);

        if (dwWindowPid == GetCurrentProcessId ())
        {
          hWndTarget = hWndForeground;
        }
      }
    }
  }

  if (hWndTarget != 0)
    SK_InstallWindowHook (hWndTarget);
}