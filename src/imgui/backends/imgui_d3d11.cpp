// ImGui Win32 + DirectX11 binding
// In this binding, ImTextureID is used to store a 'ID3D11ShaderResourceView*' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <imgui/imgui.h>
#include <imgui/backends/imgui_d3d11.h>
#include <SpecialK/render_backend.h>
#include <SpecialK/dxgi_backend.h>
#include <SpecialK/framerate.h>
#include <SpecialK/config.h>

// DirectX
#include <d3d11.h>
#include <d3dcompiler.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <atlbase.h>

// Data
static INT64                    g_Time                  = 0;
static INT64                    g_TicksPerSecond        = 0;

static HWND                     g_hWnd                  = nullptr;
static ID3D11Buffer*            g_pVB                   = nullptr;
static ID3D11Buffer*            g_pIB                   = nullptr;
static ID3D10Blob *             g_pVertexShaderBlob     = nullptr;
static ID3D11VertexShader*      g_pVertexShader         = nullptr;
static ID3D11InputLayout*       g_pInputLayout          = nullptr;
static ID3D11Buffer*            g_pVertexConstantBuffer = nullptr;
static ID3D10Blob *             g_pPixelShaderBlob      = nullptr;
static ID3D11PixelShader*       g_pPixelShader          = nullptr;
static ID3D11SamplerState*      g_pFontSampler          = nullptr;
static ID3D11ShaderResourceView*g_pFontTextureView      = nullptr;
static ID3D11RasterizerState*   g_pRasterizerState      = nullptr;
static ID3D11BlendState*        g_pBlendState           = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilState    = nullptr;

static UINT                     g_frameBufferWidth      = 0UL;
static UINT                     g_frameBufferHeight     = 0UL;

static int                      g_VertexBufferSize      = 5000,
                                g_IndexBufferSize       = 10000;

struct VERTEX_CONSTANT_BUFFER
{
  float mvp [4][4];
};


extern void
SK_ImGui_LoadFonts (void);

#include <SpecialK/tls.h>


// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
IMGUI_API
void
ImGui_ImplDX11_RenderDrawLists (ImDrawData* draw_data)
{
  SK_ScopedBool auto_bool (&SK_TLS_Bottom ()->imgui.drawing);

  SK_TLS_Bottom ()->imgui.drawing = true;

  ImGuiIO& io =
    ImGui::GetIO ();

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.swapchain)
    return;

  if (! rb.device)
    return;

  if (! rb.d3d11.immediate_ctx)
    return;

  if (! g_pVertexConstantBuffer)
    return;


  CComPtr                      <IDXGISwapChain>   pSwapChain = nullptr;
  rb.swapchain->QueryInterface <IDXGISwapChain> (&pSwapChain);

  CComPtr                   <ID3D11Device>   pDevice = nullptr;
  rb.device->QueryInterface <ID3D11Device> (&pDevice);

  CComPtr                                <ID3D11DeviceContext>   pDevCtx = nullptr;
  rb.d3d11.immediate_ctx->QueryInterface <ID3D11DeviceContext> (&pDevCtx);


  CComPtr <ID3D11Texture2D>                pBackBuffer = nullptr;
  pSwapChain->GetBuffer (0, IID_PPV_ARGS (&pBackBuffer));

  D3D11_TEXTURE2D_DESC   backbuffer_desc = { };
  pBackBuffer->GetDesc (&backbuffer_desc);

  io.DisplaySize.x             = static_cast <float> (backbuffer_desc.Width);
  io.DisplaySize.y             = static_cast <float> (backbuffer_desc.Height);

  io.DisplayFramebufferScale.x = static_cast <float> (backbuffer_desc.Width);
  io.DisplayFramebufferScale.y = static_cast <float> (backbuffer_desc.Height);


  // Create and grow vertex/index buffers if needed
  if ((! g_pVB) || g_VertexBufferSize < draw_data->TotalVtxCount)
  {
    SK_COM_ValidateRelease ((IUnknown **)&g_pVB);

    g_VertexBufferSize  =
      draw_data->TotalVtxCount + 5000;

    D3D11_BUFFER_DESC desc
                        = { };

    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = g_VertexBufferSize * sizeof (ImDrawVert);
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags      = 0;

    if (pDevice->CreateBuffer (&desc, nullptr, &g_pVB) < 0)
      return;
  }

  if ((! g_pIB) || g_IndexBufferSize < draw_data->TotalIdxCount)
  {
    SK_COM_ValidateRelease ((IUnknown **)&g_pIB);

    g_IndexBufferSize   =
      draw_data->TotalIdxCount + 10000;

    D3D11_BUFFER_DESC desc
                        = { };

    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = g_IndexBufferSize * sizeof (ImDrawIdx);
    desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (pDevice->CreateBuffer (&desc, nullptr, &g_pIB) < 0)
      return;
  }

  // Copy and convert all vertices into a single contiguous buffer
  D3D11_MAPPED_SUBRESOURCE vtx_resource = { },
                           idx_resource = { };

  if (pDevCtx->Map (g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
    return;

  if (pDevCtx->Map (g_pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
  {
    // If for some reason the first one succeeded, but this one failed.... unmap the first one
    //   then abandon all hope.
    pDevCtx->Unmap (g_pVB, 0);
    return;
  }

  auto* vtx_dst = static_cast <ImDrawVert *> (vtx_resource.pData);
  auto* idx_dst = static_cast <ImDrawIdx  *> (idx_resource.pData);

  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list =
      draw_data->CmdLists [n];

    if (config.imgui.render.disable_alpha)
    {
      for (INT i = 0; i < cmd_list->VtxBuffer.Size; i++)
      {
        ImU32& color =
          cmd_list->VtxBuffer.Data [i].col;

        uint8_t alpha = (((color & 0xFF000000U) >> 24U) & 0xFFU);

        // Boost alpha for visibility
        if (alpha < 93 && alpha != 0)
          alpha += (93  - alpha) / 2;

        float a = ((float)                       alpha / 255.0f);
        float r = ((float)((color & 0xFF0000U) >> 16U) / 255.0f);
        float g = ((float)((color & 0x00FF00U) >>  8U) / 255.0f);
        float b = ((float)((color & 0x0000FFU)       ) / 255.0f);

        color =                    0xFF000000U  |
                ((UINT)((r * a) * 255U) << 16U) |
                ((UINT)((g * a) * 255U) <<  8U) |
                ((UINT)((b * a) * 255U)       );
      }
    }

    memcpy (vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof (ImDrawVert));
    memcpy (idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof (ImDrawIdx));

    vtx_dst += cmd_list->VtxBuffer.Size;
    idx_dst += cmd_list->IdxBuffer.Size;
  }

  pDevCtx->Unmap (g_pVB, 0);
  pDevCtx->Unmap (g_pIB, 0);

  // Setup orthographic projection matrix into our constant buffer
  {
    D3D11_MAPPED_SUBRESOURCE mapped_resource;

    if (pDevCtx->Map (g_pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
      return;

    auto* constant_buffer =
      static_cast <VERTEX_CONSTANT_BUFFER *> (mapped_resource.pData);

    float L = 0.0f;
    float R = ImGui::GetIO ().DisplaySize.x;
    float B = ImGui::GetIO ().DisplaySize.y;
    float T = 0.0f;

    float mvp [4][4] =
    {
      { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
      { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
      { 0.0f,         0.0f,           0.5f,       0.0f },
      { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };

    memcpy         (&constant_buffer->mvp, mvp, sizeof (mvp));
    pDevCtx->Unmap (g_pVertexConstantBuffer, 0);
  }

  CComPtr <ID3D11RenderTargetView> pRenderTargetView = nullptr;

  D3D11_TEXTURE2D_DESC   tex2d_desc = { };
  pBackBuffer->GetDesc (&tex2d_desc);

  // SRGB Correction for UIs
  switch (tex2d_desc.Format)
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView);
    } break;

    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView);
    } break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_R16G16B16A16_FLOAT;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView);
    } break;

    default:
     pDevice->CreateRenderTargetView (pBackBuffer, nullptr, &pRenderTargetView);
  }

  pDevCtx->OMSetRenderTargets ( 1,
                                  &pRenderTargetView,
                                    nullptr );

  // Setup viewport
  D3D11_VIEWPORT vp = { };

  vp.Height   = ImGui::GetIO ().DisplaySize.y;
  vp.Width    = ImGui::GetIO ().DisplaySize.x;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  vp.TopLeftX = vp.TopLeftY = 0.0f;

  pDevCtx->RSSetViewports (1, &vp);

  // Bind shader and vertex buffers
  unsigned int stride = sizeof (ImDrawVert);
  unsigned int offset = 0;

  pDevCtx->IASetInputLayout       (g_pInputLayout);
  pDevCtx->IASetVertexBuffers     (0, 1, &g_pVB, &stride, &offset);
  pDevCtx->IASetIndexBuffer       ( g_pIB, sizeof (ImDrawIdx) == 2 ?
                                             DXGI_FORMAT_R16_UINT  :
                                             DXGI_FORMAT_R32_UINT,
                                               0 );
  pDevCtx->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  pDevCtx->VSSetShader            (g_pVertexShader, nullptr, 0);
  pDevCtx->VSSetConstantBuffers   (0, 1, &g_pVertexConstantBuffer);


  pDevCtx->PSSetShader            (g_pPixelShader, nullptr, 0);
  pDevCtx->PSSetSamplers          (0, 1, &g_pFontSampler);

  // Setup render state
  const float blend_factor [4] = { 0.f, 0.f,
                                   0.f, 0.f };

  pDevCtx->OMSetBlendState        (g_pBlendState, blend_factor, 0xffffffff);
  pDevCtx->OMSetDepthStencilState (g_pDepthStencilState,        0);
  pDevCtx->RSSetState             (g_pRasterizerState);

  // Render command lists
  int vtx_offset = 0;
  int idx_offset = 0;

  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list =
      draw_data->CmdLists [n];

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
    {
      const ImDrawCmd* pcmd =
        &cmd_list->CmdBuffer [cmd_i];

      if (pcmd->UserCallback)
        pcmd->UserCallback (cmd_list, pcmd);

      else
      {
        const D3D11_RECT r = {
          static_cast <LONG> (pcmd->ClipRect.x), static_cast <LONG> (pcmd->ClipRect.y),
          static_cast <LONG> (pcmd->ClipRect.z), static_cast <LONG> (pcmd->ClipRect.w)
        };

        pDevCtx->PSSetShaderResources (0, 1, (ID3D11ShaderResourceView **)&pcmd->TextureId);
        pDevCtx->RSSetScissorRects    (1, &r);

        pDevCtx->DrawIndexed (pcmd->ElemCount, idx_offset, vtx_offset);
      }

      idx_offset += pcmd->ElemCount;
    }

    vtx_offset += cmd_list->VtxBuffer.Size;
  }
}

