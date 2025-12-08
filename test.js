import http from 'k6/http';

export default function () {
  const res = http.get('https://httpbin.org/json');

  // 方式1：你原来的打印里用的
  const str1 = res.body.toString();                    // k6 里这就是字符串

  // 方式2：JSON.parse(JSON.stringify(...)) 再转回字符串（最常见的“标准化”方式）
  const str2 = JSON.stringify(res.json());             // 先解析成对象，再序列化

  // 严格对比两者是否 100% 相等
  const isEqual = str1 === str2;

  console.info(`方式1 (toString)      : ${str1}`);
  console.info(`方式2 (json() stringify): ${str2}`);
  console.info(`两者字符串完全相等吗？ ${isEqual}`);   // 关键结果
}