(function () {
  "use strict";

  const scene = window.EBPF_VISUAL_SCENE;
  if (!scene || !Array.isArray(scene.steps) || scene.steps.length === 0) {
    throw new Error("EBPF_VISUAL_SCENE is missing or empty");
  }

  const canvas = document.querySelector("canvas[data-visual-canvas]");
  const ctx = canvas.getContext("2d");
  const params = new URLSearchParams(window.location.search);
  const captureMode = params.get("capture") === "1";
  const captureFrame = Number.parseInt(params.get("frame") || "0", 10);
  const state = {
    index: Number.isFinite(captureFrame) ? Math.abs(captureFrame) % scene.steps.length : 0,
    playing: false,
    elapsed: 0,
    lastTime: 0,
    goal: scene.defaultGoal || null,
  };

  if (captureMode) {
    document.body.classList.add("capture-mode");
  }

  function palette(name) {
    const colors = {
      userspace: { fill: "#e8f3ef", stroke: "#197167", text: "#184f49" },
      kernel: { fill: "#e8f0f7", stroke: "#3975a7", text: "#274f70" },
      hook: { fill: "#fff0e2", stroke: "#ce772c", text: "#714515" },
      data: { fill: "#f1ecf8", stroke: "#8064a3", text: "#4c3a63" },
      inactive: { fill: "#f5f7f8", stroke: "#aebbc1", text: "#67777e" },
    };
    return colors[name] || colors.kernel;
  }

  function roundedRect(x, y, width, height, radius) {
    const r = Math.min(radius, width / 2, height / 2);
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + width, y, x + width, y + height, r);
    ctx.arcTo(x + width, y + height, x, y + height, r);
    ctx.arcTo(x, y + height, x, y, r);
    ctx.arcTo(x, y, x + width, y, r);
    ctx.closePath();
  }

  function drawArrow(from, to, active) {
    const startX = from.x + from.w;
    const startY = from.y + from.h / 2;
    const endX = to.x;
    const endY = to.y + to.h / 2;
    ctx.strokeStyle = active ? "#d8782c" : "#91a0a7";
    ctx.fillStyle = ctx.strokeStyle;
    ctx.lineWidth = active ? 4 : 2;
    ctx.beginPath();
    ctx.moveTo(startX, startY);
    ctx.bezierCurveTo(startX + 26, startY, endX - 26, endY, endX, endY);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(endX, endY);
    ctx.lineTo(endX - 9, endY - 6);
    ctx.lineTo(endX - 9, endY + 6);
    ctx.closePath();
    ctx.fill();
  }

  function drawNode(node, active, complete) {
    const color = palette(active ? node.kind : complete ? node.kind : "inactive");
    ctx.fillStyle = color.fill;
    ctx.strokeStyle = color.stroke;
    ctx.lineWidth = active ? 4 : 2;
    roundedRect(node.x, node.y, node.w, node.h, 7);
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = color.text;
    ctx.textAlign = "center";
    const compact = node.w < 70;
    ctx.font = `${compact ? "700 9px" : "700 16px"} 'Segoe UI', 'Microsoft YaHei', sans-serif`;
    ctx.fillText(node.label, node.x + node.w / 2, node.y + node.h / 2 - 4);
    ctx.font = `${compact ? "8px" : "12px"} 'Segoe UI', 'Microsoft YaHei', sans-serif`;
    ctx.fillText(node.sub, node.x + node.w / 2, node.y + node.h / 2 + 18);

    if (complete) {
      ctx.fillStyle = "#197167";
      ctx.beginPath();
      ctx.arc(node.x + node.w - 10, node.y + 10, 7, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = "#fff";
      ctx.font = "700 10px sans-serif";
      ctx.fillText("✓", node.x + node.w - 10, node.y + 13);
    }
  }

  function drawPacket(from, to, progress) {
    const startX = from.x + from.w;
    const startY = from.y + from.h / 2;
    const endX = to.x;
    const endY = to.y + to.h / 2;
    const x = startX + (endX - startX) * progress;
    const y = startY + (endY - startY) * progress;
    ctx.fillStyle = "#e0782d";
    ctx.shadowColor = "rgba(86,49,18,.22)";
    ctx.shadowBlur = 8;
    roundedRect(x - 9, y - 9, 18, 18, 4);
    ctx.fill();
    ctx.shadowBlur = 0;
  }

  function layoutNodes(width, height) {
    const count = scene.nodes.length;
    const mobile = width < 600;
    const horizontalPadding = mobile ? 12 : Math.max(34, width * 0.045);
    const available = width - horizontalPadding * 2;
    const mobileNodeWidth = Math.max(42, Math.min(62, available / count - 5));
    const nodeWidth = mobile ? mobileNodeWidth : Math.max(94, Math.min(138, available / count - 20));
    const gap = count > 1 ? (available - nodeWidth * count) / (count - 1) : 0;
    const centerY = height * 0.48;
    return scene.nodes.map((node, index) => ({
      ...node,
      x: horizontalPadding + index * (nodeWidth + gap),
      y: centerY - (mobile ? 32 : 42) + (node.offsetY || 0),
      w: nodeWidth,
      h: mobile ? 64 : 84,
    }));
  }

  function resizeCanvas() {
    const rect = canvas.getBoundingClientRect();
    // HiDPI 下扩大 backing store，CSS 尺寸保持不变，避免文字模糊。
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    const targetWidth = Math.max(1, Math.round(rect.width * ratio));
    const targetHeight = Math.max(1, Math.round(rect.height * ratio));
    // 只有布局尺寸改变时才重建 backing store，避免每帧清空并触发浏览器合成闪烁。
    if (canvas.width !== targetWidth || canvas.height !== targetHeight) {
      canvas.width = targetWidth;
      canvas.height = targetHeight;
    }
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    return { width: rect.width, height: rect.height };
  }

  function draw(timestamp) {
    const size = resizeCanvas();
    ctx.clearRect(0, 0, size.width, size.height);
    const nodes = layoutNodes(size.width, size.height);
    const step = scene.steps[state.index];

    ctx.fillStyle = "#f7fafb";
    ctx.fillRect(0, 0, size.width, size.height);
    ctx.fillStyle = "#31464f";
    ctx.textAlign = "left";
    ctx.font = "700 14px 'Segoe UI', 'Microsoft YaHei', sans-serif";
    ctx.fillText(scene.canvasTitle, 18, 28);
    ctx.fillStyle = "#65767e";
    ctx.font = "12px 'Segoe UI', 'Microsoft YaHei', sans-serif";
    ctx.fillText(step.phase, 18, 49);

    for (let index = 0; index < nodes.length - 1; index += 1) {
      drawArrow(nodes[index], nodes[index + 1], index < state.index);
    }
    nodes.forEach((node, index) => drawNode(node, index === state.index, index < state.index));

    if (nodes.length > 1 && state.index < nodes.length - 1) {
      const duration = scene.stepDuration || 1500;
      const progress = captureMode ? (captureFrame % 8) / 7 : Math.min(1, state.elapsed / duration);
      drawPacket(nodes[state.index], nodes[state.index + 1], progress);
    }

    if (scene.drawOverlay) {
      scene.drawOverlay(ctx, size, state, nodes);
    }

    updateKnowledge(step);
    updateControls();

    if (state.playing && !captureMode) {
      const delta = state.lastTime ? timestamp - state.lastTime : 0;
      state.elapsed += delta;
      if (state.elapsed >= (scene.stepDuration || 1500)) {
        state.index = (state.index + 1) % scene.steps.length;
        state.elapsed = 0;
      }
      state.lastTime = timestamp;
      window.requestAnimationFrame(draw);
    }
  }

  function updateKnowledge(step) {
    document.querySelector("[data-step-title]").textContent = step.title;
    document.querySelector("[data-step-context]").textContent = step.context;
    document.querySelector("[data-step-meaning]").textContent = step.meaning;
    document.querySelector("[data-step-source]").textContent = step.source;
    document.querySelector("[data-step-risk]").textContent = step.risk;
    document.querySelector("[data-step-command]").textContent = step.command;
    document.querySelector("[data-context-badge]").textContent = step.context;
  }

  function updateControls() {
    const total = scene.steps.length;
    const progress = ((state.index + 1) / total) * 100;
    document.querySelector("[data-progress]").style.width = `${progress}%`;
    document.querySelector("[data-step-counter]").textContent = `${state.index + 1} / ${total}`;
    document.querySelector('[data-action="play"]').classList.toggle("active", state.playing);
  }

  function setIndex(index) {
    state.index = (index + scene.steps.length) % scene.steps.length;
    state.elapsed = 0;
    state.lastTime = 0;
    draw(0);
  }

  document.querySelector('[data-action="play"]').addEventListener("click", function () {
    if (!state.playing) {
      state.playing = true;
      state.lastTime = 0;
      window.requestAnimationFrame(draw);
    }
  });
  document.querySelector('[data-action="pause"]').addEventListener("click", function () {
    state.playing = false;
    draw(0);
  });
  document.querySelector('[data-action="step"]').addEventListener("click", function () {
    state.playing = false;
    setIndex(state.index + 1);
  });
  document.querySelector('[data-action="reset"]').addEventListener("click", function () {
    state.playing = false;
    setIndex(0);
  });

  document.querySelectorAll("[data-goal]").forEach(function (button) {
    button.addEventListener("click", function () {
      state.goal = button.dataset.goal;
      document.querySelectorAll("[data-goal]").forEach((item) => item.classList.remove("active"));
      button.classList.add("active");
      if (scene.onGoalChange) {
        scene.onGoalChange(state.goal, state);
      }
      draw(0);
    });
  });

  window.addEventListener("resize", function () { draw(0); });
  draw(0);
  window.__EBPF_VISUAL_READY__ = true;
  document.documentElement.dataset.visualReady = "EBPF_VISUAL_SCENE_READY";
}());
