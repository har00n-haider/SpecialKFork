#include <SpecialK/steam_api.h>
#include <SpecialK/config.h>
#include <SpecialK/utility.h>

#include <Windows.h>
#include <cstdint>

#include <unordered_map>

class ISteamClient;
class IWrapSteamClient;

class ISteamUser;
class IWrapSteamUser;

#include <concurrent_unordered_map.h>
concurrency::concurrent_unordered_map <ISteamUser*, IWrapSteamUser*>   SK_SteamWrapper_remap_user;

int __SK_SteamUser_BLoggedOn =
  static_cast <int> (SK_SteamUser_LoggedOn_e::Unknown);

class IWrapSteamUser : public ISteamUser
{
public:
  IWrapSteamUser (ISteamUser* pUser) :
                   pRealUser (pUser) {
  };

  virtual HSteamUser GetHSteamUser          ( void ) override { return pRealUser->GetHSteamUser (); };


  virtual bool       BLoggedOn              ( void ) override
  {
    __SK_SteamUser_BLoggedOn =
      static_cast <int> ( pRealUser->BLoggedOn () ? SK_SteamUser_LoggedOn_e::Online :
                                                    SK_SteamUser_LoggedOn_e::Offline );

    if (config.steam.spoof_BLoggedOn)
    {
      if (__SK_SteamUser_BLoggedOn != static_cast <int> (SK_SteamUser_LoggedOn_e::Online))
        __SK_SteamUser_BLoggedOn   |= static_cast <int> (SK_SteamUser_LoggedOn_e::Spoofing);

      return true;
    }

    return (__SK_SteamUser_BLoggedOn & static_cast <int> (SK_SteamUser_LoggedOn_e::Online)) != 0;
  };


  virtual CSteamID   GetSteamID             ( void ) override { return pRealUser->GetSteamID    (); };

  virtual int        InitiateGameConnection ( void     *pAuthBlob,
                                              int       cbMaxAuthBlob,
                                              CSteamID  steamIDGameServer,
                                              uint32    unIPServer,
                                              uint16    usPortServer,
                                              bool      bSecure )                        override
  {
    return pRealUser->InitiateGameConnection ( pAuthBlob,
                                                 cbMaxAuthBlob, 
                                                   steamIDGameServer, 
                                                     unIPServer, 
                                                       usPortServer, 
                                                         bSecure );
  };
  virtual void         TerminateGameConnection( uint32 unIPServer, uint16 usPortServer ) override
  {
    return pRealUser->TerminateGameConnection ( unIPServer,
                                                  usPortServer );
  };
  virtual void         TrackAppUsageEvent    (       CGameID  gameID,
                                                     int      eAppUsageEvent,
                                               const char    *pchExtraInfo = "" )        override
  {
    return pRealUser->TrackAppUsageEvent    ( gameID,
                                                eAppUsageEvent,
                                                  pchExtraInfo );
  };
  virtual bool         GetUserDataFolder     ( char *pchBuffer,
                                               int   cubBuffer ) override
  {
    return pRealUser->GetUserDataFolder     ( pchBuffer,
                                                cubBuffer );
  };

  virtual void         StartVoiceRecording   (void) override { return pRealUser->StartVoiceRecording (); };
  virtual void         StopVoiceRecording    (void) override { return pRealUser->StopVoiceRecording  (); };

