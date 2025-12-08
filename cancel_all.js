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
  apiKey: __ENV.API_KEY || '13eda2ec-0e34-43ce-8942-c2e535c43c42',
  secretKey: __ENV.SECRET_KEY || 'L9LGPLMNBHAIGN4N1MDE2FEN2QNNXRFCRHTY',
  uid: __ENV.UID || '123147254',
  userId: __ENV.USERID || '600016213',
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
 * 构建签名字符串
 * @param {number} timestamp - 时间戳
 * @param {string} method - HTTP方法
 * @param {string} apiPath - API路径
 * @param {string} requestBody - 请求体JSON字符串
 * @returns {string} 签名字符串
 */
function buildSignStringP(timestamp, method, apiPath, requestBody) {
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
function generateSignedHeadersP(method, apiPath, payload) {
  const timestamp = Date.now();
  const requestBody = payload ? JSON.stringify(payload) : '';

  // 构建签名字符串
  const signString = buildSignStringP(timestamp, method, apiPath, requestBody);

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

function generateOrderParams() {
  return [{
    instrument: 'BTC',
    direction: 'long',
    leverage: 1,
    quantityUnit: 1,
    quantity: 1,
    positionModel: 1,
    positionType: 'plan',
    openPrice: 1,
    userId:config.userId
  }];
}


/**
 * 查询当前挂单
 */
function queryCurrentOrders() {
  const apiPath = '/v1/perpum-market/orders/open/v1';
  const instrument = 'BTC';
  const positionType = 'plan';
  const queryString = `?instrument=${instrument}&positionType=${positionType}&page=1&pageSize=1`;
  const url = `${config.baseUrl}${apiPath}${queryString}`;
  const headers = generateSignedHeaders('GET', apiPath, null, queryString);

  const response = http.get(url, { headers, tags: { name: 'QueryOrders' } });

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

  try {
        const parsed = JSON.parse(response.body);
        const orderIds = parsed.data ?
                         parsed.data.map(order => order.rows.orderId) :
                         [];
        console.log(`查询到 ${orderIds.length} 个挂单: ${orderIds.join(', ')}`);
        return orderIds;
    } catch (e) {
        console.error(`解析查询响应失败: ${e.message}`);
        return [];
    }
}

/**
 * 批量撤单
 */
function cancelBatchOrders(orderIds) {
  if (orderIds.length === 0) {
    console.log('无挂单，跳过撤单');
    return true;
  }
  const batchSize =10;
  const ordersToCancel = orderIds.slice(0, batchSize);

  console.log(`准备撤销 ${ordersToCancel.length} 个订单（共${orderIds.length}个）: ${ordersToCancel.join(', ')}`);


  const apiPath = '/v1/perpum-market/batchOrders/v1';
  const url = `${config.baseUrl}${apiPath}`;
  const json ={ sourceIdList: ordersToCancel,instrument:'BTC',type:1 }
  const payloadStr = JSON.stringify(json);
  const headers = generateSignedHeadersP('DELETE', apiPath, json);
  headers['Content-Length'] = payloadStr.length.toString();
  console.log(`payloadStr=${payloadStr}`)
  const response = http.del(url, payloadStr, { headers, tags: { name: 'CancelBatch' } });

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

  cancelSuccessRate.add(success);
   console.info(`${response.body}`)
  if (!success) {
    console.log(`撤单失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
    return false;
  }

  console.log(`批量撤单成功: ${ordersToCancel.length} 个订单`);
  return true;
}

export default function () {
  // 查询挂单并打印响应
  const orderIds = queryCurrentOrders();
  sleep(0.5); // 短暂等待

  // 批量撤单
  cancelBatchOrders(orderIds);
}