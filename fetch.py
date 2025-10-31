import requests, json, os

url = "https://www.vpngate.net/api/iphone/"
data_path = "data/servers.json"
size_path = "data/last_size.txt"

# 获取上次文件大小
old_size = None
if os.path.exists(size_path):
    with open(size_path, "r", encoding="utf-8") as f:
        old_size = f.read().strip()
        if old_size.isdigit():
            old_size = int(old_size)
        else:
            old_size = None

# 请求 HEAD 获取当前大小
try:
    r_head = requests.head(url, timeout=10)
    r_head.raise_for_status()
    new_size = int(r_head.headers.get("Content-Length", 0))
except Exception as e:
    print("HEAD request failed:", e)
    exit(1)

# 对比大小
if old_size == new_size:
    print("File size unchanged, skipping download.")
    exit(0)

# 下载完整数据
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

# 保存当前文件大小
with open(size_path, "w", encoding="utf-8") as f:
    f.write(str(new_size))

print(f"Updated {len(servers)} servers to {data_path}, size: {new_size}")
