# Other Solutions - Other Models

Thư mục này chứa các ví dụ sử dụng các solution khác không thuộc các model type cụ thể (ONNX, TensorRT, RKNN).

## Các Ví dụ

- `example_mllm_analysis.json` - MLLM (Multi-modal Large Language Model) analysis với Ollama

## Model Types

- **MLLM** - Multi-modal Large Language Models
  - Sử dụng Ollama backend
  - Hỗ trợ image analysis và description

## Tham số Model

### MLLM Analysis
- `MODEL_NAME`: Tên model (ví dụ: "llava")
- `API_BASE_URL`: URL của Ollama API (ví dụ: "http://localhost:11434")
- `PROMPT`: Prompt cho model
- `API_KEY`: API key (optional)
- `BACKEND_TYPE`: Loại backend (ví dụ: "ollama")

## Yêu cầu

- Ollama server đã được cài đặt và chạy
- Model đã được download trong Ollama
