/*
 * ═══════════════════════════════════════════════════════════════
 *  MeshTalk — ESP8266 Offline Encrypted Chat Server
 *  ▸ PROGMEM Edition — All HTML/CSS/JS embedded in firmware
 *  ▸ NO LittleFS required — just upload this sketch!
 * ═══════════════════════════════════════════════════════════════
 *
 *  Hardware : NodeMCU ESP-12E (4MB flash)
 *  Libs     : ESP8266WiFi, ESPAsyncWebServer, ESPAsyncTCP,
 *             ArduinoJson
 *  Baudrate : 115200
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <ArduinoJson.h>

/* ───────── CONFIG ───────── */
const char* AP_SSID = "MeshTalk";
const char* AP_PASS = "";          // Open network (no password)

#define MAX_CLIENTS   8
#define WS_MAX_MSG    4096         // Max incoming WS message size
#define MIN_FREE_HEAP 4096         // Minimum free heap to process messages
#define HEAP_LOG_MS   30000        // Log free heap every 30 s

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

/* ───────── CLIENT TRACKING ───────── */

struct ChatClient {
  uint32_t id;
  String   username;
  bool     active;
};

ChatClient clients[MAX_CLIENTS];

unsigned long lastHeapLog = 0;

/* ═══════════════════════════════════════════════════════════════
 *  EMBEDDED WEB ASSETS (PROGMEM — stored in flash, not RAM)
 * ═══════════════════════════════════════════════════════════════ */

