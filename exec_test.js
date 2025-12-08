import { sleep } from 'k6';
import exec from 'k6/execution'; // 确保导入正确

export const options = {
  scenarios: {
    my_scenario: {
      executor: 'shared-iterations',
      vus: 5,
      iterations: 20, // 总共20次迭代
      maxDuration: '1m',
    },
  },
};

export default function () {
  // 打印当前 VU 的各种 ID
  console.log(
    `VU ID (global): ${exec.vu.idInTest}, ` +
    `VU ID (scenario): ${exec.vu.idInScenario}, ` +
    `Iteration in Test: ${exec.scenario.iterationInTest}`
  );

  sleep(1);
}
