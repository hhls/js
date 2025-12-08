import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';

// 自定义指标
const errorRate = new Rate('errors');
const querySuccessRate = new Rate('query_success');
const reverseSuccessRate = new Rate('reverse_success');

// 压测配置
//export const options = {
//  stages: [
//    { duration: '30s', target: 10 },  // 30秒内逐步增加到10个虚拟用户
//    { duration: '1m', target: 20 },   // 1分钟内增加到20个虚拟用户
//    { duration: '30s', target: 0 },   // 30秒内降至0
//  ],
//  thresholds: {
//    http_req_duration: ['p(95)<2000'], // 95%的请求应在2秒内完成
//    errors: ['rate<0.1'],              // 错误率应低于10%
//  },
//};
export const options = {
  vus: 1, // 固定1个虚拟用户
  //  duration: '30s', // 持续30秒
  duration: '100m', // 持续30秒
  iterations: 100,  // 执行10次后结束
  thresholds: {
    http_req_duration: ['p(95)<2000'], // 95%的请求应在2秒内完成
    errors: ['rate<0.1'],              // 错误率应低于10%
  },
  thresholds: {
    'http_req_duration{name:QueryPositions}': ['p(99)<500'],
    'http_req_duration{name:ReversePosition}': ['p(99)<1000']
  }
};

// 公共请求头
const commonHeaders = {
  'Cache-Control': 'no-cache',
  'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0.1 Safari/605.1.15',
  'Pragma': 'no-cache',
  'Origin': 'https://www.coinw.com',
  'Sec-Fetch-Dest': 'empty',
  'Sec-Fetch-Site': 'same-site',
  'Accept-Language': 'zh_CN',
  'Accept': 'application/json, text/plain, */*',
  'Accept-Encoding': 'gzip, deflate, br',
  'Sec-Fetch-Mode': 'cors',
  'language': 'zh_CN',
  'x-requested-with': 'XMLHttpRequest',
  'thirdapptoken': 'FBE22A86565D79715B4465FE2DD34DF8web_1763016050062_26330741',
  'logintoken': 'FBE22A86565D79715B4465FE2DD34DF8web_1763016050062_26330741',
  'x-authorization': 'FBE22A86565D79715B4465FE2DD34DF8web_1763016050062_26330741',
  'token': 'FBE22A86565D79715B4465FE2DD34DF8web_1763016050062_26330741',
  'devicename': 'Safari V26.0.1 (macOS)',
  'systemversion': 'macOS 10.15.7',
  'thirdappid': 'coinw',
  'appversion': '100.100.100',
  'selecttype': 'USD',
  'Priority': 'u=3, i',
  'x-language': 'zh_CN',
  'x-locale': 'zh_CN',
  'clienttag': 'web',
  'cwdeviceid': '6c670ebcbd4a633714bfc53eb95780d1',
  'withcredentials': 'true',
};

// 配置参数
const config = {
  baseUrl: 'https://futuresapi.coinw.com',
  positionModelType: 2, // 仓位模式类型
  targetInstrument: 'BTC', // 目标交易对，如果为空则反向所有仓位
};

export default function () {
  // 步骤1: 查询账户和仓位列表
  const queryUrl = `${config.baseUrl}/v1/futuresc/thirdClient/trade/accountsCountInfo/list/${config.positionModelType}?dataType=trade_cclist_data&positionModelType=${config.positionModelType}`;

  const queryResponse = http.get(queryUrl, {
    headers: commonHeaders,
    tags: { name: 'QueryPositions' },
  });

  // 检查查询请求
  const querySuccess = check(queryResponse, {
    '查询状态为200': (r) => r.status === 200,
    '查询响应有body': (r) => r.body && r.body.length > 0,
  });

  querySuccessRate.add(querySuccess);

  if (!querySuccess) {
    console.error(`查询失败: status=${queryResponse.status}, body=${queryResponse.body}`);
    errorRate.add(1);
    return;
  }

  // 解析响应获取仓位列表
  let positions = [];
  try {
    const data = JSON.parse(queryResponse.body);

    if (data.code !== 0) {
      console.error(`API返回错误: code=${data.code}, msg=${data.msg}`);
      errorRate.add(1);
      return;
    }

    // 遍历所有账户数据
    if (data.data && Array.isArray(data.data)) {
      data.data.forEach(account => {
        if (account.userPositions && Array.isArray(account.userPositions)) {
          account.userPositions.forEach(position => {
            // 如果指定了目标交易对，只处理该交易对
            if (!config.targetInstrument || position.instrument === config.targetInstrument) {
              // 只处理开仓状态的仓位
              if (position.status === 'open') {
                positions.push({
                  id: position.id,
                  instrument: position.instrument,
                  direction: position.direction,
                  quantity: position.quantity,
                  openPrice: position.openPrice,
                  leverage: position.leverage,
                });
              }
            }
          });
        }
      });
    }

    console.log(`找到 ${positions.length} 个仓位`);

    if (positions.length === 0) {
      console.log('没有找到可反向的仓位');
      return;
    }
  } catch (e) {
    console.error(`解析响应失败: ${e.message}, body=${queryResponse.body}`);
    errorRate.add(1);
    return;
  }

  // 步骤2: 对每个仓位执行反向开仓
  positions.forEach((position, index) => {
    console.log(`处理仓位 ${index + 1}/${positions.length}: ${position.instrument} (${position.direction}), ID: ${position.id}`);

    const reverseUrl = `${config.baseUrl}/v1/futuresc/thirdClient/trade/${position.instrument}/closeAndOpenReverse/${position.id}`;

    const reversePayload = JSON.stringify({
      dataType: 'trade_take',
      takeType: 'reverseOpenPosition',
      activeType: 'holdShares',
      instrument: position.instrument,
      id: position.id,
    });

    const reverseResponse = http.post(reverseUrl, reversePayload, {
      headers: {
        ...commonHeaders,
        'Content-Type': 'application/json',
        'Content-Length': reversePayload.length.toString(),
      },
      tags: {
        name: 'ReversePosition',
        instrument: position.instrument,
      },
    });

    // 检查反向开仓请求
    const reverseSuccess = check(reverseResponse, {
      '反向开仓状态为200': (r) => r.status === 200,
      '反向开仓响应成功': (r) => {
        try {
          const data = JSON.parse(r.body);
          return data.code === 0;
        } catch (e) {
          return false;
        }
      },
    });

    reverseSuccessRate.add(reverseSuccess);

    if (!reverseSuccess) {
      console.error(`反向开仓失败: ${position.instrument}, positionId=${position.id}, status=${reverseResponse.status}, body=${reverseResponse.body}`);
      errorRate.add(1);
    } else {
      console.log(`成功执行反向开仓: ${position.instrument}, ${position.direction} -> ${position.direction === 'long' ? 'short' : 'long'}`);
    }

    // 如果有多个仓位，每个请求之间增加间隔
    if (index < positions.length - 1) {
      sleep(0.5);
    }
  });

  // 主循环间隔
  sleep(2);
}