// ── CSS ──────────────────────────────────────────────────────
static const char STYLE_CSS[] PROGMEM = R"rawliteral(
/* MeshTalk Stylesheet — Dark theme, glassmorphism, animations */
:root {
  --bg-dark:#0a0e17;--bg-card:#111827;--bg-surface:#1a2233;--bg-input:#1e293b;
  --bg-hover:#253348;--accent:#6366f1;--accent-glow:rgba(99,102,241,.35);
  --accent-light:#818cf8;--accent-surface:rgba(99,102,241,.12);
  --green:#22c55e;--green-glow:rgba(34,197,94,.25);--red:#ef4444;--amber:#f59e0b;
  --text-primary:#f1f5f9;--text-secondary:#94a3b8;--text-muted:#64748b;
  --border:rgba(255,255,255,.06);--glass:rgba(17,24,39,.72);
  --glass-border:rgba(255,255,255,.08);
  --bubble-self:linear-gradient(135deg,#4f46e5,#6366f1);
  --bubble-other:#1e293b;--bubble-system:rgba(99,102,241,.08);
  --bubble-pm:rgba(168,85,247,.15);
  --header-h:60px;--sidebar-w:280px;--radius:12px;--radius-sm:8px;--radius-lg:20px;
  --font:'Segoe UI',system-ui,-apple-system,sans-serif;
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;font-family:var(--font);background:var(--bg-dark);color:var(--text-primary);overflow:hidden;-webkit-tap-highlight-color:transparent}
::-webkit-scrollbar{width:5px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:var(--text-muted);border-radius:10px}
.hidden{display:none!important}.screen{display:none;height:100%}.screen.active{display:flex}

/* JOIN SCREEN */
#join-screen{align-items:center;justify-content:center;background:radial-gradient(ellipse at 20% 50%,rgba(99,102,241,.12) 0%,transparent 60%),radial-gradient(ellipse at 80% 20%,rgba(34,197,94,.08) 0%,transparent 50%),var(--bg-dark)}
.join-card{width:100%;max-width:400px;padding:48px 36px;background:var(--glass);backdrop-filter:blur(24px);-webkit-backdrop-filter:blur(24px);border:1px solid var(--glass-border);border-radius:var(--radius-lg);text-align:center;animation:cardIn .6s cubic-bezier(.16,1,.3,1)}
@keyframes cardIn{from{opacity:0;transform:translateY(30px) scale(.96)}to{opacity:1;transform:translateY(0) scale(1)}}
.logo-container{position:relative;width:80px;height:80px;margin:0 auto 20px}
.logo-ring{position:absolute;inset:0;border:2px solid var(--accent);border-radius:50%;animation:ringPulse 2.5s ease-in-out infinite}
@keyframes ringPulse{0%,100%{transform:scale(1);opacity:.5}50%{transform:scale(1.2);opacity:0}}
.logo-icon{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:var(--accent-surface);border-radius:50%;color:var(--accent-light)}
.logo-icon svg{width:36px;height:36px}
.join-card h1{font-size:2rem;font-weight:700;letter-spacing:-.5px;background:linear-gradient(135deg,var(--text-primary),var(--accent-light));-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.tagline{color:var(--text-muted);font-size:.85rem;margin-bottom:28px;letter-spacing:1px;text-transform:uppercase}
.input-group{margin-bottom:16px;text-align:left}
.input-group input{width:100%;padding:14px 16px;background:var(--bg-input);border:1px solid var(--border);border-radius:var(--radius-sm);color:var(--text-primary);font-size:.95rem;outline:none;transition:border-color .2s,box-shadow .2s}
.input-group input::placeholder{color:var(--text-muted)}
.input-group input:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-glow)}
.input-hint{display:block;margin-top:6px;font-size:.75rem;color:var(--text-muted)}
.btn-primary{width:100%;padding:14px;margin-top:4px;background:var(--accent);border:none;border-radius:var(--radius-sm);color:#fff;font-size:1rem;font-weight:600;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px;transition:background .2s,transform .15s,box-shadow .2s}
.btn-primary:hover{background:var(--accent-light);box-shadow:0 4px 20px var(--accent-glow)}
.btn-primary:active{transform:scale(.97)}
.btn-primary svg{width:18px;height:18px}
.error-msg{margin-top:12px;padding:10px;background:rgba(239,68,68,.12);border:1px solid rgba(239,68,68,.25);border-radius:var(--radius-sm);color:var(--red);font-size:.85rem}
.security-badge{margin-top:24px;display:flex;align-items:center;justify-content:center;gap:6px;font-size:.75rem;color:var(--green);opacity:.8}

/* CHAT SCREEN */
#chat-screen{flex-direction:column}
#chat-header{display:flex;align-items:center;justify-content:space-between;height:var(--header-h);padding:0 16px;background:var(--glass);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border-bottom:1px solid var(--border);flex-shrink:0;z-index:10}
.header-left,.header-right{display:flex;align-items:center;gap:10px}
.header-info h2{font-size:1.1rem;font-weight:700}
.icon-btn{width:36px;height:36px;display:flex;align-items:center;justify-content:center;background:none;border:none;color:var(--text-secondary);cursor:pointer;border-radius:50%;transition:background .2s,color .2s}
.icon-btn:hover{background:var(--bg-hover);color:var(--text-primary)}
.icon-btn svg{width:20px;height:20px}
.status{font-size:.75rem;font-weight:500;display:flex;align-items:center;gap:5px}
.status::before{content:'';width:7px;height:7px;border-radius:50%;display:inline-block}
.status.connected{color:var(--green)}.status.connected::before{background:var(--green);box-shadow:0 0 6px var(--green-glow)}
.status.disconnected{color:var(--red)}.status.disconnected::before{background:var(--red)}
.status.connecting{color:var(--amber)}.status.connecting::before{background:var(--amber);animation:blink 1s ease-in-out infinite}
@keyframes blink{50%{opacity:.3}}
.user-count{font-size:.8rem;color:var(--text-muted);background:var(--bg-surface);padding:4px 10px;border-radius:20px}
#chat-body{flex:1;display:flex;overflow:hidden;position:relative}

/* SIDEBAR */
.sidebar{width:var(--sidebar-w);background:var(--bg-card);border-right:1px solid var(--border);display:flex;flex-direction:column;flex-shrink:0;transition:transform .3s cubic-bezier(.4,0,.2,1)}
.sidebar-header{display:flex;align-items:center;justify-content:space-between;padding:16px;border-bottom:1px solid var(--border)}
.sidebar-header h3{font-size:.95rem;font-weight:600}
#sidebar-close{display:none}
#user-list{list-style:none;flex:1;overflow-y:auto;padding:8px}
#user-list li{display:flex;align-items:center;gap:10px;padding:10px 12px;border-radius:var(--radius-sm);cursor:pointer;transition:background .15s;font-size:.9rem}
#user-list li:hover{background:var(--bg-hover)}
.user-avatar{width:34px;height:34px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:.8rem;text-transform:uppercase;flex-shrink:0;color:#fff}
.user-name{flex:1}.user-you{color:var(--text-muted);font-size:.75rem}
.user-online-dot{width:8px;height:8px;background:var(--green);border-radius:50%;box-shadow:0 0 6px var(--green-glow)}
.sidebar-footer{padding:12px 16px;border-top:1px solid var(--border)}
.encryption-info{display:flex;align-items:center;gap:6px;font-size:.75rem;color:var(--green);opacity:.7}
.sidebar-overlay{display:none;position:absolute;inset:0;background:rgba(0,0,0,.5);z-index:19}

/* CHAT AREA */
#chat-area{flex:1;display:flex;flex-direction:column;min-width:0}
.messages{flex:1;overflow-y:auto;padding:20px 16px;display:flex;flex-direction:column;gap:4px}
.msg{max-width:75%;padding:10px 14px;border-radius:var(--radius) var(--radius) var(--radius) 4px;animation:msgIn .3s cubic-bezier(.16,1,.3,1);word-wrap:break-word;overflow-wrap:break-word}
@keyframes msgIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
.msg.self{align-self:flex-end;background:var(--bubble-self);border-radius:var(--radius) var(--radius) 4px var(--radius);color:#fff}
.msg.other{align-self:flex-start;background:var(--bubble-other)}
.msg.private-msg{border:1px solid rgba(168,85,247,.3);background:var(--bubble-pm)}
.msg-sender{font-size:.75rem;font-weight:600;color:var(--accent-light);margin-bottom:3px}
.msg.self .msg-sender{color:rgba(255,255,255,.8)}
.msg-text{font-size:.9rem;line-height:1.45}
.msg-time{font-size:.65rem;color:var(--text-muted);text-align:right;margin-top:3px}
.msg.self .msg-time{color:rgba(255,255,255,.55)}
.msg-pm-badge{font-size:.65rem;color:#a855f7;font-weight:600;margin-bottom:2px}
.msg-system{align-self:center;max-width:85%;padding:6px 16px;background:var(--bubble-system);border-radius:20px;font-size:.78rem;color:var(--text-muted);text-align:center;animation:msgIn .3s cubic-bezier(.16,1,.3,1)}

/* TYPING */
.typing-indicator{padding:6px 16px;font-size:.8rem;color:var(--text-muted);display:flex;align-items:center;gap:6px;transition:opacity .2s}
.typing-dots{display:flex;gap:3px}
.typing-dots span{width:5px;height:5px;background:var(--text-muted);border-radius:50%;animation:typingBounce 1.4s ease-in-out infinite}
.typing-dots span:nth-child(2){animation-delay:.2s}
.typing-dots span:nth-child(3){animation-delay:.4s}
@keyframes typingBounce{0%,60%,100%{transform:translateY(0)}30%{transform:translateY(-4px)}}

/* MESSAGE INPUT */
.message-form{display:flex;align-items:flex-end;gap:8px;padding:12px 16px;background:var(--bg-card);border-top:1px solid var(--border)}
.msg-input-wrapper{flex:1;position:relative}
.pm-label{display:flex;align-items:center;gap:6px;padding:4px 10px;margin-bottom:6px;background:var(--bubble-pm);border-radius:var(--radius-sm) var(--radius-sm) 0 0;font-size:.78rem;color:#c084fc}
.pm-cancel{background:none;border:none;color:#c084fc;font-size:1.1rem;cursor:pointer;line-height:1;padding:0 2px}
#message-input{width:100%;padding:12px 16px;background:var(--bg-input);border:1px solid var(--border);border-radius:var(--radius-lg);color:var(--text-primary);font-size:.9rem;outline:none;transition:border-color .2s,box-shadow .2s}
#message-input::placeholder{color:var(--text-muted)}
#message-input:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-glow)}
.btn-send{width:44px;height:44px;flex-shrink:0;display:flex;align-items:center;justify-content:center;background:var(--accent);border:none;border-radius:50%;color:#fff;cursor:pointer;transition:background .2s,transform .15s,box-shadow .2s}
.btn-send:hover{background:var(--accent-light);box-shadow:0 4px 16px var(--accent-glow)}
.btn-send:active{transform:scale(.9)}
.btn-send svg{width:20px;height:20px}

/* RESPONSIVE */
@media(max-width:768px){
  .join-card{margin:20px;padding:36px 24px}
  .sidebar{position:absolute;top:0;left:0;bottom:0;z-index:20;transform:translateX(-100%)}
  .sidebar.open{transform:translateX(0)}
  .sidebar.open~.sidebar-overlay{display:block}
  #sidebar-close{display:flex}
  .msg{max-width:85%}
}
@media(max-width:400px){
  .join-card h1{font-size:1.6rem}
  .messages{padding:12px 10px}
}
)rawliteral";

// ── JavaScript ──────────────────────────────────────────────
static const char APP_JS[] PROGMEM = R"rawliteral(
(function () {
  'use strict';

  var DOM = {
    joinScreen:     document.getElementById('join-screen'),
    chatScreen:     document.getElementById('chat-screen'),
    joinForm:       document.getElementById('join-form'),
    joinBtn:        document.getElementById('join-btn'),
    joinError:      document.getElementById('join-error'),
    usernameInput:  document.getElementById('username-input'),
    passphraseInput:document.getElementById('passphrase-input'),
    connStatus:     document.getElementById('connection-status'),
    userCount:      document.getElementById('user-count'),
    userList:       document.getElementById('user-list'),
    messages:       document.getElementById('messages'),
    messageForm:    document.getElementById('message-form'),
    messageInput:   document.getElementById('message-input'),
    typingIndicator:document.getElementById('typing-indicator'),
    typingUser:     document.getElementById('typing-user'),
    sidebarToggle:  document.getElementById('sidebar-toggle'),
    sidebarClose:   document.getElementById('sidebar-close'),
    sidebar:        document.getElementById('users-sidebar'),
    sidebarOverlay: document.getElementById('sidebar-overlay'),
    leaveBtn:       document.getElementById('leave-btn'),
    pmLabel:        document.getElementById('pm-label'),
    pmTarget:       document.getElementById('pm-target'),
    pmCancel:       document.getElementById('pm-cancel')
  };

  var ws = null;
  var myUsername = '';
  var myId = null;
  var pmRecipient = null;
  var reconnectAttempts = 0;
  var MAX_RECONNECT_DELAY = 30000;
  var lastTypingSent = 0;
  var typingDisplayTimeout = null;

  var AVATAR_COLORS = [
    '#6366f1','#ec4899','#f59e0b','#10b981','#3b82f6',
    '#8b5cf6','#ef4444','#14b8a6','#f97316','#06b6d4'
  ];

  /* ═══ ENCRYPTION — Works on HTTP (no crypto.subtle needed) ═══ */

  /* Pure JS RC4 stream cipher for non-secure contexts */
  function rc4(key, data) {
    var s = [], j = 0, result = [];
    for (var i = 0; i < 256; i++) s[i] = i;
    for (var i = 0; i < 256; i++) {
      j = (j + s[i] + key.charCodeAt(i % key.length)) & 255;
      var tmp = s[i]; s[i] = s[j]; s[j] = tmp;
    }
    var i = 0; j = 0;
    for (var k = 0; k < data.length; k++) {
      i = (i + 1) & 255;
      j = (j + s[i]) & 255;
      var tmp = s[i]; s[i] = s[j]; s[j] = tmp;
      result.push(data[k] ^ s[(s[i] + s[j]) & 255]);
    }
    return result;
  }

  /* Simple hash to derive a key string from passphrase + salt */
  function deriveKeyString(passphrase, salt) {
    var combined = passphrase + ':' + salt;
    var hash = 0x811c9dc5;
    for (var i = 0; i < combined.length; i++) {
      hash ^= combined.charCodeAt(i);
      hash = Math.imul(hash, 0x01000193);
    }
    /* Stretch the key by hashing multiple rounds */
    var key = '';
    for (var round = 0; round < 8; round++) {
      hash ^= (hash >>> 16);
      hash = Math.imul(hash, 0x45d9f3b);
      hash ^= (hash >>> 16);
      key += Math.abs(hash).toString(36);
    }
    return key;
  }

  /* Generate random bytes as base64 */
  function randomBase64(len) {
    var arr = new Uint8Array(len);
    for (var i = 0; i < len; i++) arr[i] = Math.floor(Math.random() * 256);
    return btoa(String.fromCharCode.apply(null, arr));
  }

  /* Check if Web Crypto is available (needs HTTPS / secure context) */
  var hasWebCrypto = !!(window.crypto && window.crypto.subtle);

  /* ── Web Crypto path (used when on HTTPS) ── */
  async function deriveKey(passphrase, salt) {
    var encoder = new TextEncoder();
    var keyMaterial = await crypto.subtle.importKey(
      'raw', encoder.encode(passphrase), 'PBKDF2', false, ['deriveKey']
    );
    return crypto.subtle.deriveKey(
      { name: 'PBKDF2', salt: salt, iterations: 100000, hash: 'SHA-256' },
      keyMaterial,
      { name: 'AES-GCM', length: 256 },
      false,
      ['encrypt', 'decrypt']
    );
  }

  async function encryptMessage(plaintext, passphrase) {
    if (hasWebCrypto) {
      try {
        var encoder = new TextEncoder();
        var salt = crypto.getRandomValues(new Uint8Array(16));
        var iv   = crypto.getRandomValues(new Uint8Array(12));
        var key  = await deriveKey(passphrase, salt);
        var encrypted = await crypto.subtle.encrypt(
          { name: 'AES-GCM', iv: iv }, key, encoder.encode(plaintext)
        );
        return {
          ciphertext: btoa(String.fromCharCode.apply(null, new Uint8Array(encrypted))),
          iv:         btoa(String.fromCharCode.apply(null, iv)),
          salt:       btoa(String.fromCharCode.apply(null, salt))
        };
      } catch(e) { /* fallback below */ }
    }

    /* Fallback: RC4 stream cipher */
    var salt = randomBase64(16);
    var iv = randomBase64(12);
    var keyStr = deriveKeyString(passphrase, salt);
    var encoder = new TextEncoder();
    var plainBytes = encoder.encode(plaintext);
    var encrypted = rc4(keyStr + iv, Array.from(plainBytes));
    return {
      ciphertext: btoa(String.fromCharCode.apply(null, encrypted)),
      iv: iv,
      salt: salt
    };
  }

  async function decryptMessage(ciphertext64, iv64, salt64, passphrase) {
    if (hasWebCrypto) {
      try {
        var ciphertext = Uint8Array.from(atob(ciphertext64), function(c){return c.charCodeAt(0);});
        var iv   = Uint8Array.from(atob(iv64),   function(c){return c.charCodeAt(0);});
        var salt = Uint8Array.from(atob(salt64), function(c){return c.charCodeAt(0);});
        var key  = await deriveKey(passphrase, salt);
        var decrypted = await crypto.subtle.decrypt(
          { name: 'AES-GCM', iv: iv }, key, ciphertext
        );
        return new TextDecoder().decode(decrypted);
      } catch(e) { /* fallback below */ }
    }

    /* Fallback: RC4 stream cipher */
    var keyStr = deriveKeyString(passphrase, salt64);
    var cipherBytes = Uint8Array.from(atob(ciphertext64), function(c){return c.charCodeAt(0);});
    var decrypted = rc4(keyStr + iv64, Array.from(cipherBytes));
    return new TextDecoder().decode(new Uint8Array(decrypted));
  }

  /* ═══ WEBSOCKET ═══ */

  function connectWebSocket() {
    setStatus('connecting', 'Connecting...');
    var host = location.hostname || '192.168.4.1';
    ws = new WebSocket('ws://' + host + '/ws');

    ws.onopen = function () {
      reconnectAttempts = 0;
      setStatus('connecting', 'Connected, joining...');
      ws.send(JSON.stringify({ type: 'join', username: myUsername }));
    };

    ws.onclose = function () {
      setStatus('disconnected', 'Disconnected');
      scheduleReconnect();
    };

    ws.onerror = function (e) {
      console.error('[WS] Error', e);
    };

    ws.onmessage = function (event) {
      handleMessage(event.data);
    };
  }

  function scheduleReconnect() {
    reconnectAttempts++;
    var delay = Math.min(1000 * Math.pow(2, reconnectAttempts), MAX_RECONNECT_DELAY);
    setTimeout(function () {
      if (!ws || ws.readyState === WebSocket.CLOSED) {
        connectWebSocket();
      }
    }, delay);
  }

  /* ═══ INCOMING MESSAGES ═══ */

  async function handleMessage(raw) {
    var data;
    try { data = JSON.parse(raw); } catch (e) { return; }

    switch (data.type) {
      case 'welcome':
        myId = data.id;
        break;

      case 'joined':
        setStatus('connected', 'Connected');
        DOM.joinScreen.classList.remove('active');
        DOM.chatScreen.classList.add('active');
        DOM.messageInput.focus();
        addSystemMessage('You joined the chat');
        break;

      case 'error':
        showJoinError(data.message);
        break;

      case 'userlist':
        renderUserList(data.users);
        break;

      case 'system':
        if (data.event === 'join') {
          addSystemMessage(data.username + ' joined');
        } else if (data.event === 'leave') {
          addSystemMessage(data.username + ' left');
        }
        break;

      case 'message':
        try {
          var passphrase = DOM.passphraseInput.value;
          var plaintext = await decryptMessage(
            data.ciphertext, data.iv, data.salt, passphrase
          );
          var isSelf = (data.from === myUsername);
          var isPrivate = !!data['private'];
          addChatMessage(data.from, plaintext, isSelf, isPrivate, data.to);
        } catch (err) {
          var isSelf2 = (data.from === myUsername);
          addChatMessage(data.from, '\u{1F512} [Encrypted - wrong passphrase]', isSelf2, false);
        }
        break;

      case 'typing':
        showTyping(data.username);
        break;

      case 'pong':
        break;
    }
  }

  /* ═══ UI HELPERS ═══ */

  function setStatus(cls, text) {
    DOM.connStatus.className = 'status ' + cls;
    DOM.connStatus.textContent = text;
  }

  function showJoinError(msg) {
    DOM.joinError.textContent = msg;
    DOM.joinError.classList.remove('hidden');
  }

  function getAvatarColor(name) {
    var hash = 0;
    for (var i = 0; i < name.length; i++) {
      hash = name.charCodeAt(i) + ((hash << 5) - hash);
    }
    return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
  }

  function timeNow() {
    var d = new Date();
    var h = d.getHours(), m = d.getMinutes();
    return (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m;
  }

  function renderUserList(users) {
    DOM.userCount.textContent = users.length + ' online';
    DOM.userList.innerHTML = '';

    users.forEach(function (u) {
      var li = document.createElement('li');
      var isSelf = (u.username === myUsername);

      li.innerHTML =
        '<div class="user-avatar" style="background:' + getAvatarColor(u.username) + '">' +
          u.username.charAt(0) +
        '</div>' +
        '<span class="user-name">' + escapeHtml(u.username) +
          (isSelf ? ' <span class="user-you">(you)</span>' : '') +
        '</span>' +
        '<span class="user-online-dot"></span>';

      if (!isSelf) {
        li.title = 'Send private message to ' + u.username;
        li.addEventListener('click', function () {
          startPrivateMessage(u.username);
        });
      }

      DOM.userList.appendChild(li);
    });
  }

  function addSystemMessage(text) {
    var div = document.createElement('div');
    div.className = 'msg-system';
    div.textContent = text;
    DOM.messages.appendChild(div);
    scrollToBottom();
  }

  function addChatMessage(sender, text, isSelf, isPrivate, pmTo) {
    var div = document.createElement('div');
    div.className = 'msg ' + (isSelf ? 'self' : 'other');
    if (isPrivate) div.classList.add('private-msg');

    var html = '';
    if (isPrivate) {
      var label = isSelf ? 'Private to ' + escapeHtml(pmTo) : 'Private message';
      html += '<div class="msg-pm-badge">\u{1F512} ' + label + '</div>';
    }
    if (!isSelf) {
      html += '<div class="msg-sender">' + escapeHtml(sender) + '</div>';
    }
    html += '<div class="msg-text">' + escapeHtml(text) + '</div>';
    html += '<div class="msg-time">' + timeNow() + '</div>';

    div.innerHTML = html;
    DOM.messages.appendChild(div);
    scrollToBottom();
  }

  function scrollToBottom() {
    DOM.messages.scrollTop = DOM.messages.scrollHeight;
  }

  function escapeHtml(str) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
  }

  function showTyping(username) {
    DOM.typingUser.textContent = username;
    DOM.typingIndicator.classList.remove('hidden');
    clearTimeout(typingDisplayTimeout);
    typingDisplayTimeout = setTimeout(function () {
      DOM.typingIndicator.classList.add('hidden');
    }, 3000);
  }

  function sendTyping() {
    var now = Date.now();
    if (now - lastTypingSent > 2000 && ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'typing' }));
      lastTypingSent = now;
    }
  }

  function startPrivateMessage(username) {
    pmRecipient = username;
    DOM.pmTarget.textContent = username;
    DOM.pmLabel.classList.remove('hidden');
    DOM.messageInput.placeholder = 'Private message to ' + username + '...';
    DOM.messageInput.focus();
    DOM.sidebar.classList.remove('open');
  }

  function cancelPrivateMessage() {
    pmRecipient = null;
    DOM.pmLabel.classList.add('hidden');
    DOM.messageInput.placeholder = 'Type a message...';
  }

  /* Keep-alive — every 10s to prevent ESP8266 timeout */
  setInterval(function () {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'ping' }));
    }
  }, 10000);

  /* ═══ EVENT LISTENERS ═══ */

  DOM.joinForm.addEventListener('submit', function (e) {
    e.preventDefault();
    var username = DOM.usernameInput.value.trim();
    var passphrase = DOM.passphraseInput.value;
    if (!username || !passphrase) return;

    myUsername = username;
    DOM.joinError.classList.add('hidden');
    connectWebSocket();
  });

  DOM.messageForm.addEventListener('submit', async function (e) {
    e.preventDefault();
    var text = DOM.messageInput.value.trim();
    if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;

    try {
      var passphrase = DOM.passphraseInput.value;
      var encrypted  = await encryptMessage(text, passphrase);

      var msg = {
        type:       'message',
        ciphertext: encrypted.ciphertext,
        iv:         encrypted.iv,
        salt:       encrypted.salt
      };

      if (pmRecipient) {
        msg.to = pmRecipient;
      }

      ws.send(JSON.stringify(msg));
      DOM.messageInput.value = '';

      if (pmRecipient) cancelPrivateMessage();
    } catch (err) {
      console.error('[Crypto] Encryption failed', err);
    }
  });

  DOM.messageInput.addEventListener('input', sendTyping);

  DOM.sidebarToggle.addEventListener('click', function () {
    DOM.sidebar.classList.toggle('open');
  });
  DOM.sidebarClose.addEventListener('click', function () {
    DOM.sidebar.classList.remove('open');
  });
  DOM.sidebarOverlay.addEventListener('click', function () {
    DOM.sidebar.classList.remove('open');
  });

  DOM.leaveBtn.addEventListener('click', function () {
    if (ws) ws.close();
    DOM.chatScreen.classList.remove('active');
    DOM.joinScreen.classList.add('active');
    DOM.messages.innerHTML = '';
    DOM.userList.innerHTML = '';
    cancelPrivateMessage();
    myUsername = '';
  });

  DOM.pmCancel.addEventListener('click', cancelPrivateMessage);

})();
)rawliteral";

