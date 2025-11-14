import requests
import os

URL = "https://www.vpngate.net/api/iphone/"
OUTPUT = "data/servers.txt"

os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)

def fetch_l2tp_japan():
    try:
        r = requests.get(URL, timeout=15, headers={"User-Agent": "Mozilla/5.0"})
        r.raise_for_status()
    except Exception as e:
        print("Error downloading API:", e)
        return []

    hosts = set()
    for line in r.text.splitlines():
        if line.startswith("*") or not line.strip():
            continue
        cols = line.split(",")
        if len(cols) < 15:
            continue

        ip = cols[1].strip()
        country = cols[5].strip()
        l2tp = cols[14].strip()

        if country == "Japan" and l2tp == "True":
            hosts.add(ip)

    return sorted(hosts)

def read_old():
    if not os.path.exists(OUTPUT):
        return []
    with open(OUTPUT, "r") as f:
        return [line.strip() for line in f if line.strip()]

def save_new(ips):
    old = read_old()
    if ips == old:
        print("✓ No change, skip writing.")
        return False
    if not ips:
        print("✗ No IP found, skip writing to avoid overwriting old file.")
        return False

    with open(OUTPUT, "w") as f:
        for ip in ips:
            f.write(ip + "\n")
    print(f"✓ Updated {OUTPUT} with {len(ips)} IPs.")
    return True

if __name__ == "__main__":
    ips = fetch_l2tp_japan()
    save_new(ips)
