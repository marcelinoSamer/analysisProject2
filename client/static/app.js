// Tiny vanilla JS controller for the scheduler client.

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

let STATE = null;

function showToast(message, kind = "ok") {
  const el = document.createElement("div");
  el.className = `toast ${kind}`;
  el.textContent = message;
  document.body.appendChild(el);
  requestAnimationFrame(() => el.classList.add("show"));
  setTimeout(() => {
    el.classList.remove("show");
    setTimeout(() => el.remove(), 250);
  }, 2400);
}

async function api(path, options = {}) {
  const opts = { method: "GET", headers: {}, ...options };
  if (opts.body && typeof opts.body !== "string" && !(opts.body instanceof FormData)) {
    opts.headers["Content-Type"] = "application/json";
    opts.body = JSON.stringify(opts.body);
  }
  const res = await fetch(path, opts);
  let payload;
  try { payload = await res.json(); }
  catch { payload = { ok: false, error: `Bad response (${res.status})` }; }
  if (!payload.ok) {
    const msg = payload.error || `HTTP ${res.status}`;
    showToast(msg, "err");
    throw new Error(msg);
  }
  return payload;
}

// ----------------------------------------------------------------- rendering
function chipFor(urgency) {
  return `<span class="chip chip-${urgency}">${urgency}</span>`;
}

