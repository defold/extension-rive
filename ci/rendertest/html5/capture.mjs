#!/usr/bin/env node

import fs from "node:fs/promises";
import fsSync from "node:fs";
import http from "node:http";
import net from "node:net";
import path from "node:path";
import { spawn } from "node:child_process";

const RENDERTEST_BOOTSTRAP_SCRIPT = `(() => {
    const state = window.__rendertest = window.__rendertest || {};
    if (typeof state.engineStartRequestedAt === "undefined") state.engineStartRequestedAt = null;
    if (typeof state.engineFirstUpdateAt === "undefined") state.engineFirstUpdateAt = null;
    if (typeof state.engineTakeScreenshotAt === "undefined") state.engineTakeScreenshotAt = null;
    if (typeof state.engineStartSucceededAt === "undefined") state.engineStartSucceededAt = null;
    if (typeof state.engineStartError === "undefined") state.engineStartError = null;
    if (typeof state.engineStartErrorStack === "undefined") state.engineStartErrorStack = null;

    const wrapCustomParameters = (params) => {
        if (!params || params.__rendertestWrapped) {
            return params;
        }

        const queryParams = new URLSearchParams(window.location.search);
        const keys = queryParams.getAll("key");
        const values = queryParams.getAll("value");
        if (keys.length > 0) {
            if (!params.engine_arguments) {
                params.engine_arguments = [];
            }

            for (let i = 0; i < keys.length; i += 1) {
                const key = keys[i];
                const value = values[i];
                if (!key || value === null) {
                    continue;
                }

                const prefix = "--config=" + key + "=";
                params.engine_arguments = params.engine_arguments.filter((arg) => !arg.startsWith(prefix));
                params.engine_arguments.push(prefix + value);
            }
        }

        const previousStartSuccess = params.start_success;
        const previousStartError = params.start_error;

        params.start_success = function(...args) {
            if (state.engineStartSucceededAt === null) {
                state.engineStartSucceededAt = Date.now();
            }
            if (typeof previousStartSuccess === "function") {
                return previousStartSuccess.apply(this, args);
            }
            return undefined;
        };

        params.start_error = function(error, ...args) {
            const message = error && error.message ? error.message : String(error);
            state.engineStartError = message;
            state.engineStartErrorStack = error && error.stack ? String(error.stack) : null;
            if (typeof previousStartError === "function") {
                return previousStartError.call(this, error, ...args);
            }
            return undefined;
        };

        Object.defineProperty(params, "__rendertestWrapped", {
            value: true,
            configurable: true,
        });
        return params;
    };

    const wrapEngineLoader = (loader) => {
        if (!loader || loader.__rendertestWrapped) {
            return loader;
        }

        const originalLoad = loader.load;
        if (typeof originalLoad === "function") {
            loader.load = function(...args) {
                if (state.engineStartRequestedAt === null) {
                    state.engineStartRequestedAt = Date.now();
                }
                return originalLoad.apply(this, args);
            };
        }

        Object.defineProperty(loader, "__rendertestWrapped", {
            value: true,
            configurable: true,
        });
        return loader;
    };

    const wrapModule = (moduleValue) => {
        if (!moduleValue || moduleValue.__rendertestWrapped) {
            return moduleValue;
        }

        const markTimingSignal = function() {
            if (state.engineFirstUpdateAt === null) {
                state.engineFirstUpdateAt = Date.now();
            }
            if (state.engineTakeScreenshotAt === null) {
                state.engineTakeScreenshotAt = state.engineFirstUpdateAt;
            }
            if (state.engineStartRequestedAt === null) {
                state.engineStartRequestedAt = state.engineTakeScreenshotAt;
            }
        };

        moduleValue.firstEngineUpdate = markTimingSignal;
        moduleValue.takeScreenshot = markTimingSignal;

        Object.defineProperty(moduleValue, "__rendertestWrapped", {
            value: true,
            configurable: true,
        });
        return moduleValue;
    };

    let customParametersValue;
    Object.defineProperty(window, "CUSTOM_PARAMETERS", {
        configurable: true,
        enumerable: true,
        get() {
            return customParametersValue;
        },
        set(value) {
            customParametersValue = wrapCustomParameters(value);
        },
    });

    let engineLoaderValue;
    Object.defineProperty(window, "EngineLoader", {
        configurable: true,
        enumerable: true,
        get() {
            return engineLoaderValue;
        },
        set(value) {
            engineLoaderValue = wrapEngineLoader(value);
        },
    });

    let moduleValue;
    Object.defineProperty(window, "Module", {
        configurable: true,
        enumerable: true,
        get() {
            return moduleValue;
        },
        set(value) {
            moduleValue = wrapModule(value);
        },
    });

    const poll = setInterval(() => {
        if (window.CUSTOM_PARAMETERS) {
            customParametersValue = wrapCustomParameters(window.CUSTOM_PARAMETERS);
        }
        if (window.EngineLoader) {
            engineLoaderValue = wrapEngineLoader(window.EngineLoader);
        }
        if (window.Module) {
            moduleValue = wrapModule(window.Module);
        }
        if (
            customParametersValue && customParametersValue.__rendertestWrapped &&
            engineLoaderValue && engineLoaderValue.__rendertestWrapped
        ) {
            clearInterval(poll);
        }
    }, 10);
})();`;

