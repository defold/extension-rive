#!/usr/bin/env node

import fs from "node:fs/promises";
import fsSync from "node:fs";
import http from "node:http";
import net from "node:net";
import path from "node:path";
import { spawn, spawnSync } from "node:child_process";

class RenderComparisonError extends Error {
    constructor(message) {
        super(message);
        this.name = "RenderComparisonError";
    }
}

function usage() {
    console.error(`usage: capture.mjs --url URL --screenshot FILE --run-json FILE --result-json FILE --index FILE [options]

Options:
  --name TEXT
  --collection PATH
  --expected-screenshot FILE
  --browser chrome|auto
  --settle-ms MS
  --timeout-ms MS
  --likeness PERCENT
  --headed
  --width PX
  --height PX
`);
}

function parseArgs(argv) {
    const args = {
        browser: "chrome",
        settleMs: 1500,
        timeoutMs: 10000,
        likenessThreshold: 95,
        headed: false,
        width: 960,
        height: 540,
    };

    for (let i = 0; i < argv.length; i += 1) {
        const arg = argv[i];
        switch (arg) {
            case "--url":
                args.url = argv[++i];
                break;
            case "--name":
                args.testName = argv[++i];
                break;
            case "--collection":
                args.collection = argv[++i];
                break;
            case "--expected-screenshot":
                args.expectedScreenshotPath = argv[++i];
                break;
            case "--browser":
                args.browser = argv[++i];
                break;
            case "--settle-ms":
                args.settleMs = Number(argv[++i]);
                break;
            case "--timeout-ms":
                args.timeoutMs = Number(argv[++i]);
                break;
            case "--likeness":
                args.likenessThreshold = Number(argv[++i]);
                break;
            case "--screenshot":
                args.screenshotPath = argv[++i];
                break;
            case "--run-json":
                args.runJsonPath = argv[++i];
                break;
            case "--result-json":
                args.resultJsonPath = argv[++i];
                break;
            case "--index":
                args.indexPath = argv[++i];
                break;
            case "--width":
                args.width = Number(argv[++i]);
                break;
            case "--height":
                args.height = Number(argv[++i]);
                break;
            case "--headed":
                args.headed = true;
                break;
            case "-h":
            case "--help":
                usage();
                process.exit(0);
                break;
            default:
                throw new Error(`Unknown argument: ${arg}`);
        }
    }

    for (const required of ["url", "screenshotPath", "runJsonPath", "resultJsonPath", "indexPath"]) {
        if (!args[required]) {
            throw new Error(`Missing required argument: ${required}`);
        }
    }

    if (!Number.isFinite(args.settleMs) || args.settleMs < 0) {
        throw new Error(`Invalid --settle-ms: ${args.settleMs}`);
    }

    if (!Number.isFinite(args.timeoutMs) || args.timeoutMs <= 0) {
        throw new Error(`Invalid --timeout-ms: ${args.timeoutMs}`);
    }

    if (!Number.isFinite(args.likenessThreshold) || args.likenessThreshold < 0 || args.likenessThreshold > 100) {
        throw new Error(`Invalid --likeness: ${args.likenessThreshold}`);
    }

    return args;
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
}

function requestJson(url) {
    return new Promise((resolve, reject) => {
        const req = http.get(url, (res) => {
            if (res.statusCode !== 200) {
                reject(new Error(`HTTP ${res.statusCode} for ${url}`));
                res.resume();
                return;
            }

            let raw = "";
            res.setEncoding("utf8");
            res.on("data", (chunk) => {
                raw += chunk;
            });
            res.on("end", () => {
                try {
                    resolve(JSON.parse(raw));
                } catch (error) {
                    reject(error);
                }
            });
        });

        req.on("error", reject);
    });
}

function getFreePort() {
    return new Promise((resolve, reject) => {
        const server = net.createServer();
        server.on("error", reject);
        server.listen(0, "127.0.0.1", () => {
            const address = server.address();
            const port = typeof address === "object" && address ? address.port : null;
            server.close((closeError) => {
                if (closeError) {
                    reject(closeError);
                    return;
                }
                resolve(port);
            });
        });
    });
}

function findChromeExecutable() {
    const envPath = process.env.CHROME_EXECUTABLE || process.env.BROWSER_EXECUTABLE;
    if (envPath) {
        return envPath;
    }

    const candidates = process.platform === "darwin"
        ? [
            "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
            "/Applications/Chromium.app/Contents/MacOS/Chromium",
        ]
        : process.platform === "win32"
            ? [
                "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
                "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
                "C:\\Program Files\\Chromium\\Application\\chrome.exe",
            ]
            : [
                "/usr/bin/google-chrome",
                "/usr/bin/google-chrome-stable",
                "/usr/bin/chromium",
                "/usr/bin/chromium-browser",
            ];

    for (const candidate of candidates) {
        try {
            if (candidate && hasFsAccess(candidate)) {
                return candidate;
            }
        } catch {
            // Ignore missing paths.
        }
    }

    throw new Error("Could not find a Chrome/Chromium executable. Set CHROME_EXECUTABLE.");
}