  virtual EVoiceResult GetAvailableVoice     ( uint32 *pcbCompressed,
                                               uint32 *pcbUncompressed,
                                               uint32  nUncompressedVoiceDesiredSampleRate ) override
  {
    return pRealUser->GetAvailableVoice     ( pcbCompressed,
                                                pcbUncompressed,
                                                  nUncompressedVoiceDesiredSampleRate );
  }
  virtual EVoiceResult GetVoice              ( bool    bWantCompressed,
                                               void   *pDestBuffer,
                                               uint32  cbDestBufferSize,
                                               uint32 *nBytesWritten,
                                               bool    bWantUncompressed,
                                               void   *pUncompressedDestBuffer,
                                               uint32  cbUncompressedDestBufferSize,
                                               uint32 *nUncompressBytesWritten,
                                               uint32  nUncompressedVoiceDesiredSampleRate ) override
  {
    return pRealUser->GetVoice              ( bWantCompressed,
                                                pDestBuffer,
                                                  cbDestBufferSize,
                                                    nBytesWritten,
                                                      bWantUncompressed,
                                                        pUncompressedDestBuffer,
                                                          cbUncompressedDestBufferSize,
                                                            nUncompressBytesWritten,
                                                              nUncompressedVoiceDesiredSampleRate );
  }
  virtual EVoiceResult DecompressVoice       ( const void    *pCompressed, 
                                                     uint32   cbCompressed, 
                                                     void    *pDestBuffer, 
                                                     uint32   cbDestBufferSize, 
                                                     uint32  *nBytesWritten, 
                                                     uint32   nDesiredSampleRate ) override
  {
    return pRealUser->DecompressVoice       ( pCompressed,
                                                cbCompressed,
                                                  pDestBuffer,
                                                    cbDestBufferSize,
                                                      nBytesWritten,
                                                        nDesiredSampleRate );
  }
  virtual uint32       GetVoiceOptimalSampleRate (void) override { return pRealUser->GetVoiceOptimalSampleRate (); }

  virtual HAuthTicket  GetAuthSessionTicket      ( void   *pTicket,
                                                   int     cbMaxTicket,
                                                   uint32 *pcbTicket )                          override
  {
    return pRealUser->GetAuthSessionTicket  ( pTicket,
                                                cbMaxTicket,
                                                  pcbTicket );
  }
  virtual EBeginAuthSessionResult     BeginAuthSession     ( const void     *pAuthTicket,
                                                                   int       cbAuthTicket,
                                                                   CSteamID  steamID )          override
  {
    return pRealUser->BeginAuthSession      ( pAuthTicket,
                                                cbAuthTicket,
                                                  steamID );
  }
  virtual void                        EndAuthSession       (CSteamID steamID)                   override
  {
    return pRealUser->EndAuthSession        (steamID);
  }
  virtual void                        CancelAuthTicket     (HAuthTicket hAuthTicket)            override
  {
    return pRealUser->CancelAuthTicket      (hAuthTicket);
  }
  virtual EUserHasLicenseForAppResult UserHasLicenseForApp ( CSteamID steamID,
                                                             AppId_t  appID )                   override
  {
    return pRealUser->UserHasLicenseForApp  ( steamID,
                                                appID );
  }
  virtual bool                        BIsBehindNAT         (void) override { return pRealUser->BIsBehindNAT (); }

  virtual void                        AdvertiseGame        ( CSteamID steamIDGameServer,
                                                             uint32   unIPServer,
                                                             uint16   usPortServer )            override
  {
    return pRealUser->AdvertiseGame         ( steamIDGameServer,
                                                unIPServer,
                                                  usPortServer );
  }
  virtual SteamAPICall_t              RequestEncryptedAppTicket ( void *pDataToInclude,
                                                                  int   cbDataToInclude )       override
  {
    return pRealUser->RequestEncryptedAppTicket ( pDataToInclude,
                                                    cbDataToInclude );
  }
  virtual bool                        GetEncryptedAppTicket     ( void   *pTicket,
                                                                  int     cbMaxTicket,
                                                                  uint32 *pcbTicket )           override
  {
    return pRealUser->GetEncryptedAppTicket ( pTicket,
                                                cbMaxTicket,
                                                  pcbTicket );
  }
  virtual int                         GetGameBadgeLevel         ( int  nSeries,
                                                                  bool bFoil )                  override
  {
    return pRealUser->GetGameBadgeLevel     ( nSeries,
                                                bFoil );
  }
  virtual int                         GetPlayerSteamLevel       (void)                          override
  {
    return pRealUser->GetPlayerSteamLevel   ();
  };
  virtual SteamAPICall_t              RequestStoreAuthURL       ( const char *pchRedirectURL )  override
  {
    return pRealUser->RequestStoreAuthURL   (pchRedirectURL);
  };


