#!/usr/bin/env python3
"""
LoRa Environment Monitor — Web Dashboard Server
Reads serial data from STM32 + SX1278 gateway and serves a live web dashboard.
"""

import json
import re
import time
import threading
from queue import Queue

import serial
import serial.tools.list_ports
from flask import Flask, Response, render_template, request, jsonify

app = Flask(__name__)

# --- Config ---
BAUD = 9600
DEFAULT_PORT = "/dev/ttyUSB0"

# --- State ---
latest: dict = {}
lock = threading.Lock()
event_queue: Queue = Queue()

# --- Pattern to match STM32 output ---
RE_BLOCK = re.compile(
    r"--- Tram Do Moi Truong ---\s*"
    r"Station ID:\s*(\d+)\s*"
    r"Nhiet do:\s*(-?\d+)\.(\d+)\s*C\s*"
    r"Do am:\s*(-?\d+)\.(\d+)\s*%\s*"
    r"Ap suat:\s*(-?\d+)\.(\d+)\s*hPa\s*"
    r"RSSI:\s*(-?\d+)\s*dBm\s*/\s*SNR:\s*(-?\d+)\s*dB",
    re.MULTILINE,
)


def parse_block(text: str) -> dict | None:
    m = RE_BLOCK.search(text)
    if not m:
        return None
    return {
        "station_id": int(m.group(1)),
        "temp": float(f"{m.group(2)}.{m.group(3)}"),
        "hum": float(f"{m.group(4)}.{m.group(5)}"),
        "press": float(f"{m.group(6)}.{m.group(7)}"),
        "rssi": int(m.group(8)),
        "snr": int(m.group(9)),
        "time": time.strftime("%H:%M:%S"),
    }


def serial_reader(port: str):
    """Background thread: read serial, parse, update state."""
    global latest
    buf = ""
    try:
        ser = serial.Serial(port, BAUD, timeout=1)
        print(f"[serial] Opened {port} @ {BAUD}")
    except Exception as e:
        print(f"[serial] ERROR opening {port}: {e}")
        return

    while True:
        try:
            chunk = ser.read(ser.in_waiting or 1).decode(errors="replace")
            buf += chunk

            # Find complete blocks (terminated by blank line)
            while "\r\n\r\n" in buf or "\n\n" in buf:
                idx = buf.index("\n\n") if "\n\n" in buf else buf.index("\r\n\r\n")
                block = buf[:idx].strip()
                buf = buf[idx + 2 :]

                parsed = parse_block(block)
                if parsed:
                    with lock:
                        latest = parsed
                    event_queue.put(json.dumps(parsed))
        except (serial.SerialException, OSError):
            print("[serial] Disconnected, retrying...")
            time.sleep(2)
            try:
                ser.close()
            except Exception:
                pass
            try:
                ser = serial.Serial(port, BAUD, timeout=1)
            except Exception:
                time.sleep(2)


# --- SSE endpoint ---
@app.route("/events")
def sse():
    def stream():
        # Send latest on connect
        with lock:
            if latest:
                yield f"data: {json.dumps(latest)}\n\n"

        while True:
            msg = event_queue.get()
            yield f"data: {msg}\n\n"

    return Response(stream(), mimetype="text/event-stream")


@app.route("/api/latest")
def api_latest():
    with lock:
        return jsonify(latest if latest else {})


@app.route("/api/ports")
def api_ports():
    ports = [p.device for p in serial.tools.list_ports.comports()]
    return jsonify(ports)


# --- Page ---
@app.route("/")
def index():
    return render_template("index.html")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="LoRa Environment Dashboard")
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"Serial port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--host",
        default="0.0.0.0",
        help="Bind address (default: 0.0.0.0)",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Flask debug mode",
    )
    args = parser.parse_args()

    # Start serial reader thread
    t = threading.Thread(target=serial_reader, args=(args.port,), daemon=True)
    t.start()

    print(f"[web] Dashboard: http://localhost:5000")
    app.run(host=args.host, port=5000, debug=args.debug)


if __name__ == "__main__":
    main()
