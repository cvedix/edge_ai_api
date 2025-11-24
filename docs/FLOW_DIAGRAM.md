# Flow Diagram - Edge AI API

Diagram tổng quan về flow xử lý của Edge AI API project.

## Flow Tổng Quan Hệ Thống

```mermaid
flowchart TD
    Start([Khởi Động Ứng Dụng]) --> ReadEnv[Đọc Environment Variables<br/>API_HOST, API_PORT]
    ReadEnv --> ParseConfig[Parse và Validate Cấu Hình<br/>Host, Port, Threads]
    ParseConfig --> RegisterSignal[Đăng Ký Signal Handlers<br/>SIGINT, SIGTERM cho Graceful Shutdown]
    RegisterSignal --> CreateHandlers[Tạo và Đăng Ký API Handlers<br/>HealthHandler, VersionHandler,<br/>WatchdogHandler, SwaggerHandler]
    CreateHandlers --> InitWatchdog[Khởi Tạo Watchdog<br/>Kiểm tra mỗi 5s, timeout 30s]
    InitWatchdog --> InitHealthMonitor[Khởi Tạo Health Monitor<br/>Kiểm tra mỗi 1s, gửi heartbeat]
    InitHealthMonitor --> ConfigDrogon[Cấu Hình Drogon Server<br/>Max body size, Log level,<br/>Thread pool, Listener]
    ConfigDrogon --> StartServer[Khởi Động HTTP Server<br/>Listen trên host:port]
    StartServer --> Running{Server Đang Chạy}
    
    Running -->|Nhận HTTP Request| ReceiveRequest[HTTP Request Từ Client]
    ReceiveRequest --> ParseRequest[Parse HTTP Request<br/>Method, Path, Headers, Body]
    ParseRequest --> RouteRequest[Routing Request<br/>Drogon tìm handler phù hợp<br/>dựa trên path và method]
    RouteRequest --> ValidateRoute{Route Hợp Lệ?}
    ValidateRoute -->|Không| Return404[Trả về 404 Not Found]
    ValidateRoute -->|Có| ExecuteHandler[Thực Thi Handler<br/>Business Logic]
    ExecuteHandler --> ProcessLogic[Xử Lý Logic<br/>Validate input,<br/>Xử lý dữ liệu,<br/>Tạo response]
    ProcessLogic --> BuildResponse[Tạo JSON Response<br/>Status code, Headers, Body]
    BuildResponse --> SendResponse[Gửi Response Về Client]
    SendResponse --> Running
    
    Running -->|Signal Shutdown| ShutdownSignal[Nhận Signal<br/>SIGINT/SIGTERM]
    ShutdownSignal --> StopHealthMonitor[Dừng Health Monitor]
    StopHealthMonitor --> StopWatchdog[Dừng Watchdog]
    StopWatchdog --> StopServer[Dừng HTTP Server]
    StopServer --> Cleanup[Cleanup Resources]
    Cleanup --> End([Kết Thúc])
    
    InitWatchdog --> WatchdogLoop[Watchdog Loop<br/>Thread riêng]
    WatchdogLoop --> CheckHeartbeat[Kiểm Tra Heartbeat<br/>Mỗi 5 giây]
    CheckHeartbeat --> HeartbeatOK{Heartbeat OK?}
    HeartbeatOK -->|Có| UpdateStats[Cập Nhật Stats<br/>Đếm heartbeat]
    HeartbeatOK -->|Không| CheckTimeout{Kiểm Tra Timeout<br/>Quá 30s?}
    CheckTimeout -->|Có| TriggerRecovery[Kích Hoạt Recovery Action<br/>Log lỗi, xử lý recovery]
    CheckTimeout -->|Không| UpdateStats
    TriggerRecovery --> UpdateStats
    UpdateStats --> WatchdogLoop
    
    InitHealthMonitor --> HealthMonitorLoop[Health Monitor Loop<br/>Thread riêng]
    HealthMonitorLoop --> CollectMetrics[Thu Thập Metrics<br/>CPU, Memory, Request count]
    CollectMetrics --> SendHeartbeat[Gửi Heartbeat<br/>Đến Watchdog]
    SendHeartbeat --> SleepMonitor[Sleep 1 giây]
    SleepMonitor --> HealthMonitorLoop
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style Running fill:#87CEEB
    style ExecuteHandler fill:#FFD700
    style ProcessLogic fill:#FFD700
    style WatchdogLoop fill:#DDA0DD
    style HealthMonitorLoop fill:#DDA0DD
```