function hasFsAccess(filePath) {
    fsSync.accessSync(filePath);
    return true;
}

class CDPSession {
    constructor(ws) {
        this.ws = ws;
        this.nextId = 1;
        this.pending = new Map();
        this.eventListeners = new Map();

        ws.addEventListener("message", (event) => {
            const message = JSON.parse(event.data);
            if (message.id) {
                const pending = this.pending.get(message.id);
                if (!pending) {
                    return;
                }
                this.pending.delete(message.id);
                if (message.error) {
                    pending.reject(new Error(message.error.message || JSON.stringify(message.error)));
                } else {
                    pending.resolve(message.result);
                }
                return;
            }

            const listeners = this.eventListeners.get(message.method) || [];
            for (const listener of listeners) {
                listener(message.params || {});
            }
        });
    }

    send(method, params = {}) {
        return new Promise((resolve, reject) => {
            const id = this.nextId++;
            this.pending.set(id, { resolve, reject });
            this.ws.send(JSON.stringify({ id, method, params }));
        });
    }

    once(method) {
        return new Promise((resolve) => {
            const listener = (params) => {
                const listeners = this.eventListeners.get(method) || [];
                this.eventListeners.set(
                    method,
                    listeners.filter((entry) => entry !== listener),
                );
                resolve(params);
            };

            const listeners = this.eventListeners.get(method) || [];
            listeners.push(listener);
            this.eventListeners.set(method, listeners);
        });
    }

    async close() {
        if (this.ws.readyState === WebSocket.OPEN) {
            this.ws.close();
        }
    }
}

async function waitForBrowserTarget(debugPort) {
    const deadline = Date.now() + 15000;
    while (Date.now() < deadline) {
        try {
            const targets = await requestJson(`http://127.0.0.1:${debugPort}/json/list`);
            const pageTarget = targets.find((target) => target.type === "page");
            if (pageTarget?.webSocketDebuggerUrl) {
                return pageTarget;
            }
        } catch {
            // Browser may not be ready yet.
        }
        await sleep(200);
    }
    throw new Error("Timed out waiting for Chrome DevTools target");
}

async function waitForCanvas(session) {
    const deadline = Date.now() + 30000;
    while (Date.now() < deadline) {
        const result = await session.send("Runtime.evaluate", {
            expression: `(() => {
                const canvas = document.querySelector("#canvas, canvas");
                if (!canvas) return null;
                const rect = canvas.getBoundingClientRect();
                return {
                    readyState: document.readyState,
                    hasModule: typeof window.Module !== "undefined",
                    x: rect.x,
                    y: rect.y,
                    width: rect.width,
                    height: rect.height,
                    dpr: window.devicePixelRatio || 1
                };
            })()`,
            returnByValue: true,
            awaitPromise: true,
        });

        const value = result.result?.value;
        if (value && value.width > 0 && value.height > 0) {
            return value;
        }

        await sleep(250);
    }
    throw new Error("Timed out waiting for Defold canvas");
}

function compareImages(resultPath, expectedPath, reportDir) {
    const diffImageName = `diff-${path.basename(resultPath)}`;
    const diffImagePath = path.join(reportDir, diffImageName);
    const compareArgs = ["-metric", "RMSE", resultPath, expectedPath, diffImagePath];
    console.log(`Running: compare ${compareArgs.join(" ")}`);
    const compareResult = spawnSync(
        "compare",
        compareArgs,
        { encoding: "utf8" },
    );

    const rawOutput = `${compareResult.stdout || ""}${compareResult.stderr || ""}`.trim();

    if (compareResult.error) {
        throw new Error(`Failed to run ImageMagick compare: ${compareResult.error.message}`);
    }

    if (compareResult.status !== 0 && compareResult.status !== 1) {
        throw new Error(rawOutput || `ImageMagick compare exited with status ${compareResult.status}`);
    }

    const match = rawOutput.match(/\(([0-9.+\-eE]+)\)/);
    if (!match) {
        throw new Error(`Could not parse ImageMagick compare output: ${rawOutput}`);
    }

    const normalizedDifference = Number(match[1]);
    if (!Number.isFinite(normalizedDifference)) {
        throw new Error(`Invalid normalized difference from compare output: ${rawOutput}`);
    }

    const likenessPercent = Math.max(0, Math.min(100, (1 - normalizedDifference) * 100));

    return {
        metric: "RMSE",
        raw_output: rawOutput,
        normalized_difference: normalizedDifference,
        likeness_percent: likenessPercent,
        diff_image_name: diffImageName,
    };
}

