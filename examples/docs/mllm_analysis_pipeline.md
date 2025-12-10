# MLLM Analysis Pipeline - Hướng dẫn Chi tiết

## Tổng quan

Solution `mllm_analysis` là một pipeline hoàn chỉnh để phân tích video sử dụng Multimodal Large Language Model (MLLM), cho phép mô tả và phân tích nội dung video bằng ngôn ngữ tự nhiên.

### Use Case
- **Video Analysis**: Phân tích và mô tả nội dung video
- **Content Understanding**: Hiểu nội dung video bằng ngôn ngữ tự nhiên
- **Automated Reporting**: Tạo báo cáo tự động về nội dung video
- **Intelligent Monitoring**: Giám sát thông minh với phân tích ngữ nghĩa

## Pipeline Architecture

```
[RTSP Source] → [MLLM Analyser] → [JSON Console Broker]
```

### Luồng xử lý

1. **RTSP Source**: Nhận video stream từ RTSP camera
2. **MLLM Analyser**: Phân tích frame và tạo mô tả bằng ngôn ngữ tự nhiên
3. **JSON Console Broker**: Output kết quả phân tích dưới dạng JSON

## Cấu hình Instance

### File JSON: `example_mllm_analysis.json`

```json
{
  "name": "mllm_analysis_demo_1",
  "group": "demo",
  "solution": "mllm_analysis",
  "persistent": false,
  "autoStart": false,
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_NAME": "llava",
    "PROMPT": "Describe what you see in this image. Focus on objects, people, and activities.",
    "API_BASE_URL": "http://localhost:11434",
    "API_KEY": "",
    "BACKEND_TYPE": "ollama"
  }
}
```

### Các tham số

| Tham số | Mô tả | Ví dụ |
|---------|-------|-------|
| `name` | Tên instance | `mllm_analysis_demo_1` |
| `group` | Nhóm instance | `demo` |
| `solution` | Solution ID | `mllm_analysis` |
| `RTSP_URL` | URL RTSP stream | `rtsp://server:port/stream` |
| `MODEL_NAME` | Tên model MLLM | `llava`, `llava:13b`, `llava:7b` |
| `PROMPT` | Prompt cho phân tích | `"Describe what you see..."` |
| `API_BASE_URL` | URL API server | `http://localhost:11434` |
| `API_KEY` | API key (nếu cần) | `""` (empty cho Ollama) |
| `BACKEND_TYPE` | Loại backend | `ollama`, `openai`, `anthropic` |

## Chi tiết các Nodes

### 1. RTSP Source Node (`rtsp_src`)

**Chức năng**: Nhận video stream từ RTSP source

**Tham số**:
- `rtsp_url`: URL RTSP stream
- `channel`: Channel ID (mặc định: 0)
- `resize_ratio`: Tỷ lệ resize (mặc định: 1.0)

### 2. MLLM Analyser Node (`mllm_analyser`)

**Chức năng**: Phân tích frame và tạo mô tả bằng ngôn ngữ tự nhiên

**Tham số**:
- `model_name`: Tên model MLLM (từ `MODEL_NAME`)
- `prompt`: Prompt cho phân tích (từ `PROMPT`)
- `api_base_url`: URL API server (từ `API_BASE_URL`)
- `api_key`: API key (từ `API_KEY`)
- `backend_type`: Loại backend (từ `BACKEND_TYPE`)

**Supported Backends**:
- `ollama`: Ollama local server (khuyến nghị)
- `openai`: OpenAI API
- `anthropic`: Anthropic Claude API

**Model Options (Ollama)**:
- `llava`: LLaVA model chuẩn
- `llava:7b`: LLaVA 7B parameters
- `llava:13b`: LLaVA 13B parameters
- `llava:34b`: LLaVA 34B parameters

**Quy trình**:
1. Nhận frame từ RTSP source
2. Gửi frame và prompt đến MLLM API
3. Nhận mô tả từ model
4. Output kết quả qua JSON broker

### 3. JSON Console Broker Node (`json_console_broker`)

**Chức năng**: Output kết quả phân tích dưới dạng JSON

**Output Format**:
```json
{
  "timestamp": "2024-01-01T12:00:00Z",
  "frame_id": 12345,
  "analysis": "A person is walking in a park. There are trees in the background...",
  "confidence": 0.95
}
```

## Cách sử dụng

### 1. Setup Ollama (khuyến nghị)

