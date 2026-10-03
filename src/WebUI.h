#pragma once
#include <Arduino.h>

// Web page served by the board (single page, no external resources).
// Device-console styling in the spirit of modern smart-home gadget UIs:
// light grey ground (dark when the phone/PC is in dark mode), white rounded
// cards, green accent, compact rows, segmented tab bar. Battery level in the
// header (from /api/status). Four tabs: Control (buttons + raw frame), Log,
// Wi-Fi (join, scan, AP password), Settings (firmware update,
// device diagnostics, reboot). API, see the README for details:
//   GET  /api/status       GET  /api/wifiscan     GET  /api/frames
//   POST /api/send         POST /api/sendhex
//   POST /api/wifi         POST /api/appassword     POST /api/usblog     POST /api/irtest
//   POST /api/zigbeepair   POST /api/zigbeeforget
//   POST /api/update    (multipart .bin upload)
//   POST /api/reboot       POST /api/log/clear
//
// The board serves a gzipped copy of this page from WebUI_gz.h (~4x smaller,
// so less time with the radio awake per page load). AFTER EDITING THIS FILE
// run   node tools/build_webui_gz.js   to regenerate it: until you do, the
// build stops with a static_assert instead of silently serving a stale page.
//
// Polling is deliberately frugal, since every request wakes the board's
// radio (see kWifiPowerSave): every 2.5s while the page is being used, 10s
// after a minute without input, 60s after five, and not at all while the tab
// is hidden or the phone's screen is off.
static constexpr char kIndexHtml[] = R"PAGE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#ffffff" media="(prefers-color-scheme: light)">
<meta name="theme-color" content="#181d24" media="(prefers-color-scheme: dark)">
<title>VELUX WLI 130 51</title>
<style>
:root{
  color-scheme:light dark;
  --bg:#eef0f3;--card:#fff;--fg:#1b2430;--mut:#6d7885;--line:#e3e6ea;--input:#f7f8fa;--hover:#eef1f4;--bar-off:#cfd5dc;
  --grn:#3aa757;--grn-d:#2f8a48;--grn-l:#e8f6ec;--grn-t:#2f8a48;
  --red:#d9453f;--red-l:#fdecec;--blue:#2f6fd0;--blue-l:#eaf1fc;
  --amb:#c9820a;--amb-l:#fdf3e1;
  --r:14px;--r-sm:10px;
  --sans:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
  --mono:ui-monospace,Menlo,Consolas,monospace;
  --sh:0 1px 2px rgba(27,36,48,.06),0 4px 14px rgba(27,36,48,.05);
}
/* --grn-t is green used as TEXT on a surface, --grn-d green used as a
   button BACKGROUND: the same colour in light mode, far apart in dark. */
