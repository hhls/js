# -*- coding: utf-8 -*-
import time
import requests

URL = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"
PARAMS = {"granularity": "1", "interval": "1", "type": 0, "chartIndex": 0, "code": "BTC",
          "dataType": "klinedata", "isFirst": "true", "klineType": 1, "quote": "usdt"}

HEADERS = {"User-Agent": "Mozilla/5.0", "clienttag": "web"}

i = 0
while True:
    i += 1
    start = time.time()
    r = requests.get(URL, params=PARAMS, headers=HEADERS, timeout=10, stream=True)
    cost = int((time.time() - start) * 1000)

    timing = r.headers.get("Server-Timing", "")
    cdn_real = "未知"
    if "cdn-cache; desc=" in timing:
        cdn_real = timing.split("cdn-cache; desc=")[1].split(",")[0].strip('"')

    xcache = r.headers.get("x-cache-status", "无")

    print(f"第{i:3d}次 | 真实CDN: {cdn_real:6} | x-cache: {xcache:8} | 耗时: {cost:3}ms")
    r.close()
    time.sleep(0.6)