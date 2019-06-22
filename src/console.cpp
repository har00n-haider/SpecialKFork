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

#define SK_MakeKeyMask(vKey,ctrl,shift,alt) \
  (UINT)((vKey) | (((ctrl) != 0) <<  9) |   \
                  (((shift)!= 0) << 10) |   \
                  (((alt)  != 0) << 11))

SK_Console::SK_Console (void)
{
  visible        = false;
  command_issued = false;
  result_str.clear ();

  RtlSecureZeroMemory (text, 4096);
  RtlSecureZeroMemory (keys_, 256);
}

SK_Console*
SK_Console::getInstance (void)
{
  if (pConsole == nullptr)
    pConsole = std::make_unique <SK_Console> ();

  return pConsole.get ();
}

void
SK_Console::Draw (void)
{
  static bool          carret    = false;
  static LARGE_INTEGER last_time = { 0ULL };

           std::string output    = "";

  if (visible)
  {
    output.append (text);

    LARGE_INTEGER now;
    LARGE_INTEGER freq;

    QueryPerformanceFrequency        (&freq);

    extern LARGE_INTEGER SK_QueryPerf (void);
    now = SK_QueryPerf ();

    // Blink the Carret
    if ((now.QuadPart - last_time.QuadPart) > (freq.QuadPart / 3))
    {
      carret = ! carret;

      last_time.QuadPart = now.QuadPart;
    }

    if (carret)
      output.append ("-");

    // Show Command Results
    if (command_issued)
    {
      output.append ("\n");
      output.append (result_str);
    }
  }

  else
  {
    output.clear ();
  }

  SK_DrawExternalOSD ("SpecialK Console", output);
}

void
SK_Console::Start (void)
{
  RtlSecureZeroMemory (text, 4096);

  text [0] = '>';
}

void
SK_Console::End (void)
{
}

void
SK_Console::reset (void)
{
  RtlSecureZeroMemory (keys_, 256);
}


SK_LazyGlobal <std::unordered_multimap <uint32_t, SK_KeyCommand>> SK_KeyboardMacros;


// Plugins can hook this if they do not have their own input handler
__declspec (noinline)
void
CALLBACK
SK_PluginKeyPress (BOOL Control, BOOL Shift, BOOL Alt, BYTE vkCode)
{
  if (! SK_GetFramesDrawn ())
    return;


  UNREFERENCED_PARAMETER (Control);
  UNREFERENCED_PARAMETER (Shift);
  UNREFERENCED_PARAMETER (Alt);


  // Hack because some games block this
  Alt = ImGui::GetIO ().KeyAlt;


  if (! SK_ImGui_Widgets->DispatchKeybinds (Control, Shift, Alt, vkCode))
  {

  }


  auto masked =
    SK_MakeKeyMask (vkCode, Control, Shift, Alt);

  auto* kb_macros =
    SK_KeyboardMacros.getPtr ();

  if (kb_macros->find (masked) != kb_macros->cend ())
  {
    auto range =
      kb_macros->equal_range (masked);

    auto* cp =
      SK_GetCommandProcessor ();

    for_each ( range.first, range.second,
      [&] (std::unordered_multimap <uint32_t, SK_KeyCommand>::value_type& cmd_pair)
      {
        cp->ProcessCommandLine (
          SK_WideCharToUTF8 (cmd_pair.second.command).c_str ()
        );
      }
    );
  }
}

BOOL
SK_ImGui_KeyPress (BOOL Control, BOOL Shift, BOOL Alt, BYTE vkCode)
{
  if (! SK_GetFramesDrawn ())
    return TRUE;




  UNREFERENCED_PARAMETER (Alt);

  static const SHORT SK_ImGui_ToggleKeys [4] = {
    VK_BACK,              //Primary button (backspace)

    // Modifier Keys
    VK_CONTROL, VK_SHIFT, // Ctrl + Shift
    0
  };

  if ( vkCode  == SK_ImGui_ToggleKeys [0] &&
       Control && Shift )
  {
    extern void SK_ImGui_Toggle (void);

    SK_ImGui_Toggle ();
    return FALSE;
  }

  return TRUE;
}


extern SHORT SK_ImGui_ToggleKeys [4];

bool
SK_ImGui_ProcessKeyPress (const BYTE& vkCode)
{
  static bool& visible = SK_Console::getInstance ()->visible;
  static BYTE* keys_   = SK_Console::getInstance ()->keys_;

  bool new_press =
   keys_ [vkCode] != 0x81;

   keys_ [vkCode]  = 0x81;

  if (new_press)
  {
    // First give ImGui a chance to process this
    if (SK_ImGui_KeyPress (keys_ [VK_CONTROL], keys_ [VK_SHIFT], keys_ [VK_MENU], vkCode))
    {
      // Then give any plug-ins a chance
      SK_PluginKeyPress   (keys_ [VK_CONTROL], keys_ [VK_SHIFT], keys_ [VK_MENU], vkCode);


      // Finally, toggle the command console
      if ( SK_MakeKeyMask (vkCode, keys_ [VK_CONTROL], keys_ [VK_SHIFT], keys_ [VK_MENU]) ==
           SK_MakeKeyMask (VK_TAB, 1, 1, 0) )
      {
        visible = ! visible;

        // This will pause/unpause the game
        SK::SteamAPI::SetOverlayState (visible);

        return true;
      }
    }

    else
      return true;
  }

  return false;
};

