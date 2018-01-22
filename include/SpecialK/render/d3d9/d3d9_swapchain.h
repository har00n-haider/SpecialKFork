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

#ifndef __SK__D3D9_SWAPCHAIN_H__
#define __SK__D3D9_SWAPCHAIN_H__

#pragma once

#include <SpecialK/d3d9_backend.h>

interface IWrapDirect3DDevice9;

extern volatile LONG SK_D3D9_LiveWrappedSwapChains;
extern volatile LONG SK_D3D9_LiveWrappedSwapChainsEx;

struct IWrapDirect3DSwapChain9 : IDirect3DSwapChain9Ex
{
  IWrapDirect3DSwapChain9 ( IWrapDirect3DDevice9 *dev,
                            IDirect3DSwapChain9  *orig ) :
                                                           pReal   (orig),
                                                           d3d9ex_ (false),
                                                           pDev    (dev)
  {
                                  orig->AddRef  (),
    InterlockedExchange  (&refs_, orig->Release ());

    InterlockedIncrement (&SK_D3D9_LiveWrappedSwapChains);
  }

  IWrapDirect3DSwapChain9 ( IWrapDirect3DDevice9  *dev,
                            IDirect3DSwapChain9Ex *orig ) :
                                                            pReal   (orig),
                                                            d3d9ex_ (true),
                                                            pDev    (dev)
  {
                                  orig->AddRef  (),
    InterlockedExchange  (&refs_, orig->Release ());

    InterlockedIncrement (&SK_D3D9_LiveWrappedSwapChains);
    InterlockedIncrement (&SK_D3D9_LiveWrappedSwapChainsEx);
  }

	IWrapDirect3DSwapChain9            (const IWrapDirect3DSwapChain9 &) = delete;
	IWrapDirect3DSwapChain9 &operator= (const IWrapDirect3DSwapChain9 &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObj) override;
	virtual ULONG   STDMETHODCALLTYPE AddRef         (void)                       override;
	virtual ULONG   STDMETHODCALLTYPE Release        (void)                       override;
	#pragma endregion

	#pragma region IDirect3DSwapChain9
	virtual HRESULT STDMETHODCALLTYPE Present              ( const RECT            *pSourceRect,
                                                           const RECT            *pDestRect,
                                                                 HWND             hDestWindowOverride,
                                                           const RGNDATA         *pDirtyRegion,
                                                                 DWORD            dwFlags )                override;
	virtual HRESULT STDMETHODCALLTYPE GetFrontBufferData   (IDirect3DSurface9      *pDestSurface)            override;
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer        (UINT                    iBackBuffer,
                                                          D3DBACKBUFFER_TYPE      Type,
                                                          IDirect3DSurface9     **ppBackBuffer)            override;
	virtual HRESULT STDMETHODCALLTYPE GetRasterStatus      (D3DRASTER_STATUS       *pRasterStatus)           override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayMode       (D3DDISPLAYMODE         *pMode)                   override;
	virtual HRESULT STDMETHODCALLTYPE GetDevice            (IDirect3DDevice9      **ppDevice)                override;
	virtual HRESULT STDMETHODCALLTYPE GetPresentParameters (D3DPRESENT_PARAMETERS  *pPresentationParameters) override;
	#pragma endregion


	#pragma region IDirect3DSwapChain9Ex
	virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount (UINT               *pLastPresentCount)       override;
	virtual HRESULT STDMETHODCALLTYPE GetPresentStats     (D3DPRESENTSTATS    *pPresentationStatistics) override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx    (D3DDISPLAYMODEEX   *pMode,
                                                         D3DDISPLAYROTATION *pRotation)               override;
	#pragma endregion

  volatile LONG                         refs_    = 1;
           IDirect3DSwapChain9         *pReal;
           bool                         d3d9ex_  = false;
           IWrapDirect3DDevice9 *const  pDev;
};


#endif /* __SK__D3D9__SWAPCHAIN_H__ */