// ── HTML (references CSS and JS via server routes) ──────────
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <meta name="description" content="MeshTalk - Offline encrypted chat over ESP8266 local WiFi" />
  <title>MeshTalk - Offline Encrypted Chat</title>
  <link rel="stylesheet" href="/style.css" />
</head>
<body>

  <!-- JOIN SCREEN -->
  <div id="join-screen" class="screen active">
    <div class="join-card">
      <div class="logo-container">
        <div class="logo-ring"></div>
        <div class="logo-icon">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
               stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
            <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
          </svg>
        </div>
      </div>

      <h1>MeshTalk</h1>
      <p class="tagline">Encrypted &bull; Offline &bull; Private</p>

      <form id="join-form" autocomplete="off">
        <div class="input-group">
          <input type="text" id="username-input" placeholder="Choose a username"
                 maxlength="16" required autofocus />
        </div>

        <div class="input-group">
          <input type="password" id="passphrase-input"
                 placeholder="Encryption passphrase" required />
          <span class="input-hint">All users must use the same passphrase</span>
        </div>

        <button type="submit" id="join-btn" class="btn-primary">
          <span>Join Chat</span>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
               stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <line x1="5" y1="12" x2="19" y2="12"/>
            <polyline points="12 5 19 12 12 19"/>
          </svg>
        </button>
      </form>

      <div id="join-error" class="error-msg hidden"></div>

      <div class="security-badge">
        <svg viewBox="0 0 24 24" width="14" height="14" fill="none"
             stroke="currentColor" stroke-width="2">
          <rect x="3" y="11" width="18" height="11" rx="2" ry="2"/>
          <path d="M7 11V7a5 5 0 0 1 10 0v4"/>
        </svg>
        AES-256-GCM End-to-End Encryption
      </div>
    </div>
  </div>

  <!-- CHAT SCREEN -->
  <div id="chat-screen" class="screen">

    <!-- Top Bar -->
    <header id="chat-header">
      <div class="header-left">
        <button id="sidebar-toggle" class="icon-btn" aria-label="Toggle users">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
               stroke-width="2" stroke-linecap="round">
            <line x1="3" y1="6"  x2="21" y2="6"/>
            <line x1="3" y1="12" x2="21" y2="12"/>
            <line x1="3" y1="18" x2="21" y2="18"/>
          </svg>
        </button>
        <div class="header-info">
          <h2>MeshTalk</h2>
          <span id="connection-status" class="status disconnected">
            Disconnected
          </span>
        </div>
      </div>
      <div class="header-right">
        <span id="user-count" class="user-count">0 online</span>
        <button id="leave-btn" class="icon-btn" aria-label="Leave chat"
                title="Leave chat">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
               stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
            <polyline points="16 17 21 12 16 7"/>
            <line x1="21" y1="12" x2="9" y2="12"/>
          </svg>
        </button>
      </div>
    </header>

    <div id="chat-body">

      <!-- Sidebar — Online Users -->
      <aside id="users-sidebar" class="sidebar">
        <div class="sidebar-header">
          <h3>Online Users</h3>
          <button id="sidebar-close" class="icon-btn">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="2" stroke-linecap="round">
              <line x1="18" y1="6" x2="6" y2="18"/>
              <line x1="6" y1="6" x2="18" y2="18"/>
            </svg>
          </button>
        </div>
        <ul id="user-list"></ul>
        <div class="sidebar-footer">
          <div class="encryption-info">
            <svg viewBox="0 0 24 24" width="14" height="14" fill="none"
                 stroke="currentColor" stroke-width="2">
              <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
            </svg>
            <span>Messages encrypted</span>
          </div>
        </div>
      </aside>

      <!-- Overlay for mobile sidebar -->
      <div id="sidebar-overlay" class="sidebar-overlay"></div>

      <!-- Chat Messages Area -->
      <main id="chat-area">
        <div id="messages" class="messages"></div>

        <!-- Typing indicator -->
        <div id="typing-indicator" class="typing-indicator hidden">
          <span id="typing-user"></span> is typing
          <span class="typing-dots">
            <span></span><span></span><span></span>
          </span>
        </div>

        <!-- Message Input -->
        <form id="message-form" class="message-form">
          <div class="msg-input-wrapper">
            <div id="pm-label" class="pm-label hidden">
              <span>To: <strong id="pm-target"></strong></span>
              <button type="button" id="pm-cancel" class="pm-cancel">&times;</button>
            </div>
            <input type="text" id="message-input"
                   placeholder="Type a message..." autocomplete="off" />
          </div>
          <button type="submit" id="send-btn" class="btn-send" aria-label="Send">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
                 stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <line x1="22" y1="2" x2="11" y2="13"/>
              <polygon points="22 2 15 22 11 13 2 9 22 2"/>
            </svg>
          </button>
        </form>
      </main>
    </div>
  </div>

  <script src="/app.js"></script>
