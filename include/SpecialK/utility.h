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

#ifndef __SK__UTILITY_H__
#define __SK__UTILITY_H__

#include <intrin.h>
#include <Windows.h>

#include <cstdint>
#include <string>
#include <mutex>

#include <SpecialK/sha1.h>

interface iSK_INI;

using HANDLE = void *;

template <typename T, typename T2, typename Q>
  __inline
  T
    static_const_cast ( const typename Q q )
    {
      return static_cast <T>  (
               const_cast <T2>  ( q )
                              );
    };

template <typename T, typename Q>
  __inline
  T**
    static_cast_p2p ( typename Q** p2p )
    {
      return static_cast <T **> (
               static_cast <T*>   ( p2p )
                                );
    };


enum SK_UNITS {
  Celsius    = 0,
  Fahrenheit = 1,
  B          = 2,
  KiB        = 3,
  MiB        = 4,
  GiB        = 5,
  Auto       = MAXDWORD
};


const wchar_t* __stdcall
               SK_GetRootPath               (void);
const wchar_t* SK_GetHostApp                (void);
const wchar_t* SK_GetHostPath               (void);
const wchar_t* SK_GetBlacklistFilename      (void);

bool           SK_GetDocumentsDir           (_Out_opt_ wchar_t* buf, _Inout_ uint32_t* pdwLen);
std::wstring   SK_GetDocumentsDir           (void);
std::wstring   SK_GetFontsDir               (void);
std::wstring   SK_GetRTSSInstallDir         (void);
bool
__stdcall      SK_CreateDirectories         (const wchar_t* wszPath);
size_t         SK_DeleteTemporaryFiles      (const wchar_t* wszPath    = SK_GetHostPath (),
                                             const wchar_t* wszPattern = L"SKI*.tmp");
std::wstring   SK_EvalEnvironmentVars       (const wchar_t* wszEvaluateMe);
bool           SK_GetUserProfileDir         (wchar_t*       buf, uint32_t* pdwLen);
bool           SK_IsTrue                    (const wchar_t* string);
bool           SK_IsAdmin                   (void);
void           SK_ElevateToAdmin            (void); // Needs DOS 8.3 filename support
void           SK_RestartGame               (const wchar_t* wszDLL = nullptr);
int            SK_MessageBox                (std::wstring caption,
                                             std::wstring title,
                                             uint32_t     flags);

std::string    SK_WideCharToUTF8            (std::wstring in);
std::wstring   SK_UTF8ToWideChar            (std::string  in);

std::string
__cdecl        SK_FormatString              (char    const* const _Format, ...);
std::wstring
__cdecl        SK_FormatStringW             (wchar_t const* const _Format, ...);

void           SK_StripTrailingSlashesW     (wchar_t* wszInOut);
void           SK_FixSlashesW               (wchar_t* wszInOut);
void           SK_StripTrailingSlashesA     (char*     szInOut);
void           SK_FixSlashesA               (char*     szInOut);

void           SK_SetNormalFileAttribs      (std::wstring   file);
void           SK_MoveFileNoFail            (const wchar_t* wszOld,    const wchar_t* wszNew);
void           SK_FullCopy                  (std::wstring   from,      std::wstring   to);
BOOL           SK_File_SetAttribs           (std::wstring   file,      DWORD          dwAttribs);
BOOL           SK_File_ApplyAttribMask      (std::wstring   file,      DWORD          dwAttribMask,
                                             bool           clear = false);
BOOL           SK_File_SetHidden            (std::wstring   file,      bool           hidden);
BOOL           SK_File_SetTemporary         (std::wstring   file,      bool           temp);
std::wstring   SK_File_SizeToString         (uint64_t       size,      SK_UNITS       unit = Auto);
std::wstring   SK_File_SizeToStringF        (uint64_t       size,      int            width,
                                             int            precision, SK_UNITS       unit = Auto);
std::wstring   SK_SYS_GetInstallPath        (void);

const wchar_t* SK_GetHostApp                (void);
const wchar_t* SK_GetSystemDirectory        (void);
iSK_INI*       SK_GetDLLConfig              (void);

#pragma intrinsic (_ReturnAddress)

