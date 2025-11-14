import re
import requests
import os
import random

url = "https://www.vpngate.net/cn/"
save_path = "data/servers.txt"

os.makedirs(os.path.dirname(save_path), exist_ok=True)

# 可选：随机 UA，避免固定 UA 被风控
UA_LIST = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Edge/121.0.0.0 Safari/537.36",
]

headers = {
    "User-Agent": random.choice(UA_LIST),
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
              "image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Referer": "https://www.vpngate.net/",
    "Cache-Control": "no-cache",
    "Pragma": "no-cache",
    "Accept-Encoding": "gzip, deflate, br",
    "Connection": "keep-alive",
    "Upgrade-Insecure-Requests": "1",
    "DNT": "1",
}

session = requests.Session()

try:
    # 第一次 GET，拿到 cookies
    session.get("https://www.vpngate.net/", headers=headers, timeout=10)

    # 访问中文页
    r = session.get(url, headers=headers, timeout=15)
    r.raise_for_status()

    html = r.text

    # -------- 正则匹配 L2TP 服务器 IP --------
    # 来自浏览器源码的结构：
    # <tr> ... L2TP/IPsec ... <td class="vg_table_row_1">123.123.123.123</td> ...
    hosts = re.findall(
        r"L2TP/IPsec[\s\S]{0,300}?>(\d{1,3}(?:\.\d{1,3}){3})<",
        html,
        flags=re.IGNORECASE
    )

    if not hosts:
        print("⚠️ 未匹配到任何 IP，网页结构可能变化。")
        exit(0)

    print(f"✅ 提取到 {len(hosts)} 个 L2TP/IPsec 服务器 IP。")

    # -------- 加载旧文件 --------
    old_hosts = []
    if os.path.exists(save_path):
        try:
            with open(save_path, "r", encoding="utf-8") as f:
                old_hosts = [line.strip() for line in f if line.strip()]
        except Exception:
            print("⚠️ 读取旧数据失败，将覆盖。")

    # -------- 对比新旧数据 --------
    if hosts == old_hosts:
        print("ℹ️ IP 列表无变化，跳过更新。")
    else:
        with open(save_path, "w", encoding="utf-8") as f:
            for h in hosts:
                f.write(h + "\n")

        print(f"✅ 已更新 servers.txt，共 {len(hosts)} 条")
        print("📌 前 10 个：", hosts[:10])

except Exception as e:
    print("❌ 错误：", e)

    # 对比新旧数据
    if hosts == old_hosts:
        print("ℹ️ 主机名列表无变化，跳过更新。")
    else:
        # 按行写入 TXT
        with open(save_path, "w", encoding="utf-8") as f:
            for host in hosts:
                f.write(host + "\n")
        print(f"✅ 数据已更新，保存到 {save_path}")
        print("前 10 个主机名：", hosts[:10])

except Exception as e:
    print("❌ 请求或解析失败：", e)
