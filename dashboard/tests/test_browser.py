"""Optional real-broker/browser check. Requires paho, Playwright, Mosquitto and Chromium.
The simulated Pico here is a protocol fixture, not hardware validation.
"""
import argparse
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app import Dashboard, DashboardServer


def wait_until(condition, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return
        time.sleep(0.05)
    raise AssertionError("Timed out waiting for expected integration behavior")


def main():
    import paho.mqtt.client as mqtt
    from playwright.sync_api import sync_playwright, expect
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--screenshot", default="/tmp/buddy1-dashboard.png")
    args = parser.parse_args()
    broker_binary = os.environ.get("BUDDY1_MOSQUITTO") or shutil.which("mosquitto")
    assert broker_binary, "Install Mosquitto or set BUDDY1_MOSQUITTO to its executable"
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    log = open("/tmp/buddy1-dashboard-broker.log", "w")
    broker = subprocess.Popen([broker_binary, "-p", str(port)], stdout=log, stderr=log)
    server = fixture = dashboard = None
    try:
        def broker_ready():
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                    return True
            except OSError:
                return False
        wait_until(broker_ready)
        commands = []
        subscribed = threading.Event()
        root = "robot/se09/v2"
        fixture = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pico-protocol-fixture")
        def on_connect(client, userdata, flags, reason, properties):
            assert not reason.is_failure
            client.subscribe(root + "/command", 0)
        def on_subscribe(client, userdata, mid, reasons, properties):
            subscribed.set()
        def on_message(client, userdata, message):
            assert message.payload in (b"\x00", b"\x01")
            commands.append((bytes(message.payload), message.qos, bool(message.retain)))
            client.publish(root + "/ack", message.payload, qos=0, retain=False)
        fixture.on_connect, fixture.on_subscribe, fixture.on_message = on_connect, on_subscribe, on_message
        fixture.connect("127.0.0.1", port, 60)
        fixture.loop_start()
        assert subscribed.wait(5)
        fixture.publish(root + "/status", b"\x07", qos=0, retain=True).wait_for_publish(timeout=5)
        # Exercise the actual CLI entry point and its invalid/empty IP prompt.
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            http_port = sock.getsockname()[1]
        cli_log = open("/tmp/buddy1-dashboard-cli.log", "w")
        cli = subprocess.Popen([sys.executable, str(Path(__file__).resolve().parents[1] / "app.py"),
                                "--broker-port", str(port), "--port", str(http_port)],
                               stdin=subprocess.PIPE, stdout=cli_log, stderr=cli_log, text=True)
        try:
            cli.stdin.write("\n127.0.0.1\n")
            cli.stdin.flush()
            def cli_ready():
                try:
                    connection = http.client.HTTPConnection("127.0.0.1", http_port, timeout=0.3)
                    connection.request("GET", "/api/state")
                    response = connection.getresponse()
                    result = response.status == 200 and json.loads(response.read())["broker"] == "127.0.0.1"
                    connection.close()
                    return result
                except OSError:
                    return False
            wait_until(cli_ready)
            assert commands == []
            print("PASS CLI: empty address reprompted; valid IP starts local HTTP dashboard; no startup command")
        finally:
            cli.terminate()
            cli.wait(timeout=5)
            cli.stdin.close()
            cli_log.close()
        clock_offset = [0.0]
        dashboard = Dashboard("127.0.0.1", port, root, clock=lambda: time.monotonic() + clock_offset[0])
        server = DashboardServer(("127.0.0.1", 0), dashboard)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        dashboard.start(mqtt)
        wait_until(lambda: dashboard.snapshot()["monitor_ready"] and dashboard.last_status == 7)
        with sync_playwright() as playwright:
            executable = os.environ.get("BUDDY1_BROWSER") or shutil.which("chromium")
            browser = playwright.chromium.launch(executable_path=executable, headless=True,
                                                 args=["--no-sandbox", "--disable-dev-shm-usage"])
            page = browser.new_page(viewport={"width": 1280, "height": 1100})
            errors = []
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.goto(f"http://127.0.0.1:{server.server_port}")
            expect(page.locator("#status-hex")).to_have_text("07")
            assert page.locator("#start").is_disabled()  # Retained status is not live evidence.
            assert page.locator("#stop").is_enabled()
            assert page.locator(".page-header, .bottom-grid, .ack-card, .event-card, .page-footer").count() == 0
            assert page.locator(".connection-strip, .control-card, .heartbeat-card").count() == 3
            print("PASS browser: only broker bar, command controls and heartbeat panels are shown")
            print("PASS browser: retained status does not enable START; connected STOP remains available")
            fixture.publish(root + "/status", b"\x07", qos=0, retain=True).wait_for_publish(timeout=5)
            page.locator("#start").wait_for(state="visible")
            expect(page.locator("#start")).to_be_enabled()
            page.locator("#start").click()
            expect(page.locator("#command-feedback")).to_have_text("Pico ACK: START accepted.")
            page.locator("#stop").click()
            expect(page.locator("#command-feedback")).to_have_text("Pico ACK: STOP accepted.")
            assert commands == [(b"\x01", 0, False), (b"\x00", 0, False)], commands
            print("PASS end-to-end: button -> HTTP -> real Mosquitto -> binary 01/00, QoS 0, non-retained -> fixture ACK -> browser")
            page.locator("#start").click()
            page.locator("#stop").click()
            wait_until(lambda: len(commands) == 4)
            assert commands[-2:] == [(b"\x01", 0, False), (b"\x00", 0, False)]
            expect(page.locator("#command-feedback")).to_have_text("Pico ACK: STOP accepted.")
            print("PASS browser: rapid START then STOP preserves command order")
            # Fresh signal for a representative desktop screenshot.
            fixture.publish(root + "/status", b"\x07", qos=0, retain=True).wait_for_publish(timeout=5)
            page.wait_for_timeout(1100)
            page.screenshot(path=args.screenshot, full_page=True)
            page.set_viewport_size({"width": 390, "height": 844})
            page.wait_for_timeout(200)
            assert page.locator("html").evaluate("el => el.scrollWidth") <= 390, "Mobile horizontal overflow"
            print("PASS browser: mobile layout has no horizontal overflow")
            clock_offset[0] = 20.0
            expect(page.locator("#pico-state")).to_have_text("Heartbeat is stale")
            assert page.locator("#start").is_disabled() and page.locator("#stop").is_enabled()
            print("PASS browser: stale heartbeat disables START and keeps STOP available")
            broker.terminate()
            broker.wait(timeout=5)
            expect(page.locator("#pico-state")).to_have_text("Broker unavailable")
            assert page.locator("#start").is_disabled() and page.locator("#stop").is_disabled()
            assert len(commands) == 4  # No automatic command/replay.
            assert not errors, errors
            print("PASS browser: broker loss disables controls; no auto-command; no JavaScript errors")
            browser.close()
    finally:
        if dashboard:
            dashboard.close()
        if fixture:
            fixture.disconnect()
            fixture.loop_stop()
        if server:
            server.shutdown()
            server.server_close()
        if broker.poll() is None:
            broker.terminate()
            broker.wait(timeout=5)
        log.close()


if __name__ == "__main__":
    main()
