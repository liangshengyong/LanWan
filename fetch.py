import re
import requests
import os

url = "https://www.vpngate.net/cn/"
save_path = "data/servers.txt"

os.makedirs(os.path.dirname(save_path), exist_ok=True)

try:
    r = requests.get(url, timeout=15, headers={"User-Agent": "Mozilla/5.0"})
    r.raise_for_status()
    html = r.text

    # 正则匹配
    hosts = re.findall(
        r"<tr>.*?L2TP/IPsec.*?>(\d{1,3}(?:\.\d{1,3}){3})<",
    html,
    flags=re.IGNORECASE | re.DOTALL | re.VERBOSE
)

    # 未提取到主机名，直接退出
    if not hosts:
        print("⚠️ 未匹配到任何主机名，可能网页结构变化或网络异常，跳过更新。")
        exit(0)

    print(f"✅ 提取到 {len(hosts)} 个主机名。")

    # 读取旧数据
    old_hosts = []
    if os.path.exists(save_path):
        try:
            with open(save_path, "r", encoding="utf-8") as f:
                old_hosts = [line.strip() for line in f if line.strip()]
        except Exception:
            print("⚠️ 读取旧数据失败，继续执行更新。")

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