## Flow Xử Lý Request Chi Tiết

```mermaid
sequenceDiagram
    participant Client as Client Application
    participant Drogon as Drogon HTTP Server
    participant Router as Request Router
    participant Handler as API Handler
    participant Logic as Business Logic
    participant Response as Response Builder
    
    Note over Client,Response: Flow Xử Lý HTTP Request
    
    Client->>Drogon: HTTP Request<br/>(Method, Path, Headers, Body)
    Note right of Client: Ví dụ: GET /v1/core/health
    
    Drogon->>Drogon: Nhận Request<br/>Parse HTTP Protocol
    Note right of Drogon: Validate HTTP format<br/>Check headers<br/>Parse body nếu có
    
    Drogon->>Router: Chuyển Request<br/>Đến Router
    Note right of Router: Tìm handler phù hợp<br/>dựa trên path pattern
    
    Router->>Router: Tìm Handler<br/>Theo Path và Method
    Note right of Router: So khớp với<br/>METHOD_LIST_BEGIN/END<br/>trong handler
    
    alt Handler Không Tìm Thấy
        Router->>Response: Route Not Found
        Response->>Drogon: 404 Not Found
        Drogon->>Client: HTTP 404
    else Handler Tìm Thấy
        Router->>Handler: Gọi Handler Method
        Note right of Handler: Ví dụ: HealthHandler::getHealth()
        
        Handler->>Handler: Validate Request<br/>Parameters, Headers, Body
        Note right of Handler: Kiểm tra required fields<br/>Validate format<br/>Check permissions
        
        alt Validation Failed
            Handler->>Response: Build Error Response
            Response->>Drogon: 400 Bad Request
            Drogon->>Client: HTTP 400 + Error JSON
        else Validation Success
            Handler->>Logic: Xử Lý Business Logic
            Note right of Logic: Thực hiện logic nghiệp vụ<br/>Truy vấn dữ liệu<br/>Xử lý tính toán
            
            Logic->>Logic: Process Data
            Note right of Logic: Có thể gọi các<br/>core components<br/>(nếu cần)
            
            Logic->>Handler: Return Data
            Handler->>Response: Build JSON Response
            Note right of Response: Tạo Json::Value<br/>Set status code<br/>Add headers (CORS)
            
            Response->>Drogon: HTTP Response<br/>(Status, Headers, Body)
            Drogon->>Client: HTTP 200 + JSON
        end
    end
```

## Flow Khởi Động Server

