# -*- coding: utf-8 -*-
import time
from concurrent.futures import ThreadPoolExecutor

import requests
from deepdiff import DeepDiff

session = requests.Session()

CHECK_TIMES = 100
diff_count = 0

URL_MARKET = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"
# URL_MARKET = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"
URL_COM = "https://futuresapi.coinw.com/v1/markets/quotes/swap/BTC/candles"
# URL_COM = "https://futuresapi.coinw.com/v1/markets/quotes/swap/BTC/candles"

PERIODS = [
    {"granularity": "1D", "interval": "1D", "type": 11, "name": "1D"},
    {"granularity": "4H", "interval": "4H", "type": 7, "name": "4H"},
    {"granularity": "1H", "interval": "1H", "type": 5, "name": "1H"},
    {"granularity": "15", "interval": "15", "type": 3, "name": "15m"},
    {"granularity": "1", "interval": "1", "type": 0, "name": "1m"},
]

COMMON_PARAMS = {
    "chartIndex": 0, "code": "BTC", "dataType": "klinedata",
    "isFirst": "true", "klineType": 1, "quote": "usdt"
}

MARKET_HEADERS = {
    'Cache-Control': 'no-cache',
    'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
    'Pragma': 'no-cache',
    'Origin': 'https://www.coinw.market',
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Site': 'same-site',
    'Accept-Language': 'zh_CN',
    'Accept': 'application/json, text/plain, */*',
    # 'Accept-Encoding': 'gzip, deflate, br',
    'Sec-Fetch-Mode': 'cors',
    'thirdapptoken': '',
    'logintoken': '',
    'language': 'zh_CN',
    'x-requested-with': 'XMLHttpRequest',
    'devicename': 'Safari V26.0.1 (macOS)',
    'systemversion': 'macOS 10.15.7',
    'thirdappid': 'coinw',
    'appversion': '100.100.100',
    'selecttype': 'USD',
    'x-authorization': '',
    'Priority': 'u=3, i',
    'deviceid': '19df7e2a8980322f26a54279b7ee004f',
    'x-language': 'zh_CN',
    'token': '',
    'x-locale': 'zh_CN',
    'clienttag': 'web',
    'cwdeviceid': '19df7e2a8980322f26a54279b7ee004f',
    'withcredentials': 'true'
}

COM_HEADERS = {
    'Cache-Control': 'no-cache',
    'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
    'Pragma': 'no-cache',
    'Origin': 'https://www.coinw.com',
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Site': 'same-site',
    'Accept-Language': 'zh_CN',
    'Accept': 'application/json, text/plain, */*',
    # 'Accept-Encoding': 'gzip, deflate, br',
    'Sec-Fetch-Mode': 'cors',
    'thirdapptoken': '',
    'logintoken': '',
    'language': 'zh_CN',
    'x-requested-with': 'XMLHttpRequest',
    'devicename': 'Safari V26.0.1 (macOS)',
    'systemversion': 'macOS 10.15.7',
    'thirdappid': 'coinw',
    'appversion': '100.100.100',
    'selecttype': 'USD',
    'x-authorization': '',
    'Priority': 'u=3, i',
    'deviceid': '19df7e2a8980322f26a54279b7ee004f',
    'x-language': 'zh_CN',
    'token': '',
    'x-locale': 'zh_CN',
    'clienttag': 'web',
    'cwdeviceid': '19df7e2a8980322f26a54279b7ee004f',
    'withcredentials': 'true'
}


def fetch(url, params, headers):
    start = time.time()
    try:
        r = session.get(url, params=params, headers=headers, timeout=15, stream=True)
        cost_ms = int((time.time() - start) * 1000)

        timing = r.headers.get("Server-Timing", "")
        origin = "未知"
        for part in timing.split(","):
            if "origin; dur=" in part:
                origin = part.split("dur=")[1]
                break

        data = r.json()

        if data.get("code") == 0 and "data" in data and len(data["data"]) >= 500:
            sorted_k = sorted(data["data"], key=lambda x: x[0])
            data["data"] = sorted_k[:-1]  # 直接去掉最后一根，剩下499根闭合K

        return data, cost_ms, origin
    except Exception as e:
        return {"error": str(e)}, 9999, "错误"


with ThreadPoolExecutor(max_workers=10) as pool:
    for i in range(1, CHECK_TIMES + 1):
        print(f"=== 第 {i} 轮 ===")
        round_diff = 0

        for p in PERIODS:
            params = {**COMMON_PARAMS, **p}

            market_future = pool.submit(fetch, URL_MARKET, params, MARKET_HEADERS)
            com_future = pool.submit(fetch, URL_COM, params, COM_HEADERS)

            data_market, t_market, o_market = market_future.result()
            data_com, t_com, o_com = com_future.result()

            if data_market != data_com:
                diff_count += 1
                round_diff += 1
                diff = DeepDiff(data_market, data_com, ignore_order=True)
                print(f" {p['name']} 不一致 | market {t_market}ms (后端{o_market}) | com {t_com}ms (后端{o_com})")
                print("   差异：", diff.pretty().replace("\n", "\n     "))
            else:
                print(
                    f" {p['name']} 一致 | market {t_market:3}ms (后端{o_market:>4}) | com {t_com:3}ms (后端{o_com:>4})")

        status = f"本轮不一致 {round_diff} 个" if round_diff else "本轮全一致"
        print(f" {status}，累计不一致：{diff_count}")
        time.sleep(1)

print(f"\n全部结束，总不一致：{diff_count} 次")
