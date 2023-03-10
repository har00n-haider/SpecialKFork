#include <SpecialK/steam_api.h>
#include <SpecialK/input/steam.h>
#include <SpecialK/input/input.h>

sk_input_api_context_s SK_Steam_Backend;

#include <concurrent_unordered_map.h>
concurrency::concurrent_unordered_map <ISteamController*, IWrapSteamController*>  SK_SteamWrapper_remap_controller;

using SteamAPI_ISteamClient_GetISteamController_pfn = ISteamController* (S_CALLTYPE *)(
  ISteamClient *This,
  HSteamUser    hSteamUser,
  HSteamPipe    hSteamPipe,
  const char   *pchVersion
  );
SteamAPI_ISteamClient_GetISteamController_pfn   SteamAPI_ISteamClient_GetISteamController_Original = nullptr;

ISteamController*
S_CALLTYPE
SteamAPI_ISteamClient_GetISteamController_Detour ( ISteamClient *This,
                                                   HSteamUser    hSteamUser,
                                                   HSteamPipe    hSteamPipe,
                                                   const char   *pchVersion )
{
  //SK_RunOnce (
    steam_log.Log ( L"[!] %hs (..., %hs)",
                      __FUNCTION__, pchVersion );
  //);

  ISteamController* pController =
    SteamAPI_ISteamClient_GetISteamController_Original ( This,
                                                           hSteamUser,
                                                             hSteamPipe,
                                                               pchVersion );

  if (pController != nullptr)
  {
    if (! lstrcmpA (pchVersion, STEAMCONTROLLER_INTERFACE_VERSION))
    {
      if (SK_SteamWrapper_remap_controller.count (pController) && SK_SteamWrapper_remap_controller [pController] != nullptr)
         return SK_SteamWrapper_remap_controller [pController];

      else
      {
        SK_SteamWrapper_remap_controller [pController] =
                new IWrapSteamController (pController);

        return SK_SteamWrapper_remap_controller [pController];
      }
    }

    else
    {
      SK_RunOnce (
        steam_log.Log ( L"Game requested unexpected interface version (%hs)!",
                          pchVersion )
      );

      return pController;
    }
  }

  return nullptr;
}

ISteamController*
SK_SteamWrapper_WrappedClient_GetISteamController ( ISteamClient *pRealClient,
                                                    HSteamUser    hSteamUser,
                                                    HSteamPipe    hSteamPipe,
                                                    const char   *pchVersion )
{
  steam_log.Log (L"[Steam Wrap] [!] GetISteamController (%hs)", pchVersion);

  //if (! lstrcmpA (pchVersion, "SteamController"))
  //{
  //  ISteamController0* pController =
  //    reinterpret_cast <ISteamController0 *> ( pRealClient->GetISteamController (hSteamUser, hSteamPipe, pchVersion) );
  //
  //  if (pController != nullptr)
  //  {
  //    if (SK_SteamWrapper_remap_controller0.count (pController))
  //       return dynamic_cast <ISteamController *> (SK_SteamWrapper_remap_controller0 [pController]);
  //
  //    else
  //    {
  //      SK_SteamWrapper_remap_controller0 [pController] =
  //              new ISteamControllerFake0 (pController);
  //
  //      return dynamic_cast <ISteamController *> (SK_SteamWrapper_remap_controller0 [pController]);
  //    }
  //  }
  //}

  if (! lstrcmpA (pchVersion, "SteamController005"))
  {
    if (SK_IsInjected ())
    {
      static volatile LONG __init = FALSE;

      if (! InterlockedCompareExchange (&__init, 1, 0))
      {
        void** vftable = *(void***)*(&pRealClient);

        SK_CreateVFTableHook (             L"ISteamClient017_GetISteamController",
                                 vftable, 25,
                                 SteamAPI_ISteamClient_GetISteamController_Detour,
        static_cast_p2p <void> (&SteamAPI_ISteamClient_GetISteamController_Original) );

        SK_EnableHook (vftable [25]);
      }
    }

    else
    {
      auto *pController =
        reinterpret_cast <ISteamController *> ( pRealClient->GetISteamController (hSteamUser, hSteamPipe, pchVersion) );
  
      if (pController != nullptr)
      {
        if (SK_SteamWrapper_remap_controller.count (pController) && SK_SteamWrapper_remap_controller [pController] != nullptr)
          return dynamic_cast <ISteamController *> ( SK_SteamWrapper_remap_controller [pController] );
  
        else
        {
          SK_SteamWrapper_remap_controller [pController] =
            new IWrapSteamController (pController);
  
          return dynamic_cast <ISteamController *> ( SK_SteamWrapper_remap_controller [pController] );
        }
      }
    }
  }

  return pRealClient->GetISteamController (hSteamUser, hSteamPipe, pchVersion);
}