async function writeReport(runJsonPath, resultJsonPath, indexPath, runData, resultData) {
    await fs.mkdir(path.dirname(runJsonPath), { recursive: true });

    const resultScreenshotName = path.basename(runData.screenshot_path);
    let expectedScreenshotName = null;
    let comparison = resultData;
    if (runData.expected_screenshot_path) {
        expectedScreenshotName = `expected-${path.basename(runData.expected_screenshot_path)}`;
        await fs.copyFile(
            runData.expected_screenshot_path,
            path.join(path.dirname(indexPath), expectedScreenshotName),
        );

        comparison = compareImages(
            runData.screenshot_path,
            path.join(path.dirname(indexPath), expectedScreenshotName),
            path.dirname(indexPath),
        );
    }

    const finalizedResultData = {
        ...resultData,
        ...(comparison || {}),
    };

    if (finalizedResultData.likeness_percent !== undefined) {
        finalizedResultData.status = finalizedResultData.likeness_percent >= runData.likeness_threshold ? "pass" : "fail";
    } else {
        finalizedResultData.status = "fail";
    }

    const runJson = JSON.stringify(runData, null, 2);
    const resultJson = JSON.stringify(finalizedResultData, null, 2);
    await fs.writeFile(runJsonPath, `${runJson}\n`, "utf8");
    await fs.writeFile(resultJsonPath, `${resultJson}\n`, "utf8");

    const title = runData.test_name || runData.collection || "Render Test";
    const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Render Test Report</title>
  <style>
    body {
      margin: 24px;
      font-family: Menlo, Monaco, Consolas, monospace;
      background: #101418;
      color: #d7e0ea;
    }
    h1, h2 {
      font-weight: 600;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 24px;
      margin-top: 24px;
    }
    .panel {
      background: #182028;
      border-radius: 8px;
      padding: 16px;
    }
    .result-summary {
      margin-top: 24px;
    }
    .result-detail {
      margin: 6px 0;
      color: #b9c7d5;
    }
    .result-details {
      margin-top: 16px;
      background: #101418;
      border-radius: 8px;
      overflow: hidden;
    }
    .result-details summary {
      padding: 12px 14px;
      font-weight: 600;
    }
    .result-details[open] summary {
      border-bottom: 1px solid #2b3947;
    }
    .result-details-body {
      padding: 16px;
    }
    .result-details-body .meta {
      margin-bottom: 16px;
    }
    .meta {
      white-space: pre-wrap;
      background: #182028;
      border-radius: 8px;
      padding: 16px;
      line-height: 1.5;
    }
    details {
      margin-top: 24px;
      margin-bottom: 24px;
      background: #182028;
      border-radius: 8px;
      padding: 0;
      overflow: hidden;
    }
    summary {
      cursor: pointer;
      padding: 14px 16px;
      font-weight: 600;
      user-select: none;
    }
    details[open] summary {
      border-bottom: 1px solid #2b3947;
    }
    details .meta {
      margin: 0;
      border-radius: 0;
      background: transparent;
    }
    img {
      display: block;
      max-width: 100%;
      height: auto;
      border-radius: 8px;
      border: 1px solid #2b3947;
      background: #0b0f13;
    }
  </style>
</head>
<body>
  <h1>${escapeHtml(title)}</h1>
  <details>
    <summary>Meta Info</summary>
    <div class="meta">${escapeHtml(runJson)}</div>
  </details>
  <section class="panel result-summary">
    <h2>Result: ${finalizedResultData.likeness_percent !== undefined
        ? `${escapeHtml(finalizedResultData.likeness_percent.toFixed(2))}% Likeness - ${finalizedResultData.status === "pass" ? "✅ PASS" : "❌ FAIL"}`
        : "No comparison result available"}</h2>
    ${finalizedResultData.likeness_percent !== undefined
        ? `<details class="result-details">
      <summary>Detailed Info</summary>
      <div class="result-details-body">
        <p class="result-detail">Threshold: ${escapeHtml(runData.likeness_threshold.toFixed(2))}%</p>
        <p class="result-detail">Metric: ${escapeHtml(finalizedResultData.metric)}</p>
        <p class="result-detail">Normalized difference: ${escapeHtml(finalizedResultData.normalized_difference.toFixed(6))}</p>
        <div class="meta">${escapeHtml(resultJson)}</div>
        <img src="${escapeHtml(finalizedResultData.diff_image_name)}" alt="Difference image from ImageMagick compare">
      </div>
    </details>`
        : `<p class="result-detail">No comparison result available.</p>`}
  </section>
  <div class="grid">
    <section class="panel">
      <h2>Captured:</h2>
      <img src="${escapeHtml(resultScreenshotName)}" alt="Render test result screenshot">
    </section>
    <section class="panel">
      <h2>Expected:</h2>
      ${expectedScreenshotName
        ? `<img src="${escapeHtml(expectedScreenshotName)}" alt="Expected render screenshot">`
        : `<p>No expected screenshot provided.</p>`}
    </section>
  </div>
</body>
</html>
`;

    await fs.writeFile(indexPath, html, "utf8");
}

async function main() {
    const args = parseArgs(process.argv.slice(2));
    if (typeof WebSocket !== "function") {
        throw new Error("WebSocket is not available in this Node runtime. Use Node.js 22 or newer.");
    }
    const chromePath = findChromeExecutable();
    const debugPort = await getFreePort();
    const chromeArgs = [
        `--remote-debugging-port=${debugPort}`,
        "--disable-background-networking",
        "--disable-background-timer-throttling",
        "--disable-breakpad",
        "--disable-component-update",
        "--disable-default-apps",
        "--disable-renderer-backgrounding",
        "--disable-sync",
        "--hide-scrollbars",
        "--mute-audio",
        "--no-first-run",
        "--no-default-browser-check",
        `--window-size=${args.width},${args.height}`,
        args.url,
    ];

    if (!args.headed) {
        chromeArgs.unshift("--headless=new");
    }

    const chrome = spawn(chromePath, chromeArgs, {
        stdio: ["ignore", "ignore", "ignore"],
    });

    let chromeExited = false;
    chrome.on("exit", () => {
        chromeExited = true;
    });

    try {
        await Promise.race([
            (async () => {
                const target = await waitForBrowserTarget(debugPort);
                const ws = new WebSocket(target.webSocketDebuggerUrl);
                await new Promise((resolve, reject) => {
                    ws.addEventListener("open", resolve, { once: true });
                    ws.addEventListener("error", reject, { once: true });
                });

                const session = new CDPSession(ws);
                try {
                    await session.send("Page.enable");
                    await session.send("Runtime.enable");
                    await session.send("Emulation.setDeviceMetricsOverride", {
                        width: args.width,
                        height: args.height,
                        deviceScaleFactor: 1,
                        mobile: false,
                    });

                    const loadEvent = session.once("Page.loadEventFired");
                    await session.send("Page.navigate", { url: args.url });
                    await loadEvent;

                    const canvas = await waitForCanvas(session);
                    await sleep(args.settleMs);

                    const clip = {
                        x: canvas.x,
                        y: canvas.y,
                        width: canvas.width,
                        height: canvas.height,
                        scale: 1,
                    };

                    const screenshot = await session.send("Page.captureScreenshot", {
                        format: "png",
                        fromSurface: true,
                        clip,
                    });

                    await fs.mkdir(path.dirname(args.screenshotPath), { recursive: true });
                    await fs.writeFile(args.screenshotPath, Buffer.from(screenshot.data, "base64"));

                    const runData = {
                        mode: "direct",
                        test_name: args.testName || "",
                        collection: args.collection,
                        browser: "chrome",
                        browser_executable: chromePath,
                        url: args.url,
                        screenshot_path: args.screenshotPath,
                        expected_screenshot_path: args.expectedScreenshotPath || "",
                        captured_at: new Date().toISOString(),
                        settle_ms: args.settleMs,
                        timeout_ms: args.timeoutMs,
                        likeness_threshold: args.likenessThreshold,
                        canvas: clip,
                    };

                    const resultData = {
                    };

                    await writeReport(args.runJsonPath, args.resultJsonPath, args.indexPath, runData, resultData);

                    const resultJsonRaw = await fs.readFile(args.resultJsonPath, "utf8");
                    const finalizedResultData = JSON.parse(resultJsonRaw);
                    if (finalizedResultData.status !== "pass") {
                        throw new RenderComparisonError(
                            `Render comparison failed: ${finalizedResultData.likeness_percent?.toFixed(2) ?? "n/a"}% likeness, threshold ${args.likenessThreshold.toFixed(2)}%`,
                        );
                    }
                } finally {
                    await session.close();
                }
            })(),
            new Promise((_, reject) => {
                setTimeout(() => {
                    reject(new Error(`Timed out after ${args.timeoutMs} ms`));
                }, args.timeoutMs);
            }),
        ]);
    } finally {
        if (!chromeExited) {
            chrome.kill("SIGTERM");
            await sleep(250);
            if (!chromeExited) {
                chrome.kill("SIGKILL");
            }
        }
    }
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(error instanceof RenderComparisonError ? 2 : 1);
});
