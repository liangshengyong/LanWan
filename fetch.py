import requests
from bs4 import BeautifulSoup
import re
import os
import sys

URL = "https://www.vpngate.net/en/"
OUTPUT = "data/servers.txt"


def fetch_servers():
    try:
        html = requests.get(URL, timeout=15).text
    except Exception as e:
        print("Error downloading page:", e)
        return []

    soup = BeautifulSoup(html, "html.parser")
    ips = set()

    # 遍历所有行
    for tr in soup.select("tr"):
        tds = tr.find_all("td")
        if len(tds) < 5:
            continue

        country_td = tds[0]
        detail_td = tds[1]
        l2tp_td = tds[4]  # 官方表格中第 5 列是 L2TP/IPsec 列

        # 必须是日本
        if "Japan" not in country_td.get_text():
            continue

        # 必须支持 L2TP
        if "L2TP" not in l2tp_td.get_text():
            continue

        # detail 里提取 IPv4
        text = detail_td.get_text()
        found_ip = re.findall(r"\b\d+\.\d+\.\d+\.\d+\b", text)

        for ip in found_ip:
            ips.add(ip)

    return sorted(ips)


def read_old():
    if not os.path.exists(OUTPUT):
        return []
    with open(OUTPUT, "r") as f:
        return [line.strip() for line in f if line.strip()]


def save_new(ips):
    """只有内容变化才写入"""
    old = read_old()

    if ips == old:
        print("✓ No change, skip writing.")
        return False  # no update

    if not ips:
        print("✗ No IP found, skip writing to avoid overwriting old file.")
        return False

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)

    with open(OUTPUT, "w") as f:
        for ip in ips:
            f.write(ip + "\n")

    print(f"✓ Updated {OUTPUT} with {len(ips)} IPs.")
    return True  # updated


if __name__ == "__main__":
    ips = fetch_servers()
    changed = save_new(ips)
    if not changed:
        sys.exit(0)  # 不触发 commit
