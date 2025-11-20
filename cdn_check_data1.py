# -*- coding: utf-8 -*-
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone, timedelta

import requests

session = requests.Session()
CHECK_TIMES = 100

URL_COM = "https://futuresapi.coinw.com/v1/markets/quotes/swap/BTC/candles"
URL_MARKET = "https://futuresapi.coinw.market/v1/markets/quotes/swap/BTC/candles"

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

# 你原来的 headers 全保留

MARKET_HEADERS = {  # 你原来的market头
    'Cache-Control': 'no-cache',
    'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15',
    'Pragma': 'no-cache',
    'Origin': 'https://www.coinw.market',
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Site': 'same-site',
    'Accept-Language': 'zh_CN',
    'Accept': 'application/json, text/plain, */*',
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

COM_HEADERS = {  # 你原来的com头
    'Cache-Control': 'no-cache',
    'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15',
    'Pragma': 'no-cache',
    'Origin': 'https://www.coinw.com',
    'Sec-Fetch-Dest': 'empty',
    'Sec-Fetch-Site': 'same-site',
    'Accept-Language': 'zh_CN',
    'Accept': 'application/json, text/plain, */*',
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


def fetch_and_clean(url, params, headers):
    start = time.time()
    try:
        r = session.get(url, params=params, headers=headers, timeout=10)
        cost = int((time.time() - start) * 1000)
        data = r.json()
        if data.get("code") != 0 or "data" not in data:
            return None, cost

        klines = sorted(data["data"], key=lambda x: x[0])
        if len(klines) > 1:
            klines = klines[:-1]  # 强制去掉最后一根（最稳）

        ts = {k[0] for k in klines}
        kd = {k[0]: k for k in klines}
        return {"ts": ts, "k": kd}, cost
    except Exception as e:
        print("请求出错:", e)
        return None, 9999


with ThreadPoolExecutor(max_workers=10) as pool:
    for i in range(1, CHECK_TIMES + 1):
        print(f"\n=== 第 {i} 轮 ===")
        diff_this_round = 0

        for p in PERIODS:
            params = COMMON_PARAMS | p

            com_task = pool.submit(fetch_and_clean, URL_COM, params, COM_HEADERS)
            market_task = pool.submit(fetch_and_clean, URL_MARKET, params, MARKET_HEADERS)

            com_data, com_ms = com_task.result()
            market_data, market_ms = market_task.result()

            if not com_data or not market_data:
                print(f" {p['name']} 数据错误")
                continue

            com_ts = com_data["ts"]
            com_k = com_data["k"]
            market_k = market_data["k"]
            market_ts = market_data["ts"]

            inconsistent = False
            diff_details = []

            # 关键修复：先判断 market 是否有这根K线
            for ts in com_ts:
                c_kline = com_k.get(ts)
                m_kline = market_k.get(ts)

                if m_kline is None:  # market 完全缺这根
                    inconsistent = True
                    t_str = time.strftime("%Y-%m-%d %H:%M", time.localtime(ts // 1000))
                    diff_details.append(f"   【{t_str}】 ts={ts} → market 完全缺失这根K线！")
                    continue

                if m_kline != c_kline:  # 完全对比所有字段，包括成交量
                    inconsistent = True
                    t_str = time.strftime("%Y-%m-%d %H:%M", time.localtime(ts // 1000))
                    diff_details.append(
                        f"   【{t_str}】 ts={ts}\n"
                        f"     com    → O={c_kline[1]:.2f} H={c_kline[2]:.2f} L={c_kline[3]:.2f} C={c_kline[4]:.2f} V={c_kline[5]:.4f}\n"
                        f"     market → O={m_kline[1]:.2f} H={m_kline[2]:.2f} L={m_kline[3]:.2f} C={m_kline[4]:.2f} V={m_kline[5]:.4f}"
                    )

            if len(com_ts) != len(market_ts):
                inconsistent = True
                diff_details.append(f"   闭合K线数量不一致：com {len(com_ts)}根 vs market {len(market_ts)}根")

            if inconsistent:
                diff_this_round += 1
                now_beijing = datetime.now(timezone(timedelta(hours=8))).strftime("%Y-%m-%d %H:%M:%S")
                print(f" {p['name']} 不一致 | market {market_ms:3}ms | com {com_ms:3}ms")
                print(f" 当前北京时间：{now_beijing}")
                for line in diff_details:
                    print(line)
            else:
                print(f" {p['name']} 完全一致 | market {market_ms:3}ms | com {com_ms:3}ms")

        status = "本轮全一致" if diff_this_round == 0 else f"本轮不一致 {diff_this_round} 个"
        print(status)
        time.sleep(1)

print("\n全部结束！再也不会 NoneType 报错了，不一致时还带北京时间，完美！")
