import { md5 } from 'k6/crypto';
import { URLSearchParams } from 'https://jslib.k6.io/url/1.0.0/index.js';

/**
 * K6 API 签名工具类
 * 支持 GET、POST、DELETE 等请求方法的参数加签
 */
export class SignUtil {
  constructor(apiKey, secretKey, options = {}) {
    this.apiKey = apiKey;
    this.secretKey = secretKey;
    this.debug = options.debug || false;

    if (!this.apiKey || !this.secretKey) {
      throw new Error('API Key 和 Secret Key 不能为空');
    }
  }

  /**
   * 生成 MD5 签名
   * @param {string} input - 待签名的字符串
   * @returns {string} - MD5 签名（大写）
   */
  generateMD5(input) {
    if (!input) {
      console.error('MD5 输入为空');
      return '';
    }
    return md5(input, 'hex').toUpperCase();
  }

  /**
   * 对参数进行排序并拼接
   * @param {Object} params - 请求参数对象
   * @returns {string} - 拼接后的参数字符串
   */
  buildSignString(params) {
    // 深拷贝参数，避免修改原始对象
    const allParams = Object.assign({}, params);

    // 添加 api_key
    allParams.api_key = this.apiKey;

    if (this.debug) {
      console.log('排序前参数:', JSON.stringify(allParams));
    }

    // 按键名升序排序
    const sortedKeys = Object.keys(allParams).sort();

    // 拼接参数字符串（过滤空值、null、undefined）
    const paramPairs = [];
    sortedKeys.forEach(key => {
      const value = allParams[key];
      // 严格按照原逻辑：过滤 null、undefined 和空字符串（trim后）
      if (value !== null && value !== undefined) {
        const strValue = String(value).trim();
        if (strValue !== '') {
          paramPairs.push(`${key}=${value}`); // 使用原始值，不使用trim后的值
        }
      }
    });

    const signParam = paramPairs.join('&');

    if (this.debug) {
      console.log('排序后参数串:', signParam);
    }

    // 添加 secret_key
    return `${signParam}&secret_key=${this.secretKey}`;
  }

  /**
   * 生成签名
   * @param {Object} params - 请求参数
   * @returns {string} - 签名值
   */
  sign(params) {
    const signString = this.buildSignString(params);

    if (this.debug) {
      console.log(`待签名字符串: ${signString}`);
    }

    const signature = this.generateMD5(signString);

    if (this.debug) {
      console.log(`生成签名: ${signature}`);
    }

    return signature;
  }

  /**
   * 为 GET 请求准备参数（带签名的参数对象）
   * @param {Object} params - 请求参数
   * @returns {Object} - 包含签名的完整参数对象
   */
  signGetParams(params = {}) {
    const signature = this.sign(params);

    // 注意：返回新对象，不修改原参数
    return {
      ...params,
      api_key: this.apiKey,
      sign: signature
    };
  }

  /**
   * 为 POST/PUT/DELETE 请求准备参数（application/x-www-form-urlencoded）
   * @param {Object} params - 请求参数
   * @returns {Object} - 包含 body 和 headers 的对象
   */
  signPostParams(params = {}) {
    const signature = this.sign(params);

    const signedParams = {
      ...params,
      api_key: this.apiKey,
      sign: signature
    };

    // 直接返回编码后的 body 字符串，更符合 k6 使用习惯
    const body = new URLSearchParams(signedParams).toString();

    return {
      body: body,
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
      }
    };
  }

  /**
   * 构建 GET 请求的完整 URL
   * @param {string} baseUrl - 基础 URL
   * @param {Object} params - 请求参数
   * @returns {string} - 带签名参数的完整 URL
   */
  buildGetUrl(baseUrl, params = {}) {
    const signedParams = this.signGetParams(params);
    const queryString = new URLSearchParams(signedParams).toString();

    // 处理 baseUrl 已有查询参数的情况
    const separator = baseUrl.includes('?') ? '&' : '?';
    return `${baseUrl}${separator}${queryString}`;
  }

  /**
   * 构建 POST 请求体（x-www-form-urlencoded 格式）
   * @param {Object} params - 请求参数
   * @returns {string} - 编码后的请求体字符串
   */
  buildPostBody(params = {}) {
    const { body } = this.signPostParams(params);
    return body;
  }

  /**
   * 构建 POST 请求的完整配置（body + headers）
   * @param {Object} params - 请求参数
   * @returns {Object} - { body: string, headers: Object }
   */
  buildPostRequest(params = {}) {
    return this.signPostParams(params);
  }
}
}

/**
 * 使用示例：
 *
 * import http from 'k6/http';
 * import { SignUtil } from './sign-util.js';
 *
 * // 初始化签名工具（支持 debug 模式）
 * const signUtil = new SignUtil('your_api_key', 'your_secret_key', { debug: true });
 *
 * export default function() {
 *   // ============ GET 请求示例 ============
 *
 *   // 方式1: 构建完整 URL（推荐，最简洁）
 *   const getUrl = signUtil.buildGetUrl('https://api.example.com/data', {
 *     user_id: '123',
 *     action: 'query'
 *   });
 *   http.get(getUrl);
 *
 *   // 方式2: 使用 params 参数
 *   const getParams = signUtil.signGetParams({ user_id: '123' });
 *   http.get('https://api.example.com/data', { params: getParams });
 *
 *   // ============ POST 请求示例 ============
 *
 *   // 方式1: 使用 buildPostRequest（推荐，返回 body + headers）
 *   const postData = signUtil.buildPostRequest({
 *     user_id: '123',
 *     name: 'test',
 *     age: '25'
 *   });
 *   http.post('https://api.example.com/user', postData.body, {
 *     headers: postData.headers
 *   });
 *
 *   // 方式2: 只构建 body
 *   const postBody = signUtil.buildPostBody({ user_id: '123', name: 'test' });
 *   http.post('https://api.example.com/user', postBody, {
 *     headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
 *   });
 *
 *   // ============ DELETE 请求示例 ============
 *   const deleteData = signUtil.buildPostRequest({ user_id: '123' });
 *   http.del('https://api.example.com/user', deleteData.body, {
 *     headers: deleteData.headers
 *   });
 *
 *   // ============ PUT 请求示例 ============
 *   const putData = signUtil.buildPostRequest({ user_id: '123', status: 'active' });
 *   http.put('https://api.example.com/user', putData.body, {
 *     headers: putData.headers
 *   });
 *
 *   // ============ 高级用法：只获取签名 ============
 *   const signature = signUtil.sign({ user_id: '123' });
 *   console.log(`签名值: ${signature}`);
 * }
 *
 * // ============ 性能测试场景示例 ============
 * export default function() {
 *   const signUtil = new SignUtil(__ENV.API_KEY, __ENV.SECRET_KEY);
 *
 *   // 场景1: 查询接口
 *   const queryUrl = signUtil.buildGetUrl('https://api.example.com/query', {
 *     page: '1',
 *     size: '10'
 *   });
 *   http.get(queryUrl);
 *
 *   // 场景2: 创建接口
 *   const createData = signUtil.buildPostRequest({
 *     name: 'user_' + __ITER,
 *     email: `user${__ITER}@test.com`
 *   });
 *   http.post('https://api.example.com/create', createData.body, {
 *     headers: createData.headers
 *   });
 * }
 */