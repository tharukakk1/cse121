from http.server import BaseHTTPRequestHandler, HTTPServer

class WeatherServer(BaseHTTPRequestHandler):
    # def do_POST(self):
    #     # 1. Read the length of the incoming data payload [cite: 571]
    #     content_length = int(self.headers['Content-Length'])
    #     post_data = self.rfile.read(content_length).decode('utf-8')
        
    #     # 2. Print/Log the received temperature info precisely 
    #     print(f"[SERVER LOG] Received HTTP POST from ESP32!")
    #     print(f"Payload Data: {post_data}\n")
        
    #     # 3. Send a clean 200 OK HTTP response back to the client
    #     self.send_response(200)
    #     self.send_header('Content-type', 'text/plain')
    #     self.end_headers()
    #     self.wfile.write(b"OK")
    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length).decode('utf-8')
        
        print(f"[SERVER LOG] Received HTTP POST from ESP32!")
        print(f"Payload Data: {post_data}\n")
        
        # Define response body
        response_body = b"OK"
        
        # Send headers with explicit Content-Length
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.send_header('Content-Length', str(len(response_body))) # <-- FIX HERE
        self.end_headers()
        self.wfile.write(response_body)

# Run the server on port 1234 as strictly specified by the lab guide [cite: 571]
if __name__ == '__main__':
    server_address = ('', 1234)
    print("Starting local HTTP POST server on port 1234...")
    httpd = HTTPServer(server_address, WeatherServer)
    httpd.serve_forever()