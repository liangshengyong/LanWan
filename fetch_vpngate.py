import requests, json, os

url = "https://www.vpngate.net/api/iphone/"
data_path = "data/servers.json"
header_path = "data/last_header.txt"

# 获取上次的header
old_header = None
if os.path.exists(header_path):
    with open(header_path, "r", encoding="utf-8") as f:
        old_header = f.read().strip()

# 请求新头
r_head = requests.head(url, timeout=10)
new_header = str(r_head.headers)

# 比较头文件是否变化
if new_header != old_header:
    print("Header changed, downloading new data...")
    r = requests.get(url, timeout=15)
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
    with open(header_path, "w", encoding="utf-8") as f:
        f.write(new_header)
else:
    print("No header change detected, skipping.")
