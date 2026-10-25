(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const moduleInfo = {
    pi: ['Pi / ROS 2', '主用 mecanum_mavlink 发送运动与管理请求，管理 GPIO17 READY。GPIO22 HALTED 由内核产生，成功 ACK 不表示已经断电。', '../ros2_ws/src/mecanum_mavlink/mecanum_mavlink/bridge.py'],
    esp: ['ESP32 / 无线桥', '转发 UART 与 Wi-Fi UDP，接收 RC。无线数据仍必须通过 MCU 的来源、指令期限与故障联锁检查。', '../ESP32_Radio_Bridge/src/main.cpp'],
    host: ['HostRx / MAVLink', '每个串口独立解析。检查目标与来源，接受配置和显式 ARM；有效参数防抖后追加到 Flash，加载参数不会自动解锁。', 'vehicle_mavlink.c'],
    safety: ['Safety / 统一授权', '维护控制来源、租约、指令期限和故障状态。Pi 还要求 READY；参数保存维护窗口拒绝解锁，故障恢复不自动恢复运动。', 'vehicle_link.c'],
    control: ['Control / 四轮速度闭环', '读取四路编码器，将车体速度逆解为四轮目标，统一限速并限制目标斜率，再计算 PI + 前馈。实际输出还要经过健康与硬件联锁复核。', 'vehicle_control.c'],
    health: ['Health / 电气保护', '以 ADC 有效性与轮速、占空比判断过流、堵转、欠压等。清除必须连续满足恢复条件。NTC 默认未启用，电流和电压仍待实板标定。', 'vehicle_health.c'],
    sensor: ['SensorRx / Imu', 'DMA 接收与纯 C 帧解析分层。SBUS 租约使用物理帧时间，不能由缓存转发续租；ADC 与 PWM 尚未硬件锁相。', 'vehicle_sensor_app.c'],
    support: ['Power / Telemetry / Store', 'PowerTask 等待停轮与真实 HALTED 后单次切电；Telemetry 调度诊断并在禁止运动时追加参数。启动 Flash 擦除在 SRAM 里服务外部看门狗。', 'vehicle_power.c'],
    driver: ['A 板 / 四电机与反馈', 'STSPIN948 驱动四个电机，编码器返回四路正交计数。电机、编码器线序和极性必须实测；图示不代替真实接线验证。', 'vehicle_app.c']
  };
  function sourceURL(file) {
    return file.startsWith('../') ? file : '../MecanumB_Control/App/src/' + file;
  }
  document.querySelectorAll('[data-module]').forEach(node => {
    const choose = () => {
      document.querySelectorAll('[data-module]').forEach(n => n.setAttribute('aria-pressed', String(n === node)));
      const info = moduleInfo[node.dataset.module];
      $('module-title').textContent = info[0]; $('module-detail').textContent = info[1];
      $('module-source').href = sourceURL(info[2]); $('module-source').textContent = '查看 ' + info[2].split('/').pop();
    };
    node.addEventListener('click', choose);
    node.addEventListener('keydown', event => {
      if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); choose(); }
    });
  });
  // Values below illustrate state transitions, not a running MCU or sampled GPIO.
  const S = (title, detail, state, motor = false, pi = true, halted = false, file = 'vehicle_app.c', fault = false) =>
    ({title, detail, state, motor, pi, halted, file, fault});
  const flows = {
    boot: {
      normal: {label: 'Flash 中有有效配置', steps: [
        S('复位 / SRAM 初始化', '.data 包含 .RamFunc，启动汇编先复制到 SRAM；驱动许可保持关闭。', 'RESET'),
        S('HAL 与外设初始化', '时钟、GPIO、DMA、UART、TIM、ADC 等按当前初始化代码建立；这里没有运动授权。', 'INIT'),
        S('扫描 A/B 参数日志', '逐条检查记录、CRC 和配置范围；损坏的非空槽被跳过，空白槽无需擦除。', 'SCAN', false, true, false, 'vehicle_store.c'),
        S('装载有效配置', 'configured=1，但不设置 ARM 或电机许可。若要启动擦除，先进入禁止输出的 SRAM 维护路径。', 'DISARMED'),
        S('建立七个静态任务', '建立 Safety、Control、HostRx、Imu、SensorRx、Telemetry 和 Power；随后启动内部 IWDG 并进入调度。', 'DISARMED'),
        S('等待显式授权', '参数已有效；仍需所选来源满足 READY（Pi）、无故障、有效租约和显式 ARM。', 'DISARMED', false, true, false, 'vehicle_link.c')
      ]},
      missing: {label: '无有效参数 / CRC 均无效', steps: [
        S('复位 / 外设初始化', '驱动许可关闭；初始化时钟、外设和软件状态。', 'INIT'),
        S('扫描 A/B 参数日志', '没有记录通过 CRC 与配置范围校验，不将随机数据当作标定值。', 'SCAN', false, true, false, 'vehicle_store.c'),
        S('保持 configured=0', '创建任务并允许诊断通信，等待主机下发有效参数；不填入猜测几何值。', 'DISARMED'),
        S('ARM 请求被拒绝', 'configured=0 不满足授权条件；MOE 与 ARM 保持关闭。补齐配置后也需要重新显式 ARM。', 'DISARMED', false, true, false, 'vehicle_link.c', true)
      ]}
    },
    control: {
      normal: {label: '有效许可 / 正常控制周期', steps: [
        S('TIM6 通知 Control', '名义每 1 ms 唤醒；检查通知数、采样 dt 和配置版本。演示不表示实测任务耗时。', 'ACTIVE', true),
        S('编码器差分与滤波', '处理 TIM2/3/4/5 的 32/16/16/32 位回绕，换算 rad/s 并做一阶滤波。', 'ACTIVE', true, true, false, 'vehicle_control.c'),
        S('逆解 / 统一限幅', '车体速度转为四轮 desired；超过最大轮速时四轮使用同一个比例缩放。', 'ACTIVE', true, true, false, 'vehicle_control.c'),
        S('目标斜率 / 换向保护', '四轮按同一个 ratio 更新目标；目标反向而实测仍快时先零输出，允许改向时清积分。', 'ACTIVE', true, true, false, 'vehicle_control.c'),
        S('PI + 前馈 / 抗饱和', '方向归一化误差，条件积分与占空比裁剪；不是电流闭环，也不模拟机械响应。', 'ACTIVE', true, true, false, 'vehicle_control.c'),
        S('健康 / 联锁复核', '健康结果和实际硬件输入共同约束输出。没有故障且指令/租约有效时，才允许写入 DIR/CCR/MOE。', 'ACTIVE', true),
        S('输出并等待下一周期', '实际电机响应由编码器返回，下一周期继续闭环；演示数值不代表实车状态。', 'ACTIVE', true)
      ]},
      expired: {label: '本周期发现指令过期', steps: [
        S('Control 周期到达', '上一周期曾有运动许可，此时新的指令有效期已经结束。', 'ACTIVE', true),
        S('许可判断失败', 'vl_permitted() 要求当前时间仍在指令和授权期限内，过期不能由旧数据恢复。', 'ACTIVE / PERMIT=0', false, true, false, 'vehicle_link.c', true),
        S('目标 / 积分 / 占空比清零', 'vc_step() 在 permitted=false 分支清零运动输出，编码器观测仍可更新。', 'ACTIVE / OUTPUT=0', false, true, false, 'vehicle_control.c'),
        S('撤销运动授权', 'Safety 的状态检查进入 FAILSAFE；必须在原因解除后重新显式授权，不能靠迟到指令自动复活。', 'FAILSAFE', false, true, false, 'vehicle_link.c', true)
      ]}
    },
    safety: {
      normal: {label: '显式 ARM 与稳定确认', steps: [
        S('DISARMED', '有有效参数与通信，但当前没有运动权。心跳仅证明链路存在。', 'DISARMED', false, true, false, 'vehicle_link.c'),
        S('检查 ARM 条件', '检查来源、READY（Pi）、物理故障、配置、电源禁止和维护联锁。', 'DISARMED', false, true, false, 'vehicle_link.c'),
        S('进入 ARMING', '允许驱动唤醒请求；速度命令保持零，尚不允许非零 PWM。', 'ARMING', false, true, false, 'vehicle_link.c'),
        S('硬件就绪且轮静止', '就绪与中立条件连续满足 20 ms，且授权未超时，才晋升到来源 ACTIVE。', 'ARMING', false, true, false, 'vehicle_link.c'),
        S('ACTIVE + 新鲜指令', '只有新鲜指令、有效租约和无故障同时成立时，运动输出才被许可。', 'ACTIVE', true, true, false, 'vehicle_link.c')
      ]},
      estop: {label: '急停 / 硬件禁止', steps: [
        S('当前正在运动', '本场景从已有有效授权开始。硬件急停独立于无线与 ROS。', 'ACTIVE', true),
        S('急停输入 / 关闭输出', '硬件 Break/中断安全路径关闭 MOE、撤销 ARM 并清占空比；不等待低优先级诊断任务。', 'ACTIVE / OUTPUT=0', false, true, false, 'vehicle_app.c', true),
        S('故障锁存与撤权', 'Safety/Link 记录物理故障，旧授权和指令不能继续驱动。', 'FAULT', false, true, false, 'vehicle_link.c', true),
        S('释放急停 ≠ 自动运行', '物理原因解除后，仍要按授权流程重新 ARM。该演示不会执行任何实际故障注入。', 'FAULT / WAIT ARM', false, true, false, 'vehicle_link.c', true)
      ]},
      maintenance: {label: '参数保存期间请求 ARM', steps: [
        S('DISARMED / 参数稳定', '配置修改稳定 750 ms，且没有待确认授权，准备追加日志。', 'DISARMED'),
        S('原子取得维护联锁', '同一临界区检查状态并设置 maintenance；硬件 ARM/MOE 关闭。', 'MAINTENANCE'),
        S('并发 ARM 被拒绝', '所有来源使用同一维护联锁，不能在 Flash 编程时开始运动。', 'MAINTENANCE', false, true, false, 'vehicle_link.c', true),
        S('回读 / 保持 20 ms', '编程中不能提前解除窗口；完成后再等待 20 ms，由 Control 解除维护。', 'SETTLING'),
        S('回到等待显式授权', '维护结束不自动解锁。新配置若又变化，则保留 dirty 并重新防抖。', 'DISARMED')
      ]}
    },
    power: {
      normal: {label: '内核 HALTED 正常到达', steps: [
        S('接受 Pi 关机命令', '仅 Pi 通道、param1=0/param2=2、未解锁且 READY 有效；接受后立即禁止新授权。', 'VW_WAIT_STOP', false, true, false, 'vehicle_mavlink.c'),
        S('确认停轮 100 ms', '要求编码器不再变化并且实际驱动已关闭；WAIT_STOP 超过 5 s 则报故障。', 'VW_WAIT_STOP', false, true, false, 'vehicle_power.c'),
        S('请求 Linux 有序关机', 'PC6 经 A 板转换为 GPIO27 低有效请求。READY 在接受 ACK 后拉低，HALTED 尚未到达。', 'VW_WAIT_HALT', false, true, false, 'vehicle_power.c'),
        S('真实 HALTED 稳定 20 ms', 'GPIO22 必须由内核产生；必须有此前低电平观察，且仍停轮和驱动关闭。', 'VW_WAIT_HALT', false, true, true, 'vehicle_power.c'),
        S('单次切电脉冲 20 ms', 'PD3 发一次完整脉冲，清 A 板保持锁存。动画只表现顺序，不使用 20 ms 实时播放。', 'VW_CUT_PULSE', false, true, true, 'vehicle_power.c'),
        S('确认电源下降', '等待 PGOOD 低连续 20 ms；超过 2 s 不成功则故障，不自动重复切电。', 'VW_WAIT_OFF', false, false, true, 'vehicle_power.c'),
        S('进入 OFF', '请求线释放；重新上电还须满足新的 PGOOD 与 HALTED 低确认，且不会自动恢复运动。', 'VW_OFF', false, false, true, 'vehicle_power.c')
      ]},
      timeout: {label: 'HALTED 一直未到达', steps: [
        S('接受请求并禁止运动', '电源状态机先 inhibit，确认停轮与电机关闭。', 'VW_WAIT_STOP', false, true, false, 'vehicle_power.c'),
        S('请求 Linux 有序关机', '关机请求已施加，开始等待真实内核 HALTED。未启用验证后的 overlay 时可能停在此处。', 'VW_WAIT_HALT', false, true, false, 'vehicle_power.c'),
        S('等待超时 60 s', 'HALTED 未满足稳定条件，不能把 READY 下降或服务结束当作内核已停机。', 'VW_WAIT_HALT', false, true, false, 'vehicle_power.c', true),
        S('报错并保持供电', '进入 VW_FAULT / VW_HALT_TIMEOUT，不发切电脉冲；故障锁存，不强切或自动重试。', 'VW_FAULT', false, true, false, 'vehicle_power.c', true)
      ]}
    }
  };
  let steps = [], index = 0, timer = null;
  function pause() {
    if (timer !== null) clearInterval(timer);
    timer = null;
    $('flow-play').textContent = index === steps.length - 1 ? '重新播放' : '播放';
    $('flow-play').setAttribute('aria-pressed', 'false');
  }
  function render() {
    const step = steps[index];
    $('flow-progress').textContent = `${index + 1} / ${steps.length} 步 · 演示`;
    $('flow-title').textContent = step.title; $('flow-detail').textContent = step.detail;
    $('flow-phase').textContent = (step.fault ? 'BRANCH / ' : 'STEP / ') + step.state;
    $('flow-source').textContent = '代码依据：' + step.file; $('flow-source').href = sourceURL(step.file);
    $('signal-state').textContent = step.state; $('signal-motor').textContent = step.motor ? '允许（示意）' : '关闭 / 无 PWM';
    $('signal-pi').textContent = step.pi ? '保持供电（示意）' : 'PGOOD 低（示意）';
    $('signal-halted').textContent = step.halted ? '高（示意）' : '低 / 未确认';
    $('flow-prev').disabled = index === 0; $('flow-next').disabled = index === steps.length - 1;
    document.querySelectorAll('#flow-nodes button').forEach((button, i) => {
      button.classList.toggle('is-done', i < index);
      button.classList.toggle('is-fault', steps[i].fault);
      if (i === index) button.setAttribute('aria-current', 'step'); else button.removeAttribute('aria-current');
    });
    if (index === steps.length - 1) pause();
    else if (timer === null) $('flow-play').textContent = '播放';
  }
  function loadScenario() {
    pause(); index = 0; steps = flows[$('flow-kind').value][$('flow-scenario').value].steps;
    $('flow-nodes').replaceChildren(...steps.map((step, i) => {
      const li = document.createElement('li'), button = document.createElement('button');
      const number = document.createElement('span'), title = document.createElement('span');
      button.type = 'button'; button.dataset.step = String(i); number.className = 'step-number';
      number.textContent = String(i + 1).padStart(2, '0'); title.textContent = step.title;
      button.append(number, title); button.addEventListener('click', () => {pause(); index = i; render();}); li.append(button); return li;
    })); render(); pause();
  }
  function loadFlow() {
    $('flow-scenario').replaceChildren(...Object.entries(flows[$('flow-kind').value]).map(([value, scenario]) => {
      const option = document.createElement('option'); option.value = value; option.textContent = scenario.label; return option;
    })); loadScenario();
  }
  function play() {
    if (timer !== null) {pause(); return;}
    if (index === steps.length - 1) index = 0;
    render(); $('flow-play').textContent = '暂停'; $('flow-play').setAttribute('aria-pressed', 'true');
    timer = setInterval(() => {index++; render();}, Number($('flow-speed').value));
  }
  $('flow-kind').addEventListener('change', loadFlow); $('flow-scenario').addEventListener('change', loadScenario);
  $('flow-play').addEventListener('click', play);
  $('flow-prev').addEventListener('click', () => {pause(); index = Math.max(0, index - 1); render();});
  $('flow-next').addEventListener('click', () => {pause(); index = Math.min(steps.length - 1, index + 1); render();});
  $('flow-reset').addEventListener('click', loadScenario);
  $('flow-speed').addEventListener('change', () => {if (timer !== null) {pause(); play();}});
  document.addEventListener('visibilitychange', () => {if (document.hidden) pause();});
  loadFlow();

  // Same algebra/order as vc_inverse / vc_forward; UI inputs are illustrative.
  function inverse(vx, vy, wz, radius = .05, arm = .27, maximum = 20) {
    const turn = arm * wz;
    const raw = [(vx-vy-turn)/radius, (vx+vy+turn)/radius, (vx+vy-turn)/radius, (vx-vy+turn)/radius];
    const largest = Math.max(...raw.map(Math.abs));
    const scale = largest > maximum ? maximum / largest : 1;
    return {raw, scale, limited: raw.map(w => w * scale)};
  }
  function forward(w, radius = .05, arm = .27) {
    const scale = radius * .25;
    return [scale*(w[0]+w[1]+w[2]+w[3]), scale*(-w[0]+w[1]+w[2]-w[3]), scale*(-w[0]+w[1]-w[2]+w[3])/arm];
  }
  const wheels = ['FL', 'FR', 'RL', 'RR'];
  const bodyRows = $('wheel-results').querySelector('tbody');
  bodyRows.replaceChildren(...wheels.map(name => {
    const row = document.createElement('tr'); row.dataset.wheel = name;
    for (let c = 0; c < 4; c++) row.append(document.createElement('td'));
    return row;
  }));
  function updateKinematics() {
    const vx = Number($('kin-vx').value), vy = Number($('kin-vy').value), wz = Number($('kin-wz').value), r = Number($('kin-radius').value);
    $('kin-vx-value').textContent = vx.toFixed(2) + ' m/s'; $('kin-vy-value').textContent = vy.toFixed(2) + ' m/s';
    $('kin-wz-value').textContent = wz.toFixed(2) + ' rad/s'; $('kin-radius-value').textContent = r.toFixed(3) + ' m';
    const result = inverse(vx, vy, wz, r);
    result.limited.forEach((w, i) => {
      const dir = Math.abs(w) < 1e-9 ? 'stopped' : w > 0 ? 'positive' : 'negative';
      const group = $('wheel-' + wheels[i]); group.dataset.direction = dir;
      group.style.setProperty('--wheel-duration', (Math.max(.25, 12 / Math.max(1, Math.abs(w)))).toFixed(2) + 's');
      $('wheel-value-' + wheels[i]).textContent = (w >= 0 ? '+' : '') + w.toFixed(2) + ' rad/s';
      const cells = bodyRows.children[i].children;
      [wheels[i], result.raw[i].toFixed(3), w.toFixed(3), dir === 'stopped' ? '停止' : w > 0 ? '正转 +' : '反转 −'].forEach((value, j) => cells[j].textContent = value);
    });
    $('kin-scale').textContent = `统一缩放 s=${result.scale.toFixed(4)} · ${result.scale < 1 ? '超过 20 rad/s，四轮同比限幅' : '未触发轮速上限'}。`;
    const reconstructed = forward(result.limited, r);
    $('kin-forward').textContent = `正解回算（不含滑移）：vx=${reconstructed[0].toFixed(3)} m/s，vy=${reconstructed[1].toFixed(3)} m/s，wz=${reconstructed[2].toFixed(3)} rad/s。`;
    const length = Math.hypot(vx, vy), unit = length > 0 ? 70 / Math.max(1, length) : 0;
    $('body-vector').setAttribute('d', `M210 245L${210 + vy*unit} ${245-vx*unit}`);
    $('body-vector').style.opacity = length > 0 ? '1' : '0';
    $('yaw-vector').style.opacity = Math.abs(wz) > 0 ? '1' : '0';
    $('yaw-vector').setAttribute('d', wz >= 0 ? 'M250 275A48 48 0 1 0 170 275' : 'M170 275A48 48 0 1 1 250 275');
  }
  ['vx', 'vy', 'wz', 'radius'].forEach(name => $('kin-' + name).addEventListener('input', updateKinematics));
  const presets = {forward:[.4,0,0], lateral:[0,.4,0], rotate:[0,0,1], mixed:[1.2,.8,2], stop:[0,0,0]};
  document.querySelectorAll('[data-preset]').forEach(button => button.addEventListener('click', () => {
    ['vx','vy','wz'].forEach((name,i) => $('kin-' + name).value = presets[button.dataset.preset][i]); updateKinematics();
  }));
  $('wheel-motion').addEventListener('click', () => {
    const active = document.querySelector('.vehicle-panel').classList.toggle('wheel-animating');
    $('wheel-motion').setAttribute('aria-pressed', String(active)); $('wheel-motion').textContent = active ? '暂停轮组示意' : '播放轮组示意';
  });
  function piStep(target, measured) {
    if (Math.abs(target) < .001) return {error:0, candidate:0, integral:0, output:0, duty:0, accepted:false};
    const direction = target < 0 ? -1 : 1, error = direction * (target - measured);
    const candidate = Math.max(-.5, Math.min(.5, error * .2 * .001));
    const output = Math.abs(target) * .02 + .1 * error + candidate;
    const accepted = (output >= 0 && output <= .5) || (output > .5 && error < 0) || (output < 0 && error > 0);
    const integral = accepted ? candidate : 0;
    const duty = direction * Math.max(0, Math.min(.5, Math.abs(target) * .02 + .1 * error + integral));
    return {error, candidate, integral, output, duty, accepted};
  }
  function updatePI() {
    const target = Number($('pi-target').value), measured = Number($('pi-measured').value), result = piStep(target, measured);
    $('pi-target-value').textContent = target.toFixed(1); $('pi-measured-value').textContent = measured.toFixed(1);
    $('pi-result').textContent = Math.abs(target) < .001 ? '目标接近零：duty=0，积分清零。' :
      `方向归一化误差 e=${result.error.toFixed(3)}；候选积分=${result.candidate.toFixed(4)}；候选输出=${result.output.toFixed(4)}；${result.accepted ? '接受积分' : '本步冻结积分（抗饱和）'}；最终带符号 duty=${result.duty.toFixed(4)}（${(Math.abs(result.duty)*100).toFixed(2)}%）。`;
  }
  $('pi-target').addEventListener('input', updatePI); $('pi-measured').addEventListener('input', updatePI);
  updateKinematics(); updatePI();
  window.MecanumReport = Object.freeze({inverse, forward, piStep});
})();