#include <SpecialK/config.h>

static void
ImGui_ImplDX11_CreateFontsTexture (void)
{
  SK_ScopedBool auto_bool0 (&SK_TLS_Bottom ()->texture_management.injection_thread);
  SK_ScopedBool auto_bool1 (&SK_TLS_Bottom ()->imgui.drawing                      );

  // Do not dump ImGui font textures
  SK_TLS_Bottom ()->texture_management.injection_thread = true;
  SK_TLS_Bottom ()->imgui.drawing                       = true;

  // Build texture atlas
  ImGuiIO& io (ImGui::GetIO ());

  static bool           init   = false;
  static unsigned char* pixels = nullptr;
  static int            width  = 0,
                        height = 0;

  // Only needs to be done once, the raw pixels are API agnostic
  if (! init)
  {
    SK_ImGui_LoadFonts ();

    io.Fonts->GetTexDataAsRGBA32 (&pixels, &width, &height);

    init = true;
  }

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  CComPtr                   <ID3D11Device>   pDev = nullptr;
  rb.device->QueryInterface <ID3D11Device> (&pDev);

  // Upload texture to graphics system
  {
    D3D11_TEXTURE2D_DESC desc
                          = { };

    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags   = 0;

    ID3D11Texture2D        *pTexture    = nullptr;
    D3D11_SUBRESOURCE_DATA  subResource = { };

    subResource.pSysMem          = pixels;
    subResource.SysMemPitch      = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    pDev->CreateTexture2D (&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { };

    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    pDev->CreateShaderResourceView (pTexture, &srvDesc, &g_pFontTextureView);

    pTexture->Release ();
  }

  // Store our identifier
  io.Fonts->TexID =
    static_cast <void *> (g_pFontTextureView);

  // Create texture sampler
  {
    D3D11_SAMPLER_DESC desc = { };

    desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;    //WRAP
    desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;    //WRAP
    desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;    //WRAP
    desc.MipLODBias     = 0.f;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD         = 0.f;
    desc.MaxLOD         = 0.f;

    pDev->CreateSamplerState (&desc, &g_pFontSampler);
  }

  SK_TLS_Bottom ()->texture_management.injection_thread = false;
}

bool
ImGui_ImplDX11_CreateDeviceObjects (void)
{
  SK_ScopedBool auto_bool (&SK_TLS_Bottom ()->imgui.drawing);

  // Do not dump ImGui font textures
  SK_TLS_Bottom ()->imgui.drawing = true;

  if (g_pFontSampler)
    ImGui_ImplDX11_InvalidateDeviceObjects ();

  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return false;

  if (! rb.d3d11.immediate_ctx)
    return false;

  if (! rb.swapchain)
    return false;

  CComPtr                   <ID3D11Device>   pDev = nullptr;
  rb.device->QueryInterface <ID3D11Device> (&pDev);

  // Create the vertex shader
  {
    static const char* vertexShader =
      "cbuffer vertexBuffer : register(b0) \
      {\
      float4x4 ProjectionMatrix; \
      };\
      struct VS_INPUT\
      {\
      float2 pos : POSITION;\
      float4 col : COLOR0;\
      float2 uv  : TEXCOORD0;\
      };\
      \
      struct PS_INPUT\
      {\
      float4 pos : SV_POSITION;\
      float4 col : COLOR0;\
      float2 uv  : TEXCOORD0;\
      };\
      \
      PS_INPUT main(VS_INPUT input)\
      {\
      PS_INPUT output;\
      output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
      output.col = input.col;\
      output.uv  = input.uv;\
      return output;\
      }";

    D3DCompile ( vertexShader,
                   strlen (vertexShader),
                     nullptr, nullptr, nullptr,
                       "main", "vs_4_0",
                         0, 0,
                           &g_pVertexShaderBlob,
                             nullptr );

    // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in
    //       (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
    if (g_pVertexShaderBlob == nullptr)
      return false;

    if ( pDev->CreateVertexShader ( static_cast <DWORD *> (g_pVertexShaderBlob->GetBufferPointer ()),
                                      g_pVertexShaderBlob->GetBufferSize (),
                                        nullptr,
                                          &g_pVertexShader ) != S_OK )
      return false;

    // Create the input layout
    D3D11_INPUT_ELEMENT_DESC local_layout [] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert *)nullptr)->pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert *)nullptr)->uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (size_t)(&((ImDrawVert *)nullptr)->col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if ( pDev->CreateInputLayout ( local_layout, 3,
                                     g_pVertexShaderBlob->GetBufferPointer (),
                                       g_pVertexShaderBlob->GetBufferSize  (),
                                         &g_pInputLayout ) != S_OK )
    {
      return false;
    }

    // Create the constant buffer
    {
      D3D11_BUFFER_DESC desc = { };

      desc.ByteWidth         = sizeof (VERTEX_CONSTANT_BUFFER);
      desc.Usage             = D3D11_USAGE_DYNAMIC;
      desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags         = 0;

      pDev->CreateBuffer (&desc, nullptr, &g_pVertexConstantBuffer);
    }
  }

  // Create the pixel shader
  {
    static const char* pixelShader =
      "struct PS_INPUT\
      {\
      float4 pos : SV_POSITION;\
      float4 col : COLOR0;\
      float2 uv  : TEXCOORD0;\
      };\
      sampler sampler0;\
      Texture2D texture0;\
      \
      float4 main(PS_INPUT input) : SV_Target\
      {\
      float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
      return out_col; \
      }";

    D3DCompile ( pixelShader,
                   strlen (pixelShader),
                     nullptr, nullptr, nullptr,
                       "main", "ps_4_0",
                         0, 0,
                           &g_pPixelShaderBlob,
                             nullptr );

    // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in 
    //       (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
    if (g_pPixelShaderBlob == nullptr)
      return false;

    if ( pDev->CreatePixelShader ( static_cast <DWORD *> (g_pPixelShaderBlob->GetBufferPointer ()),
                                     g_pPixelShaderBlob->GetBufferSize (),
                                       nullptr,
                                         &g_pPixelShader ) != S_OK )
    {
      return false;
    }
  }

  // Create the blending setup
  {
    D3D11_BLEND_DESC desc                       = {   };

    desc.AlphaToCoverageEnable                  = false;
    desc.RenderTarget [0].BlendEnable           =  true;
    desc.RenderTarget [0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget [0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget [0].BlendOp               = D3D11_BLEND_OP_ADD;
    desc.RenderTarget [0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    desc.RenderTarget [0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    desc.RenderTarget [0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    desc.RenderTarget [0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    pDev->CreateBlendState (&desc, &g_pBlendState);
  }

  // Create the rasterizer state
  {
    D3D11_RASTERIZER_DESC desc = { };

    desc.FillMode        = D3D11_FILL_SOLID;
    desc.CullMode        = D3D11_CULL_NONE;
    desc.ScissorEnable   = true;
    desc.DepthClipEnable = true;

    pDev->CreateRasterizerState (&desc, &g_pRasterizerState);
  }

  // Create depth-stencil State
  {
    D3D11_DEPTH_STENCIL_DESC desc = { };

    desc.DepthEnable              = false;
    desc.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc                = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable            = false;
    desc.FrontFace.StencilFailOp  = desc.FrontFace.StencilDepthFailOp =
                                    desc.FrontFace.StencilPassOp      =
                                    D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc    = D3D11_COMPARISON_ALWAYS;
    desc.BackFace                 = desc.FrontFace;

    pDev->CreateDepthStencilState (&desc, &g_pDepthStencilState);
  }

  ImGui_ImplDX11_CreateFontsTexture ();

  return true;
}


using SK_ImGui_ResetCallback_pfn = void (__stdcall *)(void);

std::vector <IUnknown *>                 external_resources;
std::set    <SK_ImGui_ResetCallback_pfn> reset_callbacks;

__declspec (dllexport)
void
__stdcall
SKX_ImGui_RegisterResource (IUnknown* pRes)
{
  external_resources.push_back (pRes);
}


__declspec (dllexport)
void
__stdcall
SKX_ImGui_RegisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback)
{
  reset_callbacks.emplace (pCallback);
}

__declspec (dllexport)
void
__stdcall
SKX_ImGui_UnregisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback)
{
  if (reset_callbacks.count (pCallback))
    reset_callbacks.erase (pCallback);
}

void
SK_ImGui_ResetExternal (void)
{
  for ( auto it : external_resources )
  {
    it->Release ();
  }

  external_resources.clear ();

  for ( auto reset_fn : reset_callbacks )
  {
    reset_fn ();
  }
}


void
ImGui_ImplDX11_InvalidateDeviceObjects (void)
{
  //if (! g_pd3dDevice)
  //  return;

  SK_ScopedBool auto_bool (&SK_TLS_Bottom ()->imgui.drawing);

  // Do not dump ImGui font textures
  SK_TLS_Bottom ()->imgui.drawing = true;

  SK_ImGui_ResetExternal ();

  if (g_pFontSampler)          { g_pFontSampler->Release     ();     g_pFontSampler = nullptr; }
  if (g_pFontTextureView)      { g_pFontTextureView->Release (); g_pFontTextureView = nullptr; ImGui::GetIO ().Fonts->TexID = nullptr; }
  if (g_pIB)                   { g_pIB->Release              ();              g_pIB = nullptr; }
  if (g_pVB)                   { g_pVB->Release              ();              g_pVB = nullptr; }

  if (g_pBlendState)           { g_pBlendState->Release           ();           g_pBlendState = nullptr; }
  if (g_pDepthStencilState)    { g_pDepthStencilState->Release    ();    g_pDepthStencilState = nullptr; }
  if (g_pRasterizerState)      { g_pRasterizerState->Release      ();      g_pRasterizerState = nullptr; }
  if (g_pPixelShader)          { g_pPixelShader->Release          ();          g_pPixelShader = nullptr; }
  if (g_pPixelShaderBlob)      { g_pPixelShaderBlob->Release      ();      g_pPixelShaderBlob = nullptr; }
  if (g_pVertexConstantBuffer) { g_pVertexConstantBuffer->Release (); g_pVertexConstantBuffer = nullptr; }
  if (g_pInputLayout)          { g_pInputLayout->Release          ();          g_pInputLayout = nullptr; }
  if (g_pVertexShader)         { g_pVertexShader->Release         ();         g_pVertexShader = nullptr; }
  if (g_pVertexShaderBlob)     { g_pVertexShaderBlob->Release     ();     g_pVertexShaderBlob = nullptr; }
}

bool
ImGui_ImplDX11_Init ( IDXGISwapChain* pSwapChain,
                      ID3D11Device*,
                      ID3D11DeviceContext* )
{
  static bool first = true;

  if (first)
  {
    if (! QueryPerformanceFrequency        (reinterpret_cast <LARGE_INTEGER *> (&g_TicksPerSecond)))
      return false;

    if (! QueryPerformanceCounter_Original (reinterpret_cast <LARGE_INTEGER *> (&g_Time)))
      return false;

    first = false;
  }

  DXGI_SWAP_CHAIN_DESC  swap_desc  = { };
  pSwapChain->GetDesc (&swap_desc);

  g_hWnd              = swap_desc.OutputWindow;

  g_frameBufferWidth  = swap_desc.BufferDesc.Width;
  g_frameBufferHeight = swap_desc.BufferDesc.Height;

  ImGuiIO& io =
    ImGui::GetIO ();

  // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
  io.KeyMap [ImGuiKey_Tab]        = VK_TAB;
  io.KeyMap [ImGuiKey_LeftArrow]  = VK_LEFT;
  io.KeyMap [ImGuiKey_RightArrow] = VK_RIGHT;
  io.KeyMap [ImGuiKey_UpArrow]    = VK_UP;
  io.KeyMap [ImGuiKey_DownArrow]  = VK_DOWN;
  io.KeyMap [ImGuiKey_PageUp]     = VK_PRIOR;
  io.KeyMap [ImGuiKey_PageDown]   = VK_NEXT;
  io.KeyMap [ImGuiKey_Home]       = VK_HOME;
  io.KeyMap [ImGuiKey_End]        = VK_END;
  io.KeyMap [ImGuiKey_Delete]     = VK_DELETE;
  io.KeyMap [ImGuiKey_Backspace]  = VK_BACK;
  io.KeyMap [ImGuiKey_Enter]      = VK_RETURN;
  io.KeyMap [ImGuiKey_Escape]     = VK_ESCAPE;
  io.KeyMap [ImGuiKey_A]          = 'A';
  io.KeyMap [ImGuiKey_C]          = 'C';
  io.KeyMap [ImGuiKey_V]          = 'V';
  io.KeyMap [ImGuiKey_X]          = 'X';
  io.KeyMap [ImGuiKey_Y]          = 'Y';
  io.KeyMap [ImGuiKey_Z]          = 'Z';

  // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
  io.RenderDrawListsFn = ImGui_ImplDX11_RenderDrawLists;
  io.ImeWindowHandle   = g_hWnd;

  return SK_GetCurrentRenderBackend ().device != nullptr;
}

void
ImGui_ImplDX11_Shutdown (void)
{
  SK_ScopedBool auto_bool (&SK_TLS_Bottom ()->imgui.drawing);

  // Do not dump ImGui font textures
  SK_TLS_Bottom ()->imgui.drawing = true;

  ImGui_ImplDX11_InvalidateDeviceObjects ();
  ImGui::Shutdown                        ();

  g_hWnd              = HWND_DESKTOP;
}

#include <SpecialK/window.h>

void
SK_ImGui_PollGamepad (void);

void
ImGui_ImplDX11_NewFrame (void)
{
  if (! SK_GetCurrentRenderBackend ().device)
    return;

  if (! g_pFontSampler)
    ImGui_ImplDX11_CreateDeviceObjects ();
  
  ImGuiIO& io (ImGui::GetIO ());

  // Setup display size (every frame to accommodate for window resizing)
  //io.DisplaySize =
    //ImVec2 ( g_frameBufferWidth,
               //g_frameBufferHeight );

  // Setup time step
  INT64 current_time;

  QueryPerformanceCounter_Original (
    reinterpret_cast <LARGE_INTEGER *> (&current_time)
  );

  io.DeltaTime = static_cast <float> (current_time - g_Time) /
                 static_cast <float> (g_TicksPerSecond);
  g_Time       =                      current_time;

  // Read keyboard modifiers inputs
  io.KeyCtrl   = (io.KeysDown [VK_CONTROL]) != 0;
  io.KeyShift  = (io.KeysDown [VK_SHIFT])   != 0;
  io.KeyAlt    = (io.KeysDown [VK_MENU])    != 0;

  io.KeySuper  = false;

  // For games that hijack the mouse cursor using Direct Input 8.
  //
  //  -- Acquire actually means release their exclusive ownership : )
  //
  //if (SK_ImGui_WantMouseCapture ())
  //  SK_Input_DI8Mouse_Acquire ();
  //else
  //  SK_Input_DI8Mouse_Release ();


  SK_ImGui_PollGamepad ();


  // Start the frame
  ImGui::NewFrame ();
}

void
ImGui_ImplDX11_Resize ( IDXGISwapChain *This,
                        UINT            BufferCount,
                        UINT            Width,
                        UINT            Height,
                        DXGI_FORMAT     NewFormat,
                        UINT            SwapChainFlags )
{
  UNREFERENCED_PARAMETER (BufferCount);
  UNREFERENCED_PARAMETER (NewFormat);
  UNREFERENCED_PARAMETER (SwapChainFlags);
  UNREFERENCED_PARAMETER (Width);
  UNREFERENCED_PARAMETER (Height);
  UNREFERENCED_PARAMETER (This);


  SK_RenderBackend& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return;

  SK_ScopedBool auto_bool (&SK_TLS_Bottom ()->imgui.drawing);

  // Do not dump ImGui font textures
  SK_TLS_Bottom ()->imgui.drawing = true;


  assert (This == rb.swapchain);

  CComPtr <ID3D11Device>        pDev    = nullptr;
  CComPtr <ID3D11DeviceContext> pDevCtx = nullptr;

  if (rb.d3d11.immediate_ctx != nullptr)
  {
    HRESULT hr0 = rb.device->QueryInterface              <ID3D11Device>        (&pDev);
    HRESULT hr1 = rb.d3d11.immediate_ctx->QueryInterface <ID3D11DeviceContext> (&pDevCtx);

    if (SUCCEEDED (hr0) && SUCCEEDED (hr1))
    {
      ImGui_ImplDX11_InvalidateDeviceObjects ();
    }
  }
}