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

#ifndef __SK__FRAMERATE_H__
#define __SK__FRAMERATE_H__

struct IUnknown;

#include <boost/sort/pdqsort/pdqsort.hpp>

#include <Unknwnbase.h>
#include <Windows.h>

#include <cstdint>
#include <cmath>
#include <forward_list>

template <class T, class U>
constexpr T narrow_cast(U&& u)
{
  return static_cast<T>(std::forward<U>(u));
}

static constexpr auto
  long_double_cast =
  [](auto val) ->
    long double
    {
      return
        static_cast <long double> (val);
    };

static constexpr auto
  _isreal =
  [](long double float_val) ->
    bool
    {
      return
        (! (isinf (float_val) || isnan (float_val)));
    };

extern float SK_Framerate_GetPercentileByIdx (int idx);

using QueryPerformanceCounter_pfn = BOOL (WINAPI *)(_Out_ LARGE_INTEGER *lpPerformanceCount);

BOOL
WINAPI
SK_QueryPerformanceCounter (_Out_ LARGE_INTEGER *lpPerformanceCount);

extern LARGE_INTEGER SK_GetPerfFreq (void);
extern LARGE_INTEGER SK_QueryPerf   (void);

__forceinline
LARGE_INTEGER
SK_CurrentPerf (void)
 {
   LARGE_INTEGER                time;
   SK_QueryPerformanceCounter (&time);
   return                       time;
 };

static auto SK_DeltaPerf =
 [](auto delta, auto freq)->
  LARGE_INTEGER
   {
     LARGE_INTEGER time = SK_CurrentPerf ();

     time.QuadPart -= gsl::narrow_cast <LONGLONG> (delta * freq);

     return time;
   };

static auto SK_DeltaPerfMS =
 [](auto delta, auto freq)->
  double
   {
     return
       1000.0 * (double)(SK_DeltaPerf (delta, freq).QuadPart) /
                (double)SK_GetPerfFreq           ().QuadPart;
   };


extern __forceinline
ULONG  __stdcall
SK_GetFramesDrawn (void);


namespace SK
{
  namespace Framerate
  {
    void Init     (void);
    void Shutdown (void);

    void Tick     (long double& dt, LARGE_INTEGER& now);

    class Limiter {
    public:
      Limiter (long double target = 60.0l);

      ~Limiter (void) = default;

      void            init            (long double target);
      void            wait            (void);
      bool        try_wait            (void); // No actual wait, just return
                                              //  whether a wait would have occurred.

      void        set_limit           (long double target);
      long double get_limit           (void) noexcept { return fps; };

      long double effective_frametime (void);

      int32_t     suspend             (void) noexcept { return ++limit_behavior; }
      int32_t     resume              (void) noexcept { return --limit_behavior; }

      void        reset (bool full = false) noexcept {
        if (full) full_restart = true;
        else           restart = true;
      }

    private:
      bool          restart      = false;
      bool          full_restart = false;
      bool          background   = false;

      long double   ms           = 0.0L,
                    fps          = 0.0L,
                    effective_ms = 0.0L;

      ULONGLONG     ticks_per_frame = 0ULL;

      volatile
        LONG64      time   = { },
                    start  = { },
                    next   = { },
                    last   = { },
                    freq   = { };

      volatile
          LONG      frames = 0;

#define LIMIT_APPLY     0
#define LIMIT_UNDEFLOW  (limit_behavvior < 0)
#define LIMIT_SUSPENDED (limit_behavivor > 0)

      // 0 = Limiter runs, < 0 = Reference Counting Bug (dumbass)
      //                   > 0 = Temporarily Ignore Limits
       int32_t      limit_behavior =
                    LIMIT_APPLY;
    };

    using EventCounter = class EventCounter_V1;

    class EventCounter_V1
    {
    public:
      class SleepStats
      {
      public:
        volatile ULONG attempts   = 0UL,
                       rejections = 0UL;

        struct
        {
          volatile LONG deprived = 0ULL,
                        allowed  = 0ULL;
        } time;


        void sleep (DWORD dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedAdd       (&time.allowed,  narrow_cast <ULONG> (dwMilliseconds)); }
        void wake  (DWORD dwMilliseconds) { InterlockedIncrement (&attempts);
                                            InterlockedIncrement (&rejections);
                                            InterlockedAdd       (&time.deprived, narrow_cast <ULONG> (dwMilliseconds)); }
      };

      SleepStats& getMessagePumpStats  (void) noexcept { return message_pump;  }
      SleepStats& getRenderThreadStats (void) noexcept { return render_thread; }
      SleepStats& getMicroStats        (void) noexcept { return micro_sleep;   }
      SleepStats& getMacroStats        (void) noexcept { return macro_sleep;   }

    protected:
      SleepStats message_pump, render_thread,
                 micro_sleep,  macro_sleep;
    } extern *events;