```mermaid
flowchart TD
    Start([main.cpp - Khởi Động]) --> Init[Khởi Tạo Ứng Dụng]
    
    Init --> ReadEnv[Đọc Environment Variables]
    ReadEnv -->|API_HOST| CheckHost{Có API_HOST?}
    CheckHost -->|Có| UseHost[Sử dụng API_HOST]
    CheckHost -->|Không| DefaultHost[Default: 0.0.0.0]
    UseHost --> ParsePort
    DefaultHost --> ParsePort[Parse API_PORT]
    
    ParsePort -->|API_PORT| CheckPort{Có API_PORT?}
    CheckPort -->|Có| ValidatePort[Validate Port<br/>1-65535]
    CheckPort -->|Không| DefaultPort[Default: 8080]
    ValidatePort -->|Invalid| DefaultPort
    ValidatePort -->|Valid| UsePort[Sử dụng Port]
    DefaultPort --> UsePort
    
    UsePort --> RegisterSignal[Đăng Ký Signal Handlers]
    RegisterSignal -->|SIGINT| SignalHandler[signalHandler<br/>Graceful Shutdown]
    RegisterSignal -->|SIGTERM| SignalHandler
    
    SignalHandler --> CreateHandlers[Tạo Handler Instances]
    CreateHandlers --> HealthHandler[HealthHandler<br/>GET /v1/core/health]
    CreateHandlers --> VersionHandler[VersionHandler<br/>GET /v1/core/version]
    CreateHandlers --> WatchdogHandler[WatchdogHandler<br/>GET /v1/core/watchdog]
    CreateHandlers --> SwaggerHandler[SwaggerHandler<br/>GET /swagger, /openapi.yaml]
    
    HealthHandler --> AutoRegister[Drogon Auto-Register<br/>Handlers]
    VersionHandler --> AutoRegister
    WatchdogHandler --> AutoRegister
    SwaggerHandler --> AutoRegister
    
    AutoRegister --> InitWatchdog[Khởi Tạo Watchdog]
    InitWatchdog -->|check_interval: 5000ms| WatchdogConfig[Watchdog Config<br/>timeout: 30000ms]
    WatchdogConfig --> StartWatchdog[Start Watchdog Thread<br/>watchdogLoop]
    
    StartWatchdog --> InitHealthMonitor[Khởi Tạo Health Monitor]
    InitHealthMonitor -->|monitor_interval: 1000ms| HealthMonitorConfig[Health Monitor Config]
    HealthMonitorConfig --> StartHealthMonitor[Start Health Monitor Thread<br/>monitorLoop]
    StartHealthMonitor --> LinkWatchdog[Link Health Monitor<br/>với Watchdog]
    
    LinkWatchdog --> ConfigDrogon[Cấu Hình Drogon]
    ConfigDrogon --> SetBodySize[setClientMaxBodySize<br/>1MB]
    SetBodySize --> SetLogLevel[setLogLevel<br/>kInfo]
    SetLogLevel --> AddListener[addListener<br/>host:port]
    AddListener --> SetThreads[setThreadNum<br/>hardware_concurrency]
    
    SetThreads --> RunServer[run - Khởi Động Server]
    RunServer --> Listening[Server Đang Lắng Nghe<br/>Sẵn Sàng Nhận Request]
    
    Listening -->|Nhận Request| ProcessRequest[Xử Lý Request]
    Listening -->|Nhận Signal| Shutdown[Graceful Shutdown]
    
    Shutdown --> StopHealthMonitor[Dừng Health Monitor]
    StopHealthMonitor --> StopWatchdog[Dừng Watchdog]
    StopWatchdog --> Cleanup[Cleanup Resources]
    Cleanup --> Exit([Thoát])
    
    style Start fill:#90EE90
    style Listening fill:#87CEEB
    style Exit fill:#FFB6C1
    style ProcessRequest fill:#FFD700
    style StartWatchdog fill:#DDA0DD
    style StartHealthMonitor fill:#DDA0DD
```

## Background Services Flow

```mermaid
flowchart TD
    subgraph WatchdogService[Watchdog Service - Thread Riêng]
        WStart([Watchdog Start]) --> WInit[Khởi Tạo Watchdog<br/>check_interval: 5s<br/>timeout: 30s]
        WInit --> WRunning{Running Flag}
        WRunning -->|true| WLoop[Watchdog Loop]
        WLoop --> WCheck[Kiểm Tra Heartbeat<br/>Mỗi 5 giây]
        WCheck --> WLastHeartbeat[Lấy Last Heartbeat Time]
        WLastHeartbeat --> WCalculate[Tính Thời Gian<br/>Từ Last Heartbeat]
        WCalculate --> WTimeout{Quá Timeout?<br/>> 30s}
        WTimeout -->|Không| WUpdateStats[Cập Nhật Stats<br/>total_heartbeats++]
        WTimeout -->|Có| WRecovery[Kích Hoạt Recovery<br/>recovery_callback]
        WRecovery --> WUpdateRecovery[Stats:<br/>recovery_actions++<br/>missed_heartbeats++]
        WUpdateRecovery --> WUpdateStats
        WUpdateStats --> WSleep[Sleep check_interval<br/>5 giây]
        WSleep --> WRunning
        WRunning -->|false| WStop([Watchdog Stop])
    end
    
    subgraph HealthMonitorService[Health Monitor Service - Thread Riêng]
        HStart([Health Monitor Start]) --> HInit[Khởi Tạo Health Monitor<br/>monitor_interval: 1s]
        HInit --> HLink[Link với Watchdog]
        HLink --> HRunning{Running Flag}
        HRunning -->|true| HLoop[Monitor Loop]
        HLoop --> HCollectCPU[Thu Thập CPU Usage<br/>Đọc /proc/stat]
        HCollectCPU --> HCollectMemory[Thu Thập Memory Usage<br/>Đọc /proc/self/status]
        HCollectMemory --> HUpdateMetrics[Cập Nhật Metrics<br/>CPU %, Memory MB]
        HUpdateMetrics --> HSendHeartbeat[Gửi Heartbeat<br/>watchdog->heartbeat]
        HSendHeartbeat --> HUpdateStats[Stats:<br/>request_count<br/>error_count]
        HUpdateStats --> HSleep[Sleep monitor_interval<br/>1 giây]
        HSleep --> HRunning
        HRunning -->|false| HStop([Health Monitor Stop])
    end
    
    HSendHeartbeat -.->|heartbeat| WLastHeartbeat
    
    style WStart fill:#90EE90
    style WStop fill:#FFB6C1
    style HStart fill:#90EE90
    style HStop fill:#FFB6C1
    style WRecovery fill:#FF6B6B
    style HSendHeartbeat fill:#4ECDC4
```

