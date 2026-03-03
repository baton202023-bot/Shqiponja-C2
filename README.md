# Shqiponja-C2 
**A lightweight, TLS-encrypted Command & Control (C2) framework for educational red teaming and detection engineering.**

Shqiponja (The Eagle") is a Proof-of-Concept (PoC) designed to demonstrate the core mechanics of modern asynchronous malware. This project showcases how agents can use native Windows libraries to "smuggle" data over encrypted channels.

> **⚠️ WARNING:** This tool is strictly for authorized security testing and educational research. Unauthorized use against systems you do not own is illegal. The author assumes no liability for misuse.

---

## Project Overview
Unlike basic socket-based scripts, **Shqiponja-C2** utilizes the native ``WinHTTP`` library and TLS encryption to mimic legitimate web traffic. This makes it an ideal tool for testing the "blind spots" of modern EDR and AV solutions.

### Key Features
* **Encrypted Flight (TLS):** All traffic is wrapped in HTTPS using Python's ``ssl`` module and C++ ``WinHTTP`` with ``WINHTTP_FLAG_SECURE``.
* **The Nest (Python C2):** A multi-threaded server capable of managing a command queue and intercepting Base64-encoded file transfers.
* **The Talon (C++ Agent):** A stealthy DLL agent that executes commands via hidden pipes and beacons to the server.
* **Automated Delivery:** Includes a VBA stager that utilizes ``certutil.exe`` (Living off the Land) to fetch and execute the payload.

---

## 🛠️ Architecture



1.  **Stager (VBA):** Downloads the DLL using ``certutil`` and executes it via ``rundll32.exe``.
2.  **Beaconing:** The agent initiates a TLS handshake and sends a JSON "init" packet.
3.  **Tasking:** The server responds with a queued command (e.g., ``SHELL``, ``UPLOAD``, ``DOWNLOAD``).
4.  **Exfiltration:** Results are Base64 encoded, wrapped in a JSON object, and sent back via an encrypted POST request.

---

## 🚀 Setup & Usage

### 1. Prepare the Nest (Server)
Generate a self-signed certificate for the HTTPS listener:
```
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
```

Run the server:
```
python server.py
```

### 2. Compile the Talon (Agent)
Compile the C++ code as a **Dynamic Link Library (DLL)**. Ensure you link the ``winhttp.lib`` and ``ws2_32.lib`` libraries.

### 3. Deploy the Flight (Stager)
Embed the provided VBA code into a Word or Excel document. Update the ``payloadURL`` to point to your server's IP.

---

## 🛡️ Detection Engineering (Blue Team)
This project is designed to help defenders identify the following indicators:
* **Process Anomalies:** ``rundll32.exe`` spawning ``cmd.exe`` or ``powershell.exe``.
* **Network Patterns:** Periodic HTTPS POST requests to non-reputable IP addresses (Beaconing).
* **LOLBins:** Use of ``certutil -urlcache`` to fetch remote binaries.
* **Suspicious Strings:** Base64 encoded strings within JSON payloads in web logs.

---
## 📜 License
Distributed under the MIT License.
