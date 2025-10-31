import requests, json, os

url = "https://www.vpngate.net/api/iphone/"
data_path = "data/servers.json"
size_path = "data/last_length.txt"

# 读取上次文件大小
old_size = 0
if os.path.exists(size_path):
    try:
        old_size = int(open(size_path, "r", encoding="utf-8").read().strip() or 0)
    except:
        old_size = 0

# 下载文件（GET 请求获取内容长度）
try:
    r = requests.get(url, timeout=30)
    r.raise_for_status()
    new_size = len(r.content)
except Exception as e:
    print("Download failed:", e)
    exit(1)

# 比较大小
if old_size == new_size:
    print(f"File size unchanged ({new_size} bytes), skipping JSON update.")
    exit(0)

# 解析数据
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

# 确保 data 目录存在
os.makedirs("data", exist_ok=True)

# 写入 JSON
with open(data_path, "w", encoding="utf-8") as f:
    json.dump(servers, f, indent=2, ensure_ascii=False)

# 保存新文件长度
with open(size_path, "w", encoding="utf-8") as f:
    f.write(str(new_size))

print(f"Updated {len(servers)} servers. File size changed {old_size} → {new_size} bytes.")