    static inline EventCounter* GetEvents  (void) noexcept { return events; }
                  Limiter*      GetLimiter (void);


    #define MAX_SAMPLES 1000

    class Stats;

    struct DeepFrameState {
      SK::Framerate::Stats* mean;
      SK::Framerate::Stats* min;
      SK::Framerate::Stats* max;
      SK::Framerate::Stats* percentile0;
      SK::Framerate::Stats* percentile1;

      void reset (void);
    } extern *frame_history_snapshots;

    std::pair <
      ULONG,  // Frame this container pertains to
      std::vector <long double> // Sorted data
              > extern _sorted_frame_history;

    class Stats {
    public:
      static LARGE_INTEGER freq;

      Stats (void) noexcept {
        QueryPerformanceFrequency (&freq);
      }
      struct sample_t {
        long double   val  = 0.0;
        LARGE_INTEGER when = { 0ULL };
      } data [MAX_SAMPLES];

      int64_t samples      = 0;

      bool addSample (long double sample, LARGE_INTEGER time) noexcept
      {
        data [samples % MAX_SAMPLES].val  = sample;
        data [samples % MAX_SAMPLES].when = time;


        return
          ( ( samples++ % MAX_SAMPLES ) == 0 &&
              samples - 1               != 0 );
      }

      long double calcDataTimespan (void)
      {
        uint64_t samples_present =
          samples < MAX_SAMPLES ?
                        samples : MAX_SAMPLES;

        LARGE_INTEGER min_time;
                      min_time.QuadPart = std::numeric_limits <LONGLONG>::max ();
        LARGE_INTEGER max_time = {                   0ULL };

        for ( auto sample_idx = 0               ;
                   sample_idx < samples_present ;
                 ++sample_idx )
        {
          LARGE_INTEGER sampled_time =
            data [sample_idx].when;

          if ( sampled_time.QuadPart >
                   max_time.QuadPart && sampled_time.QuadPart < 0xFFFFFFFFFFFFFFFF )
          {
                max_time.QuadPart =
            sampled_time.QuadPart;
          }

          if ( sampled_time.QuadPart <
                   min_time.QuadPart && sampled_time.QuadPart > 0 )
          {
                min_time.QuadPart =
            sampled_time.QuadPart;
          }
        }

        return
          long_double_cast (max_time.QuadPart - min_time.QuadPart) /
          long_double_cast ( SK::Framerate::Stats::freq.QuadPart );
      }

            int calcNumSamples   (long double seconds = 1.0L);
    long double calcMean         (long double seconds = 1.0L);
    long double calcSqStdDev     (long double mean,
                                  long double seconds = 1.0L);
    long double calcMin          (long double seconds = 1.0L);
    long double calcMax          (long double seconds = 1.0L);
            int calcHitches      (long double tolerance,
                                  long double mean,
                                  long double seconds = 1.0L);

    long double calcOnePercentLow      (long double seconds = 1.0L);
    long double calcPointOnePercentLow (long double seconds = 1.0L);

      long double calcMean (LARGE_INTEGER start) noexcept
      {
        long double mean = 0.0L;

        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart > start.QuadPart)
          {
            if (_isreal (datum.val) && datum.val > 0.0L)
            {
              ++samples_used;
              mean += datum.val;
            }
          }
        }

        return
          ( mean / static_cast <long double> (samples_used) );
      }

      std::vector <long double>& sortAndCacheFrametimeHistory (void)
      {
#pragma warning (push)
#pragma warning (disable: 4244)
        if (_sorted_frame_history.first != SK_GetFramesDrawn ())
        {
          _sorted_frame_history.second.clear ();

          for ( const auto datum : data )
          {
            if (datum.when.QuadPart > 0)
            {
              if (_isreal (datum.val) && datum.val > 0.01L)
              {
                _sorted_frame_history.second.push_back (datum.val);
              }
            }
          }

          boost::sort::pdqsort (
            _sorted_frame_history.second.begin (),
            _sorted_frame_history.second.end   (), std::greater <> ()
          );

          _sorted_frame_history.first =
            SK_GetFramesDrawn ();
        }
#pragma warning (pop)

        return
          _sorted_frame_history.second;
      }

      long double calcPercentile (float percent, LARGE_INTEGER start) noexcept
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <long double>&
               sampled_lows = sortAndCacheFrametimeHistory ();
        size_t samples_used = sampled_lows.size            ();

        size_t end_sample_idx =
          std::max ( (size_t)0, std::min
              ( samples_used,
                  (size_t)std::round ((float)samples_used * (percent / 100.0f))
              )
          );

