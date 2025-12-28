# Update Instance Examples

Thư mục này chứa các example files để cập nhật instances đã tồn tại.

## Các Files

- `update_change_model_path.json` - Thay đổi model path
- `update_change_name_group.json` - Thay đổi name và group
- `update_change_persistent_autostart.json` - Thay đổi persistent và autoStart
- `update_change_rtsp_url.json` - Thay đổi RTSP URL
- `update_change_settings.json` - Thay đổi các settings khác

## Cách sử dụng

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId} \
  -H "Content-Type: application/json" \
  -d @update_change_rtsp_url.json
```

## Lưu ý

- Cần thay thế `{instanceId}` bằng instance ID thực tế
- Chỉ các fields được cung cấp trong request sẽ được cập nhật
- Các fields khác sẽ giữ nguyên giá trị hiện tại
