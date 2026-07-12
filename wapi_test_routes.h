// Test WAPI routes to verify ESP32 compatibility

#ifdef USE_WIFI_WEB

// Test 1: Simple text response without headers (should fail)
WAPI("/test/noheaders", [](Print& out, String queryString) {
    out.print("Hello World");
}),

// Test 2: Simple text response with proper headers
WAPI("/test/withheaders", [](Print& out, String queryString) {
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: text/plain\r\n");
    out.print("Connection: close\r\n");
    out.print("\r\n");
    out.print("Hello World with headers");
}),

// Test 3: JSON response with headers
WAPI("/test/json", [](Print& out, String queryString) {
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: application/json\r\n");
    out.print("Connection: close\r\n");
    out.print("\r\n");
    out.print("{\"status\":\"ok\",\"message\":\"WAPI works!\"}");
}),

// Test 4: Echo query string
WAPI("/test/echo", [](Print& out, String queryString) {
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: text/plain\r\n");
    out.print("Connection: close\r\n");
    out.print("\r\n");
    out.print("Query String: ");
    out.print(queryString);
}),

// Test 5: JSON with CORS headers (modern UI compatibility)
WAPI("/test/cors", [](Print& out, String queryString) {
    out.print("HTTP/1.0 200 OK\r\n");
    out.print("Content-Type: application/json\r\n");
    out.print("Access-Control-Allow-Origin: *\r\n");
    out.print("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    out.print("Access-Control-Allow-Headers: Content-Type\r\n");
    out.print("Connection: close\r\n");
    out.print("\r\n");
    out.print("{\"cors\":\"enabled\",\"test\":\"success\"}");
}),

#endif