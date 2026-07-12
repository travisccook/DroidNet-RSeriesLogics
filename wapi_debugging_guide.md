# WAPI Routes Debugging Guide for ESP32

## Summary of Findings

After analyzing the WifiWebServer implementation, here's what I found about WAPI routes on ESP32:

### 1. How WAPI Routes Work

- WAPI routes are handled through the `fAPIProc` callback in the WPage class
- When a GET request matches a WAPI route URL, the callback is invoked with a Print stream and query string
- **Important**: WAPI callbacks MUST send complete HTTP responses including headers
- The framework does NOT automatically add HTTP headers for WAPI routes

### 2. Correct WAPI Response Format

WAPI routes must send responses in this format:

```cpp
WAPI("/api/example", [](Print& out, String queryString) {
    // MUST send HTTP headers first
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: application/json\r\n");
    out.print("Access-Control-Allow-Origin: *\r\n");  // For CORS if needed
    out.print("Connection: close\r\n");
    out.print("\r\n");  // Empty line to end headers
    
    // Then send the body
    out.print("{\"status\":\"ok\"}");
}),
```

### 3. Common Issues and Solutions

#### Issue 1: No Response or Empty Response
- **Cause**: Missing HTTP headers
- **Solution**: Always send complete HTTP response headers before the body

#### Issue 2: JSON Not Parsing
- **Cause**: Wrong Content-Type header
- **Solution**: Use `Content-Type: application/json` for JSON responses

#### Issue 3: CORS Errors in Browser
- **Cause**: Missing CORS headers when accessed from web apps
- **Solution**: Add `Access-Control-Allow-Origin: *` header

#### Issue 4: Connection Issues
- **Cause**: Missing or incorrect Connection header
- **Solution**: Always include `Connection: close` header

### 4. ESP32-Specific Considerations

1. **Memory Buffering**: The web server uses `RamBufferedPrintStream` with a 4KB buffer
   - Large responses may need to be flushed
   - Memory allocation failures could cause issues

2. **String Handling**: ESP32 String implementation may have size limits
   - Use streaming for large responses instead of building complete strings

3. **Query String Parsing**: The query string passed to WAPI is extracted from the header
   - May be empty if no query parameters are present
   - URL decoding may be needed for special characters

### 5. Testing WAPI Routes

I've created a test file `wapi_test_routes.h` with examples of different WAPI patterns:
- `/test/withheaders` - Basic text response with headers
- `/test/json` - JSON response
- `/test/echo` - Echo query parameters
- `/test/cors` - JSON with CORS headers

### 6. Differences from WPage Routes

- **WPage routes**: Framework handles HTTP headers automatically
- **WAPI routes**: You must send all HTTP headers manually
- **WPage routes**: Use WElement system for HTML generation
- **WAPI routes**: Direct Print stream access for any response type

### 7. Debugging Tips

1. Use Serial debugging to verify the WAPI callback is being called
2. Check that the URL exactly matches (case-sensitive)
3. Verify headers are sent before any body content
4. Test with curl or a browser's network inspector to see raw responses
5. Check ESP32 free heap to ensure sufficient memory

### 8. Example Working WAPI Route

```cpp
WAPI("/api/status", [](Print& out, String queryString) {
    // Send headers
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: application/json\r\n");
    out.print("Access-Control-Allow-Origin: *\r\n");
    out.print("Connection: close\r\n");
    out.print("\r\n");
    
    // Send JSON body
    DynamicJsonDocument doc(256);
    doc["status"] = "online";
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    serializeJson(doc, out);
}),
```

This should help identify why WAPI routes might not be working properly on your ESP32 setup.