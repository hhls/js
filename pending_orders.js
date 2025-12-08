import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';
import { hmac } from 'k6/crypto';
import encoding from 'k6/encoding';

const errorRate = new Rate('errors');
const querySuccessRate = new Rate('query_success');
const cancelSuccessRate = new Rate('cancel_success');

export const options = {
  vus: 1,
  iterations: 1, // 执行一次
  thresholds: {
    http_req_duration: ['p(95)<5000'],
    errors: ['rate<0.5'],
    'http_req_duration{name:QueryOrders}': ['p(99)<5000'],
    'http_req_duration{name:CancelBatch}': ['p(99)<5000'],
  },
};

const config = {
  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
  apiKey: __ENV.API_KEY || '42344b70-240c-45b2-a7fa-6a6ee4ed7755',
  secretKey: __ENV.SECRET_KEY || '0VHK7R24QESGMUPTBUAVZEPAP8NVISIFZIOG',
  uid: __ENV.UID || '123145335',
};

const commonHeaders = {
  'Connection': 'close',
  'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
  'Content-Type': 'application/json',
};

/**
 * 生成HMAC SHA256签名
 */
function generateHmacSHA256(data, secretKey) {
  try {
    return hmac('sha256', secretKey, data, 'base64');
  } catch (e) {
    console.error(`签名生成失败: ${e.message}`);
    throw e;
  }
}

/**
 * 构建签名字符串（支持query参数）
 */
function buildSignString(timestamp, method, apiPath, requestBody, queryString = '') {
  return `${timestamp}${method}${apiPath}${queryString}${requestBody}`;
}

/**
 * 生成签名请求头（支持GET的query）
 */
function generateSignedHeaders(method, apiPath, payload = null, queryString = '') {
  const timestamp = Date.now();
  const requestBody = payload ? JSON.stringify(payload) : '';
  const signString = buildSignString(timestamp, method, apiPath, requestBody, queryString);
  const signature = generateHmacSHA256(signString, config.secretKey);
  return {
    ...commonHeaders,
    'sign': signature,
    'api_key': config.apiKey,
    'timestamp': timestamp.toString(),
    'X-USER-ID': config.uid,
  };
}

/**
 * 查询当前挂单
 */
function queryCurrentOrders() {
  const apiPath = '/v1/perpum-market/orders/openQuantity';
  const url = `${config.baseUrl}${apiPath}`;
  const headers = generateSignedHeaders('GET', apiPath, null,'');

  const response = http.get(url, { headers, tags: { name: 'openQuantity' } });

  // 打印完整响应体
  console.log('\n========== 查询订单响应 ==========');
  console.log(`url: ${url}, 状态码: ${response.status}`);
  console.log(`响应体: ${response.body}`);
  console.log('=================================\n');

  const success = check(response, {
    '查询状态为200': (r) => r.status === 200,
    '查询响应成功': (r) => {
      try {
        const data = JSON.parse(r.body);
        return data.code === 0;
      } catch (e) {
        return false;
      }
    },
  });

  querySuccessRate.add(success);

  if (!success) {
    console.log(`查询失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
    return [];
  }

}

export default function () {
  // 查询挂单并打印响应
  const orderIds = queryCurrentOrders();

}