/**
 * ═══════════════════════════════════════════════════════════
 *  MeshTalk — Client-Side Application
 * ═══════════════════════════════════════════════════════════
 *  Features:
 *    • WebSocket real-time communication
 *    • AES-256-GCM end-to-end encryption (Web Crypto API)
 *    • PBKDF2 key derivation from shared passphrase
 *    • Private messaging
 *    • Auto-reconnect with exponential backoff
 *    • Typing indicators
 * ═══════════════════════════════════════════════════════════
 */

(function () {
  'use strict';

  // ── DOM References ──────────────────────────────────────
  const DOM = {
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
    pmCancel:       document.getElementById('pm-cancel'),
  };

  // ── State ───────────────────────────────────────────────
  let ws = null;
  let myUsername = '';
  let myId = null;
  let cryptoKey = null;
  let pmRecipient = null;     // private message target username
  let reconnectAttempts = 0;
  const MAX_RECONNECT_DELAY = 30000;
  let typingTimeout = null;
  let lastTypingSent = 0;
  let typingDisplayTimeout = null;

  // Avatar colour palette
  const AVATAR_COLORS = [
    '#6366f1','#ec4899','#f59e0b','#10b981','#3b82f6',
    '#8b5cf6','#ef4444','#14b8a6','#f97316','#06b6d4'
  ];

  // ═══════════════════════════════════════════════════════
  //  ENCRYPTION  — AES-256-GCM via Web Crypto API
  // ═══════════════════════════════════════════════════════

  /**
   * Derive an AES-256-GCM CryptoKey from a passphrase + salt
   * using PBKDF2 with 100 000 iterations.
   */
  async function deriveKey(passphrase, salt) {
    const encoder = new TextEncoder();
    const keyMaterial = await crypto.subtle.importKey(
      'raw', encoder.encode(passphrase), 'PBKDF2', false, ['deriveKey']
    );
    return crypto.subtle.deriveKey(
      { name: 'PBKDF2', salt, iterations: 100000, hash: 'SHA-256' },
      keyMaterial,
      { name: 'AES-GCM', length: 256 },
      false,
      ['encrypt', 'decrypt']
    );
  }

  /** Encrypt plaintext → { ciphertext, iv, salt } (all base64) */
  async function encryptMessage(plaintext, passphrase) {
    const encoder = new TextEncoder();
    const salt = crypto.getRandomValues(new Uint8Array(16));
    const iv   = crypto.getRandomValues(new Uint8Array(12));
    const key  = await deriveKey(passphrase, salt);
    const encrypted = await crypto.subtle.encrypt(
      { name: 'AES-GCM', iv }, key, encoder.encode(plaintext)
    );
    return {
      ciphertext: btoa(String.fromCharCode(...new Uint8Array(encrypted))),
      iv:         btoa(String.fromCharCode(...iv)),
      salt:       btoa(String.fromCharCode(...salt)),
    };
  }

  /** Decrypt { ciphertext, iv, salt } → plaintext string */
  async function decryptMessage(ciphertext64, iv64, salt64, passphrase) {
    const ciphertext = Uint8Array.from(atob(ciphertext64), c => c.charCodeAt(0));
    const iv   = Uint8Array.from(atob(iv64),   c => c.charCodeAt(0));
    const salt = Uint8Array.from(atob(salt64), c => c.charCodeAt(0));
    const key  = await deriveKey(passphrase, salt);
    const decrypted = await crypto.subtle.decrypt(
      { name: 'AES-GCM', iv }, key, ciphertext
    );
    return new TextDecoder().decode(decrypted);
  }

  // ═══════════════════════════════════════════════════════
  //  WEBSOCKET CONNECTION
  // ═══════════════════════════════════════════════════════

  function connectWebSocket() {
    setStatus('connecting', 'Connecting…');

    // ESP8266 AP default IP
    const host = location.hostname || '192.168.4.1';
    ws = new WebSocket('ws://' + host + '/ws');

    ws.onopen = function () {
      console.log('[WS] Connected');
      reconnectAttempts = 0;
      setStatus('connecting', 'Connected, joining…');

      // Send join message
      ws.send(JSON.stringify({ type: 'join', username: myUsername }));
    };

    ws.onclose = function () {
      console.log('[WS] Disconnected');
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
    const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), MAX_RECONNECT_DELAY);
    console.log('[WS] Reconnecting in ' + delay + 'ms…');
    setTimeout(function () {
      if (!ws || ws.readyState === WebSocket.CLOSED) {
        connectWebSocket();
      }
    }, delay);
  }

  // ═══════════════════════════════════════════════════════
  //  INCOMING MESSAGE HANDLER
  // ═══════════════════════════════════════════════════════

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
          // Decryption failed — wrong passphrase
          var isSelf2 = (data.from === myUsername);
          addChatMessage(data.from, '🔒 [Encrypted — wrong passphrase]', isSelf2, false);
        }
        break;

      case 'typing':
        showTyping(data.username);
        break;

      case 'pong':
        // Keep-alive acknowledged
        break;
    }
  }

  // ═══════════════════════════════════════════════════════
  //  UI HELPERS
  // ═══════════════════════════════════════════════════════

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

  // ── Render online users list ────────────────────────────
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

      // Click to start private message
      if (!isSelf) {
        li.title = 'Send private message to ' + u.username;
        li.addEventListener('click', function () {
          startPrivateMessage(u.username);
        });
      }

      DOM.userList.appendChild(li);
    });
  }

  // ── Add a system message ────────────────────────────────
  function addSystemMessage(text) {
    var div = document.createElement('div');
    div.className = 'msg-system';
    div.textContent = text;
    DOM.messages.appendChild(div);
    scrollToBottom();
  }

  // ── Add a chat message bubble ───────────────────────────
  function addChatMessage(sender, text, isSelf, isPrivate, pmTo) {
    var div = document.createElement('div');
    div.className = 'msg ' + (isSelf ? 'self' : 'other');
    if (isPrivate) div.classList.add('private-msg');

    var html = '';
    if (isPrivate) {
      var label = isSelf ? 'Private to ' + escapeHtml(pmTo) : 'Private message';
      html += '<div class="msg-pm-badge">🔒 ' + label + '</div>';
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

  // ── Typing indicator ───────────────────────────────────
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

  // ── Private messaging ──────────────────────────────────
  function startPrivateMessage(username) {
    pmRecipient = username;
    DOM.pmTarget.textContent = username;
    DOM.pmLabel.classList.remove('hidden');
    DOM.messageInput.placeholder = 'Private message to ' + username + '…';
    DOM.messageInput.focus();
    // Close sidebar on mobile
    DOM.sidebar.classList.remove('open');
  }

  function cancelPrivateMessage() {
    pmRecipient = null;
    DOM.pmLabel.classList.add('hidden');
    DOM.messageInput.placeholder = 'Type a message…';
  }

  // ── Keep-alive ping (every 10s to prevent ESP8266 timeout) ──
  setInterval(function () {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'ping' }));
    }
  }, 10000);

  // ═══════════════════════════════════════════════════════
  //  EVENT LISTENERS
  // ═══════════════════════════════════════════════════════

  // Join form submit
  DOM.joinForm.addEventListener('submit', function (e) {
    e.preventDefault();
    var username = DOM.usernameInput.value.trim();
    var passphrase = DOM.passphraseInput.value;
    if (!username || !passphrase) return;

    myUsername = username;
    DOM.joinError.classList.add('hidden');
    connectWebSocket();
  });

  // Send message
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
        salt:       encrypted.salt,
      };

      if (pmRecipient) {
        msg.to = pmRecipient;
      }

      ws.send(JSON.stringify(msg));
      DOM.messageInput.value = '';

      // Cancel PM mode after sending
      if (pmRecipient) cancelPrivateMessage();
    } catch (err) {
      console.error('[Crypto] Encryption failed', err);
    }
  });

  // Typing indicator
  DOM.messageInput.addEventListener('input', sendTyping);

  // Sidebar toggle
  DOM.sidebarToggle.addEventListener('click', function () {
    DOM.sidebar.classList.toggle('open');
  });
  DOM.sidebarClose.addEventListener('click', function () {
    DOM.sidebar.classList.remove('open');
  });
  DOM.sidebarOverlay.addEventListener('click', function () {
    DOM.sidebar.classList.remove('open');
  });

  // Leave button
  DOM.leaveBtn.addEventListener('click', function () {
    if (ws) ws.close();
    DOM.chatScreen.classList.remove('active');
    DOM.joinScreen.classList.add('active');
    DOM.messages.innerHTML = '';
    DOM.userList.innerHTML = '';
    cancelPrivateMessage();
    myUsername = '';
  });

  // Cancel private message
  DOM.pmCancel.addEventListener('click', cancelPrivateMessage);

})();