HMODULE        SK_GetCallingDLL             (LPCVOID pReturn = _ReturnAddress ());
std::wstring   SK_GetCallerName             (LPCVOID pReturn = _ReturnAddress ());
HMODULE        SK_GetModuleFromAddr         (LPCVOID addr);
std::wstring   SK_GetModuleName             (HMODULE hDll);
std::wstring   SK_GetModuleFullName         (HMODULE hDll);
std::wstring   SK_GetModuleNameFromAddr     (LPCVOID addr);
std::wstring   SK_GetModuleFullNameFromAddr (LPCVOID addr);
std::wstring   SK_MakePrettyAddress         (LPCVOID addr, DWORD dwFlags = 0x0);
bool           SK_ValidatePointer           (LPCVOID addr);

bool           SK_StripUserNameFromPathA    (   char*  szInOut);
bool           SK_StripUserNameFromPathW    (wchar_t* wszInOut);

LPVOID         SK_GetProcAddress            (const wchar_t* wszModule, const char* szFunc);


HMODULE __stdcall
               SK_GetDLL                    (void);
std::wstring
        __stdcall
               SK_GetDLLVersionStr          (const wchar_t* wszName);

const wchar_t*
        __stdcall
               SK_GetCanonicalDLLForRole    (enum DLL_ROLE role);


constexpr uint8_t
__stdcall
SK_GetBitness (void)
{
#ifdef _WIN64
  return 64;
#endif
  return 32;
}

#define SK_RunOnce(x)    { static bool first = true; if (first) { (x); first = false; } }

#define SK_RunIf32Bit(x)         { SK_GetBitness () == 32  ? (x) :  0; }
#define SK_RunIf64Bit(x)         { SK_GetBitness () == 64  ? (x) :  0; }
#define SK_RunLHIfBitness(b,l,r)   SK_GetBitness () == (b) ? (l) : (r)

#include <queue>

std::queue <DWORD>
               SK_SuspendAllOtherThreads (void);
void
               SK_ResumeThreads          (std::queue <DWORD> threads);


bool __cdecl   SK_IsRunDLLInvocation     (void);
bool __cdecl   SK_IsSuperSpecialK        (void);

// TODO: Push the SK_GetHostApp (...) stuff into this class
class SK_HostAppUtil
{
public:
               SK_HostAppUtil            (void);

  bool         isInjectionTool           (void)
  {
    return SKIM || (RunDll32 && SK_IsRunDLLInvocation ());
  }


protected:
  bool        SKIM     = false;
  bool        RunDll32 = false;
} extern SK_HostApp;

bool __stdcall SK_IsDLLSpecialK          (const wchar_t* wszName);
void __stdcall SK_SelfDestruct           (void);



struct sk_import_test_s {
  const char* szModuleName;
  bool        used;
};

void __stdcall SK_TestImports          (HMODULE hMod, sk_import_test_s* pTests, int nCount);

//
// This prototype is now completely ridiculous, this "design" sucks...
//   FIXME!!
// 
void
SK_TestRenderImports ( HMODULE hMod,
                       bool*   gl,
                       bool*   vulkan,
                       bool*   d3d9,
                       bool*   dxgi,
                       bool*   d3d11,
                       bool*   d3d8,
                       bool*   ddraw,
                       bool*   glide );

void
__stdcall
SK_wcsrep ( const wchar_t*   wszIn,
                  wchar_t** pwszOut,
            const wchar_t*   wszOld,
            const wchar_t*   wszNew );

using SK_HashProgressCallback_pfn = void (__stdcall *)(uint64_t current, uint64_t total);

uint64_t     __stdcall SK_GetFileSize     ( const wchar_t* wszFile );
uint32_t     __stdcall SK_GetFileCRC32    ( const wchar_t* wszFile,
                             SK_HashProgressCallback_pfn callback = nullptr );
uint32_t     __stdcall SK_GetFileCRC32C   ( const wchar_t* wszFile,
                             SK_HashProgressCallback_pfn callback = nullptr );

SK_SHA1_Hash __stdcall SK_File_GetSHA1     ( const wchar_t* wszFile,
                             SK_HashProgressCallback_pfn callback = nullptr );
bool         __stdcall SK_File_GetSHA1StrA ( const char* szFile,
                                                   char*  szOut,
                             SK_HashProgressCallback_pfn callback = nullptr );
bool         __stdcall SK_File_GetSHA1StrW ( const wchar_t* wszFile,
                                                   wchar_t* wszOut,
                             SK_HashProgressCallback_pfn callback = nullptr );


const wchar_t*
SK_Path_wcsrchr (const wchar_t* wszStr, wchar_t wchr);

const wchar_t*
SK_Path_wcsstr (const wchar_t* wszStr, const wchar_t* wszSubStr);

