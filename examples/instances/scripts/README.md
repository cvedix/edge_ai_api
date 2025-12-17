# Scripts

Thư mục này chứa các utility scripts để làm việc với API instances.

## Các Scripts

- `analyze_log.sh` - Phân tích logs từ instances
- `check_instance_status.sh` - Kiểm tra trạng thái instance
- `demo_script.sh` - Demo script để test API
- `monitor_instance.sh` - Monitor instance và hiển thị thông tin real-time
- `test_output_api.sh` - Test output API

## Cách sử dụng

### Check Instance Status

```bash
./check_instance_status.sh {instanceId}
```

### Monitor Instance

```bash
./monitor_instance.sh {instanceId}
```

### Analyze Logs

```bash
./analyze_log.sh {instanceId}
```

## Lưu ý

- Đảm bảo scripts có quyền thực thi: `chmod +x *.sh`
- Các scripts yêu cầu `curl` và `jq` (nếu cần)
- Cần cập nhật API endpoint trong scripts nếu không sử dụng localhost:8080
