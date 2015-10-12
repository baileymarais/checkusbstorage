#include "stdafx.h"
#include "UsbStorageDetect.h"

#include <Windows.h>
#include <Time.h>

#include <boost/algorithm/string.hpp>

#define BYTEINCREMENT 512

namespace ids
{
    namespace windows
    {
        namespace io
        {
            unsigned const char token[16] = { 0x55, 0x00, 0x53, 0x00, 0x42,
                                              0x00, 0x5c, 0x00, 0x56, 0x00,
                                              0x49, 0x00, 0x44, 0x00, 0x5f,
                                              0x00
                                            };

            UsbStorageDetect::UsbStorageDetect()
            {
                _status = stoped;
                _stop = true;
            }

            void UsbStorageDetect::Start()
            {
                if (_stop) {
                    DebugPrint("正在启动USB设备检测线程...\n");
                    _stop = false;

                    // 启动检测线程
                    boost::function<void()> detect_function(
                        boost::bind(&UsbStorageDetect::DetectUsbStorage, this)
                    );
                    boost::thread detect_thread(
                        boost::thread(boost::bind(detect_function))
                    );
                    detect_thread.joinable();
                    _status = running;
                } else {
                    DebugPrint("USB设备检测线程正在运行，不需要重复启动...\n");
                }
            }
            void UsbStorageDetect::Stop()
            {
                DebugPrint("正在结束USB设备检测线程...\n");

                _stop = true;
                _status = stoped;
            }

            void UsbStorageDetect::SetUsbStorageEvent(UsbStorageEvent DeviceNotifyCallback)
            {
                _DeviceNotifyCallback = DeviceNotifyCallback;
            }

            void UsbStorageDetect::SetNewDeviceEvent(NewDeviceEvent NewDeviceNotifyCallback)
            {
                _NewDeviceNotifyCallback = NewDeviceNotifyCallback;
            }

            void UsbStorageDetect::SetReleaseDeviceEvent(ReleaseDeviceEvent DeviceNotifyCallback)
            {
                _DestoryDeviceNotifyCallback = DeviceNotifyCallback;
            }

            UsbStorageDetect::~UsbStorageDetect()
            {
                _stop = true;
                boost::recursive_mutex::scoped_lock lock(devices_lock);
                _devices.clear();
                USB_STORAGES(_devices).swap(_devices);
            }

            size_t UsbStorageDetect::GetDevicesNameList(std::vector<char>& list)
            {
                list.clear();
                for (auto& item : _devices) {
                    list.push_back(item.DriverName);
                }

                return list.size();
            }

            detect_status UsbStorageDetect::Status()
            {
                return _status;
            }

            void UsbStorageDetect::DetectUsbStorage()
            {
                char szDriverGuid[40] = { 0 };
                char szDriver[4] = { 0 };
                char szMountPoint[MAX_PATH] = { 0 };
                bool bChanged = false;

                DebugPrint("USB可移动设备检测线程已经启动...");

                while (!_stop) {
                    boost::recursive_mutex::scoped_lock lock(devices_lock);

                    for (char c = 'C'; c <= 'Z'; c++) {
                        sprintf_s(szDriver, "%c:\\", c);
                        BOOL result = GetVolumeNameForVolumeMountPointA(szDriver,
                                      szMountPoint,
                                      sizeof(szMountPoint));

                        if (result) {
                            if (find_guid_str(szMountPoint, szDriverGuid)) {
                                if (IsRemovableDisk(szDriverGuid)) {
                                    if (!exists(c)) {
                                        USB_DEVICE_DESCRIPTION d;
                                        d.DriverName = c;
                                        d.Ticks = time(NULL);
                                        d.DeviceGuid = szDriverGuid;
                                        _devices.push_back(d);

                                        // 这个USB盘符不在列表中，通知有新设备
                                        if (_NewDeviceNotifyCallback) {
                                            _NewDeviceNotifyCallback(d);
                                            DebugPrint("test:%s::::%c\n", d.DeviceGuid, d.DriverName);
                                        } else {
                                            DebugPrint("新插入USB设备的回调函数未设置...\n");
                                        }

                                        bChanged = true;
                                    }
                                }
                            }
                        } else {
                            // 这不是一个USB设备或者设备不存在
                            if (exists(c)) {
                                // 设备以前是存在列表中的，说明这个设备已经被拔出了
                                if (_DestoryDeviceNotifyCallback) {
                                    USB_DEVICE_DESCRIPTION desc(*get_device_description(c));
                                    desc.Ticks = time(NULL);
                                    _DestoryDeviceNotifyCallback(desc);

                                    boost::recursive_mutex::scoped_lock lock(devices_lock);
                                    USB_STORAGES tmp;
                                    for (auto item : _devices) {
                                        if (item.DriverName != desc.DriverName) {
                                            tmp.push_back(item);
                                        }
                                    }
                                    _devices.clear(); USB_STORAGES(_devices).swap(_devices);
                                    for (auto item : tmp) {
                                        _devices.push_back(item);
                                    }
                                    tmp.clear(); USB_STORAGES(tmp).swap(tmp);
                                } else {
                                    DebugPrint("USB设备拔出的回调函数未设置...\n");
                                }

                                bChanged = true;
                            }
                        }

                        if (bChanged && _DeviceNotifyCallback) {
                            _DeviceNotifyCallback(_devices);
                        }
                        bChanged = false;

                        if (_stop) break;
                    }

                    Sleep(100);
                }

                DebugPrint("USB可移动存储设备检测已经退出\n");
            }