int
SK_Path_wcsicmp (const wchar_t* wszStr1, const wchar_t* wszStr2);

size_t
SK_RemoveTrailingDecimalZeros (wchar_t* wszNum, size_t bufSize = 0);

size_t
SK_RemoveTrailingDecimalZeros (char* szNum, size_t bufSize = 0);


void*
__stdcall
SK_Scan         (const void* pattern, size_t len, const void* mask);

void*
__stdcall
SK_ScanAligned (const void* pattern, size_t len, const void* mask, int align = 1);

void*
__stdcall
SK_ScanAlignedEx (const void* pattern, size_t len, const void* mask, void* after = nullptr, int align = 1);

BOOL
__stdcall
SK_InjectMemory ( LPVOID  base_addr,
                  void   *new_data,
                  size_t  data_size,
                  DWORD   permissions,
                  void   *old_data     = nullptr );

bool
SK_IsProcessRunning (const wchar_t* wszProcName);

class SK_Thread_CriticalSection
{
public:
  SK_Thread_CriticalSection ( CRITICAL_SECTION* pCS )
  {
    cs_ = pCS;
  };

  ~SK_Thread_CriticalSection (void) = default;

  void lock (void) {
    EnterCriticalSection (cs_);
  }

  void unlock (void)
  {
    LeaveCriticalSection (cs_);
  }

protected:
  CRITICAL_SECTION* cs_;
};

class SK_Thread_HybridSpinlock : public SK_Thread_CriticalSection
{
public:
  SK_Thread_HybridSpinlock (int spin_count = 3000) :
                                                     SK_Thread_CriticalSection (new CRITICAL_SECTION)
  {
    InitializeCriticalSectionAndSpinCount (cs_, spin_count);
  }

  ~SK_Thread_HybridSpinlock (void)
  {
    DeleteCriticalSection (cs_);
    delete cs_;
  }
};

class SK_AutoCriticalSection {
public:
  SK_AutoCriticalSection ( CRITICAL_SECTION* pCS,
                           bool              try_only = false )
  {
    acquired_ = false;
    cs_       = pCS;

    if (try_only)
      TryEnter ();
    else {
      Enter ();
    }
  }

  ~SK_AutoCriticalSection (void)
  {
    Leave ();
  }

  bool try_result (void)
  {
    return acquired_;
  }

  void enter (void)
  {
    EnterCriticalSection (this->cs_);

    acquired_ = true;
  }

protected:
  bool TryEnter (_Acquires_lock_(* this->cs_) void)
  {
    return (acquired_ = (TryEnterCriticalSection (cs_) != FALSE));
  }

  void Enter (_Acquires_lock_(* this->cs_) void)
  {
    EnterCriticalSection (cs_);

    acquired_ = true;
  }

  void Leave (_Releases_lock_(* this->cs_) void)
  {
    if (acquired_ != false)
      LeaveCriticalSection (cs_);

    acquired_ = false;
  }

private:
  bool              acquired_;
  CRITICAL_SECTION* cs_;
};


extern "C" uint32_t __cdecl crc32       (uint32_t crc, const void *buf, size_t size);

// Returns the value of crc if attempting to checksum this memory throws an access violation exception
           uint32_t __cdecl safe_crc32c (uint32_t crc, const void *buf, size_t size);

/*
    Computes CRC-32C (Castagnoli) checksum. Uses Intel's CRC32 instruction if it is available.
    Otherwise it uses a very fast software fallback.
*/
extern "C"
uint32_t
__cdecl
crc32c (
    uint32_t    crc,            // Initial CRC value. Typically it's 0.
                                // You can supply non-trivial initial value here.
                                // Initial value can be used to chain CRC from multiple buffers.
    const void *input,          // Data to be put through the CRC algorithm.
    size_t      length);        // Length of the data in the input buffer.


extern "C" void __cdecl __crc32_init (void);

/*
	Software fallback version of CRC-32C (Castagnoli) checksum.
*/
extern "C"
uint32_t
__cdecl
crc32c_append_sw (uint32_t crc, const void *input, size_t length);

/*
	Hardware version of CRC-32C (Castagnoli) checksum. Will fail, if CPU does not support related instructions. Use a crc32c_append version instead of.
*/
extern "C"
uint32_t
__cdecl
crc32c_append_hw (uint32_t crc, const void *input, size_t length);

/*
	Checks is hardware version of CRC-32C is available.
*/
extern "C"
int
__cdecl
crc32c_hw_available (void);


#endif /* __SK__UTILITY_H__ */