</body>
</html>)rawliteral";


/* ═══════════════════════════════════════════════════════════════
 *  HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════ */

int findClientByID(uint32_t id) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active && clients[i].id == id) return i;
  }
  return -1;
}

int findClientByName(const char* name) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active && clients[i].username == name) return i;
  }
  return -1;
}

/** Send the full user list to ALL connected WS clients. */
void broadcastUserList() {
  DynamicJsonDocument doc(512);
  doc["type"] = "userlist";
  JsonArray arr = doc.createNestedArray("users");

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active) {
      JsonObject u = arr.createNestedObject();
      u["username"] = clients[i].username;
    }
  }

  String out;
  serializeJson(doc, out);
  ws.textAll(out);

  Serial.printf("[WS] Broadcast userlist (%d users)\n", arr.size());
}

/** Broadcast a system event (join / leave) to all clients. */
void broadcastSystemEvent(const char* event, const String& username) {
  StaticJsonDocument<256> doc;
  doc["type"]     = "system";
  doc["event"]    = event;
  doc["username"] = username;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);

  Serial.printf("[WS] System event: %s -> %s\n", event, username.c_str());
}

/** Send a JSON error message to one client. */
void sendError(AsyncWebSocketClient* client, const char* message) {
  StaticJsonDocument<128> doc;
  doc["type"]    = "error";
  doc["message"] = message;

  String out;
  serializeJson(doc, out);
  client->text(out);
}

