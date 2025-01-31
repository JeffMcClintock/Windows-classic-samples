//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "SampleMultiDPIImage.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace D2D1;

CSampleMultiDPIImage::CSampleMultiDPIImage() : CSampleElementBase(0.0F, 0.0F)
{
}

CSampleMultiDPIImage::CSampleMultiDPIImage(float left, float top) : CSampleElementBase(left, top)
{
}

CSampleMultiDPIImage::~CSampleMultiDPIImage(void)
{
    ReleaseDeviceResources();
    ReleaseDeviceIndependentResources();
}

void CSampleMultiDPIImage::DoDraw()
{
    float dpi = m_deviceResources->GetDpi();

    shared_ptr<CSceneGraphicsBitmap> toDraw;

    switch ((INT)dpi)
    {
    case 192:
        toDraw = m_bitmap200;
        break;
    case 144:
        toDraw = m_bitmap150;
        break;
    case 120:
        toDraw = m_bitmap125;
        break;
    case 96:
    default:
        toDraw = m_bitmap100;
        break;
    }

    m_currentWidth = static_cast<float>(toDraw->ImageWidth);

    auto d2dContext = m_deviceResources->GetD2DDeviceContext();

    if (toDraw->Bitmap)
    {
        d2dContext->SetUnitMode(D2D1_UNIT_MODE::D2D1_UNIT_MODE_PIXELS);  // Disable D2D DPI scaling

        d2dContext->DrawBitmap(
            toDraw->Bitmap.Get(),
            RectF(
                m_left,
                m_top,
                static_cast<float>(toDraw->ImageWidth) + m_left,
                static_cast<float>(toDraw->ImageHeight) + m_top
                ),
            1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
            );

        d2dContext->SetUnitMode(D2D1_UNIT_MODE::D2D1_UNIT_MODE_DIPS);    // Reenable D2D scaling
    }
}

shared_ptr<CSceneGraphicsBitmap> CSampleMultiDPIImage::LoadImage(std::wstring name)
{
    auto bitmap = make_shared<CSceneGraphicsBitmap>();

    auto factory = m_deviceResources->GetWicImagingFactory();

    PCWSTR uri = name.c_str();

    auto attr = GetFileAttributes(uri);
    if (0xFFFFFFFF!=attr)
    {
        ComPtr<IWICBitmapDecoder> decoder;
        DX::ThrowIfFailed(
            factory->CreateDecoderFromFilename(
                uri,
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                decoder.GetAddressOf()
                )
            );

        ComPtr<IWICBitmapFrameDecode> source;
        DX::ThrowIfFailed(decoder->GetFrame(0, source.GetAddressOf()));

        source->GetSize(&bitmap->ImageWidth,&bitmap->ImageHeight);
        source->GetResolution(&bitmap->ImageDPIX, &bitmap->ImageDPIY);

        DX::ThrowIfFailed(factory->CreateFormatConverter(bitmap->Image.GetAddressOf()));
        DX::ThrowIfFailed(
            bitmap->Image->Initialize(
                source.Get(),
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeMedianCut
                )
            );
    }
    return bitmap;
}


void CSampleMultiDPIImage::CreateDeviceIndependentResources(const std::shared_ptr<DeviceResources>& deviceResources)
{
    CSampleElementBase::CreateDeviceIndependentResources(deviceResources);

    WCHAR szPath[MAX_PATH];
    DX::ThrowIfFailed(GetModuleFileName(NULL, szPath, MAX_PATH) == 0 ? E_FAIL : S_OK);
    auto path = wstring(szPath);
    auto found = path.find_last_of('\\');
    auto dirPath = path.substr(0,found);

    auto bitmap100path = dirPath;
    bitmap100path.append(L"\\png100.png");

    auto bitmap125path = dirPath;
    bitmap125path.append(L"\\png125.png");

    auto bitmap150path = dirPath;
    bitmap150path.append(L"\\png150.png");

    auto bitmap200path = dirPath;
    bitmap200path.append(L"\\png200.png");

    m_bitmap100 = LoadImage(bitmap100path.c_str());
    m_bitmap125 = LoadImage(bitmap125path.c_str());
    m_bitmap150 = LoadImage(bitmap150path.c_str());
    m_bitmap200 = LoadImage(bitmap200path.c_str());
}

