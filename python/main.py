#!/usr/bin/env python3
import os
import sys
import time
import socket
import logging
import json
import threading
from urllib.request import Request, urlopen
from flask import Flask, jsonify

# --- Configuration ---
USERNAME = ""
MINING_KEY = ""
RIG_IDENTIFIER = "Uno Q"
SOFTWARE_NAME = "Arduino Uno Q STM32 HASH Miner"
DIFFICULTY_TIER = "STM32"
DUCOID = "DUCOID2137"
WEB_PORT = 7000

# Setup basic logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)

from arduino.app_utils import *
@call()
def ducos1a(lastHash: str, expHash: str, difficulty: int) -> int:
    pass 

# --- Global Stats for Dashboard ---
mining_stats = {
    "status": "Booting...",
    "hashrate": 0,
    "hashrate_human": "0 H/s",
    "accepted": 0,
    "rejected": 0,
    "uptime_seconds": 0,
    "last_ping": "Never",
    "current_diff": 0,
    "spm": 0.0,
    "recent_shares": [],     # Stores the last 10 accepted shares
    "rejected_shares": []    # Stores the last 50 rejected shares
}

start_time_global = time.time()

# --- Networking Helper Functions ---
def fetch_fastest_node():
    try:
        req = Request(
            "https://server.duinocoin.com/getPool",
            headers={'User-Agent': 'Arduino Uno Q Miner'}
        )
        with urlopen(req, timeout=10) as response:
            data = json.loads(response.read().decode())
            return data["ip"], int(data["port"])
    except Exception as e:
        logging.warning(f"Failed to fetch pool node ({e}). Using default.")
        return "server.duinocoin.com", 2813

# --- Mining Logic (Runs in Main Thread) ---
def mine():
    global mining_stats
    while True:
        node_ip, node_port = fetch_fastest_node()
        mining_stats["status"] = f"Connecting to {node_ip}:{node_port}"
        
        sock = None
        try:
            sock = socket.create_connection((node_ip, node_port), timeout=60)
            server_version = sock.recv(100).decode().strip()
            mining_stats["status"] = f"Mining (Server v{server_version})"
            
            while True:
                # Request Job
                job_req = f"JOB,{USERNAME},{DIFFICULTY_TIER},{MINING_KEY}"
                sock.sendall(job_req.encode('utf-8'))

                # Receive Job
                job_data = sock.recv(1024).decode().strip()
                if not job_data:
                    break

                job_parts = job_data.split(",")
                if len(job_parts) < 3:
                    continue

                last_hash, exp_hash, difficulty = job_parts[0], job_parts[1], int(job_parts[2])
                mining_stats["current_diff"] = difficulty

                # Mine the Job
                job_start = time.time()
                nonce = ducos1a(last_hash, exp_hash, difficulty, timeout=300)
                job_end = time.time()

                # Calculate Stats
                elapsed = job_end - job_start
                hashrate = nonce / elapsed if elapsed > 0 else 0
                hr_human = f"{hashrate/1000:.2f} kH/s" if hashrate > 1000 else f"{int(hashrate)} H/s"
                
                # Send Results
                result_str = f"{nonce},{hashrate},{SOFTWARE_NAME},{RIG_IDENTIFIER},{DUCOID}"
                sock.sendall(result_str.encode('utf-8'))
                feedback = sock.recv(1024).decode().strip()

                # Update Global Dashboard State
                mining_stats["hashrate"] = hashrate
                mining_stats["hashrate_human"] = hr_human
                mining_stats["last_ping"] = time.strftime("%H:%M:%S")
                mining_stats["uptime_seconds"] = int(time.time() - start_time_global)

                timestamp = time.strftime("%H:%M:%S")

                if "GOOD" in feedback:
                    mining_stats["accepted"] += 1
                    uptime_mins = mining_stats["uptime_seconds"] / 60
                    mining_stats["spm"] = round(mining_stats["accepted"] / uptime_mins, 2) if uptime_mins > 0 else 0
                    
                    # Add to recent shares list (Insert at beginning)
                    mining_stats["recent_shares"].insert(0, {
                        "time": timestamp,
                        "solve_time": f"{elapsed:.2f}",
                        "hashrate": hr_human
                    })
                    # Keep only the last 10
                    mining_stats["recent_shares"] = mining_stats["recent_shares"][:10]

                elif "BAD" in feedback:
                    mining_stats["rejected"] += 1
                    
                    # Add to rejected list (Insert at beginning)
                    mining_stats["rejected_shares"].insert(0, {
                        "time": timestamp,
                        "nonce": nonce,
                        "reason": feedback
                    })
                    # Keep only the last 50 to prevent memory bloat over time
                    mining_stats["rejected_shares"] = mining_stats["rejected_shares"][:50]

        except KeyboardInterrupt:
            logging.info("Miner stopped by user.")
            if sock: sock.close()
            sys.exit(0)
        except Exception as e:
            mining_stats["status"] = f"Network error, retrying..."
            if sock: sock.close()
            time.sleep(5)

