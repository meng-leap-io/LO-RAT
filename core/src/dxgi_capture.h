#pragma once
#include <windows.h>
#include <vector>
#include <d3d11.h>
#include <dxgi1_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

struct DXGICapture {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    D3D11_TEXTURE2D_DESC desc = {};

    bool Init();
    bool CaptureFrame(std::vector<BYTE>& outBgra, int& outWidth, int& outHeight);
    void Release();
};