function render() {
  if (!STATE) return;

  $("#meta-week").textContent = STATE.current_week;
  $("#meta-fellows").textContent = STATE.fellows.length;
  $("#meta-mentors").textContent = STATE.mentors.length;
  $("#meta-slots").textContent = STATE.slots.length;
  $("#meta-pending").textContent = STATE.requests.length;

  $$("#solver-switch .switch-btn").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.solver === STATE.active_solver);
  });

  // Populate the topic autocomplete datalist
  const dataList = $("#known-topics");
  if (dataList) {
    dataList.innerHTML = "";
    (STATE.known_topics || []).forEach((t) => {
      const opt = document.createElement("option");
      opt.value = t;
      dataList.appendChild(opt);
    });
  }

  // Snapshot status + Reset button availability
  const snap = STATE.saved_snapshot || { available: false };
  const status = $("#snapshot-status");
  const resetBtn = $("#btn-reset");
  if (status) {
    if (snap.available) {
      status.classList.add("has-snapshot");
      status.textContent = snap.label
        ? `Snapshot loaded: ${snap.label}`
        : "Snapshot loaded (pasted CSV).";
    } else {
      status.classList.remove("has-snapshot");
      status.textContent = "No snapshot loaded yet.";
    }
  }
  if (resetBtn) resetBtn.disabled = !snap.available;

  // Pending requests table
  const reqBody = $("#table-requests tbody");
  reqBody.innerHTML = "";
  STATE.requests.forEach((r) => {
    const tr = document.createElement("tr");
    const warnings = r.warnings || [];
    const statusCell = warnings.length
      ? `<span class="chip chip-warn" title="${warnings.join('\n').replace(/"/g, '&quot;')}">infeasible</span>`
      : `<span class="chip chip-ok">ready</span>`;
    tr.innerHTML = `
      <td>${r.request_id}</td>
      <td>${r.fellow_id}</td>
      <td><code>${r.topic}</code></td>
      <td>${chipFor(r.urgency)}</td>
      <td>${r.available_slot_ids.join(", ") || "—"}</td>
      <td>${r.week}</td>
      <td>${statusCell}</td>
      <td><button class="icon" data-action="del-request" data-id="${r.request_id}">✕</button></td>`;
    reqBody.appendChild(tr);
  });
  if (!STATE.requests.length) {
    reqBody.innerHTML = `<tr><td colspan="8" class="empty">No requests queued for week ${STATE.current_week}.</td></tr>`;
  }

  // Mentors table
  const mentorBody = $("#table-mentors tbody");
  mentorBody.innerHTML = "";
  STATE.mentors.forEach((m) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${m.id}</td>
      <td><code>${m.specialties.join(", ")}</code></td>
      <td><button class="icon" data-action="del-mentor" data-id="${m.id}">✕</button></td>`;
    mentorBody.appendChild(tr);
  });
  if (!STATE.mentors.length) {
    mentorBody.innerHTML = `<tr><td colspan="3" class="empty">No mentors yet.</td></tr>`;
  }

  // Slots table
  const slotBody = $("#table-slots tbody");
  slotBody.innerHTML = "";
  STATE.slots.forEach((s) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${s.slot_id}</td>
      <td>${s.mentor_id}</td>
      <td>${s.week}</td>
      <td><button class="icon" data-action="del-slot" data-id="${s.slot_id}">✕</button></td>`;
    slotBody.appendChild(tr);
  });
  if (!STATE.slots.length) {
    slotBody.innerHTML = `<tr><td colspan="4" class="empty">No slots defined.</td></tr>`;
  }

  // Schedule grid (per mentor, grouped by week)
  const grid = $("#schedule-grid");
  const weeks = Object.keys(STATE.schedule).map((w) => +w).sort((a, b) => a - b);
  if (!weeks.length) {
    grid.innerHTML = `<p class="empty">No weekly schedule yet — submit some requests and run the solver.</p>`;
  } else {
    grid.innerHTML = "";
    weeks.forEach((w) => {
      const rows = STATE.schedule[String(w)] || [];
      const byMentor = new Map();
      rows.forEach((row) => {
        if (!byMentor.has(row.mentor_id)) byMentor.set(row.mentor_id, []);
        byMentor.get(row.mentor_id).push(row);
      });

      let mentorRows = "";
      for (const [mid, items] of Array.from(byMentor.entries()).sort((a, b) => a[0] - b[0])) {
        items.forEach((row) => {
          const fellow = row.fellow_id === null
            ? `<td colspan="3" class="empty-slot">unassigned</td>`
            : `<td>${row.fellow_id}</td><td><code>${row.topic}</code></td><td>${chipFor(row.urgency)}</td>`;
          mentorRows += `<tr><td>${mid}</td><td>slot ${row.slot_id}</td>${fellow}</tr>`;
        });
      }

      const block = document.createElement("div");
      block.className = "schedule-week";
      block.innerHTML = `
        <h4>Week ${w}</h4>
        <table>
          <thead><tr><th>Mentor</th><th>Slot</th><th>Fellow</th><th>Topic</th><th>Urgency</th></tr></thead>
          <tbody>${mentorRows || `<tr><td colspan="5" class="empty">No slots ran this week.</td></tr>`}</tbody>
        </table>`;
      grid.appendChild(block);
    });
  }

  // Starvation table
  const starveBody = $("#table-starvation tbody");
  starveBody.innerHTML = "";
  const entries = Object.entries(STATE.starvation).sort((a, b) => +b[1] - +a[1]);
  if (!entries.length) {
    starveBody.innerHTML = `<tr><td colspan="2" class="empty">No fellows registered yet.</td></tr>`;
  } else {
    entries.forEach(([fid, sf]) => {
      const tr = document.createElement("tr");
      const cls = +sf >= 3 ? "chip chip-blocker" : +sf >= 1 ? "chip chip-warn" : "chip chip-ok";
      tr.innerHTML = `<td>${fid}</td><td><span class="${cls}">${sf}</span></td>`;
      starveBody.appendChild(tr);
    });
  }

  // Benchmark runs
  const benchmarkRuns = STATE.benchmark_runs || [];
  const latestBenchmark = benchmarkRuns.length ? benchmarkRuns[benchmarkRuns.length - 1] : null;
  const summary = $("#benchmark-summary");
  const chart = $("#benchmark-chart");
  if (summary) {
    if (!latestBenchmark) {
      summary.innerHTML = `<p class="empty">No benchmark data yet.</p>`;
    } else {
      const okResults = latestBenchmark.results.filter((r) => !r.error);
      const fastest = okResults.slice().sort((a, b) => a.elapsed_ms - b.elapsed_ms)[0];
      const mostAssigned = okResults.slice().sort((a, b) => b.num_assigned - a.num_assigned || a.elapsed_ms - b.elapsed_ms)[0];
      const bestPenalty = okResults.slice().sort((a, b) => a.penalty - b.penalty || a.elapsed_ms - b.elapsed_ms)[0];
      summary.innerHTML = `
        <div class="stat-card"><div class="label">Latest week</div><div class="value">${latestBenchmark.week}</div></div>
        <div class="stat-card"><div class="label">Committed schedule</div><div class="value">${latestBenchmark.committed_solver || "none"}</div></div>
        <div class="stat-card"><div class="label">Fastest</div><div class="value">${fastest ? fastest.solver : "—"}</div></div>
        <div class="stat-card"><div class="label">Most assigned</div><div class="value">${mostAssigned ? `${mostAssigned.solver} (${mostAssigned.num_assigned})` : "—"}</div></div>
        <div class="stat-card"><div class="label">Lowest penalty</div><div class="value">${bestPenalty ? `${bestPenalty.solver} (${bestPenalty.penalty})` : "—"}</div></div>`;
    }
  }
  if (chart) {
    if (!latestBenchmark) {
      chart.innerHTML = "";
    } else {
      const maxElapsed = Math.max(1, ...latestBenchmark.results.map((r) => r.error ? 0 : r.elapsed_ms));
      chart.innerHTML = latestBenchmark.results.map((r) => {
        const width = r.error ? 2 : Math.max(2, (r.elapsed_ms / maxElapsed) * 100);
        const value = r.error ? "timeout/error" : `${r.elapsed_ms.toFixed(2)} ms`;
        return `
          <div class="bar-row">
            <span class="chip chip-${r.solver}">${r.solver}</span>
            <div class="bar-track"><div class="bar-fill ${r.solver}" style="width:${width}%"></div></div>
            <span>${value}</span>
          </div>`;
      }).join("");
    }
  }

  const runsBody = $("#table-runs tbody");
  runsBody.innerHTML = "";
  const rows = benchmarkRuns.slice().reverse().flatMap((batch) =>
    batch.results.map((r) => ({ ...r, week: batch.week, committed_solver: batch.committed_solver }))
  );
  if (!rows.length) {
    runsBody.innerHTML = `<tr><td colspan="7" class="empty">No benchmark runs yet.</td></tr>`;
  } else {
    rows.forEach((r) => {
      const tr = document.createElement("tr");
      const elapsed = r.error ? "—" : r.elapsed_ms.toFixed(2);
      const status = r.error
        ? `<span class="chip chip-warn" title="${String(r.error).replace(/"/g, '&quot;')}">failed</span>`
        : `${r.fallback_used ? `<span class="chip chip-warn">fallback</span> ` : ""}${r.solver === r.committed_solver ? `<span class="chip chip-ok">committed</span>` : `<span class="chip">benchmarked</span>`}`;
      tr.innerHTML = `
        <td>${r.week}</td>
        <td><span class="chip chip-${r.solver}">${r.solver}</span></td>
        <td>${elapsed}</td>
        <td>${r.num_requests}</td>
        <td>${r.num_assigned}</td>
        <td>${r.error ? "—" : r.penalty}</td>
        <td>${status}</td>`;
      runsBody.appendChild(tr);
    });
  }
}