const ENGINE_START_LOG_PREFIX = "INFO:ENGINE: Defold Engine";

function usage() {
    console.error(`usage: capture.mjs --url URL --screenshot FILE --run-json FILE [options]

Options:
  --name TEXT
  --description-file PATH
  --collection PATH
  --platform TEXT
  --browser chrome|auto
  --wait-mode timeout|signal
  --settle-ms MS
  --startup-timeout-ms MS
  --headed
  --width PX
  --height PX
`);
}

function parseArgs(argv) {
    const args = {
        browser: "chrome",
        waitMode: "timeout",
        settleMs: 1500,
        startupTimeoutMs: 10000,
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
            case "--description-file":
                args.descriptionFile = argv[++i];
                break;
            case "--collection":
                args.collection = argv[++i];
                break;
            case "--platform":
                args.platform = argv[++i];
                break;
            case "--browser":
                args.browser = argv[++i];
                break;
            case "--wait-mode":
                args.waitMode = argv[++i];
                break;
            case "--settle-ms":
                args.settleMs = Number(argv[++i]);
                break;
            case "--startup-timeout-ms":
                args.startupTimeoutMs = Number(argv[++i]);
                break;
            case "--screenshot":
                args.screenshotPath = argv[++i];
                break;
            case "--run-json":
                args.runJsonPath = argv[++i];
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

    for (const required of ["url", "screenshotPath", "runJsonPath"]) {
        if (!args[required]) {
            throw new Error(`Missing required argument: ${required}`);
        }
    }

    if (!Number.isFinite(args.settleMs) || args.settleMs < 0) {
        throw new Error(`Invalid --settle-ms: ${args.settleMs}`);
    }

    if (!["timeout", "signal"].includes(args.waitMode)) {
        throw new Error(`Invalid --wait-mode: ${args.waitMode}`);
    }

    if (!Number.isFinite(args.startupTimeoutMs) || args.startupTimeoutMs <= 0) {
        throw new Error(`Invalid --startup-timeout-ms: ${args.startupTimeoutMs}`);
    }

    if (args.descriptionFile && typeof args.descriptionFile !== "string") {
        throw new Error(`Invalid --description-file: ${args.descriptionFile}`);
    }

    return args;
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function describeRemoteObject(remoteObject) {
    if (Object.prototype.hasOwnProperty.call(remoteObject, "value")) {
        if (remoteObject.value === null) {
            return "null";
        }
        if (typeof remoteObject.value === "string") {
            return remoteObject.value;
        }
        return String(remoteObject.value);
    }
    if (typeof remoteObject.description === "string") {
        return remoteObject.description;
    }
    return remoteObject.type || "<object>";
}

function formatCallFrame(frame) {
    if (!frame) {
        return "";
    }
    const url = frame.url || frame.scriptId || "eval";
    const line = Number.isFinite(frame.lineNumber) ? frame.lineNumber : 0;
    const column = Number.isFinite(frame.columnNumber) ? frame.columnNumber : 0;
    return `${url}:${line}:${column}`;
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

    on(method, listener) {
        const listeners = this.eventListeners.get(method) || [];
        listeners.push(listener);
        this.eventListeners.set(method, listeners);
    }

    async close() {
        if (this.ws.readyState === WebSocket.OPEN) {
            this.ws.close();
        }
    }
}

async function waitForBrowserTarget(debugPort, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
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
    throw new Error(`Timed out waiting for Chrome DevTools target after ${timeoutMs} ms`);
}

async function getRenderState(session) {
    const result = await session.send("Runtime.evaluate", {
        expression: `(() => {
            const hasRenderTestState = typeof window.__rendertest !== "undefined";
            const state = window.__rendertest || {};
            const canvas = document.querySelector("#canvas, canvas");
            const rect = canvas ? canvas.getBoundingClientRect() : null;
            return {
                hasRenderTestState,
                readyState: document.readyState,
                hasModule: typeof window.Module !== "undefined",
                engineStartRequestedAt: state.engineStartRequestedAt || null,
                engineFirstUpdateAt: state.engineFirstUpdateAt || null,
                engineTakeScreenshotAt: state.engineTakeScreenshotAt || null,
                engineStartSucceededAt: state.engineStartSucceededAt || null,
                engineStartError: state.engineStartError || null,
                engineStartErrorStack: state.engineStartErrorStack || null,
                canvas: rect ? {
                    x: rect.x,
                    y: rect.y,
                    width: rect.width,
                    height: rect.height,
                    dpr: window.devicePixelRatio || 1,
                } : null,
            };
        })()`,
        returnByValue: true,
        awaitPromise: true,
    });

    return result.result?.value || null;
}

async function waitForEngineStart(session, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const state = await getRenderState(session);
        if (state?.engineStartError) {
            const details = state.engineStartErrorStack ? `\n${state.engineStartErrorStack}` : "";
            throw new Error(`Engine start failed: ${state.engineStartError}${details}`);
        }

        if (state?.engineStartSucceededAt) {
            return state;
        }

        await sleep(250);
    }
    throw new Error(`Timed out waiting for Defold engine start after ${timeoutMs} ms`);
}

async function waitForEngineStartLog(consoleEntries, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        if (consoleEntries.some((entry) => entry.includes(ENGINE_START_LOG_PREFIX))) {
            return;
        }

        await sleep(100);
    }

    throw new Error(`Timed out waiting for Defold engine log line after ${timeoutMs} ms`);
}

