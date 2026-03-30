#!/usr/bin/env python3
import os
import sys
import time
import socket
import logging
import json
from urllib.request import Request, urlopen

# --- Configuration ---
USERNAME = ""
MINING_KEY = ""
RIG_IDENTIFIER = "UnoQ_STM32U585"
SOFTWARE_NAME = "Minimal_PC_Miner 1.0"
DIFFICULTY_TIER = "ESP32"
DUCOID = "DUCOID2137"

# Setup basic logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)

from arduino.app_utils import *
@call()
def ducos1a(lastHash: str, expHash: str, difficulty: int) -> int:
        ...

# --- Networking Helper Functions ---

def fetch_fastest_node():
    """
    Retrieves the best mining node from Duino-Coin main server.
    Returns a tuple (ip, port).
    """
    try:
        logging.info("Fetching fastest pool node...")
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


def mine():
    while True:
        node_ip, node_port = fetch_fastest_node()
        logging.info(f"Connecting to {node_ip}:{node_port}")

        sock = None
        try:
            # Create socket with a timeout for network operations
            sock = socket.create_connection((node_ip, node_port), timeout=60)
            server_version = sock.recv(100).decode().strip()
            logging.info(f"Connected. Server Version: {server_version}")

            while True:
                # Request Job
                # Protocol: JOB,username,diff_tier,key
                job_req = f"JOB,{USERNAME},{DIFFICULTY_TIER},{MINING_KEY}"
                sock.sendall(job_req.encode('utf-8'))

                # Receive Job
                job_data = sock.recv(1024).decode().strip()
                if not job_data:
                    break # Connection closed by server

                job_parts = job_data.split(",")
                if len(job_parts) < 3:
                    logging.error(f"Malformed job received: {job_data}")
                    continue

                last_hash = job_parts[0]
                exp_hash = job_parts[1]
                difficulty = int(job_parts[2])

                logging.info(f"Job received. Diff: {difficulty}")

                start_time = time.time()
                nonce = ducos1a(last_hash, exp_hash, difficulty, timeout=300)
                end_time = time.time()

                elapsed = end_time - start_time
                hashrate = nonce / elapsed if elapsed > 0 else 0

                # Send Results
                # Protocol: nonce,hashrate,miner_name,rig_id,duco_id
                result_str = f"{nonce},{hashrate},{SOFTWARE_NAME},{RIG_IDENTIFIER},{DUCOID}"

                sock.sendall(result_str.encode('utf-8'))

                feedback = sock.recv(1024).decode().strip()

                hr_human = f"{int(hashrate/1000)} kH/s" if hashrate > 1000 else f"{int(hashrate)} H/s"

                if "GOOD" in feedback:
                    logging.info(f"✓ Accepted share ({nonce}) at {hr_human}")
                elif "BAD" in feedback:
                    logging.warning(f"✗ Rejected share. Feedback: {feedback}")
                else:
                    logging.info(f"Unknown feedback: {feedback}")

        except KeyboardInterrupt:
            logging.info("Miner stopped by user.")
            if sock: sock.close()
            sys.exit(0)
        except (socket.error, socket.timeout, ConnectionError) as e:
            logging.error(f"Network error: {e}. Retrying in 15s...")
            if sock: sock.close()
            time.sleep(15)
        except Exception as e:
            logging.error(f"Unexpected error: {e}. Retrying in 15s...")
            if sock: sock.close()
            time.sleep(15)

if __name__ == "__main__":
    mine()