# --- Web Dashboard Code (Runs in Background Thread) ---

app = Flask(__name__)

DASHBOARD_HTML = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>STM32 HASH Miner UI</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body { background-color: #0f172a; color: #e2e8f0; }
        .neon-card { background: #1e293b; border: 1px solid #334155; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.5); }
        .neon-text-green { color: #4ade80; text-shadow: 0 0 10px rgba(74, 222, 128, 0.5); }
        .neon-text-blue { color: #38bdf8; text-shadow: 0 0 10px rgba(56, 189, 248, 0.5); }
        
        /* Custom Scrollbar for the tables */
        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: #1e293b; border-radius: 4px; }
        ::-webkit-scrollbar-thumb { background: #475569; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #64748b; }
    </style>
</head>
<body class="p-8 font-sans antialiased">
    <div class="max-w-5xl mx-auto">
        <header class="mb-8 border-b border-slate-700 pb-4">
            <h1 class="text-3xl font-bold text-slate-100 tracking-tight">DUCO STM32 Hardware Miner</h1>
            <p class="text-slate-400 mt-2">Rig: <span class="text-slate-300 font-mono">Uno Q</span> | Status: <span id="ui-status" class="text-yellow-400 font-semibold">Connecting...</span></p>
        </header>

        <div class="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
            <!-- Hashrate Card -->
            <div class="neon-card p-6 rounded-xl text-center flex flex-col justify-center">
                <h2 class="text-slate-400 uppercase tracking-widest text-sm font-bold mb-2">Live Hashrate</h2>
                <div id="ui-hashrate" class="text-5xl font-black neon-text-green font-mono tracking-tighter">0.00 kH/s</div>
                <p class="text-slate-500 text-xs mt-4">Last update: <span id="ui-ping">Never</span></p>
            </div>

            <!-- Stats Grid -->
            <div class="grid grid-cols-2 gap-4">
                <div class="neon-card p-4 rounded-xl">
                    <h3 class="text-slate-400 text-xs font-bold uppercase mb-1">Accepted</h3>
                    <p id="ui-accepted" class="text-2xl font-bold neon-text-blue">0</p>
                </div>
                <div class="neon-card p-4 rounded-xl">
                    <h3 class="text-slate-400 text-xs font-bold uppercase mb-1">Rejected</h3>
                    <p id="ui-rejected" class="text-2xl font-bold text-red-400">0</p>
                </div>
                <div class="neon-card p-4 rounded-xl">
                    <h3 class="text-slate-400 text-xs font-bold uppercase mb-1">Difficulty</h3>
                    <p id="ui-diff" class="text-xl font-bold text-slate-200">0</p>
                </div>
                <div class="neon-card p-4 rounded-xl">
                    <h3 class="text-slate-400 text-xs font-bold uppercase mb-1">Shares / Min</h3>
                    <p id="ui-spm" class="text-xl font-bold text-slate-200">0.0</p>
                </div>
            </div>
        </div>

        <!-- Logs Section -->
        <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
            
            <!-- Recent Shares -->
            <div class="neon-card p-4 rounded-xl flex flex-col">
                <h3 class="text-slate-400 text-sm font-bold uppercase mb-3 border-b border-slate-700 pb-2">Recent Shares (Last 10)</h3>
                <div class="overflow-y-auto flex-grow max-h-64">
                    <table class="w-full text-sm text-left text-slate-300">
                        <thead class="text-xs text-slate-500 uppercase bg-slate-800/80 sticky top-0">
                            <tr>
                                <th class="px-3 py-2 font-semibold">Time</th>
                                <th class="px-3 py-2 font-semibold">Solve Time</th>
                                <th class="px-3 py-2 font-semibold">Hashrate</th>
                            </tr>
                        </thead>
                        <tbody id="ui-recent-shares">
                            <tr><td colspan="3" class="px-3 py-4 text-center text-slate-600">Waiting for shares...</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>

            <!-- Rejected Shares -->
            <div class="neon-card p-4 rounded-xl flex flex-col">
                <h3 class="text-red-400/80 text-sm font-bold uppercase mb-3 border-b border-slate-700 pb-2">Rejected Log</h3>
                <div class="overflow-y-auto flex-grow max-h-64">
                    <table class="w-full text-sm text-left text-slate-300">
                        <thead class="text-xs text-slate-500 uppercase bg-slate-800/80 sticky top-0">
                            <tr>
                                <th class="px-3 py-2 font-semibold">Time</th>
                                <th class="px-3 py-2 font-semibold">Nonce</th>
                                <th class="px-3 py-2 font-semibold">Reason</th>
                            </tr>
                        </thead>
                        <tbody id="ui-rejected-shares">
                            <tr><td colspan="3" class="px-3 py-4 text-center text-slate-600">No rejected shares</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>

        </div>
        
        <footer class="mt-8 text-center text-slate-600 text-sm">
            Uptime: <span id="ui-uptime">0s</span> | Optimized for Cortex-M33
        </footer>
    </div>

    <script>
        setInterval(async () => {
            try {
                const res = await fetch('/api/stats');
                const data = await res.json();
                
                document.getElementById('ui-status').innerText = data.status;
                document.getElementById('ui-hashrate').innerText = data.hashrate_human;
                document.getElementById('ui-accepted').innerText = data.accepted;
                document.getElementById('ui-rejected').innerText = data.rejected;
                document.getElementById('ui-diff').innerText = data.current_diff;
                document.getElementById('ui-spm').innerText = data.spm;
                document.getElementById('ui-ping').innerText = data.last_ping;
                
                // Format Uptime
                const totalSec = data.uptime_seconds;
                const hours = Math.floor(totalSec / 3600);
                const minutes = Math.floor((totalSec % 3600) / 60);
                const seconds = totalSec % 60;
                document.getElementById('ui-uptime').innerText = `${hours}h ${minutes}m ${seconds}s`;

                // Render Recent Shares
                if (data.recent_shares.length > 0) {
                    const recentHtml = data.recent_shares.map(s => `
                        <tr class="border-b border-slate-700/30 hover:bg-slate-700/20 transition-colors">
                            <td class="px-3 py-2 text-slate-400 whitespace-nowrap">${s.time}</td>
                            <td class="px-3 py-2 font-mono whitespace-nowrap">${s.solve_time}s</td>
                            <td class="px-3 py-2 text-green-400 font-mono whitespace-nowrap">${s.hashrate}</td>
                        </tr>
                    `).join('');
                    document.getElementById('ui-recent-shares').innerHTML = recentHtml;
                }

                // Render Rejected Shares
                if (data.rejected_shares.length > 0) {
                    const rejectedHtml = data.rejected_shares.map(s => `
                        <tr class="border-b border-slate-700/30 hover:bg-slate-700/20 transition-colors">
                            <td class="px-3 py-2 text-slate-400 whitespace-nowrap">${s.time}</td>
                            <td class="px-3 py-2 font-mono text-slate-300 whitespace-nowrap">${s.nonce}</td>
                            <td class="px-3 py-2 text-red-400/80 text-xs">${s.reason}</td>
                        </tr>
                    `).join('');
                    document.getElementById('ui-rejected-shares').innerHTML = rejectedHtml;
                }

            } catch (err) {
                console.error("Failed to fetch stats", err);
            }
        }, 1500);
    </script>
</body>
</html>
"""

@app.route('/')
def index():
    return DASHBOARD_HTML

@app.route('/api/stats')
def api_stats():
    global mining_stats
    return jsonify(mining_stats)

def run_web_dashboard():
    import logging as flask_logging
    log = flask_logging.getLogger('werkzeug')
    log.setLevel(flask_logging.ERROR)
    
    logging.info(f"Starting Web Dashboard on port {WEB_PORT}")
    app.run(host='0.0.0.0', port=WEB_PORT, debug=False, use_reloader=False)

# --- Application Entry Point ---
if __name__ == "__main__":
    web_thread = threading.Thread(target=run_web_dashboard, daemon=True)
    web_thread.start()
    mine()