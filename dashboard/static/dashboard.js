// Polls the Flask API and redraws the charts. Live view refreshes every 5 s.
const REFRESH_MS = 5000;
const threshold = Number(document.body.dataset.threshold);
const target = Number(document.body.dataset.target);
const windowSelect = document.getElementById("window");

const toDate = (ts) => new Date(ts * 1000);
const fmtTime = (ts) => toDate(ts).toLocaleString();

const moistureChart = new Chart(document.getElementById("moistureChart"), {
  type: "line",
  data: {
    datasets: [
      { label: "Moisture %", data: [], borderColor: "#2e7d32", pointRadius: 0, borderWidth: 2, tension: 0.2 },
      { label: "Pump on", data: [], borderColor: "#1565c0", backgroundColor: "rgba(21,101,192,.15)",
        pointRadius: 0, borderWidth: 0, fill: true, stepped: true },
      { label: `Threshold (${threshold}%)`, data: [], borderColor: "#c62828", borderDash: [6, 4], pointRadius: 0, borderWidth: 1 },
      { label: `Target (${target}%)`, data: [], borderColor: "#f9a825", borderDash: [6, 4], pointRadius: 0, borderWidth: 1 },
    ],
  },
  options: {
    animation: false,
    maintainAspectRatio: false,
    interaction: { mode: "index", intersect: false },
    scales: {
      x: { type: "time" },
      y: { min: 0, max: 100, title: { display: true, text: "%" } },
    },
  },
});

const eventsChart = new Chart(document.getElementById("eventsChart"), {
  type: "bar",
  data: { datasets: [{ label: "Pump run (s)", data: [], backgroundColor: "#1565c0" }] },
  options: {
    animation: false,
    maintainAspectRatio: false,
    scales: {
      x: { type: "time" },
      y: { beginAtZero: true, title: { display: true, text: "seconds" } },
    },
  },
});

async function getJSON(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url}: ${res.status}`);
  return res.json();
}

async function refresh() {
  const hours = windowSelect.value;
  try {
    const [readings, events, status] = await Promise.all([
      getJSON(`/api/readings?hours=${hours}`),
      getJSON(`/api/events?hours=${hours}`),
      getJSON("/api/status"),
    ]);

    const points = readings.map((r) => ({ x: toDate(r.ts), y: r.moisture_pct }));
    moistureChart.data.datasets[0].data = points;
    moistureChart.data.datasets[1].data = readings.map((r) => ({ x: toDate(r.ts), y: r.pump_on ? 100 : 0 }));
    const ends = points.length ? [points[0].x, points[points.length - 1].x] : [];
    moistureChart.data.datasets[2].data = ends.map((x) => ({ x, y: threshold }));
    moistureChart.data.datasets[3].data = ends.map((x) => ({ x, y: target }));
    moistureChart.update();

    eventsChart.data.datasets[0].data = events.map((e) => ({ x: toDate(e.started_at), y: e.duration_s ?? 0 }));
    eventsChart.update();

    document.getElementById("eventsTable").innerHTML = events
      .slice()
      .reverse()
      .map((e) => `<tr><td>${fmtTime(e.started_at)}</td>
        <td>${e.duration_s ?? "running"} s</td>
        <td>${e.start_moisture.toFixed(1)}% &rarr; ${e.end_moisture != null ? e.end_moisture.toFixed(1) + "%" : "--"}</td>
        <td>${e.stop_reason ?? "--"}</td></tr>`)
      .join("");

    if (status.latest) {
      document.getElementById("moisture").textContent = `${status.latest.moisture_pct.toFixed(1)}%`;
      const pump = document.getElementById("pump");
      pump.textContent = status.latest.pump_on ? "ON" : "off";
      pump.className = "value" + (status.latest.pump_on ? " on" : "");
    }
    document.getElementById("last").textContent = status.last_watering ? fmtTime(status.last_watering.started_at) : "never";
    document.getElementById("runs").textContent = status.total_runs;
  } catch (err) {
    console.error(err);
  }
}

windowSelect.addEventListener("change", refresh);
refresh();
setInterval(refresh, REFRESH_MS);
