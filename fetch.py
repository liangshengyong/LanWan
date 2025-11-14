import requests
from bs4 import BeautifulSoup
import re
import os
import sys

URL = "https://www.vpngate.net/en/"
OUTPUT = "data/servers.txt"

os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)


def fetch_japan_l2tp():
    print("Downloading page...")
    r = requests.get(
        URL,
        timeout=20,
        headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"
        }
    )
    r.raise_for_status()

    soup = BeautifulSoup(r.text, "html.parser")

    ips = set()

    rows = soup.find_all("tr")
    for tr in rows:
        tds = tr.find_all("td")
        if len(tds) < 5:
            continue

        country_text = tds[0].get_text(strip=True)

        # ✔ 必须是日本
        if "Japan" not in country_text:
            continue

        full_row_text = tr.get_text(" ", strip=True)

        # ✔ 必须支持 L2TP
        #   页面里 L2TP 出现的位置可能不同，所以用模糊匹配
        if "L2TP" not in full_row_text:
            continue

        # ✔ 从整行提取 IPv4
        found = re.findall(r"\b\d{1,3}(?:\.\d{1,3}){3}\b", full_row_text)
        for ip in found:
            ips.add(ip)

    return sorted(ips)


def read_old():
    if not os.path.exists(OUTPUT):
        return []
    with open(OUTPUT, "r") as f:
        return [line.strip() for line in f if line.strip()]


def save_new(ips):
    old = read_old()

    if not ips:
        print("✗ No Japan L2TP IP found. Skip writing.")
        return False

    if ips == old:
        print("✓ No change. Skip writing.")
        return False

    with open(OUTPUT, "w") as f:
        for ip in ips:
            f.write(ip + "\n")

    print(f"✓ Updated {OUTPUT} with {len(ips)} IPs.")
    return True


if __name__ == "__main__":
    try:
        ips = fetch_japan_l2tp()
        print("Found:", ips)
        save_new(ips)
    except Exception as e:
        print("Error:", e)
        sys.exit(1)
