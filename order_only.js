import http from 'k6/http';
import { check, sleep } from 'k6';
import { hmac } from 'k6/crypto';
import { SharedArray } from 'k6/data';
import papaparse from 'https://jslib.k6.io/papaparse/5.1.1/index.js';

const credentials = new SharedArray('credentials', function () {
  const csvData = open('./user_credentials2.csv');
  return papaparse.parse(csvData, { header: true }).data;
});

const INSTRUMENTS = ['BNB', 'BTC', 'ETH'];

export const options = {
  iterations: 1,
  vus: 1,
};

const BASE_URL = __ENV.BASE_URL || 'https://api.cwfutures.top';

function sign(method, apiPath, body, apiKey, secretKey, uid) {
  const timestamp = Date.now();
  const signStr = `${timestamp}${method}${apiPath}${body ? JSON.stringify(body) : ''}`;
  const signature = hmac('sha256', secretKey, signStr, 'base64');

  return {
    'Content-Type': 'application/json',
    'Connection': 'close',
    'sign': signature,
    'api_key': apiKey,
    'timestamp': timestamp.toString(),
    'X-USER-ID': uid,
  };
}

function placeBatchOrders(user, instrument) {
  const apiPath = '/v1/perpum-market/batchOrders';
  const url = `${BASE_URL}${apiPath}`;

  const orders = Array(10).fill({
    instrument: instrument,
    direction: 'long',
    leverage: 1,
    quantityUnit: 1,
    quantity: 1,
    positionModel: 1,
    positionType: 'plan',
    openPrice: 1,
    userId: user.userid
  });

  const payload = JSON.stringify(orders);
  const headers = sign('POST', apiPath, orders, user.apikey, user.secretkey, user.uid);
  headers['Content-Length'] = payload.length.toString();

  const res = http.post(url, payload, { headers });

  const success = check(res, {
    '下单200': r => r.status === 200,
    '下单成功': r => JSON.parse(r.body).code === 0,
  });

  if (success) {
    console.log(`[${user.account}] ${instrument} 下单成功（10单）`);
  } else {
    console.error(`[${user.account}] ${instrument} 下单失败: ${res.status} - ${res.body}`);
  }
}

export default function () {
  for (const user of credentials) {
    // 每个用户对所有币对都下单
    for (const instrument of INSTRUMENTS) {
      placeBatchOrders(user, instrument);
      sleep(0.3);
    }
    console.log(`[${user.account}] 所有币种下单完成`);
  }

  console.log(`全部用户全部币种下单完成！总计 ${credentials.length * INSTRUMENTS.length * 10} 单`);
}