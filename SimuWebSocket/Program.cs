using System.Collections.Concurrent;
using System.Net;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SimuWebSocketServer;

class Program
{
    // ── 配置 ────────────────────────────────────────────────────────────
    static string host           = "localhost";
    static int    port           = 8080;
    static string wsPath         = "/ws";
    static string dataFilePath   = "data.json";
    static string treeFilePath   = "tree.json";
    static int    sendIntervalMs = 1000;

    // ── 运行时状态 ────────────────────────────────────────────────────────
    static readonly ConcurrentDictionary<string, WebSocket> _clients = new();
    static string   _currentPayload = "{}";   // 上次读取的推送内容（原始 JSON 字符串）
    static readonly object _payloadLock = new();

    // ────────────────────────────────────────────────────────────────────
    static async Task Main(string[] args)
    {
        LoadConfig();

        Console.OutputEncoding = Encoding.UTF8;
        Console.WriteLine("====================================");
        Console.WriteLine(" SCADA WebSocket 模拟服务器");
        Console.WriteLine("====================================");
        Console.WriteLine($"  监听地址 : http://{host}:{port}/");
        Console.WriteLine($"  WS 路径  : {wsPath}");
        Console.WriteLine($"  数据文件 : {Path.GetFullPath(dataFilePath)}");
        Console.WriteLine($"  树形文件 : {Path.GetFullPath(treeFilePath)}");
        Console.WriteLine($"  推送间隔 : {sendIntervalMs} ms");
        Console.WriteLine("====================================");
        Console.WriteLine("提示：直接修改 data.json 即可更换推送数据（无需重启）");
        Console.WriteLine("      Ctrl+C 退出程序");
        Console.WriteLine();

        // 初始加载数据文件
        ReloadData();

        // 启动 HTTP 监听
        var prefix = $"http://{host}:{port}/";
        var listener = new HttpListener();
        listener.Prefixes.Add(prefix);
        try
        {
            listener.Start();
        }
        catch (HttpListenerException ex)
        {
            Console.Error.WriteLine($"[ERROR] 无法启动监听器: {ex.Message}");
            Console.Error.WriteLine("  请检查端口是否被占用，或以管理员身份运行。");
            Console.Error.WriteLine("  若使用非 localhost 地址，需先执行:");
            Console.Error.WriteLine($"    netsh http add urlacl url=http://{host}:{port}/ user=Everyone");
            Environment.Exit(1);
        }

        Console.WriteLine($"[INFO] 服务器已启动，等待连接...");

        // 后台：定时推送任务
        _ = Task.Run(DataPushLoopAsync);

        // 主循环：接受连接
        while (true)
        {
            HttpListenerContext ctx;
            try
            {
                ctx = await listener.GetContextAsync();
            }
            catch (HttpListenerException)
            {
                break;   // 服务器被停止
            }

            _ = Task.Run(() => HandleRequestAsync(ctx));
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  配置加载
    // ────────────────────────────────────────────────────────────────────
    static void LoadConfig()
    {
        const string cfgFile = "appsettings.json";
        if (!File.Exists(cfgFile))
        {
            Console.WriteLine($"[WARN] 未找到 {cfgFile}，使用默认配置");
            return;
        }

        try
        {
            var text = File.ReadAllText(cfgFile, Encoding.UTF8);
            var root = JsonNode.Parse(text);
            if (root == null) return;

            var srv = root["Server"];
            if (srv != null)
            {
                host = srv["Host"]?.GetValue<string>() ?? host;
                port = srv["Port"]?.GetValue<int>()    ?? port;
                wsPath = srv["WebSocketPath"]?.GetValue<string>() ?? wsPath;
            }

            dataFilePath   = root["DataFile"]?.GetValue<string>()    ?? dataFilePath;
            treeFilePath   = root["TreeFile"]?.GetValue<string>()    ?? treeFilePath;
            sendIntervalMs = root["SendIntervalMs"]?.GetValue<int>() ?? sendIntervalMs;

            Console.WriteLine($"[INFO] 配置加载成功");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WARN] 配置文件解析失败: {ex.Message}，使用默认配置");
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  数据文件热加载
    // ────────────────────────────────────────────────────────────────────
    static void ReloadData()
    {
        if (!File.Exists(dataFilePath))
        {
            Console.WriteLine($"[WARN] 数据文件不存在: {Path.GetFullPath(dataFilePath)}");
            return;
        }

        try
        {
            var text = File.ReadAllText(dataFilePath, Encoding.UTF8);
            // 验证是合法 JSON
            JsonNode.Parse(text);
            lock (_payloadLock) { _currentPayload = text; }
            Console.WriteLine($"[DATA] 数据文件加载成功 ({text.Length} 字节)");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WARN] 数据文件读取/解析失败: {ex.Message}，继续使用上次数据");
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  HTTP/WebSocket 请求分发
    // ────────────────────────────────────────────────────────────────────
    static async Task HandleRequestAsync(HttpListenerContext ctx)
    {
        var req  = ctx.Request;
        var path = req.Url?.AbsolutePath ?? "/";

        // WebSocket 升级
        if (req.IsWebSocketRequest && path.Equals(wsPath, StringComparison.OrdinalIgnoreCase))
        {
            await HandleWebSocketAsync(ctx);
            return;
        }

        // HTTP 接口：GET /scada/getTagsTree
        if (req.HttpMethod == "GET" &&
            path.Equals("/scada/getTagsTree", StringComparison.OrdinalIgnoreCase))
        {
            await ServeJsonFileAsync(ctx.Response, treeFilePath);
            return;
        }

        // HTTP 接口：GET /scada/getRealData（同步查询当前数据）
        if (path.Equals("/scada/getRealData", StringComparison.OrdinalIgnoreCase))
        {
            string payload;
            lock (_payloadLock) { payload = _currentPayload; }
            await SendHttpResponseAsync(ctx.Response, 200, payload, "application/json");
            return;
        }

        // 其余请求返回 404
        await SendHttpResponseAsync(ctx.Response, 404, "{\"error\":\"not found\"}", "application/json");
    }

    // ────────────────────────────────────────────────────────────────────
    //  WebSocket 连接处理
    // ────────────────────────────────────────────────────────────────────
    static async Task HandleWebSocketAsync(HttpListenerContext ctx)
    {
        HttpListenerWebSocketContext wsCtx;
        try
        {
            wsCtx = await ctx.AcceptWebSocketAsync(null);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WS] WebSocket 握手失败: {ex.Message}");
            ctx.Response.StatusCode = 500;
            ctx.Response.Close();
            return;
        }

        var ws    = wsCtx.WebSocket;
        var id    = Guid.NewGuid().ToString("N")[..8];
        var remoteEp = ctx.Request.RemoteEndPoint?.ToString() ?? "unknown";

        _clients.TryAdd(id, ws);
        Console.WriteLine($"[WS] 客户端连接: {id} ({remoteEp})，当前连接数: {_clients.Count}");

        var buffer = new byte[8192];
        try
        {
            while (ws.State == WebSocketState.Open)
            {
                var result = await ws.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);

                if (result.MessageType == WebSocketMessageType.Close)
                {
                    await ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "bye", CancellationToken.None);
                    break;
                }

                if (result.MessageType == WebSocketMessageType.Text)
                {
                    var msg = Encoding.UTF8.GetString(buffer, 0, result.Count);
                    Console.WriteLine($"[WS] 收到 [{id}]: {msg}");
                    // 客户端发来 read 请求时，立即回复当前数据
                    await HandleClientMessageAsync(ws, msg);
                }
            }
        }
        catch (WebSocketException ex) when (ex.WebSocketErrorCode == WebSocketError.ConnectionClosedPrematurely)
        {
            // 客户端异常断开，忽略
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WS] 连接 [{id}] 异常: {ex.Message}");
        }
        finally
        {
            _clients.TryRemove(id, out _);
            Console.WriteLine($"[WS] 客户端断开: {id}，剩余连接数: {_clients.Count}");
            if (ws.State != WebSocketState.Closed)
                ws.Dispose();
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  处理客户端发来的 WebSocket 消息
    // ────────────────────────────────────────────────────────────────────
    static async Task HandleClientMessageAsync(WebSocket ws, string message)
    {
        try
        {
            var root = JsonNode.Parse(message);
            var type = root?["type"]?.GetValue<string>();

            if (type == "read")
            {
                // 客户端请求读取数据，立即推送当前数据
                string payload;
                lock (_payloadLock) { payload = _currentPayload; }
                await SendWsTextAsync(ws, payload);
            }
            // 其他类型消息可在此扩展（如 write、subscribe 等）
        }
        catch
        {
            // 消息解析失败，忽略
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  定时推送循环（每秒向所有客户端广播）
    // ────────────────────────────────────────────────────────────────────
    static async Task DataPushLoopAsync()
    {
        var lastFileWriteTime = DateTime.MinValue;

        while (true)
        {
            await Task.Delay(sendIntervalMs);

            // 检测数据文件是否被修改，自动热加载
            try
            {
                if (File.Exists(dataFilePath))
                {
                    var wt = File.GetLastWriteTimeUtc(dataFilePath);
                    if (wt != lastFileWriteTime)
                    {
                        lastFileWriteTime = wt;
                        ReloadData();
                    }
                }
            }
            catch { /* 忽略文件检测异常 */ }

            if (_clients.IsEmpty) continue;

            string payload;
            lock (_payloadLock) { payload = _currentPayload; }

            var deadClients = new List<string>();
            foreach (var (id, ws) in _clients)
            {
                try
                {
                    if (ws.State == WebSocketState.Open)
                        await SendWsTextAsync(ws, payload);
                    else
                        deadClients.Add(id);
                }
                catch
                {
                    deadClients.Add(id);
                }
            }

            foreach (var id in deadClients)
            {
                _clients.TryRemove(id, out var dead);
                dead?.Dispose();
            }

            if (deadClients.Count > 0)
                Console.WriteLine($"[WS] 清理断开连接 {deadClients.Count} 个，剩余: {_clients.Count}");
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  辅助：发送 WebSocket 文本帧
    // ────────────────────────────────────────────────────────────────────
    static async Task SendWsTextAsync(WebSocket ws, string text)
    {
        var bytes = Encoding.UTF8.GetBytes(text);
        await ws.SendAsync(
            new ArraySegment<byte>(bytes),
            WebSocketMessageType.Text,
            true,
            CancellationToken.None);
    }

    // ────────────────────────────────────────────────────────────────────
    //  辅助：读取 JSON 文件并作为 HTTP 响应返回
    // ────────────────────────────────────────────────────────────────────
    static async Task ServeJsonFileAsync(HttpListenerResponse resp, string filePath)
    {
        if (!File.Exists(filePath))
        {
            await SendHttpResponseAsync(resp, 404,
                $"{{\"error\":\"file not found: {filePath}\"}}",
                "application/json");
            return;
        }

        try
        {
            var content = await File.ReadAllTextAsync(filePath, Encoding.UTF8);
            await SendHttpResponseAsync(resp, 200, content, "application/json");
        }
        catch (Exception ex)
        {
            await SendHttpResponseAsync(resp, 500,
                $"{{\"error\":\"{ex.Message}\"}}",
                "application/json");
        }
    }

    // ────────────────────────────────────────────────────────────────────
    //  辅助：发送 HTTP 响应
    // ────────────────────────────────────────────────────────────────────
    static async Task SendHttpResponseAsync(
        HttpListenerResponse resp,
        int statusCode,
        string body,
        string contentType)
    {
        try
        {
            resp.StatusCode  = statusCode;
            resp.ContentType = $"{contentType}; charset=utf-8";
            resp.Headers.Add("Access-Control-Allow-Origin", "*");
            var bytes = Encoding.UTF8.GetBytes(body);
            resp.ContentLength64 = bytes.Length;
            await resp.OutputStream.WriteAsync(bytes);
            resp.Close();
        }
        catch { /* 客户端已断开，忽略 */ }
    }
}
