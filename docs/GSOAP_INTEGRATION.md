# gSOAP Integration Guide

## Tổng Quan

Project này đã được tích hợp với **gSOAP** (GPL v2 license) để hỗ trợ ONVIF operations. gSOAP cung cấp một cách chính thức và mạnh mẽ để làm việc với ONVIF SOAP services.

## License

Project này sử dụng **dual license**:
- **Apache License 2.0** - cho các phần không sử dụng gSOAP
- **GNU General Public License v2 (GPL v2)** - khi sử dụng gSOAP

Xem `LICENSE.md` và `LICENSE_GPLv2.md` để biết thêm chi tiết.

## Cài Đặt gSOAP

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install libgsoap++-dev
```

### Từ Source

1. Download gSOAP từ https://www.genivia.com/downloads.html
2. Extract và build:
```bash
cd gsoap-2.8
./configure
make
sudo make install
```

## Cấu Hình CMake

gSOAP sẽ được tự động phát hiện khi:
- `libgsoap++-dev` được cài đặt qua apt
- gSOAP được cài đặt trong `/usr/local` hoặc `/opt/gsoap`

Nếu gSOAP được tìm thấy, macro `USE_GSOAP` sẽ được định nghĩa và gSOAP sẽ được link vào project.

## Sử Dụng gSOAP với ONVIF

### Bước 1: Generate Code từ ONVIF WSDL

Để sử dụng gSOAP với ONVIF, bạn cần generate C++ code từ ONVIF WSDL files:

```bash
# Download ONVIF WSDL files
wget https://www.onvif.org/onvif/ver10/device/wsdl/devicemgmt.wsdl
wget https://www.onvif.org/onvif/ver10/media/wsdl/media.wsdl
wget https://www.onvif.org/onvif/ver20/ptz/wsdl/ptz.wsdl

# Generate header file
wsdl2h -o onvif.h devicemgmt.wsdl media.wsdl ptz.wsdl

# Generate C++ code
soapcpp2 -j -x onvif.h
```

### Bước 2: Include Generated Code

Sau khi generate, bạn sẽ có các files:
- `soapH.h`, `soapStub.h` - Headers
- `soapC.cpp`, `soapClient.cpp` - Implementation
- Service proxy classes

Copy các files này vào project và update `onvif_gsoap_wrapper.cpp` để sử dụng.

### Bước 3: Implement Wrapper Methods

Update các methods trong `ONVIFGSoapWrapper` để sử dụng generated code:

```cpp
bool ONVIFGSoapWrapper::getDeviceInformation(...) {
  // Create service proxy
  DeviceBindingProxy proxy(soap_);
  
  // Set endpoint
  proxy.soap_endpoint = endpoint.c_str();
  
  // Set authentication
  proxy.soap->userid = username.c_str();
  proxy.soap->passwd = password.c_str();
  
  // Call service
  _tds__GetDeviceInformation request;
  _tds__GetDeviceInformationResponse response;
  
  if (proxy.GetDeviceInformation(&request, &response) == SOAP_OK) {
    // Parse response
    camera.manufacturer = response.Manufacturer ? response.Manufacturer->c_str() : "";
    camera.model = response.Model ? response.Model->c_str() : "";
    camera.serialNumber = response.SerialNumber ? response.SerialNumber->c_str() : "";
    return true;
  }
  
  return false;
}
```

## Wrapper Class

`ONVIFGSoapWrapper` cung cấp một interface thống nhất để sử dụng gSOAP:

```cpp
#include "core/onvif_gsoap_wrapper.h"

ONVIFGSoapWrapper wrapper;

// Get device information
ONVIFCamera camera;
if (wrapper.getDeviceInformation(endpoint, username, password, camera)) {
  // Use camera information
}

// Get profiles
std::vector<std::string> profiles = wrapper.getProfiles(endpoint, username, password);

// Get stream URI
std::string streamUri = wrapper.getStreamUri(endpoint, profileToken, username, password);
```

## Fallback Mechanism

Nếu gSOAP không có sẵn, project sẽ tự động fallback về implementation hiện tại (manual SOAP building). Điều này đảm bảo project vẫn hoạt động ngay cả khi gSOAP chưa được cài đặt.

## Tích Hợp Với Code Hiện Tại

Code hiện tại sử dụng manual SOAP building (`ONVIFSoapBuilder`, `ONVIFXmlParser`). Để tích hợp gSOAP:

1. **Option 1**: Thay thế hoàn toàn manual implementation bằng gSOAP
2. **Option 2**: Sử dụng gSOAP như một fallback option (khi manual implementation fails)
3. **Option 3**: Cho phép user chọn implementation (manual hoặc gSOAP)

Hiện tại, wrapper class đã được tạo nhưng chưa implement đầy đủ. Cần generate code từ ONVIF WSDL và implement các methods.

## Testing

Sau khi implement, test với real ONVIF cameras:

```bash
# Test discovery
curl -X POST http://localhost:8080/v1/onvif/discover

# Test get cameras
curl http://localhost:8080/v1/onvif/cameras

# Test get streams
curl http://localhost:8080/v1/onvif/streams/{cameraid}
```

## Troubleshooting

### gSOAP không được tìm thấy

```bash
# Kiểm tra gSOAP đã được cài đặt
pkg-config --modversion gsoap++

# Hoặc
ls /usr/lib/libgsoap*.so
```

### Compilation errors

- Đảm bảo ONVIF WSDL code đã được generate
- Kiểm tra include paths trong CMakeLists.txt
- Đảm bảo gSOAP libraries được link đúng

### Runtime errors

- Kiểm tra gSOAP libraries có trong LD_LIBRARY_PATH
- Kiểm tra ONVIF camera endpoint có đúng không
- Kiểm tra authentication credentials

## Tài Liệu Tham Khảo

- [gSOAP Official Documentation](https://www.genivia.com/docs.html)
- [ONVIF WSDL Files](https://www.onvif.org/profiles/specifications/)
- [gSOAP ONVIF Examples](https://www.genivia.com/examples.html)

## Notes

- gSOAP được license dưới GPL v2, project này cũng hỗ trợ GPL v2 để tương thích
- Manual SOAP implementation vẫn được giữ lại như fallback
- Wrapper class cho phép dễ dàng switch giữa manual và gSOAP implementation

