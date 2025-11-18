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
    http_req_duration: ['p(95)<5000'],
    errors: ['rate<0.5'],
    'http_req_duration{name:PlaceOrder}': ['p(99)<5000'],
    'http_req_duration{name:QueryOrders}': ['p(99)<3000'],
    'http_req_duration{name:CancelBatch}': ['p(99)<5000'],
  },

  scenarios: {
    my_ramping_scenario: {
      executor: 'ramping-arrival-rate',
      startRate: 10,
      timeUnit: '1s',
      preAllocatedVUs: 50,
      maxVUs: 100,
      gracefulStop: '10s',              // 优雅停止时间
      stages: [
        { duration: '100s', target: 3000 },
        { duration: '200s', target: 6000 },
        { duration: '100s', target: 6000 },
        { duration: '5s', target: 0 },
      ],
    },
  },
};


const config = {
  baseUrl: __ENV.BASE_URL || 'https://api.cwfutures.top',
};

//  一致性hash找出同一个节点的币对进行压测
const INSTRUMENTS = ['ADA', 'TREE', 'XRP']

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
function generateOrderParams(userId,INSTRUMENT) {
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
function placeOrders(user,INSTRUMENT) {
  const apiPath = '/v1/perpum-market/batchOrders';
  const url = `${config.baseUrl}${apiPath}`;
  const orderParams = generateOrderParams(user.userid,INSTRUMENT);
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
function queryCurrentOrders(user,INSTRUMENT) {
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
   线程1挂底单（1000单，价格不一样，不区分用户），线程2下查全撤（3000tps)
 * 批量撤单
 */
function cancelBatchOrders(user, orderIds,INSTRUMENT) {
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
  const index = exec.scenario.iterationInTest;
  const userIndex = index % Math.min(15, credentials.length);   //最多多少个压测用户参与压测
  const INSTRUMENT = INSTRUMENTS[index % INSTRUMENTS.length];
  const user = credentials[userIndex];

  // 批量下单
  const placeOrderSuccess = placeOrders(user,INSTRUMENT);
  if (!placeOrderSuccess) {
    console.error(`[${user.account}] 下单失败，跳过后续步骤`);
    return;
  }

  // 查询订单
  const orderIds = queryCurrentOrders(user,INSTRUMENT);

  // 批量撤单
  if (orderIds.length > 0) {
    cancelBatchOrders(user, orderIds,INSTRUMENT);
  }

  sleep(0.02);
}