async function refresh() {
  const payload = await api("/api/state");
  STATE = payload.state;
  render();
}

// ----------------------------------------------------------------- handlers
function bindTabs() {
  $$(".tab").forEach((btn) => {
    btn.addEventListener("click", () => {
      $$(".tab").forEach((b) => b.classList.toggle("active", b === btn));
      $$(".panel").forEach((p) => p.classList.add("hidden"));
      $(`#tab-${btn.dataset.tab}`).classList.remove("hidden");
    });
  });
}

function bindSolverSwitch() {
  $$("#solver-switch .switch-btn").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const name = btn.dataset.solver;
      try {
        await api("/api/solver", { method: "POST", body: { solver: name } });
        showToast(`Solver switched to "${name}"`);
        await refresh();
      } catch (e) { /* toast already shown */ }
    });
  });
}

function readFormJSON(form) {
  const fd = new FormData(form);
  const out = {};
  fd.forEach((v, k) => { if (v !== "") out[k] = v; });
  return out;
}

let _validateTimer = null;
function scheduleValidation() {
  clearTimeout(_validateTimer);
  _validateTimer = setTimeout(runValidation, 200);
}

async function runValidation() {
  const form = $("#form-request");
  if (!form) return;
  const body = readFormJSON(form);
  const box = $("#request-validation");
  if (!body.fellow_id && !body.topic && !body.available_slot_ids) {
    box.classList.add("hidden");
    return;
  }
  let res;
  try {
    res = await fetch("/api/validate_request", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    }).then((r) => r.json());
  } catch (_) {
    box.classList.add("hidden");
    return;
  }
  box.classList.remove("hidden", "ok", "warn", "err");
  const errs = res.errors || [];
  const warns = res.warnings || [];
  if (errs.length) {
    box.classList.add("err");
    box.innerHTML = `<strong>Won't submit:</strong><ul>${errs.map((m) => `<li>${m}</li>`).join("")}</ul>`;
  } else if (warns.length) {
    box.classList.add("warn");
    box.innerHTML = `<strong>Will queue, but heads up:</strong><ul>${warns.map((m) => `<li>${m}</li>`).join("")}</ul>`;
  } else if (body.topic && body.available_slot_ids) {
    box.classList.add("ok");
    box.textContent = "Looks feasible — at least one of the listed slots can satisfy this request.";
  } else {
    box.classList.add("hidden");
  }
}

