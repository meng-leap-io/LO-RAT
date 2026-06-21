#include "dxgi_capture.h"
#include <vector>

bool DXGICapture::Init() {
    HRESULT hr;

    IDXGIFactory1* factory = nullptr;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr)) return false;

    IDXGIAdapter1* adapter = nullptr;
    hr = factory->EnumAdapters1(0, &adapter);
    factory->Release();
    if (FAILED(hr)) return false;

    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    if (FAILED(hr)) { adapter->Release(); return false; }

    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) { adapter->Release(); return false; }

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                           featureLevels, 3, D3D11_SDK_VERSION,
                           &device, nullptr, &context);
    if (FAILED(hr)) {
        output1->Release();
        adapter->Release();
        return false;
    }

    hr = output1->DuplicateOutput(device, &duplication);
    output1->Release();
    adapter->Release();
    if (FAILED(hr)) {
        context->Release();
        device->Release();
        return false;
    }

    return true;
}

bool DXGICapture::CaptureFrame(std::vector<BYTE>& outBgra, int& outWidth, int& outHeight) {
    if (!duplication) return false;

    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    HRESULT hr = duplication->AcquireNextFrame(100, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (FAILED(hr)) return false;

    ID3D11Texture2D* desktopTexture = nullptr;
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktopTexture);
    desktopResource->Release();
    if (FAILED(hr)) { duplication->ReleaseFrame(); return false; }

    desktopTexture->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = desc.Width;
    stagingDesc.Height = desc.Height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = desc.Format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* stagingTexture = nullptr;
    hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        desktopTexture->Release();
        duplication->ReleaseFrame();
        return false;
    }

    context->CopyResource(stagingTexture, desktopTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        stagingTexture->Release();
        desktopTexture->Release();
        duplication->ReleaseFrame();
        return false;
    }

    outWidth = (int)desc.Width;
    outHeight = (int)desc.Height;
    outBgra.resize(desc.Width * desc.Height * 4);

    BYTE* src = (BYTE*)mapped.pData;
    BYTE* dst = outBgra.data();
    for (UINT y = 0; y < desc.Height; y++) {
        memcpy(dst, src, desc.Width * 4);
        src += mapped.RowPitch;
        dst += desc.Width * 4;
    }

    context->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    desktopTexture->Release();
    duplication->ReleaseFrame();
    return true;
}

void DXGICapture::Release() {
    if (duplication) { duplication->Release(); duplication = nullptr; }
    if (context) { context->Release(); context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
}