void CSampleMultiDPIImage::CreateDeviceBitmap(CSceneGraphicsBitmap* bitmap)
{
    auto d2dContext = m_deviceResources->GetD2DDeviceContext();

#if 0 // 8-bit Images
    DX::ThrowIfFailed(
        d2dContext->CreateBitmapFromWicBitmap(
            bitmap->Image.Get(),
            bitmap->Bitmap.GetAddressOf()
        )
    );
#else // float Images
    // Create a Device bitmap, except brightened to HDR levels.
    const float whiteMult = 3.0; // meant to get from Adaptor, but let's just fake it for now. Assumes that your monitor is already set to HDR.

    // Create a WIC bitmap to draw on.
    UINT w, h;
    const auto bitmapSize = bitmap->Image->GetSize(&w, &h);

    Microsoft::WRL::ComPtr<IWICBitmap> diBitmap_HDR_;
    HRESULT hr = m_deviceResources->GetWicImagingFactory()->CreateBitmap(
        w,
        h,
        GUID_WICPixelFormat64bppPRGBAHalf,
        WICBitmapNoCache,
        diBitmap_HDR_.ReleaseAndGetAddressOf()
    );

    // Create a WIC render target.
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> pWICRenderTarget;
    D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    hr = m_deviceResources->GetD2DFactory()->CreateWicBitmapRenderTarget(
        diBitmap_HDR_.Get(),
        renderTargetProperties,
        pWICRenderTarget.ReleaseAndGetAddressOf()
    );

    if (SUCCEEDED(hr))
    {
        // Create a device context from the WIC render target.
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> pDeviceContext;
        hr = pWICRenderTarget->QueryInterface(pDeviceContext.ReleaseAndGetAddressOf());

        if (SUCCEEDED(hr))
        {
            // Convert original image to D2D format
            D2D1_BITMAP_PROPERTIES props;
            props.dpiX = props.dpiY = 96;
            props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

            Microsoft::WRL::ComPtr<ID2D1Bitmap> pSourceBitmap;
            hr = pDeviceContext->CreateBitmapFromWicBitmap(
                bitmap->Image.Get(),
                //?                    &props,
                pSourceBitmap.ReleaseAndGetAddressOf()
            );

            // create whitescale effect
            Microsoft::WRL::ComPtr<ID2D1Effect> m_whiteScaleEffect;
            {
                // White level scale is used to multiply the color values in the image; this allows the user
                // to adjust the brightness of the image on an HDR display.
                pDeviceContext->CreateEffect(CLSID_D2D1ColorMatrix, m_whiteScaleEffect.ReleaseAndGetAddressOf());

                // SDR white level scaling is performing by multiplying RGB color values in linear gamma.
                // We implement this with a Direct2D matrix effect.
                D2D1_MATRIX_5X4_F matrix = D2D1::Matrix5x4F(
                    whiteMult, 0, 0, 0,  // [R] Multiply each color channel
                    0, whiteMult, 0, 0,  // [G] by the scale factor in 
                    0, 0, whiteMult, 0,  // [B] linear gamma space.
                    0, 0, 0, 1,		 // [A] Preserve alpha values.
                    0, 0, 0, 0);	 //     No offset.

                m_whiteScaleEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, matrix);

                // increase the bit-depth of the filter, else it does a shitty 8-bit conversion. Which results in serious degredation of the image.
                if (pDeviceContext->IsBufferPrecisionSupported(D2D1_BUFFER_PRECISION_16BPC_FLOAT))
                {
                    auto hr = m_whiteScaleEffect->SetValue(D2D1_PROPERTY_PRECISION, D2D1_BUFFER_PRECISION_16BPC_FLOAT);
                }
                else if (pDeviceContext->IsBufferPrecisionSupported(D2D1_BUFFER_PRECISION_32BPC_FLOAT))
                {
                    auto hr = m_whiteScaleEffect->SetValue(D2D1_PROPERTY_PRECISION, D2D1_BUFFER_PRECISION_32BPC_FLOAT);
                }
            }

            if (SUCCEEDED(hr))
            {
                // Set the effect input.
                m_whiteScaleEffect->SetInput(0, pSourceBitmap.Get());

                // Begin drawing on the device context.
                pDeviceContext->BeginDraw();

                // Draw the effect onto the device context.
                pDeviceContext->DrawImage(m_whiteScaleEffect.Get());

                // End drawing.
                hr = pDeviceContext->EndDraw();
            }
        }
    }
    hr = d2dContext->CreateBitmapFromWicBitmap(
        diBitmap_HDR_.Get(),
        bitmap->Bitmap.GetAddressOf()
    );
#endif
}

void CSampleMultiDPIImage::CreateDeviceResources()
{
    if (m_bitmap100->Image)
    {
        CreateDeviceBitmap(m_bitmap100.get());
    }

    if (m_bitmap125->Image) 
    {
        CreateDeviceBitmap(m_bitmap125.get());
    }

    if (m_bitmap150->Image)
    {
        CreateDeviceBitmap(m_bitmap150.get());
    }

    if (m_bitmap200->Image)
    {
        CreateDeviceBitmap(m_bitmap200.get());
    }
}

void CSampleMultiDPIImage::ReleaseDeviceResources()
{
    m_bitmap100.reset();
    m_bitmap125.reset();
    m_bitmap150.reset();
    m_bitmap200.reset();
}


float CSampleMultiDPIImage::GetWidth()
{
    return m_currentWidth;
}