function bindForms() {
  const reqForm = $("#form-request");
  reqForm.addEventListener("input", scheduleValidation);
  reqForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const body = readFormJSON(e.target);
    try {
      const res = await api("/api/request", { method: "POST", body });
      e.target.reset();
      $("#request-validation").classList.add("hidden");
      if ((res.warnings || []).length) {
        showToast(`Queued with warnings: ${res.warnings[0]}`, "warn" /* not a real toast class, falls back to ok */);
      } else {
        showToast("Request queued");
      }
      await refresh();
    } catch (_) {}
  });

  $("#form-mentor").addEventListener("submit", async (e) => {
    e.preventDefault();
    const body = readFormJSON(e.target);
    try {
      await api("/api/mentor", { method: "POST", body });
      e.target.reset();
      showToast("Mentor saved");
      await refresh();
    } catch (_) {}
  });

  $("#form-slot").addEventListener("submit", async (e) => {
    e.preventDefault();
    const body = readFormJSON(e.target);
    try {
      await api("/api/slot", { method: "POST", body });
      e.target.reset();
      showToast("Slot added");
      await refresh();
    } catch (_) {}
  });

  $("#form-import").addEventListener("submit", async (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    const file = fd.get("file");
    let res;
    try {
      if (file && file.size > 0) {
        const upload = new FormData();
        upload.append("file", file);
        res = await api("/api/import_csv", { method: "POST", body: upload });
      } else {
        const csv = fd.get("csv") || "";
        if (!String(csv).trim()) {
          showToast("Provide a CSV file or paste CSV text", "err");
          return;
        }
        res = await api("/api/import_csv", { method: "POST", body: { csv } });
      }
    } catch (_) { return; }
    const card = $("#import-result");
    card.classList.remove("hidden");
    card.classList.add("ok");
    card.classList.remove("err");
    card.textContent =
      `Loaded snapshot:\n` +
      `  fellows:  ${res.summary.fellows}\n` +
      `  mentors:  ${res.summary.mentors}\n` +
      `  slots:    ${res.summary.slots}\n` +
      `  pending:  ${res.summary.requests}\n` +
      `  week:     ${res.summary.current_week}\n` +
      `  solver:   ${res.summary.solver}`;
    showToast("Snapshot loaded");
    await refresh();
  });
}

