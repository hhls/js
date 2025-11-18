import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';
import { hmac } from 'k6/crypto';
import { SharedArray } from 'k6/data';
import papaparse from 'https://jslib.k6.io/papaparse/5.1.1/index.js';

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
  executor: 'ramping-arrival-rate',    // 执行器类型：渐进式到达率
            startRate: 10,                       // 起始TPS：每秒10个请求
            timeUnit: '1s',                      // 时间单位：1秒
            preAllocatedVUs: 50,                 // 预分配虚拟用户数：50个
            maxVUs: 500,                         // 最大虚拟用户数：100个
            gracefulStop: '10s',                 // 自定义优雅停止时间为 10 秒
            stages: [{duration: '30s', target: 100},    // 阶段1：5秒内爬升到20 TPS
                 { duration: '60s', target: 300 },  // 阶段2：60秒内爬升到50 TPS
                 { duration: '30s', target: 300 },  // 阶段3：保持50 TPS持续30秒
                {duration: '10s', target: 0},     // 阶段4：5秒内降到0 TPS
            ],
  thresholds: {
    http_req_duration: ['p(95)<5000'],
    errors: ['rate<0.5'],
    'http_req_duration{name:PlaceOrder}': ['p(99)<5000'],
    'http_req_duration{name:QueryOrders}': ['p(99)<3000'],
    'http_req_duration{name:CancelBatch}': ['p(99)<5000'],
  },
};

const config = {
  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
};

const INSTRUMENT ='BNB'

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

  return {
    ...commonHeaders,
    'sign': signature,
    'api_key': apiKey,
    'timestamp': timestamp.toString(),
    'X-USER-ID': uid,
  };
}

/**
 * 生成订单参数
 */
function generateOrderParams(userId) {
  const singleOrder = {
    instrument:INSTRUMENT,
    direction: 'long',
    leverage: 1,
    quantityUnit: 1,
    quantity: 1,
    positionModel: 1,
    positionType: 'plan',
    openPrice: 1,
    userId: userId
  };
  return Array(10).fill(singleOrder);
}

/**
 * 批量下单
 */
function placeOrders(user) {
  const apiPath = '/v1/perpum-market/batchOrders';
  const url = `${config.baseUrl}${apiPath}`;
  const orderParams = generateOrderParams(user.userid);
  const headers = generateSignedHeaders('POST', apiPath, orderParams, user.apikey, user.secretkey, user.uid);
  const payload = JSON.stringify(orderParams);
  headers['Content-Length'] = payload.length.toString();

  console.log(`[${user.account}] 开始下单，数量: ${orderParams.length}`);

  const response = http.post(url, payload, {
    headers: headers,
    tags: { name: 'PlaceOrder', account: user.account },
  });

  const success = check(response, {
    '下单状态为200': (r) => r.status === 200,
    '下单响应成功': (r) => {
      try {
        const data = JSON.parse(r.body);
        return data.code === 0 || data.success === true;
      } catch (e) {
        return false;
      }
    },
  });

//  console.info(`下单响应体: ${response.body}`);
  orderSuccessRate.add(success);

  if (!success) {
    console.error(`[${user.account}] 下单失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
  } else {
    console.log(`[${user.account}] 下单成功`);
  }

  return success;
}

/**
 * 查询当前挂单
 */
function queryCurrentOrders(user) {
  const apiPath = '/v1/perpum-market/orders/open';
  const instrument = INSTRUMENT;
  const positionType = 'plan';
  const queryString = `?instrument=${instrument}&positionType=${positionType}&page=1&rows=100&pageSize=100&sort=id&order=desc`;
  const url = `${config.baseUrl}${apiPath}${queryString}`;
  const headers = generateSignedHeadersForGet('GET', apiPath, queryString, user.apikey, user.secretkey, user.uid);

  console.log(`[${user.account}] 开始查询订单`);

  const response = http.get(url, {
    headers,
    tags: { name: 'QueryOrders', account: user.account }
  });

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
//  console.info(`查询订单响应体: ${response.body}`);

  if (!success) {
    console.error(`[${user.account}] 查询失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
    return [];
  }

  try {
    const parsed = JSON.parse(response.body);
    const orderIds = parsed.data && parsed.data.rows ?
                     parsed.data.rows.map(order => order.orderId) : [];
    console.log(`[${user.account}] 查询到 ${orderIds.length} 个挂单`);
    return orderIds;
  } catch (e) {
    console.error(`[${user.account}] 解析查询响应失败: ${e.message}`);
    return [];
  }
}

/**
 * 批量撤单
 */
function cancelBatchOrders(user, orderIds) {
  if (orderIds.length === 0) {
    console.log(`[${user.account}] 无挂单，跳过撤单`);
    return true;
  }

  console.log(`[${user.account}] 准备撤销 ${orderIds.length} 个订单`);

  const apiPath = '/v1/perpum-market/batchOrders';
  const url = `${config.baseUrl}${apiPath}`;
  const payload = {
    sourceIds: orderIds,
    instrument: INSTRUMENT
  };
  const payloadStr = JSON.stringify(payload);
  const headers = generateSignedHeaders('DELETE', apiPath, payload, user.apikey, user.secretkey, user.uid);
  headers['Content-Length'] = payloadStr.length.toString();

  const response = http.del(url, payloadStr, {
    headers,
    tags: { name: 'CancelBatch', account: user.account }
  });

  const success = check(response, {
    '撤单状态为200': (r) => r.status === 200,
    '撤单响应成功': (r) => {
      try {
        const data = JSON.parse(r.body);
        return data.code === 0;
      } catch (e) {
        return false;
      }
    },
  });
//  console.info(`撤订单响应体: ${response.body}`);
  cancelSuccessRate.add(success);

  if (!success) {
    console.error(`[${user.account}] 撤单失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
    return false;
  }

  console.log(`[${user.account}] 批量撤单成功: ${orderIds.length} 个订单`);
  return true;
}

export default function () {
  // 获取当前迭代对应的用户
  const userIndex = __ITER % credentials.length;
  const user = credentials[userIndex];

  console.log(`\n========== 开始处理账户[${userIndex}] [${user.account}] ==========`);

  // 1. 批量下单
  const placeOrderSuccess = placeOrders(user);
  if (!placeOrderSuccess) {
    console.error(`[${user.account}] 下单失败，跳过后续步骤`);
    return;
  }
  sleep(1); // 等待订单处理

  // 2. 查询订单
  const orderIds = queryCurrentOrders(user);
  sleep(0.5);

  // 3. 批量撤单
  if (orderIds.length > 0) {
    cancelBatchOrders(user, orderIds);
  }

  console.log(`========== 完成账户[${userIndex}] [${user.account}] ==========\n`);
}