  // 019
  //
  virtual bool                        BIsPhoneVerified          (void)                          override
  {
    return pRealUser->BIsPhoneVerified              ();
  }
  virtual bool                        BIsTwoFactorEnabled       (void)                          override
  {
    return pRealUser->BIsTwoFactorEnabled           ();
  }
  virtual bool                        BIsPhoneIdentifying       (void)                          override
  {
    return pRealUser->BIsPhoneIdentifying           ();
  }
  virtual bool                        BIsPhoneRequiringVerification (void)                      override
  {
    return pRealUser->BIsPhoneRequiringVerification ();
  }

private:
  ISteamUser* pRealUser;
};



using SteamAPI_ISteamClient_GetISteamUser_pfn = ISteamUser* (S_CALLTYPE *)(
  ISteamClient *This,
  HSteamUser    hSteamUser,
  HSteamPipe    hSteamPipe,
  const char   *pchVersion
  );
SteamAPI_ISteamClient_GetISteamUser_pfn   SteamAPI_ISteamClient_GetISteamUser_Original = nullptr;

ISteamUser*
S_CALLTYPE
SteamAPI_ISteamClient_GetISteamUser_Detour (ISteamClient *This,
                                            HSteamUser    hSteamUser,
                                            HSteamPipe    hSteamPipe,
                                            const char   *pchVersion)
{
  SK_RunOnce (
    steam_log.Log ( L"[!] %hs (..., %hs)",
                      __FUNCTION__, pchVersion )
  );

  ISteamUser* pUser =
    SteamAPI_ISteamClient_GetISteamUser_Original ( This,
                                                     hSteamUser,
                                                       hSteamPipe,
                                                         pchVersion );

  if (pUser != nullptr)
  {
    if ((! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_018)) ||
        (! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_019)) ||
        (! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_017)))
    {
      if (SK_SteamWrapper_remap_user.count (pUser))
         return SK_SteamWrapper_remap_user [pUser];

      else
      {
        SK_SteamWrapper_remap_user [pUser] =
                new IWrapSteamUser (pUser);

        return SK_SteamWrapper_remap_user [pUser];
      }
    }

    else
    {
      SK_RunOnce (
        steam_log.Log ( L"Game requested unexpected interface version (%hs)!",
                          pchVersion )
      );

      return pUser;
    }
  }

  return nullptr;
}


ISteamUser*
SK_SteamWrapper_WrappedClient_GetISteamUser ( ISteamClient *This,
                                              HSteamUser    hSteamUser,
                                              HSteamPipe    hSteamPipe,
                                              const char   *pchVersion )
{
  SK_RunOnce (
    steam_log.Log ( L"[!] %hs (..., %hs)",
                      __FUNCTION__, pchVersion )
  );

  ISteamUser* pUser =
    This->GetISteamUser ( hSteamUser,
                            hSteamPipe,
                              pchVersion );

  if (pUser != nullptr)
  {
    if ((! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_018)) ||
        (! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_019)) ||
        (! lstrcmpA (pchVersion, STEAMUSER_INTERFACE_VERSION_017)))
    {
      if (SK_SteamWrapper_remap_user.count (pUser))
         return SK_SteamWrapper_remap_user [pUser];

      else
      {
        SK_SteamWrapper_remap_user [pUser] =
                new IWrapSteamUser (pUser);

        return SK_SteamWrapper_remap_user [pUser];
      }
    }

    else
    {
      SK_RunOnce (
        steam_log.Log ( L"Game requested unexpected interface version (%hs)!",
                          pchVersion )
      );

      return pUser;
    }
  }

  return nullptr;
}

using SteamUser_pfn = ISteamUser* (S_CALLTYPE *)(
        void
      );
SteamUser_pfn SteamUser_Original = nullptr;

ISteamUser*
S_CALLTYPE
SteamUser_Detour (void)
{
  SK_RunOnce (
    steam_log.Log ( L"[!] %hs ()",
                      __FUNCTION__ )
  );

  ISteamUser* pUser =
    static_cast <ISteamUser *> ( SteamUser_Original () );

  if (pUser != nullptr)
  {
    if (SK_SteamWrapper_remap_user.count (pUser))
       return SK_SteamWrapper_remap_user [pUser];

    else
    {
      SK_SteamWrapper_remap_user [pUser] =
              new IWrapSteamUser (pUser);

      return SK_SteamWrapper_remap_user [pUser];
    }
  }

  return nullptr;
}