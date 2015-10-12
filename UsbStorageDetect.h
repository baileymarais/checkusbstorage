#pragma once
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <vector>

namespace ids
{
    namespace windows
    {
        namespace io
        {
            enum detect_status {
                running,
                stoped,
            };

            typedef struct _USB_DEVICE_DESCRIPTION {
                char DriverName;                // 驱动器名(如C)
                std::string DeviceGuid;         // USB设备GUID
                time_t Ticks;                   // 连接/断开计算机的时间戳
            } USB_DEVICE_DESCRIPTION;
            typedef std::vector<USB_DEVICE_DESCRIPTION> USB_STORAGES;
            // 当有新设备插入的时候触发，并返回所有的USB存储设备
            typedef boost::function <void(USB_STORAGES devices)> UsbStorageEvent;
            // 返回最新插入的USB存储设备
            typedef boost::function <void(USB_DEVICE_DESCRIPTION)> NewDeviceEvent;
            // 当有设备拔出的时候出发，返回被拔出的设备
            typedef boost::function <void(USB_DEVICE_DESCRIPTION)> ReleaseDeviceEvent;
            class UsbStorageDetect
            {
              public:
                UsbStorageDetect();
                ~UsbStorageDetect();

                void Start();
                void Stop();
                void SetUsbStorageEvent(UsbStorageEvent DeviceNotifyCallback);
                void SetNewDeviceEvent(NewDeviceEvent DeviceNotifyCallback);
                void SetReleaseDeviceEvent(ReleaseDeviceEvent DeviceNotifyCallback);
                size_t GetDevicesNameList(std::vector<char>& list);
                detect_status Status();

              private:
                void DetectUsbStorage();
                bool IsRemovableDisk(char* guid);
                bool find_token(unsigned char* source,
                                int source_len,
                                const unsigned char* target,
                                int target_len);
                void repalce_all(unsigned char* source,
                                 const char findch,
                                 unsigned char* target,
                                 int sourcelen);
                bool find_guid_str(char* source_guid,
                                   char* guid);
                bool exists(char driver);
                USB_DEVICE_DESCRIPTION* get_device_description(char driver);
                bool checkforuser(char* ssid, char* guid);

              private:
                NewDeviceEvent _NewDeviceNotifyCallback;
                UsbStorageEvent _DeviceNotifyCallback;
                ReleaseDeviceEvent _DestoryDeviceNotifyCallback;
                boost::recursive_mutex devices_lock;

                USB_STORAGES _devices;
                bool _stop;
                detect_status _status;
            };
        }
    }
}