async function waitForScreenshotSignal(session, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const state = await getRenderState(session);
        if (state?.engineStartError) {
            const details = state.engineStartErrorStack ? `\n${state.engineStartErrorStack}` : "";
            throw new Error(`Engine start failed: ${state.engineStartError}${details}`);
        }

        if (state?.engineTakeScreenshotAt) {
            return state;
        }

        await sleep(250);
    }
    throw new Error(`Timed out waiting for Defold screenshot signal after ${timeoutMs} ms`);
}

async function waitForSettledCapture(session, consoleEntries, timeoutMs, settleMs) {
    await waitForEngineStartLog(consoleEntries, timeoutMs);
    const engineState = await waitForEngineStart(session, timeoutMs);
    if (settleMs > 0) {
        await sleep(settleMs);
    }

    const renderState = await getRenderState(session);
    const canvas = renderState?.canvas;
    if (!canvas || canvas.width <= 0 || canvas.height <= 0) {
        throw new Error("Defold canvas was not ready after engine start");
    }

    return { engineState, canvas };
}

async function main() {
    const args = parseArgs(process.argv.slice(2));
    const reportDir = path.dirname(args.runJsonPath);
    const outputDir = path.dirname(reportDir);
    const consoleLogPath = path.join(outputDir, "console.log");
    const consoleEntries = [];
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
    let engineStartLogSeenAt = null;
    chrome.on("exit", () => {
        chromeExited = true;
    });

    try {
        await (async () => {
                const target = await waitForBrowserTarget(debugPort, args.startupTimeoutMs);
                const ws = new WebSocket(target.webSocketDebuggerUrl);
                await new Promise((resolve, reject) => {
                    ws.addEventListener("open", resolve, { once: true });
                    ws.addEventListener("error", reject, { once: true });
                });

                const session = new CDPSession(ws);
                session.on("Runtime.consoleAPICalled", (params) => {
                    const argsText = (params.args || []).map(describeRemoteObject).join(" ");
                    const location = formatCallFrame(params.stackTrace?.callFrames?.[0]);
                    const entry = `${new Date().toISOString()} [${params.type}] ${argsText}${location ? ` (${location})` : ""}`;
                    consoleEntries.push(entry);
                    if (engineStartLogSeenAt === null && argsText.includes(ENGINE_START_LOG_PREFIX)) {
                        engineStartLogSeenAt = Date.now();
                    }
                });
                session.on("Runtime.exceptionThrown", (params) => {
                    const details = params.exceptionDetails || {};
                    const message =
                        details.text ||
                        details.exception?.description ||
                        details.exceptionText ||
                        "exception";
                    const location =
                        formatCallFrame(details.stackTrace?.callFrames?.[0]) ||
                        formatCallFrame(params.stackTrace?.callFrames?.[0]);
                    const entry = `${new Date().toISOString()} [exception] ${message}${location ? ` (${location})` : ""}`;
                    consoleEntries.push(entry);
                });
                try {
                    await session.send("Page.enable");
                    await session.send("Runtime.enable");
                    await session.send("Emulation.setDeviceMetricsOverride", {
                        width: args.width,
                        height: args.height,
                        deviceScaleFactor: 1,
                        mobile: false,
                    });
                    await session.send("Page.addScriptToEvaluateOnNewDocument", {
                        source: RENDERTEST_BOOTSTRAP_SCRIPT,
                    });

                    const loadEvent = session.once("Page.loadEventFired");
                    await session.send("Page.navigate", { url: args.url });
                    await loadEvent;

                    let engineState;
                    let canvas;
                    if (args.waitMode === "signal") {
                        engineState = await waitForScreenshotSignal(session, args.startupTimeoutMs);
                        const renderState = await getRenderState(session);
                        canvas = renderState?.canvas;
                        if (!canvas || canvas.width <= 0 || canvas.height <= 0) {
                            throw new Error("Defold canvas was not ready when takeScreenshot() fired");
                        }
                    } else {
                        const settledCapture = await waitForSettledCapture(session, consoleEntries, args.startupTimeoutMs, args.settleMs);
                        engineState = settledCapture.engineState;
                        canvas = settledCapture.canvas;
                    }

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

                    let description = "";
                    if (args.descriptionFile) {
                        const descriptionPath = path.isAbsolute(args.descriptionFile)
                            ? args.descriptionFile
                            : path.resolve(process.cwd(), args.descriptionFile);
                        description = (await fs.readFile(descriptionPath, "utf8")).trim();
                        await fs.mkdir(outputDir, { recursive: true });
                        await fs.writeFile(path.join(outputDir, "description.txt"), `${description}\n`, "utf8");
                    }

                    const runData = {
                        mode: "direct",
                        test_name: args.testName || "",
                        test_group: path.basename(outputDir),
                        collection: args.collection,
                        platform: args.platform || "",
                        description,
                        browser: "chrome",
                        browser_executable: chromePath,
                        url: args.url,
                        wait_mode: args.waitMode,
                        screenshot_path: args.screenshotPath,
                        captured_at: new Date().toISOString(),
                        settle_ms: args.settleMs,
                        startup_timeout_ms: args.startupTimeoutMs,
                        engine_start_requested_at: engineState.engineStartRequestedAt,
                        engine_first_update_at: engineState.engineFirstUpdateAt,
                        engine_take_screenshot_at: engineState.engineTakeScreenshotAt,
                        engine_start_succeeded_at: engineState.engineStartSucceededAt,
                        engine_start_log_prefix: ENGINE_START_LOG_PREFIX,
                        engine_start_log_seen_at: engineStartLogSeenAt,
                        engine_start_duration_ms:
                            engineState.engineStartSucceededAt &&
                            (engineState.engineTakeScreenshotAt || engineState.engineFirstUpdateAt || engineState.engineStartRequestedAt)
                                ? engineState.engineStartSucceededAt - (engineState.engineTakeScreenshotAt || engineState.engineFirstUpdateAt || engineState.engineStartRequestedAt)
                                : null,
                        canvas: clip,
                    };

                    await fs.mkdir(outputDir, { recursive: true });
                    await fs.mkdir(path.dirname(args.runJsonPath), { recursive: true });
                    await fs.writeFile(
                        consoleLogPath,
                        consoleEntries.join("\n") + (consoleEntries.length ? "\n" : ""),
                        "utf8",
                    );
                    await fs.writeFile(args.runJsonPath, `${JSON.stringify(runData, null, 2)}\n`, "utf8");
                } finally {
                    await session.close();
                }
            })();
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
    process.exit(1);
});