## Mô Tả Các Component

### 1. Khởi Động Ứng Dụng
- **Đọc Environment Variables**: Đọc `API_HOST` và `API_PORT` từ môi trường
- **Parse và Validate Cấu Hình**: Kiểm tra và validate host, port (1-65535)
- **Đăng Ký Signal Handlers**: Xử lý SIGINT/SIGTERM để graceful shutdown
- **Tạo và Đăng Ký API Handlers**: Tạo instances của các handlers, Drogon tự động đăng ký
- **Khởi Tạo Watchdog**: Tạo Watchdog với interval 5s, timeout 30s
- **Khởi Tạo Health Monitor**: Tạo Health Monitor với interval 1s, link với Watchdog
- **Cấu Hình Drogon Server**: Set max body size, log level, thread pool, listener
- **Khởi Động HTTP Server**: Server bắt đầu lắng nghe trên host:port

### 2. Xử Lý HTTP Request
- **Nhận HTTP Request**: Drogon nhận request từ client
- **Parse HTTP Request**: Parse method, path, headers, body
- **Routing Request**: Drogon router tìm handler phù hợp dựa trên path pattern
- **Validate Route**: Kiểm tra route có tồn tại không
- **Thực Thi Handler**: Gọi method tương ứng trong handler
- **Xử Lý Logic**: Validate input, xử lý business logic, tạo response
- **Tạo JSON Response**: Build Json::Value, set status code, add headers
- **Gửi Response**: Trả về HTTP response cho client

### 3. Watchdog Service
- **Mục đích**: Giám sát ứng dụng, phát hiện khi ứng dụng không phản hồi
- **Cơ chế**: Chạy trên thread riêng, kiểm tra heartbeat mỗi 5 giây
- **Timeout**: Nếu không nhận heartbeat trong 30 giây, kích hoạt recovery
- **Recovery Action**: Gọi callback để xử lý khi phát hiện lỗi
- **Stats**: Đếm total heartbeats, missed heartbeats, recovery actions

### 4. Health Monitor Service
- **Mục đích**: Thu thập metrics về hệ thống và gửi heartbeat
- **Cơ chế**: Chạy trên thread riêng, thu thập metrics mỗi 1 giây
- **Metrics**: CPU usage, memory usage, request count, error count
- **Heartbeat**: Gửi heartbeat đến Watchdog để báo hiệu ứng dụng còn sống
- **Integration**: Liên kết với Watchdog để đảm bảo ứng dụng hoạt động bình thường

## Lưu Ý

1. **Thread Safety**: Watchdog và Health Monitor chạy trên threads riêng, sử dụng atomic flags và mutex để đảm bảo thread safety
2. **Graceful Shutdown**: Khi nhận signal, server sẽ dừng nhận request mới, xử lý request đang chạy, sau đó cleanup resources
3. **Auto Registration**: Drogon tự động đăng ký handlers khi tạo instance, không cần đăng ký thủ công
4. **Error Handling**: Mọi handler đều có try-catch để xử lý exception và trả về error response phù hợp
5. **CORS Headers**: Các response đều có CORS headers để hỗ trợ cross-origin requests

