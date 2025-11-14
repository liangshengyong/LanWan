import requests
from bs4 import BeautifulSoup
import os
import random

url = "https://www.vpngate.net/cn/"
save_path = "data/servers.txt"
os.makedirs(os.path.dirname(save_path), exist_ok=True)

# 随机UA，避免风控
UA_LIST = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
]

headers = {
    "User-Agent": random.choice(UA_LIST),
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Referer": "https://www.vpngate.net/",
}

session = requests.Session()

try:
    # 先访问主页获取 cookie
    session.get("https://www.vpngate.net/", headers=headers, timeout=10)

    r = session.get(url, headers=headers, timeout=15)
    r.raise_for_status()

    soup = BeautifulSoup(r.text, "html.parser")

    # vpngate 每个服务器是一个表格 <table class='vg_table'>
    hosts = []

    tables = soup.find_all("table", class_="vg_table_row")
    if not tables:
        tables = soup.find_all("table")  # 兼容旧版结构

    for table in tables:
        rows = table.find_all("tr")

        # 检查是否为服务器条目
        for row in rows:
            cols = row.find_all("td")
            if len(cols) < 12:
                continue

            # 某一列包含 “L2TP/IPsec”
            if not any("L2TP" in c.text for c in cols):
                continue

            # 通常 IP 在第 2 列或第 3 列
            for col in cols:
                text = col.get_text(strip=True)
                if text.count(".") == 3:  # 简单判断IPv4
                    hosts.append(text)
                    break

    hosts = list(dict.fromkeys(hosts))  # 去重

    if not hosts:
        print("⚠️ 未匹配到任何 IP（结构变化），已自动适配失败。")
        exit(0)

    print(f"✅ 成功提取 {len(hosts)} 个 L2TP 服务器 IP")
    print("前 10 个：", hosts[:10])

    # 加载旧文件
    old_hosts = []
    if os.path.exists(save_path):
        with open(save_path, "r", encoding="utf-8") as f:
            old_hosts = [line.strip() for line in f if line.strip()]

    # 对比是否需要更新
    if hosts == old_hosts:
        print("ℹ️ 无变化，跳过写入。")
    else:
        with open(save_path, "w", encoding="utf-8") as f:
            for h in hosts:
                f.write(h + "\n")
        print(f"✅ 已更新 {save_path}")

except Exception as e:
    print("❌ 发生错误：", e)
