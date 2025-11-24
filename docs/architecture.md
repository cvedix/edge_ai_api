# Edge AI REST API - Architecture Diagram

## System Architecture

```mermaid
graph TB
    Client[Client Application] -->|HTTP Request| API[REST API Server<br/>Drogon Framework]
    API --> HealthHandler[Health Handler<br/>/v1/core/health]
    API --> VersionHandler[Version Handler<br/>/v1/core/version]
    
    HealthHandler -->|JSON Response| Client
    VersionHandler -->|JSON Response| Client
    
    subgraph "API Endpoints"
        HealthHandler
        VersionHandler
    end
    
    subgraph "Server Components"
        API
        Config[Configuration<br/>Host/Port/Threads]
    end
    
    Config --> API
```

## Request Flow

```mermaid
sequenceDiagram
    participant Client
    participant Server as API Server
    participant Handler as Endpoint Handler
    
    Client->>Server: GET /v1/core/health
    Server->>Handler: Route request
    Handler->>Handler: Process request
    Handler->>Handler: Generate JSON response
    Handler->>Server: Return response
    Server->>Client: HTTP 200 + JSON
```

## Component Structure

```mermaid
graph LR
    A[main.cpp] --> B[HealthHandler]
    A --> C[VersionHandler]
    B --> D[Health Logic]
    C --> E[Version Logic]
    D --> F[JSON Response]
    E --> F
```

## API Endpoints

```mermaid
graph TD
    API[Edge AI API<br/>:8080] --> Health[/v1/core/health<br/>GET]
    API --> Version[/v1/core/version<br/>GET]
    
    Health --> HealthResp{Response}
    HealthResp -->|200 OK| HealthJSON[JSON:<br/>status, timestamp, uptime]
    
    Version --> VersionResp{Response}
    VersionResp -->|200 OK| VersionJSON[JSON:<br/>version, build_time, git_commit]
```

