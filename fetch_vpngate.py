import requests, json, os

url = "https://www.vpngate.net/api/iphone/"
data_path = "data/servers.json"

try:
    r = requests.get(url, timeout=30)
    r.raise_for_status()
except Exception as e:
    print("Download failed:", e)
    exit(1)

lines = r.text.strip().split("\n")
servers = []

for line in lines:
    if not line or line.startswith("*") or line.startswith("#"):
        continue
    parts = line.split(",")
    if len(parts) > 14:
        servers.append({
            "ip": parts[1],
            "hostname": parts[0],
            "country": parts[5],
            "score": parts[2],
            "uptime": parts[3],
            "session": parts[4],
        })

os.makedirs("data", exist_ok=True)
with open(data_path, "w", encoding="utf-8") as f:
    json.dump(servers, f, indent=2, ensure_ascii=False)

print(f"Updated {len(servers)} servers to {data_path}")