        const long double
            avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                  0.0L ) / (long double)(end_sample_idx);

        return
          avg_ms;
      }

      long double calcOnePercentLow (LARGE_INTEGER start) noexcept
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <long double>&
               sampled_lows = sortAndCacheFrametimeHistory ();
        size_t samples_used = sampled_lows.size            ();

        size_t end_sample_idx =
          (size_t)std::round ((float)samples_used / 100.0f);

        const long double
            one_percent_avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                  0.0L ) / (long double)(end_sample_idx);

        return
          one_percent_avg_ms;
      }

      long double calcPointOnePercentLow (LARGE_INTEGER start) noexcept
      {
        UNREFERENCED_PARAMETER (start);

        std::vector <long double>&
               sampled_lows = sortAndCacheFrametimeHistory ();
        size_t samples_used = sampled_lows.size            ();

        size_t end_sample_idx =
          (size_t)std::round ((float)samples_used / 1000.0f);

        const long double
            point_one_percent_avg_ms =
              std::accumulate ( sampled_lows.begin (),
                                sampled_lows.begin ()  + end_sample_idx,
                                  0.0L ) / (long double)(end_sample_idx);

        return
          point_one_percent_avg_ms;
      }

      long double calcSqStdDev (long double mean, LARGE_INTEGER start) noexcept
      {
        long double sd2  = 0.0L;
        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart > start.QuadPart)
          {
            if (_isreal (datum.val) && datum.val > 0.0L)
            {
              sd2 += (datum.val - mean) *
                     (datum.val - mean);
              samples_used++;
            }
          }
        }

        return sd2 / static_cast <long double> (samples_used);
      }

      long double calcMin (LARGE_INTEGER start) noexcept
      {
        long double min = INFINITY;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart > start.QuadPart)
          {
            if (_isreal (datum.val) && datum.val > 0.0L)
            {
              if (datum.val < min)
                min = datum.val;
            }
          }
        }

        return min;
      }

      long double calcMax (LARGE_INTEGER start) noexcept
      {
        long double max = -INFINITY;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart > start.QuadPart)
          {
            if (_isreal (datum.val) && datum.val > 0.0L)
            {
              if (datum.val > max)
                max = datum.val;
            }
          }
        }

        return max;
      }

      int calcHitches (long double tolerance, long double mean, LARGE_INTEGER start) noexcept
      {
        int  hitches   = 0;
        bool last_late = false;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart >= start.QuadPart)
          {
            if (datum.val > tolerance * mean)
            {
              if (! last_late)
                hitches++;
              last_late = true;
            }

            else
            {
              last_late = false;
            }
          }
        }

        return hitches;
      }

      int calcNumSamples (LARGE_INTEGER start) noexcept
      {
        int samples_used = 0;

        for ( const auto datum : data )
        {
          if (datum.when.QuadPart > start.QuadPart)
          {
            if (_isreal (datum.val) && datum.val > 0.0L)
            {
              samples_used++;
            }
          }
        }

        return samples_used;
      }
    };
  };
};

using  Sleep_pfn = void (WINAPI *)(DWORD dwMilliseconds);
extern Sleep_pfn
       Sleep_Original;

using  SleepEx_pfn = DWORD (WINAPI *)(DWORD dwMilliseconds,
                                      BOOL  bAlertable);
extern SleepEx_pfn
       SleepEx_Original;

extern int __SK_FramerateLimitApplicationSite;



using NtQueryTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  OUT PULONG              MinimumResolution,
  OUT PULONG              MaximumResolution,
  OUT PULONG              CurrentResolution
);

using NtSetTimerResolution_pfn = NTSTATUS (NTAPI *)
(
  IN  ULONG               DesiredResolution,
  IN  BOOLEAN             SetResolution,
  OUT PULONG              CurrentResolution
);

typedef NTSTATUS (WINAPI *NtDelayExecution_pfn)(
  IN  BOOLEAN        Alertable,
  IN  PLARGE_INTEGER DelayInterval
);


struct IDXGIOutput;

using WaitForVBlank_pfn = HRESULT (STDMETHODCALLTYPE *)(
  IDXGIOutput *This
);

typedef
NTSTATUS (NTAPI *NtWaitForSingleObject_pfn)(
  IN HANDLE         Handle,
  IN BOOLEAN        Alertable,
  IN PLARGE_INTEGER Timeout    // Microseconds
);


typedef enum _OBJECT_WAIT_TYPE {
  WaitAllObject,
  WaitAnyObject
} OBJECT_WAIT_TYPE,
*POBJECT_WAIT_TYPE;

typedef
NTSTATUS (NTAPI *NtWaitForMultipleObjects_pfn)(
  IN ULONG                ObjectCount,
  IN PHANDLE              ObjectsArray,
  IN OBJECT_WAIT_TYPE     WaitType,
  IN BOOLEAN              Alertable,
  IN PLARGE_INTEGER       TimeOut OPTIONAL );


extern void SK_Scheduler_Init     (void);
extern void SK_Scheduler_Shutdown (void);


#endif /* __SK__FRAMERATE_H__ */