```bash
# Install Ollama
curl -fsSL https://ollama.ai/install.sh | sh

# Pull LLaVA model
ollama pull llava

# Start Ollama server
ollama serve
```

### 2. Tạo Instance qua API

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_mllm_analysis.json
```

### 3. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 4. Xem Output

Output sẽ được gửi qua JSON console broker. Kiểm tra logs để xem kết quả phân tích.

## Tùy chỉnh Cấu hình

### Thay đổi Model

```json
{
  "additionalParams": {
    "MODEL_NAME": "llava:13b"  // Sử dụng model lớn hơn
  }
}
```

### Thay đổi Prompt

```json
{
  "additionalParams": {
    "PROMPT": "What activities are happening in this scene? List all objects and people."
  }
}
```

### Sử dụng OpenAI API

```json
{
  "additionalParams": {
    "MODEL_NAME": "gpt-4-vision-preview",
    "API_BASE_URL": "https://api.openai.com/v1",
    "API_KEY": "your-api-key-here",
    "BACKEND_TYPE": "openai"
  }
}
```

### Sử dụng Anthropic Claude API

```json
{
  "additionalParams": {
    "MODEL_NAME": "claude-3-opus-20240229",
    "API_BASE_URL": "https://api.anthropic.com/v1",
    "API_KEY": "your-api-key-here",
    "BACKEND_TYPE": "anthropic"
  }
}
```

## Performance Tips

1. **Model Selection**:
   - `llava:7b`: Nhanh nhất, phù hợp cho real-time
   - `llava:13b`: Cân bằng tốc độ và chất lượng
   - `llava:34b`: Chất lượng cao nhất nhưng chậm hơn

2. **Prompt Optimization**:
   - Prompt ngắn gọn, rõ ràng
   - Specify output format nếu cần
   - Use examples để guide model

3. **API Server**:
   - Sử dụng Ollama local để tránh latency
   - Cấu hình GPU nếu có thể
   - Tối ưu batch size

## Troubleshooting

### 1. API không kết nối được

**Triệu chứng**: "Failed to connect to API" hoặc "Connection refused"

**Giải pháp**:
- Kiểm tra API server đang chạy
- Verify API_BASE_URL đúng
- Kiểm tra network connectivity
- Kiểm tra firewall rules

### 2. Model không tìm thấy

**Triệu chứng**: "Model not found" hoặc "Model unavailable"

**Giải pháp**:
- Kiểm tra model đã được pull chưa (Ollama: `ollama list`)
- Verify MODEL_NAME đúng
- Kiểm tra API server có hỗ trợ model không

### 3. Response chậm

**Triệu chứng**: FPS thấp, delay lớn

**Giải pháp**:
- Sử dụng model nhỏ hơn (llava:7b)
- Giảm frequency của analysis (không phân tích mọi frame)
- Sử dụng GPU acceleration
- Tối ưu prompt để giảm response time

### 4. Output không chính xác

**Triệu chứng**: Mô tả không đúng hoặc thiếu thông tin

**Giải pháp**:
- Sử dụng model lớn hơn
- Cải thiện prompt với examples
- Kiểm tra chất lượng video input
- Tăng resolution nếu cần

## Kết quả và Output

### JSON Output

Pipeline sẽ output JSON với:
- Timestamp của frame
- Frame ID
- Analysis text (mô tả bằng ngôn ngữ tự nhiên)
- Confidence score (nếu có)

### Log Output

Pipeline sẽ log các thông tin:
- API connection status
- Model loading status
- Analysis results
- Processing FPS
- Error messages (nếu có)

## Use Cases và Examples

### 1. Security Monitoring

**Prompt**: "Describe any suspicious activities or unusual objects in this scene."

### 2. Traffic Analysis

**Prompt**: "Count vehicles and describe traffic flow. Identify vehicle types."

### 3. Retail Analytics

**Prompt**: "Describe customer behavior and product interactions in this store."

### 4. Healthcare Monitoring

**Prompt**: "Describe patient activities and any concerning behaviors."

## Tài liệu tham khảo

- [LLaVA Documentation](https://github.com/haotian-liu/LLaVA)
- [Ollama Documentation](https://ollama.ai/docs)
- [OpenAI Vision API](https://platform.openai.com/docs/guides/vision)
- [Anthropic Claude API](https://docs.anthropic.com/claude/docs)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/CREATE_INSTANCE_GUIDE.md)

