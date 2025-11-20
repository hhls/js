# -*- coding: utf-8 -*-
import time

import requests

URL = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"
PARAMS = {"granularity": "1", "interval": "1", "type": 0,  # 随便挑一个1分钟的
          "chartIndex": 0, "code": "BTC", "dataType": "klinedata",
          "isFirst": "true", "klineType": 1, "quote": "usdt"}

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15",
    "clienttag": "web"
}

print("开始监控 market 站点的 x-cache-status，按 Ctrl+C 停止")
i = 0
while True:
    i += 1
    try:
        r = requests.get(URL, params=PARAMS, headers=HEADERS, timeout=10)
        cache_status = r.headers.get("x-cache-status", "无此header")
        print(f"第 {i:4d} 次 | x-cash-status: {cache_status} | 状态码: {r.status_code}")
    except Exception as e:
        print(f"第 {i:4d} 次 请求出错: {e}")

    time.sleep(0.5)  # 改这里控制频率，建议别太快免得被封