int
SK_HandleConsoleKey (bool keyDown, BYTE vkCode, LPARAM lParam)
{
  bool&  visible = SK_Console::getInstance ()->visible;
  BYTE*  keys_   = SK_Console::getInstance ()->keys_;

  char*                          text           = SK_Console::getInstance ()->text;
  SK_Console::command_history_t& commands       = SK_Console::getInstance ()->commands;
  bool&                          command_issued = SK_Console::getInstance ()->command_issued;
  std::string&                   result_str     = SK_Console::getInstance ()->result_str;

  // This is technically mouse button 0 (left mouse button on default systems),
  //  but for convenience we allow indexing this array using 0 to have a constant
  //    state, allowing algorithms to use 0 to mean "don't care."
  keys_ [0] = TRUE;

  if (! SK_IsSteamOverlayActive ())
  {
    // Proprietary HACKJOB:  lParam = MAXDWORD indicates a make/break event, not meant for text input
    if (lParam == MAXDWORD && keyDown)
    {
      return (! SK_ImGui_ProcessKeyPress (vkCode));
    }


    BYTE scanCode = HIWORD (lParam) & 0x7F;


    // Disable the command console if the ImGui overlay is visible
    if (SK_ImGui_Active ())
      visible = false;


    if (visible && vkCode == VK_BACK)
    {
      if (keyDown)
      {
        size_t len = strlen (text);
               len--;

        if (len < 1)
          len = 1;

        text [len] = '\0';
      }
    }

    else if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)
    {
      vkCode = VK_SHIFT;

      if (keyDown) keys_ [vkCode] = 0x81;
      else         keys_ [vkCode] = 0x00;
    }

    else if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU)
    {
      vkCode = VK_MENU;

      if (keyDown) keys_ [vkCode] = 0x81;
      else         keys_ [vkCode] = 0x00;
    }

    else if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)
    {
      vkCode = VK_CONTROL;

      if (keyDown) keys_ [vkCode] = 0x81;
      else         keys_ [vkCode] = 0x00;
    }

    else if (keyDown && visible && ((vkCode == VK_UP) || (vkCode == VK_DOWN)))
    {
      if (vkCode == VK_UP)
        commands.idx--;
      else
        commands.idx++;

      // Clamp the index
      if (gsl::narrow_cast <int> (commands.idx) < 0)
        commands.idx = 0;
      else if (commands.idx >= commands.history.size ())
        commands.idx = commands.history.size () - 1;

      if (! commands.history.empty ())
      {
        strcpy (&text [1], commands.history [commands.idx].c_str ());
        command_issued = false;
      }
    }

    else if (visible && vkCode == VK_RETURN)
    {
      bool new_press = keys_ [vkCode] != 0x81;

      if (keyDown)
        keys_ [vkCode] = 0x81;
      else
        keys_ [vkCode] = 0x00;

      if (keyDown && ( (lParam & 0x40000000UL) == 0 || new_press ))
      {
        size_t len = strlen (text+1);
        // Don't process empty or pure whitespace command lines
        if (len > 0 && strspn (text+1, " ") != len)
        {
          SK_ICommandResult result =
            SK_GetCommandProcessor ()->ProcessCommandLine (text+1);

          if (result.getStatus ())
          {
            // Don't repeat the same command over and over
            if (commands.history.empty () ||
                commands.history.back  () != &text [1]) {
              commands.history.emplace_back (&text [1]);
            }

            commands.idx =
              commands.history.size ();

            text [1] = '\0';

            command_issued = true;
          }

          else
          {
            command_issued = false;
          }

          result_str = result.getWord () + std::string (" ")   +
                       result.getArgs () + std::string (":  ") +
                       result.getResult ();
        }
      }
    }

    else
    {
      if (! keyDown) keys_ [vkCode] = 0x00;

      if (keyDown)
      {
        // First trigger an event if this is a make/break
        if (SK_ImGui_ProcessKeyPress (vkCode))
          return 0;

        // If not, use the key press for text input
        //

        // Don't print the tab character, it's pretty useless.
        if ( visible && vkCode != VK_TAB )
        {
          keys_ [VK_CAPITAL] = SK_GetKeyState (VK_CAPITAL) & 0xFFUL;

          unsigned char key_str [2];
                        key_str [1] = '\0';

          SK_TLS* pTLS =
            SK_TLS_Bottom ();

          static HKL
            keyboard_layout =
              SK_Input_GetKeyboardLayout ();

          if (pTLS != nullptr)
          {
            auto& language =
              pTLS->input_core->input_language;

            language.update ();

            keyboard_layout =
              language.keybd_layout;
          }

          if (1 == ToAsciiEx ( vkCode,
                                scanCode,
                                keys_,
   reinterpret_cast <LPWORD> (key_str),
                                0x04,
                                keyboard_layout ) &&
               isprint ( *key_str ) )
          {
            strncat (text, reinterpret_cast <char *> (key_str), 1);
            command_issued = false;
          }
        }
      }
    }
  }

  if (visible)
    return 0;

  return 1;
}

int
SK_Console::KeyUp (BYTE vkCode, LPARAM lParam)
{
  return SK_HandleConsoleKey (false, vkCode, lParam);
}

int
SK_Console::KeyDown (BYTE vkCode, LPARAM lParam)
{
  return SK_HandleConsoleKey (true, vkCode, lParam);
}

void
SK_DrawConsole (void)
{
  SK_Console::getInstance ()->Draw ();
}

BOOL
__stdcall
SK_IsConsoleVisible (void)
{
  return
    SK_Console::getInstance ()->isVisible ();
}

std::unique_ptr <SK_Console> SK_Console::pConsole = nullptr;