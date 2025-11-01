import re
import json
import requests
import os

url = "https://www.vpngate.net/cn/"
save_path = "data/servers.json"

os.makedirs(os.path.dirname(save_path), exist_ok=True)

try:
    r = requests.get(url, timeout=15, headers={"User-Agent": "Mozilla/5.0"})
    r.raise_for_status()
    html = r.text

    # 高效正则匹配 SSTP 主机名（支持全角冒号、标签干扰）
    hosts = re.findall(
        r"SSTP 主机名\s*[:：]\s*(?:<[^>]*>\s*)*([a-z0-9\.-]+\.[a-z]{2,})",
        html
    )

    # 若未提取到任何主机名，直接退出，不读旧数据也不写文件
    if not hosts:
        print("⚠️ 未匹配到任何主机名，可能网页结构变化或网络异常，跳过更新。")
        exit(0)

    print(f"✅ 提取到 {len(hosts)} 个主机名。")

    # 读取旧数据
    old_hosts = []
    if os.path.exists(save_path):
        try:
            with open(save_path, "r", encoding="utf-8") as f:
                old_hosts = json.load(f)
        except Exception:
            print("⚠️ 读取旧数据失败，继续执行更新。")

    # 对比是否有变化
    if hosts == old_hosts:
        print("ℹ️ 主机名列表无变化，跳过更新。")
    else:
        with open(save_path, "w", encoding="utf-8") as f:
            json.dump(hosts, f, ensure_ascii=False, indent=2)
        print(f"✅ 数据已更新，保存到 {save_path}")

except Exception as e:
    print("❌ 请求或解析失败：", e)
