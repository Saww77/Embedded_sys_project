"use strict";
const el = (id) => document.getElementById(id);
let state = null;
let commandBusy = false;
let commandQueue = Promise.resolve();
let polling = false;
let lastAckTime = "";
let commandPosting = false;
function setFlag(id, value, fresh) {
  const node = el(id);
  node.className = "flag" + (fresh ? value ? " good" : " bad" : "");
  node.textContent = fresh ? value ? "Connected" : "Offline" : "Unconfirmed";
  if (id === "subscribed" && fresh && value) node.textContent = "Subscribed";
}
function draw(data) {
  state = data;
  el("broker-address").textContent = `${data.broker}:${data.broker_port}`;
  el("topic-root").textContent = data.root;
  el("live-monitor").classList.toggle("online", data.broker_connected);
  el("start").disabled = !data.start_allowed || commandBusy;
  el("stop").disabled = !data.stop_allowed;
  el("ready-dot").classList.toggle("online", data.start_allowed);
  let title = "Waiting for the Pico";
  let description = "Waiting for a live heartbeat. Stored broker messages do not confirm the Pico is online.";
  if (!data.broker_connected) {
    title = "Broker unavailable";
    description = "Check the broker address, listener and firewall. The dashboard retries automatically.";
  } else if (!data.monitor_ready) {
    description = "Waiting for the dashboard’s MQTT subscriptions.";
  } else if (data.start_allowed) {
    title = "Pico ready for commands";
    description = "The live heartbeat confirms Wi-Fi, MQTT and the command subscription.";
  } else if (data.pico.age !== null && !data.pico.fresh) {
    title = "Heartbeat is stale";
    description = "No live signal for over 15 seconds. START is paused; STOP remains available.";
  } else if (data.pico.fresh) {
    title = "Pico connection not ready";
    description = "Wait for all three connection flags before sending START.";
  }
  el("pico-state").textContent = title;
  el("pico-description").textContent = description;
  el("notice").hidden = !data.error;
  el("notice").textContent = data.error;
  el("heartbeat-age").textContent = data.pico.age === null ? "—" : `${Math.floor(data.pico.age)}s ago`;
  el("heartbeat-age").closest(".heartbeat-card").classList.toggle("live", data.pico.fresh);
  for (const id of ["wifi", "mqtt", "subscribed"]) setFlag(id, data.pico[id], data.pico.fresh && data.broker_connected);
  el("status-hex").textContent = data.pico.hex ?? "—";
  el("drop-status").textContent = data.pico.hex === null ? "Waiting for data" : data.pico.tx_drop || data.pico.rx_drop ? "DROP FLAG SET" : "NO DROP FLAGS";
  if (data.last_ack && data.last_ack.time !== lastAckTime && !commandPosting) {
    const ack = data.last_ack;
    lastAckTime = ack.time;
    el("command-feedback").className = "command-feedback" + (ack.rejected ? " error" : "");
    el("command-feedback").textContent = `Pico ACK: ${ack.command.toUpperCase()} ${ack.rejected ? "rejected" : "accepted"}.`;
  }
}
async function refresh() {
  if (polling) return;
  polling = true;
  try {
    const result = await fetch("/api/state", { cache: "no-store", signal: AbortSignal.timeout(4000) });
    if (!result.ok) throw new Error("Dashboard connection failed");
    draw(await result.json());
  } catch (error) {
    state = null;
    el("start").disabled = el("stop").disabled = true;
    el("live-monitor").classList.remove("online");
    el("ready-dot").classList.remove("online");
    for (const id of ["wifi", "mqtt", "subscribed"]) setFlag(id, false, false);
    el("heartbeat-age").textContent = "—";
    el("heartbeat-age").closest(".heartbeat-card").classList.remove("live");
    el("pico-state").textContent = "Dashboard server unavailable";
    el("pico-description").textContent = "Keep app.py running, then refresh this page.";
    el("notice").hidden = false;
    el("notice").textContent = "Could not reach the local dashboard server.";
  } finally {
    polling = false;
  }
}
function send(command) {
  if (!state || (command === "start" && commandBusy)) return;
  if (command === "start") { commandBusy = true; el("start").disabled = true; }
  // Preserve operator click order, including STOP clicked while START is posting.
  commandQueue = commandQueue.then(() => performSend(command));
}
async function performSend(command) {
  const token = state?.csrf ?? "";
  commandPosting = true;
  el("command-feedback").className = "command-feedback";
  el("command-feedback").textContent = `Sending ${command.toUpperCase()}…`;
  try {
    const response = await fetch("/api/command", {
      method: "POST", headers: { "Content-Type": "application/json", "X-Dashboard-Token": token },
      body: JSON.stringify({ command }), signal: AbortSignal.timeout(5000)
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || "Command was not accepted");
    el("command-feedback").textContent = `${command.toUpperCase()} queued for the broker. Waiting for a Pico acknowledgment.`;
  } catch (error) {
    el("command-feedback").className = "command-feedback error";
    el("command-feedback").textContent = error.name === "TimeoutError" ? "Delivery is uncertain. Check the ACK and USB serial before retrying." : error.message;
  } finally {
    commandPosting = false;
    if (command === "start") commandBusy = false;
    await refresh();
  }
}
el("start").addEventListener("click", () => send("start"));
el("stop").addEventListener("click", () => send("stop"));
refresh();
setInterval(refresh, 1000);
