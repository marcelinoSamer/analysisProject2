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

  // Pending requests table
  const reqBody = $("#table-requests tbody");
  reqBody.innerHTML = "";
  STATE.requests.forEach((r) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${r.request_id}</td>
      <td>${r.fellow_id}</td>
      <td><code>${r.topic}</code></td>
      <td>${chipFor(r.urgency)}</td>
      <td>${r.available_slot_ids.join(", ") || "—"}</td>
      <td>${r.week}</td>
      <td><button class="icon" data-action="del-request" data-id="${r.request_id}">✕</button></td>`;
    reqBody.appendChild(tr);
  });
  if (!STATE.requests.length) {
    reqBody.innerHTML = `<tr><td colspan="7" class="empty">No requests queued for week ${STATE.current_week}.</td></tr>`;
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

  // Solver runs
  const runsBody = $("#table-runs tbody");
  runsBody.innerHTML = "";
  if (!STATE.solver_runs.length) {
    runsBody.innerHTML = `<tr><td colspan="6" class="empty">No solver runs yet.</td></tr>`;
  } else {
    STATE.solver_runs.slice().reverse().forEach((r) => {
      const tr = document.createElement("tr");
      const elapsed = r.elapsed_ms.toFixed(2);
      tr.innerHTML = `
        <td>${r.week}</td>
        <td><span class="chip chip-${r.solver}">${r.solver}</span></td>
        <td>${elapsed}</td>
        <td>${r.num_requests}</td>
        <td>${r.num_assigned}</td>
        <td>${r.penalty}</td>`;
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

function bindForms() {
  $("#form-request").addEventListener("submit", async (e) => {
    e.preventDefault();
    const body = readFormJSON(e.target);
    try {
      await api("/api/request", { method: "POST", body });
      e.target.reset();
      showToast("Request queued");
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
      const fallback = res.fallback_used ? "  (improved: backtracking timed out → greedy fallback)\n" : "";
      const lines = res.run.assignments.map((slot, idx) =>
        slot === -1
          ? `  Request ${idx}: not assigned`
          : `  Request ${idx}: -> slot ${slot}`
      );
      card.classList.add("ok");
      card.textContent =
        `Solver:  ${res.run.solver}\n` +
        `Week:    ${res.run.week}\n` +
        `Elapsed: ${res.run.elapsed_ms.toFixed(2)} ms\n` +
        `Penalty: ${res.run.penalty}\n` +
        `Assigned: ${res.run.num_assigned}/${res.run.num_requests}\n` +
        fallback +
        `\n` +
        lines.join("\n");
      showToast(`Solved in ${res.run.elapsed_ms.toFixed(1)} ms with "${res.run.solver}"`);
      await refresh();
    } catch (e) {
      card.classList.add("err");
      card.textContent = String(e.message || e);
    }
  });

  $("#btn-flush").addEventListener("click", async () => {
    if (!confirm("Flush all state? This cannot be undone.")) return;
    try {
      await api("/api/flush", { method: "POST" });
      showToast("Flushed");
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

(async function init() {
  bindTabs();
  bindSolverSwitch();
  bindForms();
  bindButtons();
  await refresh();
})();
