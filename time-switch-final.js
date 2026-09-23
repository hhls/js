// time-switch-final.js
// 高峰期定义：工作日 19:00-23:00，周末 12:00-23:00
// 高峰期 → Final-FB，非高峰 → Wepc-FB

const GROUP = '兜底分流';
const now = new Date();
const hour = now.getHours();
const day = now.getDay(); // 0=周日, 1-5=周一至周五, 6=周六

const isWeekend = (day === 0 || day === 6);
const isPeak = isWeekend ? (hour >= 12 && hour < 23) : (hour >= 19 && hour < 23);
const target = isPeak ? 'Final-FB' : 'Wepc-FB';

// 幂等：已是目标策略直接退出，无感知
const current = $persistentStore.read('final_policy');
if (current === target) {
    $done();
}

const result = $surge.setSelectGroupPolicy(GROUP, target);
if (result) {
    $persistentStore.write(target, 'final_policy');

    // 切换成功通知（默认关闭，取消下方注释即可开启）
    $notification.post('兜底分流 定时切换', isPeak ? '⚡ 高峰期' : '🌙 非高峰期', `已切换到 ${target} · ✅`);
} else {
    // 切换失败通知（建议保持开启，便于排查异常）
    $notification.post('兜底分流', '切换失败', `目标: ${target}`);
}

$done();
