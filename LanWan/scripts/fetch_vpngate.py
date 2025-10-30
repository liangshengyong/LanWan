import os, requests, json

URL = "https://www.vpngate.net/api/iphone/"
LAST_FILE = "data/last_length.txt"
OUT_FILE = "data/servers.json"

os.makedirs("data", exist_ok=True)

# 读取上次的文件长度
last_length = 0
if os.path.exists(LAST_FILE):
    with open(LAST_FILE, "r") as f:
        try:
            last_length = int(f.read().strip())
        except:
            last_length = 0

# 获取当前的 Content-Length
r = requests.head(URL, timeout=10)
length = int(r.headers.get("Content-Length", 0))

if length != last_length:
    print(f"Detected change: {last_length} -> {length}")
    # 下载完整数据
    res = requests.get(URL, timeout=20)
    res.encoding = "utf-8"
    text = res.text.strip()

    # VPNGate 数据是以 CSV 格式结尾 "#EOF"
    lines = [x for x in text.splitlines() if x and not x.startswith("*") and not x.startswith("#")]
    headers = lines[0].split(",")
    data = []
    for line in lines[1:]:
        parts = line.split(",")
        if len(parts) == len(headers):
            data.append(dict(zip(headers, parts)))

    # 保存为 JSON
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    # 更新文件长度记录
    with open(LAST_FILE, "w") as f:
        f.write(str(length))

else:
    print("No change detected.")
