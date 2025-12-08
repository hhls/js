import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';
import { md5 } from 'k6/crypto';
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
  userId: __ENV.USERID || '600015875',
};

const commonHeaders = {
  'Connection': 'close',
  'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
};

/**
 * 生成MD5签名（参考JMeter脚本逻辑）
 * @param {Object} params - 请求参数对象
 * @param {string} secretKey - 密钥
 * @returns {string} MD5签名（大写）
 */
function generateMD5Sign(params, secretKey) {
  try {
    // 按键名升序排列
    const sortedKeys = Object.keys(params).sort();

    // 拼接签名字符串（过滤空值）
    const signParams = [];
    sortedKeys.forEach(key => {
      const value = params[key];
      if (value !== null && value !== undefined && value !== '') {
        signParams.push(`${key}=${value}`);
      }
    });

    // 添加secret_key
    const signString = signParams.join('&') + `&secret_key=${secretKey}`;

    console.log(`签名字符串: ${signString}`);

    // 生成MD5并转大写
    const signature = md5(signString, 'hex').toUpperCase();
    console.log(`生成签名: ${signature}`);

    return signature;
  } catch (e) {
    console.error(`签名生成失败: ${e.message}`);
    throw e;
  }
}

/**
 * 为GET请求生成签名和headers
 * @param {string} apiPath - API路径
 * @param {Object} queryParams - 查询参数
 * @returns {Object} {url, headers}
 */
function buildGetRequest(apiPath, queryParams = {}) {
  // 构建查询字符串（不包含签名）
  const queryString = Object.keys(queryParams)
    .map(key => `${key}=${queryParams[key]}`)
    .join('&');

  const url = `${config.baseUrl}${apiPath}?${queryString}`;

  // 添加api_key到参数用于签名计算
  const signParams = {
    ...queryParams,
    api_key: config.apiKey
  };

  // 生成签名
  const sign = generateMD5Sign(signParams, config.secretKey);

  console.log(`GET请求URL: ${url}`);
  console.log(`签名: ${sign}`);

  return {
    url,
    headers: {
      ...commonHeaders,
      'sign': sign,
      'api_key': config.apiKey
    }
  };
}

/**
 * 为POST请求生成签名和body
 * @param {string} apiPath - API路径
 * @param {Object} bodyParams - 请求体参数
 * @returns {Object} {url, body, headers}
 */
function buildPostRequest(apiPath, bodyParams = {}) {
  // 添加api_key到参数
  const params = {
    ...bodyParams,
    api_key: config.apiKey
  };

  // 生成签名
  const sign = generateMD5Sign(params, config.secretKey);
  params.sign = sign;

  // 构建表单body
  const formBody = Object.keys(params)
    .map(key => `${encodeURIComponent(key)}=${encodeURIComponent(params[key])}`)
    .join('&');

  return {
    url: `${config.baseUrl}${apiPath}`,
    body: formBody,
    headers: {
      ...commonHeaders,
      'Content-Type': 'application/x-www-form-urlencoded'
    }
  };
}

/**
 * 为DELETE请求生成签名和body
 * @param {string} apiPath - API路径
 * @param {Object} bodyParams - 请求体参数
 * @returns {Object} {url, body, headers}
 */
function buildDeleteRequest(apiPath, bodyParams = {}) {
  // DELETE请求使用与POST相同的签名逻辑
  return buildPostRequest(apiPath, bodyParams);
}

/**
 * 查询当前挂单
 */
function queryCurrentOrders() {
  const apiPath = '/v1/perpum-market/orders/open';
  const queryParams = {
    instrument: 'BTC',
    positionType: 'plan',
    page: 1,
    pageSize: 20  // 增加页大小以确保能获取足够的订单
  };

  const { url, headers } = buildGetRequest(apiPath, queryParams);

  const response = http.get(url, { headers, tags: { name: 'QueryOrders' } });

  console.log('\n========== 查询订单响应 ==========');
  console.log(`URL: ${url}`);
  console.log(`状态码: ${response.status}`);
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
    const orderIds = parsed.data && parsed.data.rows ?
                     parsed.data.rows.map(order => order.id) :
                     [];
    console.log(`查询到 ${orderIds.length} 个挂单: ${orderIds.join(', ')}`);
    return orderIds;
  } catch (e) {
    console.error(`解析查询响应失败: ${e.message}`);
    return [];
  }
}

/**
 * 批量撤单（每次撤10个）
 */
function cancelBatchOrders(orderIds) {
  if (orderIds.length === 0) {
    console.log('无挂单，跳过撤单');
    return true;
  }

  // 每次只撤10个订单
  const batchSize = 10;
  const ordersToCancel = orderIds.slice(0, batchSize);

  console.log(`准备撤销 ${ordersToCancel.length} 个订单（共${orderIds.length}个）: ${ordersToCancel.join(', ')}`);

  const apiPath = '/v1/perpum-market/batchOrders';

  // 将orderIds数组转为逗号分隔的字符串或根据实际API要求调整
  const bodyParams = {
    orderIds: ordersToCancel.join(',')  // 如果API要求数组格式，需要调整
  };

  const { url, body, headers } = buildDeleteRequest(apiPath, bodyParams);

  console.log('\n========== 撤单请求 ==========');
  console.log(`URL: ${url}`);
  console.log(`Body: ${body}`);
  console.log('============================\n');

  const response = http.del(url, body, { headers, tags: { name: 'CancelBatch' } });

  console.log('\n========== 撤单响应 ==========');
  console.log(`状态码: ${response.status}`);
  console.log(`响应体: ${response.body}`);
  console.log('============================\n');

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

  if (!success) {
    console.log(`撤单失败: ${response.status} - ${response.body}`);
    errorRate.add(1);
    return false;
  }

  console.log(`批量撤单成功: ${ordersToCancel.length} 个订单`);
  return true;
}

export default function () {
  // 查询挂单
  const orderIds = queryCurrentOrders();
  sleep(0.5);

  // 批量撤单（每次10个）
  if (orderIds.length > 0) {
    cancelBatchOrders(orderIds);
  }
}