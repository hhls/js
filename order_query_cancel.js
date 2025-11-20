import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';
import { hmac } from 'k6/crypto';
import { SharedArray } from 'k6/data';
import papaparse from 'https://jslib.k6.io/papaparse/5.1.1/index.js';
import exec from 'k6/execution';
const errorRate = new Rate('errors');
const orderSuccessRate = new Rate('order_success');
const querySuccessRate = new Rate('query_success');
const cancelSuccessRate = new Rate('cancel_success');

const credentials = new SharedArray('credentials', function () {
  const csvData = open('./user_credentials3.csv');
  const parsedData = papaparse.parse(csvData, { header: true });
  return parsedData.data;
});

export const options = {
  thresholds: {
//  http_req_failed: [{ threshold: 'rate<0.01', abortOnFail: true }],
    'http_req_duration{name:PlaceOrder}': ['p(99)<20000'],
    'http_req_duration{name:QueryOrders}': ['p(99)<20000'],
    'http_req_duration{name:CancelBatch}': ['p(99)<20000'],
  },

  scenarios: {
    my_ramping_scenario: {
      executor: 'ramping-arrival-rate',
      startRate: 10,
      timeUnit: '1s',
      preAllocatedVUs: 50,
      maxVUs: 120,
      gracefulStop: '10s',              // 优雅停止时间
      stages: [
        { duration: '120s', target: 20 },
        { duration: '180s', target: 50 },
        { duration: '1200s', target: 50 },
        { duration: '10s', target: 0 },
      ],
    },
  },
};


const config = {
  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
};

//  一致性hash找出同一个节点的币对进行压测
const INSTRUMENTS = ['BNB']

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
 * 构建签名字符串
 */
function buildSignString(timestamp, method, apiPath, requestBody, queryString = '') {
  return `${timestamp}${method}${apiPath}${queryString}${requestBody}`;
}

/**
 * 生成签名请求头（支持POST/DELETE）
 */
function generateSignedHeaders(method, apiPath, payload, apiKey, secretKey, uid) {
  const timestamp = Date.now();
  const requestBody = payload ? JSON.stringify(payload) : '';
  const signString = buildSignString(timestamp, method, apiPath, requestBody, '');
  const signature = generateHmacSHA256(signString, secretKey);

  return {
    ...commonHeaders,
    'sign': signature,
    'api_key': apiKey,
    'timestamp': timestamp.toString(),
    'X-USER-ID': uid,
  };
}

/**
 * 生成签名请求头（支持GET）
 */
function generateSignedHeadersForGet(method, apiPath, queryString, apiKey, secretKey, uid) {
  const timestamp = Date.now();
  const requestBody = '';
  const signString = buildSignString(timestamp, method, apiPath, requestBody, queryString);
  const signature = generateHmacSHA256(signString, secretKey);
