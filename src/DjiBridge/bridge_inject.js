/**
 * DjiBridge 注入脚本
 * 在 WebEngine 页面创建阶段（DocumentCreation）注入，
 * 在 window 上挂载 djiBridge 对象，所有方法通过同步 XHR
 * 转发到本地 C++ HTTP 服务（127.0.0.1:__PORT__），
 * 响应文本直接返回，与大疆安卓端 addJavascriptInterface 行为一致。
 */
(function () {
    'use strict';

    var PORT = __PORT__;
    var API_URL = 'http://127.0.0.1:' + PORT + '/api';

    /**
     * 同步调用本地 C++ 服务
     * @param {string} method - djiBridge 方法名
     * @param {Array} args - 参数数组
     * @returns {string} JsResponse JSON 字符串（或 platformGetVersion 的纯字符串）
     */
    function callNative(method, args) {
        try {
            var xhr = new XMLHttpRequest();
            xhr.open('POST', API_URL, false); // false = 同步
            xhr.setRequestHeader('Content-Type', 'application/json');
            var payload = JSON.stringify({ method: method, args: args || [] });
            xhr.send(payload);
            if (xhr.status === 200) {
                return xhr.responseText;
            }
            return JSON.stringify({ code: -1, message: 'HTTP ' + xhr.status, data: null });
        } catch (e) {
            return JSON.stringify({ code: -1, message: String(e), data: null });
        }
    }

    var bridge = {};

    // ==================== 平台方法 ====================
    bridge.platformLoadComponent = function (name, param) {
        return callNative('platformLoadComponent', [name, param]);
    };
    bridge.platformUnloadComponent = function (name) {
        return callNative('platformUnloadComponent', [name]);
    };
    bridge.platformIsComponentLoaded = function (module) {
        return callNative('platformIsComponentLoaded', [module]);
    };
    bridge.platformSetWorkspaceId = function (uuid) {
        return callNative('platformSetWorkspaceId', [uuid]);
    };
    bridge.platformSetInformation = function (platformName, title, desc) {
        return callNative('platformSetInformation', [platformName, title, desc]);
    };
    bridge.platformGetRemoteControllerSN = function () {
        return callNative('platformGetRemoteControllerSN', []);
    };
    bridge.platformGetAircraftSN = function () {
        return callNative('platformGetAircraftSN', []);
    };
    bridge.platformStopSelf = function () {
        return callNative('platformStopSelf', []);
    };
    bridge.platformSetLogEncryptKey = function (key) {
        return callNative('platformSetLogEncryptKey', [key]);
    };
    bridge.platformClearLogEncryptKey = function () {
        return callNative('platformClearLogEncryptKey', []);
    };
    bridge.platformGetLogPath = function () {
        return callNative('platformGetLogPath', []);
    };
    bridge.platformVerifyLicense = function (appId, appKey, appLicense) {
        return callNative('platformVerifyLicense', [appId, appKey, appLicense]);
    };
    bridge.platformIsVerified = function () {
        return callNative('platformIsVerified', []);
    };
    bridge.platformIsAppInstalled = function (pkgName) {
        return callNative('platformIsAppInstalled', [pkgName]);
    };
    bridge.platformGetVersion = function () {
        return callNative('platformGetVersion', []);
    };

    // ==================== Thing ====================
    bridge.thingGetConnectState = function () {
        return callNative('thingGetConnectState', []);
    };
    bridge.thingGetConfigs = function () {
        return callNative('thingGetConfigs', []);
    };

    // ==================== API ====================
    bridge.apiGetToken = function () {
        return callNative('apiGetToken', []);
    };
    bridge.apiSetToken = function (token) {
        return callNative('apiSetToken', [token]);
    };
    bridge.apiGetHost = function () {
        return callNative('apiGetHost', []);
    };

    // ==================== Liveshare ====================
    bridge.liveshareSetVideoPublishType = function (type) {
        return callNative('liveshareSetVideoPublishType', [type]);
    };
    bridge.liveshareGetConfig = function () {
        return callNative('liveshareGetConfig', []);
    };
    bridge.liveshareSetConfig = function (type, params) {
        return callNative('liveshareSetConfig', [type, params]);
    };
    bridge.liveshareSetStatusCallback = function (callbackFunc) {
        return callNative('liveshareSetStatusCallback', [callbackFunc]);
    };
    bridge.liveshareGetStatus = function () {
        return callNative('liveshareGetStatus', []);
    };
    bridge.liveshareStartLive = function () {
        return callNative('liveshareStartLive', []);
    };
    bridge.liveshareStopLive = function () {
        return callNative('liveshareStopLive', []);
    };

    // ==================== WebSocket ====================
    bridge.wsGetConnectState = function () {
        return callNative('wsGetConnectState', []);
    };
    bridge.wsConnect = function (host, token, callback) {
        return callNative('wsConnect', [host, token, callback]);
    };
    bridge.wsDisconnect = function () {
        // 注意：pilot-bridge.ts 源码第238行 bug 调用的是 wsConnect()（无参数），
        // C++ 端已兼容无参数 wsConnect = disconnect。这里也提供标准 wsDisconnect。
        return callNative('wsDisconnect', []);
    };
    bridge.wsSend = function (message) {
        return callNative('wsSend', [message]);
    };

    // ==================== Media ====================
    bridge.mediaSetAutoUploadPhoto = function (auto) {
        return callNative('mediaSetAutoUploadPhoto', [auto]);
    };
    bridge.mediaGetAutoUploadPhoto = function () {
        return callNative('mediaGetAutoUploadPhoto', []);
    };
    bridge.mediaSetUploadPhotoType = function (type) {
        return callNative('mediaSetUploadPhotoType', [type]);
    };
    bridge.mediaGetUploadPhotoType = function () {
        return callNative('mediaGetUploadPhotoType', []);
    };
    bridge.mediaSetAutoUploadVideo = function (auto) {
        return callNative('mediaSetAutoUploadVideo', [auto]);
    };
    bridge.mediaGetAutoUploadVideo = function () {
        return callNative('mediaGetAutoUploadVideo', []);
    };
    bridge.mediaSetDownloadOwner = function (rcIndex) {
        return callNative('mediaSetDownloadOwner', [rcIndex]);
    };
    bridge.mediaGetDownloadOwner = function () {
        return callNative('mediaGetDownloadOwner', []);
    };

    // ==================== 回调属性（赋值型） ====================
    // pilot-bridge.ts 中：window.djiBridge.onBackClick = function(){...}
    // 用 defineProperty 拦截赋值，保存回调引用。
    var backClickCallback = null;
    var stopPlatformCallback = null;

    Object.defineProperty(bridge, 'onBackClick', {
        configurable: true,
        enumerable: true,
        get: function () { return backClickCallback; },
        set: function (fn) {
            backClickCallback = fn;
            // 保存到全局，方便 C++ 端通过 runJavaScript 调用
            window.__djiBackClickCallback = fn;
        }
    });

    Object.defineProperty(bridge, 'onStopPlatform', {
        configurable: true,
        enumerable: true,
        get: function () { return stopPlatformCallback; },
        set: function (fn) {
            stopPlatformCallback = fn;
            window.__djiStopPlatformCallback = fn;
        }
    });

    // 挂载到 window
    window.djiBridge = bridge;

    // ==================== window.thing 对象 ====================
    // 官方接口：window.thing.setConnectCallback(String callback)
    // thing 模块加载后，安卓端会在 window 上挂载 thing 对象
    window.thing = {
        setConnectCallback: function (callback) {
            return callNative('thingSetConnectCallback', [callback]);
        }
    };

    // 调试标记
    console.log('[DjiBridgeMock] window.djiBridge injected, native port = ' + PORT);
})();
