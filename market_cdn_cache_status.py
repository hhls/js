# -*- coding: utf-8 -*-
import time

import requests

session = requests.Session()

URL = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"

PARAMS = {
    "granularity": "1",
    "chartIndex": "0",
    "code": "BTC",
    "interval": "1",
    "dataType": "klinedata",
    "type": "0",
    "isFirst": "true",
    "klineType": "1",
    "quote": "usdt"
}

HEADERS = {
    "Cache-Control": "no-cache",
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15",
    "Pragma": "no-cache",
    "Origin": "https://www.coinw.market",
    "Accept-Language": "zh_CN",
    "Accept": "application/json, text/plain, */*",
    "Accept-Encoding": "gzip, deflate, br",
    "x-requested-with": "XMLHttpRequest",
    "language": "zh_CN",
    "devicename": "Safari V26.0.1 (macOS)",
    "systemversion": "macOS 10.15.7",
    "thirdappid": "coinw",
    "appversion": "100.100.100",
    "selecttype": "USD",
    "Priority": "u=3, i",
    "deviceid": "19df7e2a8980322f26a54279b7ee004f",
    "x-language": "zh_CN",
    "x-locale": "zh_CN",
    "clienttag": "web",
    "cwdeviceid": "19df7e2a8980322f26a54279b7ee004f",
    "withcredentials": "true",
    "thirdapptoken": "",
    "logintoken": "",
    "x-authorization": "",
    "token": "",
}

i = 0
while True:
    i += 1
    r = session.get(URL, params=PARAMS, headers=HEADERS, timeout=10, stream=True)

    timing = r.headers.get("Server-Timing", "")
    cache_control = r.headers.get("Cache-Control", "")
    x_cache = r.headers.get("x-cache-status", "无")

    cdn_status = edge = origin = "未知"
    for part in timing.split(","):
        part = part.strip()
        if "cdn-cache; desc=" in part:
            cdn_status = part.split("desc=")[1].strip('"')
        if "edge; dur=" in part:
            edge = part.split("dur=")[1]
        if "origin; dur=" in part:
            origin = part.split("dur=")[1]

    print(f"第{i:3d}次 | CDN状态: {cdn_status:6} | "
          f"CDN节点: {edge:>4}ms | 后端: {origin:>6} | "
          f"Cache-Control: {cache_control}")

    r.close()
    time.sleep(0.6)
