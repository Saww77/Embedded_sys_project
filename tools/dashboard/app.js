"use strict";
const el = (id) => document.getElementById(id);
let state = null;
let commandBusy = false;
let commandQueue = Promise.resolve();
let polling = false;
let lastEvents = "";
function timeLabel(value) {
  return value ? new Date(value).toLocaleTimeString([], { hour12: false }) : "—";
}
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
  el("broker-pill").classList.toggle("online", data.broker_connected);
  el("broker-label").textContent = data.broker_connected ? "Broker connected" : "Broker disconnected";
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
  if (data.last_ack) {
    const ack = data.last_ack;
    el("ack-result").textContent = `${ack.command.toUpperCase()} ${ack.rejected ? "rejected" : "accepted"}`;
    el("ack-result").className = "ack-result " + (ack.rejected ? "rejected" : "accepted");
    el("ack-detail").textContent = ack.rejected ? "The Pico declined this command request." : "Request submitted to Buddy 2. Confirm actual movement on the robot.";
    el("ack-time").textContent = timeLabel(ack.time);
    el("ack-hex").textContent = `0x${ack.hex}`;
  } else {
    el("ack-result").textContent = "No acknowledgment yet";
    el("ack-result").className = "ack-result";
    el("ack-detail").textContent = "Send START or STOP to test the command path.";
    el("ack-time").textContent = el("ack-hex").textContent = "—";
  }
  const eventKey = data.csrf + ":" + data.events.map((event) => event.id).join(",");
  if (eventKey !== lastEvents) {
    lastEvents = eventKey;
    el("events").replaceChildren();
    for (const event of data.events) {
      const row = document.createElement("tr");
      row.className = `event-${event.kind}`;
      const time = document.createElement("td");
      const message = document.createElement("td");
      time.textContent = timeLabel(event.time);
      message.textContent = event.message;
      row.append(time, message);
      el("events").append(row);
    }
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
    el("broker-pill").classList.remove("online");
    el("broker-label").textContent = "Dashboard unavailable";
    el("ready-dot").classList.remove("online");
    for (const id of ["wifi", "mqtt", "subscribed"]) setFlag(id, false, false);
    el("heartbeat-age").textContent = "—";
    el("heartbeat-age").closest(".heartbeat-card").classList.remove("live");
    el("pico-state").textContent = "Dashboard server unavailable";
    el("pico-description").textContent = "Keep the dashboard command running in Ubuntu, then refresh this page.";
    el("notice").hidden = false;
    el("notice").textContent = "Could not reach the local dashboard. Displayed ACKs are previous observations.";
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
  el("command-feedback").className = "command-feedback";
  el("command-feedback").textContent = `Sending ${command.toUpperCase()}…`;
  try {
    const response = await fetch("/api/command", {
      method: "POST", headers: { "Content-Type": "application/json", "X-Dashboard-Token": token },
      body: JSON.stringify({ command }), signal: AbortSignal.timeout(5000)
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || "Command was not accepted");
    el("command-feedback").textContent = `${command.toUpperCase()} queued for the broker. Watch the Pico ACK below.`;
  } catch (error) {
    el("command-feedback").className = "command-feedback error";
    el("command-feedback").textContent = error.name === "TimeoutError" ? "Delivery is uncertain. Check the ACK and USB serial before retrying." : error.message;
  } finally {
    if (command === "start") commandBusy = false;
    await refresh();
  }
}
el("start").addEventListener("click", () => send("start"));
el("stop").addEventListener("click", () => send("stop"));
refresh();
setInterval(refresh, 1000);