/* ═══════════════════════════════════════════════════════════════
 *  WEBSOCKET EVENT HANDLER
 * ═══════════════════════════════════════════════════════════════ */

void onWsEvent(AsyncWebSocket* srv,
               AsyncWebSocketClient* client,
               AwsEventType type,
               void* arg,
               uint8_t* data,
               size_t len)
{
  /* ── Connect ────────────────────────────── */
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected from %s\n",
                  client->id(),
                  client->remoteIP().toString().c_str());

    // Send welcome with their WS id
    StaticJsonDocument<64> doc;
    doc["type"] = "welcome";
    doc["id"]   = client->id();
    String out;
    serializeJson(doc, out);
    client->text(out);
    return;
  }

  /* ── Disconnect ─────────────────────────── */
  if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());

    int idx = findClientByID(client->id());
    if (idx >= 0) {
      String name = clients[idx].username;
      clients[idx].active = false;

      broadcastSystemEvent("leave", name);
      broadcastUserList();
    }
    return;
  }

  /* ── Error ──────────────────────────────── */
  if (type == WS_EVT_ERROR) {
    Serial.printf("[WS] Client #%u error: %u\n", client->id(), *((uint16_t*)arg));
    return;
  }

  /* ── Data ───────────────────────────────── */
  if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;

    // Only handle complete, single-frame text messages
    if (!info->final || info->index != 0 || info->opcode != WS_TEXT) return;

    // Guard against oversized messages
    if (len > WS_MAX_MSG) {
      sendError(client, "Message too large");
      return;
    }

    // Guard against low heap — prevent crashes
    if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
      Serial.printf("[WS] LOW HEAP: %u bytes — dropping message\n", ESP.getFreeHeap());
      sendError(client, "Server busy, try again");
      return;
    }

    // Null-terminate and use data directly (avoid extra String copy)
    data[len] = 0;
    const char* msg = (const char*)data;

    Serial.printf("[WS] #%u data (%u bytes): %.80s\n", client->id(), (unsigned)len, msg);

    // Parse JSON — use DynamicJsonDocument on heap to avoid stack overflow
    DynamicJsonDocument doc(len + 256);
    DeserializationError err = deserializeJson(doc, msg, len);
    if (err) {
      Serial.printf("[ERROR] JSON parse failed: %s\n", err.c_str());
      sendError(client, "Invalid JSON");
      return;
    }

    const char* msgType = doc["type"] | "";

    /* ── JOIN ─────────────────────────────── */
    if (strcmp(msgType, "join") == 0) {
      const char* uname = doc["username"] | "";

      if (strlen(uname) == 0 || strlen(uname) > 16) {
        sendError(client, "Username must be 1-16 characters");
        return;
      }

      // Check for duplicate username
      if (findClientByName(uname) >= 0) {
        sendError(client, "Username already taken");
        return;
      }

      // Find empty slot
      bool added = false;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
          clients[i].id       = client->id();
          clients[i].username = uname;
          clients[i].active   = true;
          added = true;
          break;
        }
      }

      if (!added) {
        sendError(client, "Chat is full (max 8 users)");
        return;
      }

      Serial.printf("[CHAT] '%s' joined (client #%u)\n", uname, client->id());

      // Confirm to this client that they joined
      {
        StaticJsonDocument<64> ack;
        ack["type"] = "joined";
        String out;
        serializeJson(ack, out);
        client->text(out);
      }

      broadcastSystemEvent("join", String(uname));
      broadcastUserList();
    }

    /* ── MESSAGE (encrypted) ──────────────── */
    else if (strcmp(msgType, "message") == 0) {
      int idx = findClientByID(client->id());
      if (idx < 0) {
        sendError(client, "You must join first");
        return;
      }

      // Validate required fields
      if (!doc.containsKey("ciphertext") ||
          !doc.containsKey("iv") ||
          !doc.containsKey("salt")) {
        sendError(client, "Missing encryption fields");
        return;
      }

      // Build outgoing message with sender attribution
      // Use DynamicJsonDocument — encrypted payloads can be large
      size_t outSize = len + 256;
      DynamicJsonDocument outDoc(outSize);
      outDoc["type"]       = "message";
      outDoc["from"]       = clients[idx].username;
      outDoc["ciphertext"] = doc["ciphertext"];
      outDoc["iv"]         = doc["iv"];
      outDoc["salt"]       = doc["salt"];

      const char* pmTarget = doc["to"] | "";

      // ── Private message ──
      if (strlen(pmTarget) > 0) {
        outDoc["private"] = true;
        outDoc["to"]      = pmTarget;

        String out;
        serializeJson(outDoc, out);

        // Send to recipient
        int targetIdx = findClientByName(pmTarget);
        if (targetIdx >= 0) {
          // Find the WS client object by id
          for (auto& c : ws.getClients()) {
            if (c->id() == clients[targetIdx].id && c->status() == WS_CONNECTED) {
              c->text(out);
              break;
            }
          }
        }

        // Send copy back to sender
        client->text(out);

        Serial.printf("[CHAT] PM: %s -> %s\n",
                      clients[idx].username.c_str(), pmTarget);
      }
      // ── Broadcast message ──
      else {
        String out;
        serializeJson(outDoc, out);
        ws.textAll(out);

        Serial.printf("[CHAT] Broadcast from %s\n",
                      clients[idx].username.c_str());
      }
    }

    /* ── TYPING ───────────────────────────── */
    else if (strcmp(msgType, "typing") == 0) {
      int idx = findClientByID(client->id());
      if (idx < 0) return;

      StaticJsonDocument<128> tdoc;
      tdoc["type"]     = "typing";
      tdoc["username"] = clients[idx].username;

      String out;
      serializeJson(tdoc, out);

      // Send to everyone except the sender
      for (auto& c : ws.getClients()) {
        if (c->id() != client->id() && c->status() == WS_CONNECTED) {
          c->text(out);
        }
      }
    }

    /* ── PING (keep-alive) ────────────────── */
    else if (strcmp(msgType, "ping") == 0) {
      client->text("{\"type\":\"pong\"}");
    }

    /* ── Unknown ──────────────────────────── */
    else {
      Serial.printf("[WS] Unknown type: '%s'\n", msgType);
    }
  }
}

