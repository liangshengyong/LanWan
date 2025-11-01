import re
import json
import requests
import os

url = "https://www.vpngate.net/cn/"
json_path = os.path.join("data", "servers.json")

try:
    print("Fetching page:", url)
    r = requests.get(url, timeout=15, headers={"User-Agent": "Mozilla/5.0"})
    r.raise_for_status()
    html = r.text
    print(f"Downloaded {len(html)} characters")
except Exception as e:
    print("❌ Failed to fetch page:", e)
    exit(1)

# 匹配所有 “SSTP 主机名 : ” 后面的域名（包括重复）
hosts = re.findall(r"SSTP 主机名 :(?:<[^>]*>\s*)*([a-z0-9\.-]+\.[a-z]{2,})", html)

print(f"✅ Found {len(hosts)} SSTP hosts")

# 保存为 JSON
os.makedirs("data", exist_ok=True)
with open(json_path, "w", encoding="utf-8") as f:
    json.dump({"sstp_hosts": hosts}, f, ensure_ascii=False, indent=2)

print(f"✅ Saved to {json_path}")
