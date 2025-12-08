import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';
import { hmac } from 'k6/crypto';
import encoding from 'k6/encoding';

const errorRate = new Rate('errors');
const orderSuccessRate = new Rate('order_success');

export const options = {
  vus: 1,
  iterations: 1,
  thresholds: {
    http_req_duration: ['p(95)<5000'],
    errors: ['rate<0.5'],
    'http_req_duration{name:PlaceOrder}': ['p(99)<5000'],
  },
};

const config = {
  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
  apiKey: __ENV.API_KEY || '13eda2ec-0e34-43ce-8942-c2e535c43c42',
  secretKey: __ENV.SECRET_KEY || 'L9LGPLMNBHAIGN4N1MDE2FEN2QNNXRFCRHTY',
  uid: __ENV.UID || '123147254',
  userId: __ENV.USERID || '600016213',
};

//const config = {
//  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
//  apiKey: __ENV.API_KEY || '7dd38425-6f18-4d0c-a130-b362b4042ffe',
//  secretKey: __ENV.SECRET_KEY || 'VS5TUWZFNGFK0V9AGV2T3V076FSCIZ11RFN0',
//  uid: __ENV.UID || '123147255',
//  userId: __ENV.USERID || '600016214',
//};

const commonHeaders = {
  'Connection': 'close',
  'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
  'Content-Type': 'application/json',
};

/**
 * 生成HMAC SHA256签名
 * @param {string} data - 待签名的数据
 * @param {string} secretKey - 密钥
 * @returns {string} Base64编码的签名
 */
function generateHmacSHA256(data, secretKey) {
  try {
    // 使用k6的hmac函数生成HMAC SHA256
    // hmac(algorithm, secret, data, outputEncoding)
    // 返回base64编码的签名
    return hmac('sha256', secretKey, data, 'base64');
  } catch (e) {
    console.error(`签名生成失败: ${e.message}`);
    throw e;
  }
}

/**
 * 构建签名字符串
 * @param {number} timestamp - 时间戳
 * @param {string} method - HTTP方法
 * @param {string} apiPath - API路径
 * @param {string} requestBody - 请求体JSON字符串
 * @returns {string} 签名字符串
 */
function buildSignString(timestamp, method, apiPath, requestBody) {
  // 格式: timestamp + method + apiUrl + requestBody
  return `${timestamp}${method}${apiPath}${requestBody}`;
}

/**
 * 生成签名请求头
 * @param {string} method - HTTP方法
 * @param {string} apiPath - API路径
 * @param {object} payload - 请求体对象
 * @returns {object} 包含签名的请求头
 */
function generateSignedHeaders(method, apiPath, payload) {
  const timestamp = Date.now();
  const requestBody = payload ? JSON.stringify(payload) : '';

  // 构建签名字符串
  const signString = buildSignString(timestamp, method, apiPath, requestBody);

  // 生成签名
  const signature = generateHmacSHA256(signString, config.secretKey);

  // 返回包含签名的请求头
  return {
    ...commonHeaders,
    'sign': signature,
    'api_key': config.apiKey,
    'timestamp': timestamp.toString(),
    'X-USER-ID': config.uid,
  };
}

//function generateOrderParams() {
//  return [{
//    instrument: 'bnb',
//    direction: 'long',
//    leverage: 1,
//    quantityUnit: 1,
//    quantity: 1,
//    positionModel: 1,
//    positionType: 'plan',
//    openPrice: 1100,
//    userId:config.userId
//  }];
//}

function generateOrderParams() {
  const singleOrder = {
    instrument: 'BNB',
    direction: 'long',
    leverage: 1,
    quantityUnit: 1,
    quantity: 1,
    positionModel: 1,
    positionType: 'plan',
    openPrice: 1,
    userId: config.userId
  };

  // 返回 100 个完全相同的对象组成的数组
  return Array(100).fill(singleOrder);
}

export default function () {
  // API路径
  const apiPath = '/v1/perpum-market/batchOrders';
  const url = `${config.baseUrl}${apiPath}`;

  // 生成订单参数
  const orderParams = generateOrderParams();

  // 生成签名请求头
  const headers = generateSignedHeaders('POST', apiPath, orderParams);

  // 请求体
  const payload = JSON.stringify(orderParams);

  // 添加Content-Length
  headers['Content-Length'] = payload.length.toString();

  // 打印请求信息
//  console.log('========== 请求信息 ==========');
//  console.log(`URL: ${url}`);
//  console.log(`Method: POST`);
//  console.log('\n请求头:');
//  for (const [key, value] of Object.entries(headers)) {
//    // 对敏感信息做部分脱敏
//    if (key === 'api_key' || key === 'sign') {
//      console.log(`  ${key}: ${value.substring(0, 20)}...`);
//    } else {
//      console.log(`  ${key}: ${value}`);
//    }
//  }
  console.log('\n请求体:');
  console.log(payload);
//  console.log('==============================\n');

  // 发送POST请求
  const response = http.post(url, payload, {
    headers: headers,
    tags: { name: 'PlaceOrder' },
  });

  // 检查请求结果
  const success = check(response, {
    '订单请求状态为200': (r) => r.status === 200,
    '订单响应有body': (r) => r.body && r.body.length > 0,
    '订单响应成功': (r) => {
      try {
        const data = JSON.parse(r.body);
        // 根据实际API返回格式调整判断逻辑
        return data.code === 0 || data.success === true;
      } catch (e) {
        return false;
      }
    },
  });

  orderSuccessRate.add(success);

  console.info(`响应体: ${response.body}`);
  if (!success) {
    console.log('\n========== 失败响应 ==========');
    console.error(`状态码: ${response.status}`);
    console.error(`响应体: ${response.body}`);
    console.log('==============================\n');
    errorRate.add(1);
  }
}