function bindButtons() {
  $("#btn-solve").addEventListener("click", async () => {
    const card = $("#solve-result");
    card.classList.remove("hidden", "err", "ok");
    card.textContent = "Running solver…";
    try {
      const res = await api("/api/solve", { method: "POST", body: {} });
      const comparisons = res.comparison_results || [];
      const comparisonLines = comparisons.map((r) => {
        if (r.error) return `  ${r.solver.padEnd(8)} failed: ${r.error}`;
        const fb = r.fallback_used ? " (fallback)" : "";
        return `  ${r.solver.padEnd(8)} ${r.elapsed_ms.toFixed(2).padStart(8)} ms | assigned ${r.num_assigned}/${r.num_requests} | penalty ${r.penalty}${fb}`;
      });
      const commitLine = res.run
        ? `Committed schedule: ${res.run.solver} (week ${res.run.week})`
        : `No schedule committed: ${res.commit_error || "selected solver failed"}`;
      const assignmentLines = res.run
        ? res.run.assignments.map((slot, idx) =>
            slot === -1
              ? `  Request ${idx}: not assigned`
              : `  Request ${idx}: -> slot ${slot}`
          )
        : [];
      card.classList.add("ok");
      card.textContent =
        `Benchmarked all algorithms:\n` +
        comparisonLines.join("\n") +
        `\n` +
        `\n${commitLine}\n` +
        assignmentLines.join("\n");
      showToast(res.run ? `Committed "${res.run.solver}" after benchmarking all algorithms` : "Benchmarked all algorithms; no schedule committed", res.run ? "ok" : "warn");
      await refresh();
    } catch (e) {
      card.classList.add("err");
      card.textContent = String(e.message || e);
    }
  });

  $("#btn-flush").addEventListener("click", async () => {
    if (!confirm("Flush the current week state? The saved snapshot will be kept so you can still reset to it.")) return;
    try {
      await api("/api/flush", { method: "POST" });
      showToast("Flushed");
      await refresh();
    } catch (_) {}
  });

  $("#btn-reset").addEventListener("click", async () => {
    if (!STATE || !STATE.saved_snapshot || !STATE.saved_snapshot.available) {
      showToast("No snapshot to reset to", "warn");
      return;
    }
    if (!confirm("Reset to the imported snapshot? Any solves and edits made since the import will be discarded.")) return;
    try {
      const res = await api("/api/reset", { method: "POST" });
      showToast(`Reset to ${res.summary.snapshot_label || "snapshot"}`);
      await refresh();
    } catch (_) {}
  });

  $("#btn-add-fellow").addEventListener("click", async () => {
    const id = prompt("Fellow ID to register:");
    if (id === null || id === "") return;
    try {
      await api("/api/fellow", { method: "POST", body: { fellow_id: id } });
      showToast(`Fellow ${id} registered`);
      await refresh();
    } catch (_) {}
  });

  document.addEventListener("click", async (e) => {
    const btn = e.target.closest("[data-action]");
    if (!btn) return;
    const action = btn.dataset.action;
    const id = btn.dataset.id;
    try {
      if (action === "del-request") {
        await api(`/api/request/${id}`, { method: "DELETE" });
      } else if (action === "del-mentor") {
        if (!confirm(`Remove mentor ${id} and all their slots?`)) return;
        await api(`/api/mentor/${id}`, { method: "DELETE" });
      } else if (action === "del-slot") {
        if (!confirm(`Remove slot ${id}?`)) return;
        await api(`/api/slot/${id}`, { method: "DELETE" });
      } else {
        return;
      }
      await refresh();
    } catch (_) {}
  });
}

async function loadTestCases() {
  const container = $("#test-case-list");
  if (!container) return;
  let cases = [];
  try {
    const res = await api("/api/test_cases");
    cases = res.cases || [];
  } catch (_) { return; }

  if (!cases.length) {
    container.innerHTML = `<p class="empty">No built-in test cases discovered. Drop a CSV into <code>client/tests/</code> and reload.</p>`;
    return;
  }
  container.innerHTML = "";
  cases.forEach((c) => {
    const btn = document.createElement("button");
    btn.className = "test-case";
    btn.innerHTML = `
      <div class="tc-title">${c.title} <code style="opacity:0.6">${c.name}</code></div>
      <div class="tc-desc">${c.description || "—"}</div>`;
    btn.addEventListener("click", async () => {
      try {
        const res = await api("/api/load_test_case", { method: "POST", body: { name: c.name } });
        showToast(`Loaded test case "${c.title}"`);
        // Surface a small summary in the import-result card too.
        const card = $("#import-result");
        if (card) {
          card.classList.remove("hidden", "err");
          card.classList.add("ok");
          card.textContent =
            `Loaded ${c.name}:\n` +
            `  fellows:  ${res.summary.fellows}\n` +
            `  mentors:  ${res.summary.mentors}\n` +
            `  slots:    ${res.summary.slots}\n` +
            `  pending:  ${res.summary.requests}\n` +
            `  week:     ${res.summary.current_week}\n` +
            `  solver:   ${res.summary.solver}`;
        }
        await refresh();
      } catch (_) {}
    });
    container.appendChild(btn);
  });
}

(async function init() {
  bindTabs();
  bindSolverSwitch();
  bindForms();
  bindButtons();
  await refresh();
  await loadTestCases();
})();