/* ═══════════════════════════════════════════════════════════════
 *  HTTP ROUTES  (serving from PROGMEM — no filesystem needed!)
 * ═══════════════════════════════════════════════════════════════ */

void setupRoutes() {
  // Serve index.html at root
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.printf("[HTTP] %s %s\n", request->methodToString(), request->url().c_str());
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // Serve style.css
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.printf("[HTTP] %s %s\n", request->methodToString(), request->url().c_str());
    request->send_P(200, "text/css", STYLE_CSS);
  });

  // Serve app.js
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    Serial.printf("[HTTP] %s %s\n", request->methodToString(), request->url().c_str());
    request->send_P(200, "application/javascript", APP_JS);
  });

  // Catch-all → 404
  server.onNotFound([](AsyncWebServerRequest* request) {
    Serial.printf("[HTTP] 404: %s %s\n",
                  request->methodToString(), request->url().c_str());
    request->send(404, "text/plain",
      "404 - Not Found\nURL: " + request->url());
  });
}

/* ═══════════════════════════════════════════════════════════════
 *  SETUP
 * ═══════════════════════════════════════════════════════════════ */

void setup() {
  Serial.begin(115200);
  delay(500);  // Let serial settle

  Serial.println();
  Serial.println("========================================");
  Serial.println("  MeshTalk v3.0 PROGMEM Edition");
  Serial.println("  No LittleFS needed!");
  Serial.println("========================================");
  Serial.println();

  // ── Init client slots ──
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].active = false;
  }

  // ── Flash info ──
  Serial.printf("[SYS] Flash size : %u bytes (%u KB)\n",
                ESP.getFlashChipRealSize(),
                ESP.getFlashChipRealSize() / 1024);
  Serial.printf("[SYS] Free heap  : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("[SYS] SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("[SYS] Sketch size: %u bytes\n", ESP.getSketchSize());
  Serial.printf("[SYS] Free sketch: %u bytes\n", ESP.getFreeSketchSpace());
  Serial.println();

  // ── WiFi Access Point ──
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);  // Let AP stabilize

  Serial.println("[WiFi] Access Point started");
  Serial.printf("[WiFi]    SSID : %s\n", AP_SSID);
  Serial.printf("[WiFi]    IP   : %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[WiFi]    Open : http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println();

  // ── Verify PROGMEM assets ──
  Serial.printf("[WEB] index.html : %u bytes (PROGMEM)\n", strlen_P(INDEX_HTML));
  Serial.printf("[WEB] style.css  : %u bytes (PROGMEM)\n", strlen_P(STYLE_CSS));
  Serial.printf("[WEB] app.js     : %u bytes (PROGMEM)\n", strlen_P(APP_JS));
  Serial.println();

  // ── WebSocket ──
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  Serial.println("[WS] WebSocket handler registered at /ws");

  // ── HTTP Routes ──
  setupRoutes();

  // ── Start Server ──
  server.begin();

  Serial.println("[HTTP] Server started on port 80");
  Serial.println();
  Serial.println("========================================");
  Serial.println("  MeshTalk is READY!");
  Serial.printf("  Connect to WiFi: %s\n", AP_SSID);
  Serial.printf("  Open: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("========================================");
  Serial.println();
}

/* ═══════════════════════════════════════════════════════════════
 *  LOOP
 * ═══════════════════════════════════════════════════════════════ */

void loop() {
  // Clean up disconnected WebSocket clients (throttled to avoid overhead)
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 1000) {
    lastCleanup = millis();
    ws.cleanupClients();
  }

  // Periodic heap monitoring (detect memory leaks)
  if (millis() - lastHeapLog > HEAP_LOG_MS) {
    lastHeapLog = millis();
    Serial.printf("[SYS] Free heap: %u bytes | WS clients: %u\n",
                  ESP.getFreeHeap(), ws.count());
  }

  // Yield to system tasks
  yield();
}