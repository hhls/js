import hmac
import hashlib
import base64
import json
import time
import csv
import requests
from typing import List, Dict, Optional

# 配置
BASE_URL = 'https://api.cwfutures.top'
INSTRUMENTS = ['BNB']

# 通用请求头
COMMON_HEADERS = {
    'Connection': 'close',
    'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
    'Content-Type': 'application/json',
}


def generate_hmac_sha256(data: str, secret_key: str) -> str:
    """生成HMAC SHA256签名"""
    try:
        signature = hmac.new(
            secret_key.encode('utf-8'),
            data.encode('utf-8'),
            hashlib.sha256
        ).digest()
        return base64.b64encode(signature).decode('utf-8')
    except Exception as e:
        print(f"签名生成失败: {e}")
        raise


def build_sign_string(timestamp: int, method: str, api_path: str,
                      request_body: str, query_string: str = '') -> str:
    """构建签名字符串"""
    return f"{timestamp}{method}{api_path}{query_string}{request_body}"


def generate_signed_headers(method: str, api_path: str, payload: Optional[Dict],
                            api_key: str, secret_key: str, uid: str) -> Dict:
    """生成签名请求头（支持POST/DELETE）"""
    timestamp = int(time.time() * 1000)
    request_body = json.dumps(payload) if payload else ''
    sign_string = build_sign_string(timestamp, method, api_path, request_body, '')
    signature = generate_hmac_sha256(sign_string, secret_key)

    headers = COMMON_HEADERS.copy()
    headers.update({
        'sign': signature,
        'api_key': api_key,
        'timestamp': str(timestamp),
        'X-USER-ID': uid,
    })

    return headers


def generate_signed_headers_for_get(method: str, api_path: str, query_string: str,
                                    api_key: str, secret_key: str, uid: str) -> Dict:
    """生成签名请求头（支持GET）"""
    timestamp = int(time.time() * 1000)
    request_body = ''
    sign_string = build_sign_string(timestamp, method, api_path, request_body, query_string)
    signature = generate_hmac_sha256(sign_string, secret_key)

    headers = COMMON_HEADERS.copy()
    headers.update({
        'sign': signature,
        'api_key': api_key,
        'timestamp': str(timestamp),
        'X-USER-ID': uid,
    })

    return headers


def generate_order_params(user_id: str, instrument: str) -> List[Dict]:
    """生成订单参数"""
    single_order = {
        'instrument': instrument,
        'direction': 'long',
        'leverage': 1,
        'quantityUnit': 1,
        'quantity': 1,
        'positionModel': 1,
        'positionType': 'plan',
        'openPrice': 1,
        'userId': user_id
    }
    return [single_order] * 10


def place_orders(user: Dict, instrument: str) -> bool:
    """批量下单"""
    api_path = '/v1/perpum-market/batchOrders'
    url = f"{BASE_URL}{api_path}"
    order_params = generate_order_params(user['userid'], instrument)
    headers = generate_signed_headers('POST', api_path, order_params,
                                      user['apikey'], user['secretkey'], user['uid'])

    print(f"[{user['account']}] 开始下单，数量: {len(order_params)}")

    try:
        response = requests.post(url, json=order_params, headers=headers)

        if response.status_code == 200:
            data = response.json()
            if data.get('code') == 0 or data.get('success') == True:
                print(f"[{user['account']}] 下单成功")
                return True

        print(f"[{user['account']}] 下单失败: {response.status_code}")
        return False
    except Exception as e:
        print(f"[{user['account']}] 下单异常: {e}")
        return False


def query_current_orders(user: Dict, instrument: str) -> List[str]:
    """查询当前挂单"""
    api_path = '/v1/perpum-market/orders/open'
    position_type = 'plan'
    query_string = f"?instrument={instrument}&positionType={position_type}&page=1&pageSize=100"
    url = f"{BASE_URL}{api_path}{query_string}"
    headers = generate_signed_headers_for_get('GET', api_path, query_string,
                                              user['apikey'], user['secretkey'], user['uid'])

    print(f"[{user['account']}] 开始查询订单")

    try:
        response = requests.get(url, headers=headers)

        if response.status_code == 200:
            data = response.json()
            if data.get('code') == 0:
                rows = data.get('data', {}).get('rows', [])
                order_ids = [order['id'] for order in rows]
                print(f"[{user['account']}] 查询到 {len(order_ids)} 个挂单")
                return order_ids

        print(f"[{user['account']}] 查询失败: {response.status_code}")
        return []
    except Exception as e:
        print(f"[{user['account']}] 查询异常: {e}")
        return []


def cancel_batch_orders(user: Dict, order_ids: List[str], instrument: str) -> bool:
    """批量撤单"""
    if not order_ids:
        print(f"[{user['account']}] 无挂单，跳过撤单")
        return True

    print(f"[{user['account']}] 准备撤销 {len(order_ids)} 个订单")

    api_path = '/v1/perpum-market/batchOrders'
    url = f"{BASE_URL}{api_path}"
    payload = {
        'sourceIds': order_ids,
        'instrument': instrument
    }
    headers = generate_signed_headers('DELETE', api_path, payload,
                                      user['apikey'], user['secretkey'], user['uid'])

    print(f"payload={json.dumps(payload)}")

    try:
        response = requests.delete(url, json=payload, headers=headers)

        if response.status_code == 200:
            data = response.json()
            if data.get('code') == 0:
                print(f"[{user['account']}] 批量撤单成功: {len(order_ids)} 个订单")
                return True

        print(f"[{user['account']}] 撤单失败: {response.status_code}")
        return False
    except Exception as e:
        print(f"[{user['account']}] 撤单异常: {e}")
        return False


def load_credentials(csv_file: str) -> List[Dict]:
    """从CSV文件加载用户凭证"""
    credentials = []
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            credentials.append(row)
    return credentials


def main():
    """主函数"""
    credentials = load_credentials('./user_credentials3.csv')

    if not credentials:
        print("未找到用户凭证")
        return

    user = credentials[0]
    instrument = INSTRUMENTS[0]

    order_ids = query_current_orders(user, instrument)

    if order_ids:
        # 去重后打印数量
        unique_order_ids = list(set(order_ids))  # 去重
        print(f"当前挂单数量（去重后）：{len(unique_order_ids)}")

        cancel_batch_orders(user, order_ids, instrument)

    time.sleep(0.02)


if __name__ == '__main__':
    main()
