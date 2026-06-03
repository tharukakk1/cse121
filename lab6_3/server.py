from http.server import BaseHTTPRequestHandler, HTTPServer

# Configure your station's location here (e.g., Santa-Cruz)
STATION_LOCATION = "Santa-Cruz"

class IntegratedWeatherServer(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/location':
            # 1. Prepare the location response payload
            response_body = STATION_LOCATION.encode('utf-8')
            
            # 2. Send 200 OK headers with explicit length
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.send_header('Content-Length', str(len(response_body)))
            self.end_headers()
            
            # 3. Transmit the location text
            self.wfile.write(response_body)
            print(f"[SERVER LOG] Handed out location '{STATION_LOCATION}' to ESP32 Client.")
        else:
            self.send_error(404, "Endpoint Not Found")

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length).decode('utf-8')
        
        print("\n" + "="*40)
        print("[SERVER LOG] Received Combined Weather Update!")
        # The payload will look like: Location: Santa-Cruz | Outdoor: +17C | Onboard: 26.5C
        print(f"{post_data}")
        print("="*40 + "\n")
        
        response_body = b"OK"
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.send_header('Content-Length', str(len(response_body)))
        self.end_headers()
        self.wfile.write(response_body)

if __name__ == '__main__':
    server_address = ('', 1234)
    print(f"Starting Integrated Weather Server on port 1234... Location: {STATION_LOCATION}")
    httpd = HTTPServer(server_address, IntegratedWeatherServer)
    httpd.serve_forever()