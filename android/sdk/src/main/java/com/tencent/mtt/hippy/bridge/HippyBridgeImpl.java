/* Tencent is pleased to support the open source community by making Hippy available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.tencent.mtt.hippy.bridge;

import com.tencent.mtt.hippy.HippyEngineContext;
import com.tencent.mtt.hippy.devsupport.DevServerCallBack;
import com.tencent.mtt.hippy.devsupport.DevSupportManager;
import com.tencent.mtt.hippy.serialization.compatible.Deserializer;
import com.tencent.mtt.hippy.serialization.nio.reader.BinaryReader;
import com.tencent.mtt.hippy.serialization.nio.reader.SafeDirectReader;
import com.tencent.mtt.hippy.serialization.nio.reader.SafeHeapReader;
import com.tencent.mtt.hippy.serialization.string.InternalizedStringTable;
import com.tencent.mtt.hippy.devsupport.inspector.Inspector;
import com.tencent.mtt.hippy.utils.UIThreadUtils;
import com.tencent.mtt.hippy.utils.UrlUtils;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

import android.content.Context;
import android.content.res.AssetManager;
import android.text.TextUtils;

import com.tencent.mtt.hippy.common.HippyArray;
import com.tencent.mtt.hippy.devsupport.DebugWebSocketClient;
import com.tencent.mtt.hippy.devsupport.DevRemoteDebugProxy;
import com.tencent.mtt.hippy.utils.ArgumentUtils;
import com.tencent.mtt.hippy.utils.FileUtils;
import com.tencent.mtt.hippy.utils.LogUtils;
import java.nio.ByteOrder;

@SuppressWarnings({"unused", "JavaJniMissingFunction"})
public class HippyBridgeImpl implements HippyBridge, DevRemoteDebugProxy.OnReceiveDataListener {

  private static final Object sBridgeSyncLock;

  static {
    sBridgeSyncLock = new Object();
  }

  private static volatile String mCodeCacheRootDir;
  private long mV8RuntimeId = 0;
  private BridgeCallback mBridgeCallback;
  private boolean mInit = false;
  private final boolean mIsDevModule;
  private String mDebugServerHost;
  private final boolean mSingleThreadMode;
  private final boolean enableV8Serialization;
  private DebugWebSocketClient mDebugWebSocketClient;
  private String mDebugGlobalConfig;
  private NativeCallback mDebugInitJSFrameworkCallback;
  private final HippyEngineContext mContext;
  private Deserializer deserializer;
  private BinaryReader safeHeapReader;
  private BinaryReader safeDirectReader;

  public HippyBridgeImpl(HippyEngineContext engineContext, BridgeCallback callback,
      boolean singleThreadMode,
      boolean enableV8Serialization, boolean isDevModule, String debugServerHost) {
    this.mBridgeCallback = callback;
    this.mSingleThreadMode = singleThreadMode;
    this.enableV8Serialization = enableV8Serialization;
    this.mIsDevModule = isDevModule;
    this.mDebugServerHost = debugServerHost;
    this.mContext = engineContext;

    synchronized (sBridgeSyncLock) {
      if (mCodeCacheRootDir == null) {
        Context context = mContext.getGlobalConfigs().getContext();
        File hippyFile = FileUtils.getHippyFile(context);
        if (hippyFile != null) {
          mCodeCacheRootDir =
              hippyFile.getAbsolutePath() + File.separator + "codecache" + File.separator;
        }
      }
    }

    if (enableV8Serialization) {
      deserializer = new Deserializer(null, new InternalizedStringTable());
    }
  }

  @Override
  public void initJSBridge(String globalConfig, NativeCallback callback, final int groupId) {
    mDebugGlobalConfig = globalConfig;
    mDebugInitJSFrameworkCallback = callback;

    if (this.mIsDevModule) {
      mDebugWebSocketClient = new DebugWebSocketClient();
      mDebugWebSocketClient.setOnReceiveDataCallback(this);
      if (TextUtils.isEmpty(mDebugServerHost)) {
        mDebugServerHost = "localhost:38989";
      }
      String clientId = mContext.getDevSupportManager().getDevInstanceUUID();  // 方便区分不同的 Hippy 调试页面
      mDebugWebSocketClient.connect(
          String.format(Locale.US, "ws://%s/debugger-proxy?role=android_client&clientId=%s", mDebugServerHost, clientId),
          new DebugWebSocketClient.JSDebuggerCallback() {
            @SuppressWarnings("unused")
            @Override
            public void onSuccess(String response) {
              LogUtils.d("hippyCore", "js debug socket connect success");
              initJSEngine(groupId);
            }

            @SuppressWarnings("unused")
            @Override
            public void onFailure(final Throwable cause) {
              LogUtils.e("hippyCore", "js debug socket connect failed");
              initJSEngine(groupId);
            }
          });
    } else {
      initJSEngine(groupId);
    }
  }

  private void initJSEngine(int groupId) {
    synchronized (HippyBridgeImpl.class) {
      try {
        byte[] globalConfig = mDebugGlobalConfig.getBytes(StandardCharsets.UTF_16LE);
        mV8RuntimeId = initJSFramework(globalConfig, mSingleThreadMode, enableV8Serialization, mIsDevModule, mDebugInitJSFrameworkCallback, groupId);
        mInit = true;
      } catch (Throwable e) {
        if (mBridgeCallback != null) {
          mBridgeCallback.reportException(e);
        }
      }
		}
	}

  @Override
  public long getV8RuntimeId() {
    return mV8RuntimeId;
  }

  @Override
  public boolean runScriptFromUri(String uri, AssetManager assetManager, boolean canUseCodeCache,
      String codeCacheTag, NativeCallback callback) {
    if (!mInit) {
      return false;
    }

    if (!TextUtils.isEmpty(codeCacheTag) && !TextUtils.isEmpty(mCodeCacheRootDir)) {
      String codeCacheDir = mCodeCacheRootDir + codeCacheTag + File.separator;
      File codeCacheFile = new File(codeCacheDir);
      if (!codeCacheFile.exists()) {
        boolean ret = codeCacheFile.mkdirs();
        if (!ret) {
          canUseCodeCache = false;
          codeCacheDir = "";
        }
      }

      return runScriptFromUri(uri, assetManager, canUseCodeCache, codeCacheDir, mV8RuntimeId,
          callback);
    } else {
      boolean ret = false;
      LogUtils.d("HippyEngineMonitor", "runScriptFromAssets codeCacheTag is null");
      try {
        ret = runScriptFromUri(uri, assetManager, false, "" + codeCacheTag + File.separator,
            mV8RuntimeId, callback);
      } catch (Throwable e) {
        if (mBridgeCallback != null) {
          mBridgeCallback.reportException(e);
        }
      }
      return ret;
    }
  }

  @Override
  public void callFunction(String action, NativeCallback callback, ByteBuffer buffer) {
    if (!mInit || TextUtils.isEmpty(action) || buffer == null || buffer.limit() == 0) {
      return;
    }

    int offset = buffer.position();
    int length = buffer.limit() - buffer.position();
    if (buffer.isDirect()) {
      callFunction(action, mV8RuntimeId, callback, buffer, offset, length);
    } else {
      /*
       * In Android's DirectByteBuffer implementation.
       *
       * {@link DirectByteBuffer#hb backing array} will be used to store buffer data,
       * {@link DirectByteBuffer#offset} will be used to handle the alignment,
       * it's already add to {@link DirectByteBuffer#address},
       * so the {@link DirectByteBuffer} has backing array and offset.
       *
       * In the other side, JNI method |void* GetDirectBufferAddress(JNIEnv*, jobject)|
       * will be directly return {@link DirectByteBuffer#address} as the starting buffer address.
       *
       * So in this situation if, and only if, buffer is direct,
       * {@link ByteBuffer#arrayOffset} will be ignored, treated as 0.
       */
      offset += buffer.arrayOffset();
      callFunction(action, mV8RuntimeId, callback, buffer.array(), offset, length);
    }
  }

  @Override
  public void callFunction(String action, NativeCallback callback, byte[] buffer) {
    callFunction(action, callback, buffer, 0, buffer.length);
  }

  @Override
  public void callFunction(String action, NativeCallback callback, byte[] buffer, int offset,
      int length) {
    if (!mInit || TextUtils.isEmpty(action) || buffer == null || offset < 0 || length < 0
        || offset + length > buffer.length) {
      return;
    }

    callFunction(action, mV8RuntimeId, callback, buffer, offset, length);
  }

  @Override
  public void onDestroy() {
    if (mDebugWebSocketClient != null) {
      mDebugWebSocketClient.closeQuietly();
      mDebugWebSocketClient = null;
    }

    if (!mInit) {
      return;
    }

    mInit = false;
    if (enableV8Serialization) {
      deserializer.getStringTable().release();
    }

    mV8RuntimeId = 0;
    mBridgeCallback = null;
  }

  @Override
  public void destroy(NativeCallback callback) {
    destroy(mV8RuntimeId, mSingleThreadMode, callback);
  }

  public native long initJSFramework(byte[] gobalConfig, boolean useLowMemoryMode,
      boolean enableV8Serialization, boolean isDevModule, NativeCallback callback, long groupId);

  public native boolean runScriptFromUri(String uri, AssetManager assetManager,
      boolean canUseCodeCache, String codeCacheDir, long V8RuntimId, NativeCallback callback);

  public native void destroy(long runtimeId, boolean useLowMemoryMode, NativeCallback callback);

  public native void callFunction(String action, long V8RuntimId, NativeCallback callback,
      ByteBuffer buffer, int offset, int length);

  public native void callFunction(String action, long V8RuntimId, NativeCallback callback,
      byte[] buffer, int offset, int length);

  public native void onResourceReady(HippyUriResource output, long runtimeId, long resId);

  // c++通知java注册c++的debug拦截模块，如果c++有通知，则java所有请求需要先请求c++的拦截模块，
  // 如果没有注册，则java所有请求还是按原有逻辑调用
  public void notifyRegisterCoreDebugDelegate() {}
  // java通知c++，java有注册拦截模块，则所有c++模块发起的请求，都需要先调用到java的拦截模块再走原有逻辑
  public native void notifyCoreRegisterJavaDebugDelegate(long runtimeId);
  // java链式调用c++，获取对应的uri结果，此方法为同步方法
  public native HippyUriResource getUriContentSync(String uri, long runtimeId);
  // java链式调用c++，获取对应的uri结果，此方法为异步方法，通过NativeCallback返回给java，NativeCallback参数可以为
  // HippyUriResource，具体callback max定义下
  public native void getUriContentASync(String uri, long runtimeId, NativeCallback callback);

  // 后面是debug proxy专用
  // 请求来源java，c++通过getDebugDelegateContentSync拦截java的module请求，c++可以通过getNextSync获取下一个环节的返回，
  // 加工处理后可以通过getDebugDelegateContentSync的返回值返回给java
  public native HippyUriResource getDebugDelegateContentSync(String uri, long runtimeId);

  // 请求来源java，c++通过getDebugDelegateAsync拦截java的module的异步请求，
  // c++可以通过getNextAsync及onNextReady获取下一个环节的返回，
  // 加工处理后可以通过getDebugDelegateAsync的callback返回给java
  public native void  getDebugDelegateAsync(String uri, long runtimeId, NativeCallback callback);

  // 同步获取下一个环节的返回
  public HippyUriResource getNextSync(String uri) {
    return new HippyUriResource();
  }

  // 异步获取下一个环节的返回，id为请求id，onNextReady返回给c++，以便c++映射请求
  public void getNextAsync(String uri, long requestId) {}

  // 下一个环节异步返回的时候调用通知c++，requestId为getNextAsync得到的id
  public native void onNextReady(HippyUriResource output, long runtimeId, long requestId);

  public void callNatives(String moduleName, String moduleFunc, String callId, byte[] buffer) {
    callNatives(moduleName, moduleFunc, callId, ByteBuffer.wrap(buffer));
  }

  public void callNatives(String moduleName, String moduleFunc, String callId, ByteBuffer buffer) {
    LogUtils.d("jni_callback",
        "callNatives [moduleName:" + moduleName + " , moduleFunc: " + moduleFunc + "]");

    if (mBridgeCallback != null) {
      HippyArray hippyParam = bytesToArgument(buffer);
      mBridgeCallback.callNatives(moduleName, moduleFunc, callId, hippyParam);
    }
  }

  public void InspectorChannel(byte[] params) {
    String encoding = ByteOrder.nativeOrder() == ByteOrder.BIG_ENDIAN ? "UTF-16BE" : "UTF-16LE";
    String msg = new String(params, Charset.forName(encoding));
    if (mDebugWebSocketClient != null) {
      mDebugWebSocketClient.sendMessage(msg);
    }
  }

  // 相对于原有请求多加了个来源，fromCore true代表请求是c++模块发起的，此时c++模块本身已经执行完了调用链，都没有命中
  // 此时java如果没有找到处理的模块，不要再次调用c++处理链，避免无限循环
  // 未实现，只是让编译通过
  public HippyUriResource fetchResourceWithUriSync(final String uri, boolean fromCore) {
    return new HippyUriResource();
  }

  // 增加了异步的获取uri的方法，fromCore含义同上
  @SuppressWarnings("unused")
  public void fetchResourceWithUriAsync(final String uri, final long resId, boolean fromCore) {
    UIThreadUtils.runOnUiThread(new Runnable() {
      @Override
      public void run() {
        DevSupportManager devManager = mContext.getDevSupportManager();
        if (TextUtils.isEmpty(uri) || !UrlUtils.isWebUrl(uri) || devManager == null) {
          LogUtils.e("HippyBridgeImpl",
              "fetchResourceWithUri: can not call loadRemoteResource with " + uri);
          return;
        }

        devManager.loadRemoteResource(uri, new DevServerCallBack() {
          @Override
          public void onDevBundleReLoad() {
          }

          @Override
          public void onDevBundleLoadReady(InputStream inputStream) {
            try {
              ByteArrayOutputStream output = new ByteArrayOutputStream();

              byte[] b = new byte[2048];
              int size;
              while ((size = inputStream.read(b)) > 0) {
                output.write(b, 0, size);
              }

              byte[] resBytes = output.toByteArray();
              final ByteBuffer buffer = ByteBuffer.allocateDirect(resBytes.length);
              buffer.put(resBytes);
              HippyUriResource uri = new HippyUriResource();
              uri.code = HippyUriResource.RetCode.Success;
              uri.content = buffer;
              onResourceReady(uri, mV8RuntimeId, resId);
            } catch (Throwable e) {
              if (mBridgeCallback != null) {
                mBridgeCallback.reportException(e);
              }
              onResourceReady(null, mV8RuntimeId, resId);
            }
          }

          @Override
          public void onInitDevError(Throwable e) {
            LogUtils.e("hippy", "requireSubResource: " + e.getMessage());
            onResourceReady(null, mV8RuntimeId, resId);
          }
        });
      }
    });
  }

  private HippyArray bytesToArgument(ByteBuffer buffer) {
    HippyArray hippyParam = null;
    if (enableV8Serialization) {
      LogUtils.d("hippy_bridge", "bytesToArgument using Buffer");
      Object paramObj;
      try {
        final BinaryReader binaryReader;
        if (buffer.isDirect()) {
          if (safeDirectReader == null) {
            safeDirectReader = new SafeDirectReader();
          }
          binaryReader = safeDirectReader;
        } else {
          if (safeHeapReader == null) {
            safeHeapReader = new SafeHeapReader();
          }
          binaryReader = safeHeapReader;
        }
        binaryReader.reset(buffer);
        deserializer.setReader(binaryReader);
        deserializer.reset();
        deserializer.readHeader();
        paramObj = deserializer.readValue();
      } catch (Throwable e) {
        e.printStackTrace();
        LogUtils.e("compatible.Deserializer", "Error Parsing Buffer", e);
        return new HippyArray();
      }
      if (paramObj instanceof HippyArray) {
        hippyParam = (HippyArray) paramObj;
      }
    } else {
      LogUtils.d("hippy_bridge", "bytesToArgument using JSON");
      byte[] bytes;
      if (buffer.isDirect()) {
        bytes = new byte[buffer.limit()];
        buffer.get(bytes);
      } else {
        bytes = buffer.array();
      }
      hippyParam = ArgumentUtils.parseToArray(new String(bytes));
    }

    return hippyParam == null ? new HippyArray() : hippyParam;
  }

  public void reportException(String message, String stackTrace) {
    LogUtils.e("reportException", "!!!!!!!!!!!!!!!!!!!");

    LogUtils.e("reportException", message);
    LogUtils.e("reportException", stackTrace);

    if (mBridgeCallback != null) {
      mBridgeCallback.reportException(message, stackTrace);
    }
  }

  @Override
  public void onReceiveData(String msg) {
    if (this.mIsDevModule) {
      boolean isInspectMsg = Inspector.getInstance(mContext)
        .setWebSocketClient(mDebugWebSocketClient).dispatchReqFromFrontend(mContext, msg);
      if (!isInspectMsg) {
        callFunction("onWebsocketMsg", null, msg.getBytes(StandardCharsets.UTF_16LE));
      }
    }
  }
}
