// time-switch-final.js
// 高峰期定义：工作日 19:00-23:00，周末 12:00-23:00
// 高峰期 → US-Final，非高峰 → US-Wepc，兜底始终保留 US-V.PS（由策略组顺序兜底，脚本不干预）

const now = new Date();
const hour = now.getHours();
const day = now.getDay(); // 0=周日, 1-5=周一至周五, 6=周六

const isWeekend = (day === 0 || day === 6);

// 高峰时段判断
let isPeak;
if (isWeekend) {
  isPeak = (hour >= 12 && hour < 23);
} else {
  isPeak = (hour >= 19 && hour < 23);
}

const target = isPeak ? 'Final-FB' : 'Wepc-FB';
const result = $surge.setSelectGroupPolicy('兜底分流', target);

$notification.post(
  '兜底分流 定时切换',
  isPeak ? '⚡ 高峰期' : '🌙 非高峰期',
  `已切换到 ${target}  ${result ? '✅' : '❌ 切换失败'}`
);

$done();