@media (prefers-color-scheme:dark){:root{
  --bg:#0f1318;--card:#181d24;--fg:#e4e8ee;--mut:#98a2ae;--line:#2a313a;--input:#1f252d;--hover:#262d36;--bar-off:#3a434e;
  --grn:#35a052;--grn-d:#2b8544;--grn-l:#16301f;--grn-t:#6fd08a;
  --red:#ff6b64;--red-l:#3a1c1c;--blue:#6aa2ff;--blue-l:#17263d;
  --amb:#e6a93a;--amb-l:#3a2d12;
  --sh:0 1px 2px rgba(0,0,0,.35);
}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 var(--sans);-webkit-text-size-adjust:100%}
.page{max-width:520px;margin:0 auto;min-height:100vh;display:flex;flex-direction:column}
/* header */
header{position:sticky;top:0;z-index:5;background:var(--card);border-bottom:1px solid var(--line);padding:14px 16px 0}
.hrow{display:flex;align-items:center;gap:12px}
.logo{width:38px;height:38px;border-radius:11px;background:var(--grn-l);color:var(--grn-t);display:flex;align-items:center;justify-content:center;flex:none}
.hname{font-size:17px;font-weight:650;letter-spacing:-.01em;line-height:1.15}
.hsub{font-size:12px;color:var(--mut);font-family:var(--mono)}
.hright{margin-left:auto;display:flex;flex-direction:column;align-items:flex-end;gap:5px}
.bat{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;color:var(--fg);white-space:nowrap;padding-right:4px}
.bat svg{display:block}
.bat.low{color:var(--amb)}
.bat.crit{color:var(--red)}
.pill{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;padding:5px 10px;border-radius:999px;background:var(--grn-l);color:var(--grn-t);white-space:nowrap}
.pill i{width:7px;height:7px;border-radius:50%;background:currentColor}
.pill.warn{background:var(--amb-l);color:var(--amb)}
.pill.bad{background:var(--red-l);color:var(--red)}
nav{display:flex;gap:22px;margin-top:12px;overflow-x:auto}
nav button{background:none;border:0;border-bottom:2px solid transparent;padding:0 0 10px;font:inherit;font-size:13.5px;font-weight:600;color:var(--mut);cursor:pointer;white-space:nowrap}
nav button:hover{color:var(--fg)}
nav button.on{color:var(--grn-t);border-bottom-color:var(--grn)}
/* views */
main{flex:1;padding:16px 16px 26px;display:flex;flex-direction:column;gap:14px}
.view{display:none;flex-direction:column;gap:14px}
.view.on{display:flex}
.card{background:var(--card);border-radius:var(--r);box-shadow:var(--sh);padding:14px 16px}
.ctitle{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:2px}
.ctitle h2{font-size:15px;font-weight:650;margin:0;letter-spacing:-.01em}
.ctitle .sec{font-family:var(--mono);font-size:11.5px;color:var(--mut);background:var(--input);padding:3px 7px;border-radius:6px;letter-spacing:.08em}
.ctitle .sec.bad{background:var(--red-l);color:var(--red)}
.sub{font-size:12px;color:var(--mut);margin:0 0 10px}
.mini{font-size:12px;color:var(--mut);margin:8px 0 0;line-height:1.4}
/* control rows */
.cols{display:grid;grid-template-columns:1fr repeat(3,48px);gap:8px;padding:0 0 6px}
.colhdr{font-size:10px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--mut);text-align:center}
.row{display:grid;grid-template-columns:1fr repeat(3,48px);gap:8px;align-items:center;padding:9px 0;border-top:1px solid var(--line)}
.row .lbl{font-size:14.5px;font-weight:500}
.row.all{border-top:1px solid var(--line);background:var(--grn-l);margin:6px -16px -14px;padding:12px 16px;border-radius:0 0 var(--r) var(--r);grid-template-columns:1fr repeat(3,48px)}
.row.all .lbl{font-weight:650}
.ib{display:flex;align-items:center;justify-content:center;width:48px;height:40px;background:var(--input);border:1px solid var(--line);border-radius:var(--r-sm);color:var(--fg);cursor:pointer;transition:.13s}
.ib:hover:not(:disabled){background:var(--grn-l);border-color:var(--grn);color:var(--grn-t)}
.ib:active:not(:disabled){transform:scale(.96)}
.ib.stop{color:var(--red)}
.ib.stop:hover:not(:disabled){background:var(--red-l);border-color:var(--red);color:var(--red)}
.ib:disabled{opacity:.35;cursor:not-allowed}
.ib.busy:disabled{opacity:1;cursor:progress;animation:pulse .9s ease-in-out infinite}
.ib.done,.ib.stop.done{background:var(--grn-l);border-color:var(--grn);color:var(--grn-t)}
@keyframes pulse{50%{opacity:.4}}
@media (prefers-reduced-motion:reduce){.ib.busy:disabled{animation:none;opacity:.5}}
.ib svg{display:block}
.pmsg{grid-column:1/-1;font-size:12px;border-radius:8px;padding:7px 10px;margin-top:8px}
.pmsg.warn{color:var(--amb);background:var(--amb-l)}
.pmsg.err{color:var(--red);background:var(--red-l)}
/* banner */
.note{display:flex;gap:10px;align-items:flex-start;background:var(--red-l);color:var(--red);border-radius:var(--r);padding:12px 14px;font-size:13px;line-height:1.4;box-shadow:var(--sh)}
.note b{font-weight:650}
/* wifi */
.wifinow{display:flex;align-items:center;gap:12px}
.wifinow .ico{width:40px;height:40px;border-radius:11px;background:var(--blue-l);color:var(--blue);display:flex;align-items:center;justify-content:center;flex:none}
.wifinow .ssid{font-size:15px;font-weight:650}
.wifinow .meta{font-size:12px;color:var(--mut);font-family:var(--mono)}
.net{display:flex;align-items:center;justify-content:space-between;gap:10px;width:100%;text-align:left;background:none;border:0;border-top:1px solid var(--line);padding:11px 0;font:inherit;color:var(--fg);cursor:pointer}
.net:first-child{border-top:0}
.net:hover .ssid{color:var(--grn-t)}
.net .ssid{font-size:14.5px;font-weight:500}
.net .enc{font-size:11.5px;color:var(--mut)}
.net .right{display:flex;align-items:center;gap:10px}
.net .rssi{font-family:var(--mono);font-size:11px;color:var(--mut)}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px}
.bars i{width:3px;border-radius:1px;background:var(--bar-off)}
.bars i.on{background:var(--grn)}
/* log */
.logrow{display:flex;gap:10px;padding:10px 0;border-top:1px solid var(--line);align-items:flex-start}
.logrow:first-child{border-top:0}
.tag{font-size:10px;font-weight:700;letter-spacing:.04em;padding:3px 7px;border-radius:6px;background:var(--grn-l);color:var(--grn-t);flex:none;margin-top:1px}
.logrow .when{font-size:11px;color:var(--mut);margin-top:2px}
.tag.zb{background:var(--blue-l);color:var(--blue)}
.tag.bat{background:var(--amb-l);color:var(--amb)}
.logrow .txt{font-size:14px}
.logrow .frame{font-family:var(--mono);font-size:11px;color:var(--mut);margin-top:2px;word-break:break-all}
.empty{font-size:13.5px;color:var(--mut);padding:4px 0}
/* forms */
.field{display:flex;flex-direction:column;gap:6px;margin-bottom:10px}
.field label{font-size:12px;font-weight:600;color:var(--mut)}
input{background:var(--input);border:1px solid var(--line);border-radius:var(--r-sm);color:var(--fg);padding:11px 12px;font:inherit;font-size:15px;width:100%}
input:focus{outline:none;border-color:var(--grn);box-shadow:0 0 0 3px var(--grn-l)}
.btn{font:inherit;font-size:14px;font-weight:600;border-radius:var(--r-sm);padding:11px 16px;cursor:pointer;border:1px solid var(--grn);background:var(--grn);color:#fff;transition:.13s}
.btn:hover{background:var(--grn-d);border-color:var(--grn-d)}
.btn:active{transform:scale(.99)}
.btn.sec{background:var(--input);color:var(--fg);border-color:var(--line)}
.btn.sec:hover{background:var(--hover)}
.btn.ghost{background:none;border-color:transparent;color:var(--grn-t);padding:6px 8px;font-size:13px}
.btn.ghost:hover{background:var(--grn-l)}
.btn.block{width:100%}
.btn:disabled{opacity:.5;cursor:not-allowed}
.kv{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:9px 0;border-top:1px solid var(--line)}
.kv:first-child{border-top:0}
.check{display:flex;align-items:center;gap:10px;font-size:15px;cursor:pointer;padding:4px 0}
.check input{width:20px;height:20px;accent-color:var(--grn)}
.kv .k{font-size:13px;color:var(--mut);flex:none}
.kv .v{font-family:var(--mono);font-size:12.5px;text-align:right;overflow-wrap:anywhere}
footer{padding:0 16px 22px;text-align:center;font-size:11.5px;color:var(--mut)}
:focus-visible{outline:2px solid var(--grn);outline-offset:2px}
</style></head><body>
<div class="page">

<header>
  <div class="hrow">
    <div class="logo"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" aria-hidden="true"><path d="M4 9a11 11 0 0 1 16 0"></path><path d="M7 12.5a6.5 6.5 0 0 1 10 0"></path><path d="M10.2 16a2.6 2.6 0 0 1 3.6 0"></path><circle cx="12" cy="19.2" r="1.1" fill="currentColor" stroke="none"></circle></svg></div>
    <div>
      <div class="hname">VELUX WLI 130 51</div>
      <div class="hsub" id="dl">&hellip;</div>
    </div>
    <div class="hright">
      <span class="pill" id="conn" role="status"><i></i><span id="connTxt">&hellip;</span></span>
      <span class="bat" id="bat" style="display:none"><svg width="22" height="12" viewBox="0 0 22 12" fill="none" aria-hidden="true"><rect x=".75" y=".75" width="18.5" height="10.5" rx="2.5" stroke="currentColor" stroke-width="1.5"></rect><rect x="20.2" y="3.8" width="1.6" height="4.4" rx=".8" fill="currentColor"></rect><rect id="batFill" x="2.5" y="2.5" width="15" height="7" rx="1.2" fill="currentColor"></rect></svg><span id="batTxt"></span></span>
    </div>
  </div>
  <nav id="tabs" role="tablist" aria-label="Sections">
    <button class="on" role="tab" id="t-control" aria-controls="v-control" aria-selected="true" data-v="control">Control</button>
    <button role="tab" id="t-log" aria-controls="v-log" aria-selected="false" tabindex="-1" data-v="log">Log</button>
    <button role="tab" id="t-wifi" aria-controls="v-wifi" aria-selected="false" tabindex="-1" data-v="wifi">Wi-Fi</button>
    <button role="tab" id="t-adv" aria-controls="v-adv" aria-selected="false" tabindex="-1" data-v="adv">Settings</button>
  </nav>
</header>

<main>
  <section class="view on" id="v-control" role="tabpanel" aria-labelledby="t-control">
    <div class="note" id="bad" style="display:none"></div>
    <div id="panels" style="display:flex;flex-direction:column;gap:14px"></div>

    <div class="card">
      <div class="ctitle"><h2>Send a raw frame</h2></div>
      <p class="sub">Bypasses the buttons above and the frame builder behind them — the safety cooldown does <b>not</b> apply. Mainly for verifying frames or resending one copied from the Log tab.</p>
      <div class="field"><label for="hex">24-bit frame</label><input id="hex" placeholder="0x242E08 or 24 binary digits"></div>
      <button class="btn sec block" onclick="sendHex()">Transmit</button>

      <p class="mini" style="margin-top:14px"><b>Mini guide &mdash; frame reference</b></p>
      <p class="mini">A frame is 24 bits: 3-bit action &middot; 3-bit motor &middot; 4-bit motor set &middot; 10-bit security code &middot; 4-bit checksum (full formula in <code>VeluxIR.h</code>). Below, for each keypad, exactly what the grid buttons above send for its first motor and for all three &mdash; generated by the board from the security codes in <code>Config.h</code>, handy to compare against what shows up in the Log tab:</p>
      <div id="frames"><div class="empty">Loading&hellip;</div></div>
      <p class="mini">The protocol also has distinct "manual" action codes (motor moves only while the original remote's button stays held) &mdash; but a single Transmit here only sends the frame the same short burst as any other command, so they won't behave differently through this box.</p>
    </div>
  </section>

  <section class="view" id="v-log" role="tabpanel" aria-labelledby="t-log">
    <div class="card">
      <div class="ctitle"><h2>Frame log</h2><button class="btn ghost" onclick="clearLog()">Clear</button></div>
      <p class="sub" id="logCount">No entries</p>
      <div id="log"></div>
      <div class="row" style="display:flex;gap:8px;margin-top:10px">
        <button class="btn sec" style="flex:1" onclick="exportLog('json')">Export JSON</button>
        <button class="btn sec" style="flex:1" onclick="exportLog('txt')">Export TXT</button>
      </div>
      <p class="mini">Commands sent (from here, the API or Zigbee). Kept in RAM: a restart clears it.</p>
    </div>
    <div class="card" id="bhCard" style="display:none">
      <div class="ctitle"><h2>Battery history</h2><button class="btn ghost" onclick="clearBatHistory()">Clear</button></div>
      <p class="sub" id="bhSub">One reading every 2 hours for the last 7 days, kept in flash across restarts.</p>
      <div id="bh"></div>
      <div class="row" style="display:flex;gap:8px;margin-top:10px">
        <button class="btn sec" style="flex:1" onclick="exportBat('json')">Export JSON</button>
        <button class="btn sec" style="flex:1" onclick="exportBat('txt')">Export TXT</button>
      </div>
    </div>
  </section>

  <section class="view" id="v-wifi" role="tabpanel" aria-labelledby="t-wifi">
    <div class="card">
      <div class="wifinow">
        <div class="ico"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" aria-hidden="true"><path d="M4 9a11 11 0 0 1 16 0"></path><path d="M7 12.5a6.5 6.5 0 0 1 10 0"></path><path d="M10.2 16a2.6 2.6 0 0 1 3.6 0"></path><circle cx="12" cy="19.2" r="1.1" fill="currentColor" stroke="none"></circle></svg></div>
        <div><div class="ssid" id="wSsid">&hellip;</div><div class="meta" id="wMeta"></div></div>
      </div>
      <div class="wifinow" id="wApRow" style="display:none;margin-top:12px;padding-top:12px;border-top:1px solid var(--line)">
        <div class="ico"><svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" aria-hidden="true"><rect x="3" y="6" width="18" height="12" rx="2.5"></rect><path d="M7 10v4M11 9v6M15 10v4"></path></svg></div>
        <div><div class="ssid" id="wApSsid"></div><div class="meta" id="wApMeta"></div></div>
      </div>
    </div>
    <div class="card">
      <div class="ctitle"><h2>Available networks</h2><button class="btn ghost" id="scanBtn" onclick="scanWifi()">Scan</button></div>
      <div id="nets"><div class="empty">Tap Scan to look for networks in range.</div></div>
    </div>
    <div class="card">
      <div class="ctitle"><h2>Join manually</h2></div>
      <p class="sub">For hidden networks that do not show up in a scan.</p>
      <div class="field"><label for="ssid">Network name (SSID)</label><input id="ssid" autocomplete="off"></div>
      <div class="field"><label for="pass">Password</label><input id="pass" type="password" autocomplete="off"></div>
      <button class="btn block" onclick="saveWifi()">Save and reboot</button>
      <p class="mini">Credentials are kept in flash across reboots.</p>
    </div>
    <div class="card">
      <div class="ctitle"><h2>Access point password</h2></div>
      <p class="sub">Password for the board's own setup network ("<span class="apname">VELUX-WLI-Setup</span>"). It only runs while the board can't reach your home Wi-Fi.</p>
      <div class="field"><label for="appass">New password (min. 8 characters)</label><input id="appass" type="password" autocomplete="new-password"></div>
      <button class="btn block" onclick="saveApPassword()">Save and reboot</button>
      <button class="btn sec block" style="margin-top:8px" onclick="resetApPassword()">Reset to firmware default</button>
    </div>
  </section>

  <section class="view" id="v-adv" role="tabpanel" aria-labelledby="t-adv">
    <div class="card">
      <div class="ctitle"><h2>Firmware update</h2></div>
      <p class="sub">Flashes a new build over Wi-Fi. Saved Wi-Fi credentials, the Zigbee pairing and every setting are kept — they live in separate storage from the firmware itself.</p>
      <div class="field"><label for="fw">Compiled .bin (velux_wli_130_51_c6.bin from export_release.ps1)</label><input id="fw" type="file" accept=".bin"></div>
      <button class="btn block" id="fwBtn" onclick="uploadFirmware()">Upload and flash</button>
      <div id="fwProgWrap" style="display:none;margin-top:10px">
        <div style="background:var(--input);border-radius:999px;height:8px;overflow:hidden">
          <div id="fwBar" style="background:var(--grn);height:100%;width:0%;transition:width .15s"></div>
        </div>
        <p class="mini" id="fwStatus" role="status"></p>
      </div>
    </div>
    <div class="card">
      <div class="ctitle"><h2>Zigbee</h2></div>
      <p class="sub" id="zbState"></p>
      <button class="btn block" id="zbPairBtn" onclick="pairZigbee()">Start pairing</button>
      <button class="btn sec block" style="margin-top:8px" onclick="forgetZigbee()">Forget network</button>
      <p class="mini">Put ZHA (or Zigbee2MQTT) in permit join first, then press Start pairing. Holding the BOOT button for 3 s also forgets the network.</p>
    </div>
    <div class="card">
      <div class="ctitle"><h2>IR LED test</h2></div>
      <p class="sub">Keeps the infrared LED lit so you can check it and its aim with a phone camera (it shows as a bright violet-white dot). It switches itself off after 10 s.</p>
      <button class="btn block" id="irTestBtn" onclick="toggleIrTest()">Turn IR LED on</button>
    </div>
    <div class="card">
      <div class="ctitle"><h2>USB log</h2></div>
      <p class="sub">Debug messages on the USB port (115200 baud). While it is on the board can't use light sleep, so turn it off for battery operation. Applies after a restart.</p>
      <label class="check"><input type="checkbox" id="usbLog" onchange="setUsbLog(this.checked)"> <span id="usbLogTxt">USB log</span></label>
    </div>
    <div class="card">
      <div class="ctitle"><h2>Device</h2></div>
      <div id="dev"></div>
      <button class="btn sec block" style="margin-top:12px" onclick="rebootDevice()">Reboot</button>
    </div>
  </section>
</main>

<footer>ESP32-C6 &middot; IR 32.132&nbsp;kHz &middot; <span id="ver"></span> &middot; <a href="https://github.com/ypkdani00/velux_wli_130_51" target="_blank" rel="noopener" style="color:inherit">source on GitHub</a></footer>
</div>

<script>
// Column order MUST match kActions[] in main.cpp: open, stop, close.
var ICON=[
 '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 19V6"></path><path d="M6 11l6-6 6 6"></path></svg>',
 '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><rect x="6" y="6" width="12" height="12" rx="2.5"></rect></svg>',
 '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 5v13"></path><path d="M6 13l6 6 6-6"></path></svg>'];
var LABEL=['Open','Stop','Close'];
var st={}, offline=false, lastOk=0, panelsSig='', framesLoaded=false, framesLoading=false;

function $(id){return document.getElementById(id);}
function esc(s){return String(s).replace(/[&<>]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;'}[c];});}
function escAttr(s){return String(s).replace(/[&<>"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];});}

// Never throws: a network failure or a timeout comes back as {net:true,
// error}, so callers only ever check r.error. With power saving on, a
// healthy board can take ~0.5s to answer, hence the generous default.
async function api(u,m,b,ms){
  var o={method:m||'GET'},ctl=window.AbortController?new AbortController():null,t=0;
  if(b){o.headers={'Content-Type':'application/json'};o.body=JSON.stringify(b);}
  if(ctl){o.signal=ctl.signal;t=setTimeout(function(){ctl.abort();},ms||8000);}
  try{
    var r=await fetch(u,o);
    return await r.json().catch(function(){return r.ok?{}:{error:'HTTP '+r.status};});
  }catch(e){
    return {net:true,error:'The board did not answer ('+(e&&e.name==='AbortError'?'timed out':'network error')+').'};
  }finally{clearTimeout(t);}
}

// Tabs (ARIA tab pattern: click, or arrow keys / Home / End when focused).
function selectTab(b,focus){
  Array.prototype.forEach.call($('tabs').children,function(x){
    var on=x===b;
    x.classList.toggle('on',on);x.setAttribute('aria-selected',on?'true':'false');x.tabIndex=on?0:-1;
  });
  ['control','log','wifi','adv'].forEach(function(v){$('v-'+v).classList.toggle('on',v===b.dataset.v);});
  if(b.dataset.v==='log')loadBatHistory();   // fetched on demand, not on every poll
  if(focus)b.focus();
}

// Battery history (Log tab). Newest first; readings before the board's
// clock was set from the internet show time since that restart instead.
function fmtUp(s){var h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h>=24?Math.floor(h/24)+'d '+h%24+'h':h+'h '+(m<10?'0':'')+m+'m';}
function bhWhen(r){
  if(r.t)return new Date(r.t*1000).toLocaleString([],{weekday:'short',day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'});
  return 'restart + '+fmtUp(r.up);
}
// %/day over the current run (since the last restart), if it spans 4h+.
function bhRate(list){
  if(!list.length)return '';
  var end=list[list.length-1],start=end;
  for(var i=list.length-1;i>=0;i--){start=list[i];if(list[i].boot)break;}
  var span=end.up-start.up;
  if(span<4*3600)return '';
  var perDay=(start.pct-end.pct)/span*86400;
  return 'Drain since the last restart: about '+perDay.toFixed(1)+'% per day'
    +(perDay>0.05?' (~'+Math.round(end.pct/perDay)+' days left at this rate)':'')+'.';
}
var bhData=[];   // last history fetched, kept for the export
async function loadBatHistory(){
  if(!st.battery)return;
  var r=await api('/api/battery/history','GET',null,8000);
  if(!Array.isArray(r))return;
  bhData=r;
  var rate=bhRate(r);
  $('bhSub').textContent=(rate?rate+' ':'')+'One reading every 2 hours for the last 7 days, kept in flash across restarts.';
  if(!r.length){$('bh').innerHTML='<div class="empty">No readings yet.</div>';return;}
  $('bh').innerHTML=r.slice().reverse().map(function(x){
    return '<div class="kv"><span class="k">'+esc(bhWhen(x))+(x.boot?' · restart':'')+'</span>'
      +'<span class="v">'+x.pct+'% · '+(x.mv/1000).toFixed(2)+' V</span></div>';
  }).join('');
}
// Battery history as a file: JSON (oldest first, one object per reading with
// the Unix time, the ISO time, the percentage and the voltage) or text.
function exportBat(kind){
  if(!bhData.length){alert('The battery history is empty: nothing to export yet.');return;}
  var rows=bhData.map(function(x){
    return {time:x.t?new Date(x.t*1000).toISOString():'',unix:x.t||0,secondsSinceRestart:x.up,
            percent:x.pct,millivolts:x.mv,restart:!!x.boot};
  });
  var body,mime;
  if(kind==='json'){
    body=JSON.stringify({device:'VELUX WLI 130 IR',firmware:st.version||'',exported:new Date().toISOString(),battery:rows},null,2);
    mime='application/json';
  }else{
    body=bhData.map(function(x){
      return bhWhen(x)+'  '+x.pct+'%  '+(x.mv/1000).toFixed(2)+' V'+(x.boot?'  (restart)':'');
    }).join('\n')+'\n';
    mime='text/plain';
  }
  saveFile('velux-battery',kind,body,mime);
}
async function clearBatHistory(){
  if(!confirm('Delete the stored battery history? A fresh reading is taken again every 2 hours.'))return;
  var r=await api('/api/battery/history/clear','POST');
  if(r.error){alert(r.error);return;}
  loadBatHistory();
}
$('tabs').addEventListener('click',function(e){
  var b=e.target.closest('button[data-v]');
  if(b)selectTab(b);
});
$('tabs').addEventListener('keydown',function(e){
  var tabs=Array.prototype.slice.call($('tabs').children),i=tabs.indexOf(document.activeElement);
  if(i<0)return;
  var n={ArrowRight:i+1,ArrowLeft:i-1,Home:0,End:tabs.length-1}[e.key];
  if(n===undefined)return;
  e.preventDefault();
  selectTab(tabs[(n+tabs.length)%tabs.length],true);
});

function buttons(pi,mi,ok,name){
  var h='';
  for(var a=0;a<3;a++)
    h+='<button class="ib'+(a===1?' stop':'')+'" title="'+LABEL[a]+'" aria-label="'+escAttr(LABEL[a]+' '+name)+'"'
      +(ok?'':' disabled')+' onclick="send(this,'+pi+','+mi+','+a+')">'+ICON[a]+'</button>';
  return h;
}
// Rebuilt only when the panel list itself changes: redrawing the buttons on
// every poll could swallow a tap that lands mid-redraw, and would wipe the
// sending/sent feedback on them.
function renderPanels(){
  var panels=st.panels||[],sig=JSON.stringify(panels);
  if(sig===panelsSig)return;
  panelsSig=sig;
  var h='';
  panels.forEach(function(p,pi){
    h+='<div class="card"><div class="ctitle"><h2>'+esc(p.name)+'</h2>'
      +'<span class="sec'+(p.ok?'':' bad')+'">'+esc(p.sec)+'</span></div>'
      +'<p class="sub">3 motors &middot; keypad '+(pi+1)+'</p>'
      +'<div class="cols" aria-hidden="true"><div></div><div class="colhdr">Open</div><div class="colhdr">Stop</div><div class="colhdr">Close</div></div>';
    (p.motors||[]).forEach(function(m,mi){
      h+='<div class="row"><div class="lbl">'+esc(m)+'</div>'+buttons(pi,mi,p.ok,m)+'</div>';
    });
    h+='<div class="row all"><div class="lbl">All three</div>'+buttons(pi,3,p.ok,'all three, '+p.name)
      +'<div class="pmsg" id="pmsg'+pi+'" role="status" style="display:none"></div></div></div>';
  });
  $('panels').innerHTML=h;
  var bad=panels.filter(function(p){return !p.ok;}).map(function(p){return p.name;});
  var b=$('bad');
  b.style.display=bad.length?'flex':'none';
  if(bad.length)b.innerHTML='<b>!</b><div>Invalid security code for <b>'+esc(bad.join(', '))
    +'</b> &mdash; <code>kPanels[]</code> needs exactly ten 0/1 digits. Commands are blocked.</div>';
}
function renderLog(){
  var lines=st.log||[];
  $('logCount').textContent=lines.length?(lines.length+(lines.length===1?' entry':' entries')):'No entries yet';
  if(!lines.length){$('log').innerHTML='<div class="empty">Nothing transmitted yet.</div>';return;}
  var h='',times=st.logTimes||[];
  for(var i=lines.length-1;i>=0;i--){
    var l=lines[i];
    // "TAG  small line  main line": TX frames, ZB Zigbee commands, BAT battery readings, --/OK/!! notes
    var sp=l.indexOf(' '),dir=sp>0?l.slice(0,sp):l.slice(0,2),rest=l.slice(dir.length).trim();
    var cls=dir==='ZB'?' zb':dir==='BAT'?' bat':'';
    var cut=rest.indexOf('  ');
    var frame=cut>0?rest.slice(0,cut):rest, txt=cut>0?rest.slice(cut).trim():'';
    // When it happened: board clock (Rome time), "~" = not confirmed by NTP since boot
    var when=times[i]?'<div class="when">'+esc(times[i])+'</div>':'';
    h+='<div class="logrow"><span class="tag'+cls+'">'+esc(dir)+'</span><div>'
      +'<div class="txt">'+esc(txt||frame)+'</div><div class="frame">'+esc(frame)+'</div>'+when+'</div></div>';
  }
  $('log').innerHTML=h;
}
// Saves the frame log shown here as a file: JSON (one object per entry, oldest
// first) or plain text (one line per entry). Built in the browser from the
// last status the board sent - nothing extra is asked of the board.
function logEntries(){
  var lines=st.log||[],times=st.logTimes||[],out=[];
  for(var i=0;i<lines.length;i++){
    var l=lines[i],sp=l.indexOf(' '),dir=sp>0?l.slice(0,sp):l.slice(0,2),rest=l.slice(dir.length).trim();
    var cut=rest.indexOf('  ');
    out.push({time:times[i]||'',type:dir,frame:cut>0?rest.slice(0,cut):rest,text:cut>0?rest.slice(cut).trim():'',line:l});
  }
  return out;
}
function exportLog(kind){
  var e=logEntries();
  if(!e.length){alert('The log is empty: nothing to export yet.');return;}
  var body,mime;
  if(kind==='json'){
    body=JSON.stringify({device:'VELUX WLI 130 IR',firmware:st.version||'',exported:new Date().toISOString(),log:e},null,2);
    mime='application/json';
  }else{
    body=e.map(function(x){return (x.time?x.time+'  ':'')+x.line;}).join('\n')+'\n';
    mime='text/plain';
  }
  saveFile('velux-log',kind,body,mime);
}
// Downloads text as "<base>-YYYYMMDD-HHMMSS.json|txt".
function saveFile(base,kind,body,mime){
  var d=new Date(),p=function(n){return (n<10?'0':'')+n;};
  var name=base+'-'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'-'+p(d.getHours())+p(d.getMinutes())+p(d.getSeconds())+'.'+(kind==='json'?'json':'txt');
  var a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([body],{type:mime+';charset=utf-8'}));
  a.download=name;
  document.body.appendChild(a);a.click();document.body.removeChild(a);
  setTimeout(function(){URL.revokeObjectURL(a.href);},1000);
}
function renderWifi(){
  $('wSsid').textContent=st.staConnected?(st.ssid||'(connected)'):'(no network saved)';
  $('wMeta').textContent=st.staConnected?((st.ip||'')+' · '+(st.rssi||'')):(st.apActive?'reachable only via the access point below':'');
  var row=$('wApRow');
  if(st.apActive){
    row.style.display='flex';
    $('wApSsid').textContent=st.apSsid||'';
    $('wApMeta').textContent=(st.apIp||'')+' · setup fallback';
  }else{
    row.style.display='none';
  }
}
function host(){return (st.hostname||'velux')+'.local';}
function renderZigbee(){
  var e=$('zbState');
  if(!e)return;
  var joined=st.zigbee==='joined';
  e.textContent='Status: '+(st.zigbee||'');
  var b=$('zbPairBtn');
  b.textContent=joined?'Paired':(st.zigbeePairing?'Pairing…':'Start pairing');
  b.disabled=joined||!!st.zigbeePairing;
}
async function pairZigbee(){
  var r=await api('/api/zigbeepair','POST');
  if(r.error){alert(r.error);return;}
  if(r.note){alert(r.note);return;}
  alert('Pairing started. The board looks for the network; in the coordinator it appears as "VELUX WLI 130 IR".');
}
async function forgetZigbee(){
  if(!confirm('Forget the Zigbee network? The board leaves it and has to be paired again.'))return;
  var r=await api('/api/zigbeeforget','POST');
  if(r.error){alert(r.error);return;}
  alert('Done. If the board was paired it restarts now.');
}
function renderIrTest(){
  var b=$('irTestBtn');
  if(b)b.textContent=st.irTest?'Turn IR LED off':'Turn IR LED on';
}
async function toggleIrTest(){
  var b=$('irTestBtn');b.disabled=true;
  var r=await api('/api/irtest','POST',{enabled:!st.irTest});
  b.disabled=false;
  if(r.error){alert(r.error);return;}
  st.irTest=!!r.irTest;
  renderIrTest();
}
// USB log switch: shows the stored setting; "running" differs until the next restart.
function renderUsbLog(){
  var cb=$('usbLog');
  if(!cb||document.activeElement===cb)return;
  cb.checked=!!st.usbLogSaved;
  var pend=!!st.usbLogSaved!==!!st.usbLog;
  $('usbLogTxt').textContent=(st.usbLogSaved?'USB log on':'USB log off')+(pend?' — restart to apply':'');
}
async function setUsbLog(on){
  var r=await api('/api/usblog','POST',{enabled:on});
  if(r.error){alert(r.error);return;}
  st.usbLogSaved=on;
  renderUsbLog();
  if(confirm('Saved. Restart the board now to apply it?'))doReboot();
}
function renderDev(){
  var rows=[
    ['Firmware',st.version||''],['Hostname',host()],
    ['IP address',st.ip||'—']
  ];
  if(st.apActive)rows.push(['AP address',(st.apIp||'')+' ('+(st.apSsid||'')+')']);
  var b=st.battery;
  if(b){
    rows.push(['Battery',b.percent+'% · '+Number(b.voltage).toFixed(2)+' V'],
              ['Battery left (est.)',b.remainingMah+' / '+b.capacityMah+' mAh']);
  }
  rows.push(
    ['MAC address',st.mac||''],['Mode',st.mode||''],
    ['Wi-Fi signal',st.rssi||''],['Wi-Fi power save',st.wifiPowerSave||''],
    ['Power management',st.powerMgmt||''],['Zigbee',st.zigbee||''],
    ['Wi-Fi','on (mode switch) - flip it off for Zigbee-only low power'],
    ['CPU frequency',(st.cpuMhz||'?')+' MHz'],
    ['Free heap',fmtBytes(st.heapFree)],['Min free heap ever',fmtBytes(st.heapMin)],
    ['Uptime',st.uptime||''],['Last boot reason',st.resetReason||'']
  );
  $('dev').innerHTML=rows.map(function(r){
    return '<div class="kv"><span class="k">'+esc(r[0])+'</span><span class="v">'+esc(r[1])+'</span></div>';
  }).join('');
}
function fmtBytes(n){return (typeof n==='number')?Math.round(n/1024)+' KB':'';}
async function rebootDevice(){
  if(!confirm('Reboot the board now? It will be briefly unreachable (Wi-Fi and IR) for a few seconds while it restarts.'))return;
  doReboot();
}
async function doReboot(){
  var r=await api('/api/reboot','POST');
  if(r.error){alert(r.error);return;}
  alert('Rebooting…');
}
function ago(t){var s=Math.round((Date.now()-t)/1000);return s<90?s+'s':Math.round(s/60)+' min';}
// Connection pill + subtitle. "Unreachable" = this page can't reach the
// board (last known data stays on screen); the rest is the board's own view.
function renderConn(){
  var txt,cls='';
  if(offline){txt='Unreachable';cls=' bad';}
  else if(st.staConnected)txt='Online';
  else if(st.apActive){txt='Setup mode';cls=' warn';}
  else{txt='Offline';cls=' warn';}
  $('conn').className='pill'+cls;
  $('connTxt').textContent=txt;
  $('dl').textContent=host()+' · '+(offline?(lastOk?'no answer for '+ago(lastOk):'no answer yet'):(st.ip||st.apIp||'…'));
}
function render(){
  renderConn();
  $('ver').textContent='firmware '+(st.version||'');
  if(st.apName)Array.prototype.forEach.call(document.querySelectorAll('.apname'),function(el){el.textContent=st.apName;});
  renderBattery();renderPanels();renderLog();renderWifi();renderDev();renderUsbLog();renderIrTest();renderZigbee();
}
// Header chip. Hidden until the firmware has a battery reading.
function renderBattery(){
  var b=st.battery,el=$('bat');
  $('bhCard').style.display=b?'':'none';
  if(!b){el.style.display='none';return;}
  var p=b.percent;
  el.style.display='inline-flex';
  el.className='bat'+(p<=10?' crit':p<=20?' low':'');
  $('batFill').setAttribute('width',p>0?Math.max(1.5,15*p/100):0);
  $('batTxt').textContent=p+'%';
  el.title=Number(b.voltage).toFixed(2)+' V · about '+b.remainingMah+' mAh left';
}

function panelMsg(p,kind,text){
  var c=$('pmsg'+p);
  if(!c){alert(text);return;}
  c.className='pmsg '+kind;c.textContent=text;c.style.display='block';
  clearTimeout(c._t);c._t=setTimeout(function(){c.style.display='none';},kind==='err'?6000:4500);
}
// The button pulses while the command is on its way (with power saving on
// that can take ~0.5s, long enough to invite a second tap) and flashes green
// once the board confirms it transmitted the frame.
async function send(btn,p,m,a){
  if(btn.classList.contains('busy'))return;
  btn.classList.add('busy');btn.disabled=true;
  var r=await api('/api/send?panel='+p+'&motor='+m+'&action='+a,'POST');
  btn.classList.remove('busy');btn.disabled=false;
  if(r.error){
    panelMsg(p,'err',r.error);
  }else{
    btn.classList.add('done');setTimeout(function(){btn.classList.remove('done');},900);
    if(r.autoStop)panelMsg(p,'warn','Still moving the other way: STOP sent first, then '+LABEL[a].toLowerCase()+'.');
  }
  refresh();
}
async function sendHex(){
  var r=await api('/api/sendhex?v='+encodeURIComponent($('hex').value),'POST');
  if(r.error)alert(r.error);else $('hex').value='';
  refresh();
}
async function clearLog(){
  var r=await api('/api/log/clear','POST');
  if(r.error)alert(r.error);
  refresh();
}
// Reference frames for the raw-frame guide, built by the firmware from the
// security codes in Config.h, so they can't drift from what the buttons send.
async function loadFrames(){
  if(framesLoading)return;
  framesLoading=true;
  var r=await api('/api/frames','GET',null,6000);
  framesLoading=false;
  if(!Array.isArray(r))return;   // retried after the next successful poll
  framesLoaded=true;
  var h='';
  r.forEach(function(p){
    if(!p.ok)return;
    h+='<p class="mini" style="margin-top:12px"><b>'+esc(p.name)+'</b></p>';
    [p.motor,'All three'].forEach(function(lbl,m){
      for(var a=0;a<3;a++)
        h+='<div class="kv"><span class="k">'+esc(lbl)+' · '+LABEL[a].toLowerCase()+'</span><span class="v">'+esc(p.frames[m*3+a])+'</span></div>';
    });
  });
  $('frames').innerHTML=h||'<div class="empty">No valid security code in Config.h.</div>';
}
function uploadFirmware(){
  var input=$('fw'), file=input.files&&input.files[0];
  if(!file){alert('Choose a .bin file first (velux_wli_130_51_c6.bin, built by export_release.ps1).');return;}
  if(!confirm('Flash "'+file.name+'" ('+Math.round(file.size/1024)+' KB) onto the board?\n\nWi-Fi credentials and settings are kept. The board reboots automatically when done.'))return;

  var btn=$('fwBtn'), wrap=$('fwProgWrap'), bar=$('fwBar'), status=$('fwStatus');
  btn.disabled=true; input.disabled=true;
  wrap.style.display='block'; bar.style.width='0%'; status.textContent='Uploading…';

  var fd=new FormData(); fd.append('firmware',file);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/update');
  xhr.upload.onprogress=function(e){
    if(!e.lengthComputable)return;
    var pct=Math.round(e.loaded/e.total*100);
    bar.style.width=pct+'%';
    status.textContent='Uploading… '+pct+'%';
  };
  xhr.onload=function(){
    if(xhr.status===200){
      bar.style.width='100%';
      status.textContent='Flashed. Rebooting… this page will stop responding for about 15s.';
    }else{
      var msg='Update failed.';
      try{ msg=JSON.parse(xhr.responseText).error||msg; }catch(e){}
      status.textContent=msg;
      btn.disabled=false; input.disabled=false;
    }
  };
  xhr.onerror=function(){
    status.textContent='Upload failed (connection lost).';
    btn.disabled=false; input.disabled=false;
  };
  xhr.send(fd);
}
async function saveWifi(){
  var s=$('ssid').value,p=$('pass').value;
  if(!s){alert('Enter a network name first.');return;}
  if(p.length&&(p.length<8||p.length>64)){alert('A Wi-Fi password is 8-63 characters (leave it empty for an open network).');return;}
  var r=await api('/api/wifi','POST',{ssid:s,pass:p});
  if(r.error){alert(r.error);return;}
  alert('Saved. The board will reboot and join "'+s+'".');
}
async function saveApPassword(){
  var p=$('appass').value;
  if(p.length<8){alert('Enter at least 8 characters.');return;}
  if(p.length>63){alert('At most 63 characters.');return;}
  var r=await api('/api/appassword','POST',{pass:p});
  if(r.error){alert(r.error);return;}
  $('appass').value='';
  alert('Saved. The board will reboot.');
}
async function resetApPassword(){
  if(!confirm('Reset the access point password to the firmware default?'))return;
  var r=await api('/api/appassword','POST',{pass:''});
  if(r.error){alert(r.error);return;}
  alert('Reset. The board will reboot.');
}
function barsHtml(rssi){
  var lvl=rssi>=-55?4:rssi>=-67?3:rssi>=-78?2:1,h='';
  for(var i=1;i<=4;i++)h+='<i class="'+(i<=lvl?'on':'')+'" style="height:'+(i*3+2)+'px"></i>';
  return '<span class="bars" aria-hidden="true">'+h+'</span>';
}
async function scanWifi(){
  var btn=$('scanBtn'),list=$('nets');
  btn.disabled=true;btn.textContent='Scanning…';
  list.innerHTML='<div class="empty">Scanning…</div>';
  try{
    var nets=await api('/api/wifiscan','GET',null,20000);   // the scan itself takes a few seconds
    if(nets.error){list.innerHTML='<div class="empty">'+esc(nets.error)+'</div>';}
    else if(!Array.isArray(nets)||!nets.length){list.innerHTML='<div class="empty">No networks found.</div>';}
    else{
      list.innerHTML=nets.map(function(n){
        return '<button type="button" class="net" data-ssid="'+escAttr(n.ssid)+'" data-secure="'+(n.secure?1:0)+'">'
          +'<span><span class="ssid">'+esc(n.ssid)+'</span><br><span class="enc">'+(n.secure?'WPA2 protected':'Open')+'</span></span>'
          +'<span class="right"><span class="rssi">'+n.rssi+' dBm</span>'+barsHtml(n.rssi)+'</span></button>';
      }).join('');
    }
  }finally{btn.disabled=false;btn.textContent='Scan';}
}
async function pickWifi(ssid,secure){
  $('ssid').value=ssid;
  var pass='';
  if(secure){var p=prompt('Password for "'+ssid+'":');if(p===null)return;pass=p;}
  $('pass').value=pass;
  await saveWifi();
}
$('nets').addEventListener('click',function(e){
  var row=e.target.closest('.net');
  if(row)pickWifi(row.dataset.ssid,row.dataset.secure==='1');
});

// Status polling. One request at a time (a slow answer never piles up a
// second one), paced by how recently the page was used, paused while hidden.
var lastInput=Date.now(),polling=false,again=false,pollTimer=0;
function pollDelay(){var idle=Date.now()-lastInput;return idle<60000?2500:idle<300000?10000:60000;}
function schedule(){clearTimeout(pollTimer);if(!document.hidden)pollTimer=setTimeout(refresh,pollDelay());}
async function refresh(){
  clearTimeout(pollTimer);
  if(polling){again=true;return;}
  polling=true;
  var r=await api('/api/status','GET',null,6000);
  polling=false;
  if(r.net){offline=true;renderConn();}
  else if(!r.error){st=r;offline=false;lastOk=Date.now();render();if(!framesLoaded)loadFrames();}
  if(again){again=false;refresh();}else schedule();
}
function poke(){var wasSlow=pollDelay()>2500;lastInput=Date.now();if(wasSlow)refresh();}
['pointerdown','keydown'].forEach(function(ev){document.addEventListener(ev,poke,{passive:true});});
document.addEventListener('visibilitychange',function(){
  if(document.hidden)clearTimeout(pollTimer);
  else{lastInput=Date.now();refresh();}
});
refresh();
</script></body></html>)PAGE";