            bool UsbStorageDetect::exists(char driver)
            {
                boost::recursive_mutex::scoped_lock lock(devices_lock);

                for (auto& item : _devices) {
                    if (item.DriverName == driver) {
                        return true;
                    }
                }

                return false;
            }

            USB_DEVICE_DESCRIPTION* UsbStorageDetect::get_device_description(char driver)
            {
                boost::recursive_mutex::scoped_lock lock(devices_lock);

                for (auto& item : _devices) {
                    if (item.DriverName == driver) {
                        return &item;
                    }
                }

                return nullptr;
            }

            bool UsbStorageDetect::find_token(unsigned char* source,
                                              int source_len,
                                              const unsigned char* target,
                                              int target_len)
            {
                if (source_len < target_len) return false;

                int matched = 0;
                for (int i = 0; i < source_len; i++) {
                    for (int j = 0; j < target_len; j++) {
                        matched += source[i + j] == target[j] ? 1 : 0;
                    }

                    if (matched == target_len) {
                        break;
                    } else {
                        matched = 0;
                    }
                }

                return matched == target_len;
            }

            void UsbStorageDetect::repalce_all(unsigned char* source,
                                               const char findch,
                                               unsigned char* target,
                                               int sourcelen)
            {
                if (!source || !target) return;

                unsigned char* p = source;
                unsigned char* t = target;
                while (--sourcelen) {
                    if (*p++ != findch) *target++ = *p;
                }

                target = t;
            }

            bool UsbStorageDetect::find_guid_str(char* source_guid, char* guid)
            {
                if (!source_guid) return false;

                bool started = false;
                char* p = source_guid;
                char* o = guid;
                while (p++) {
                    if (*p == '{') {
                        started = true;
                        continue;
                    }

                    if (started) {
                        if (*p != '}') {
                            *guid++ = *p;
                        } else {
                            break;
                        }
                    }
                }

                guid = o;

                return true;
            }

            bool UsbStorageDetect::checkforuser(char* ssid, char* guid)
            {
                bool ret = false;
                char lpszSubkey[MAX_PATH] = { 0 };
                sprintf_s(lpszSubkey,
                          "%s\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MountPoints2\\CPC\\Volume\\{%s}",
                          ssid, guid);

                HKEY regKey = nullptr;
                LSTATUS result = RegOpenKeyExA(HKEY_USERS,
                                               lpszSubkey,
                                               0,
                                               KEY_READ,
                                               &regKey);

                if (result == ERROR_SUCCESS) {
                    DWORD cbType = REG_BINARY;
                    unsigned char* lpszData = NULL;
                    DWORD dwSize = 0;
                    LSTATUS result = ERROR_SUCCESS;
                    DWORD dwBufferSize = 0;
                    do {
                        dwSize += BYTEINCREMENT;
                        if (lpszData) {
                            delete lpszData;
                            lpszData = NULL;
                        }
                        lpszData = new unsigned char[dwSize];
                        dwBufferSize = dwSize;

                        result = RegQueryValueExA(regKey,
                                                  "Data",
                                                  NULL,
                                                  &cbType,
                                                  (LPBYTE)lpszData,
                                                  &dwSize);

                    } while (result == ERROR_MORE_DATA);

                    RegCloseKey(regKey);

                    ret = find_token(lpszData, dwSize, token, sizeof(token));

                    memset(lpszData, 0x00, dwSize);
                    delete lpszData;
                    lpszData = NULL;
                }

                return ret;
            }

            bool UsbStorageDetect::IsRemovableDisk(char* guid)
            {
                HKEY keyUsers = nullptr;
                int nIndex = 0;

                if (ERROR_SUCCESS == RegOpenKeyA(HKEY_USERS, NULL, &keyUsers)) {
                    char szItemName[MAX_PATH] = { 0 };
                    DWORD dwItemNameSize = MAX_PATH;
                    int dwRet = RegEnumKeyExA(keyUsers, nIndex, szItemName, &dwItemNameSize, NULL, NULL, NULL, NULL);
                    while (dwRet == ERROR_SUCCESS) {
                        dwItemNameSize = 1024;
                        dwRet = RegEnumKeyExA(keyUsers, nIndex, szItemName, &dwItemNameSize, NULL, NULL, NULL, NULL);
                        if (dwRet == ERROR_SUCCESS) {
                            if (boost::algorithm::starts_with(szItemName, "S-1-5-21-")) {
                                if (checkforuser(szItemName, guid)) {
                                    RegCloseKey(keyUsers);
                                    return true;
                                }
                            }
                        }
                        nIndex++;
                    }

                    RegCloseKey(keyUsers);
                }


                return false;
            }
        }
    }
}
