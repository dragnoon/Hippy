#include "loader/debug_delegate.h"

#include "loader/adr_loader.h"
#include "loader/jni_delegate.h"

#include "bridge/runtime.h"
#include "jni/uri.h"
#include "jni/jni_register.h"
#include "jni/jni_utils.h"

using UriLoader = hippy::base::UriLoader;
using Delegate = UriLoader::Delegate;
using HippyFile = hippy::base::HippyFile;
using unicode_string_view = tdf::base::unicode_string_view;

static std::atomic<int64_t> global_request_id{0};
std::unordered_map<int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
    DebugDelegate::request_map_ =
    std::unordered_map < int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
        {};

DebugDelegate::DebugDelegate(std::shared_ptr<JavaRef> bridge): bridge_(bridge) {}

jobject GetDebugDelegateContentSync(JNIEnv *j_env,
                          jobject j_object,
                          jstring j_uri,
                          jlong j_runtime_id) {
  TDF_BASE_DLOG(INFO) << "GetDebugDelegateContentSync j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "GetDebugDelegateContentSync, j_runtime_id invalid";
    return nullptr;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "GetDebugDelegateContentSync, scope invalid";
    return nullptr;
  }

  std::shared_ptr<ADRLoader> loader = std::static_pointer_cast<ADRLoader>(scope->GetUriLoader());
  if (!loader) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentSync, loader invalid";
    return nullptr;
  }

  auto instance = JNIEnvironment::GetInstance();
  jmethodID j_method_id = instance->GetMethods().j_get_next_sync_method_id;
  if (j_method_id) {
    // todo proxy
    return j_env->CallObjectMethod(j_object, j_method_id, j_uri);
  }
  return JniDelegate::CreateJniUriResource(j_env,
                                           UriLoader::RetCode::ResourceNotFound,
                                           UriLoader::bytes());
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
             "getDebugDelegateContentSync",
             "(Ljava/lang/String;J)Lcom/tencent/mtt/hippy/bridge/HippyUriResource;",
             GetDebugDelegateContentSync)

void GetDebugDelegateAsync(JNIEnv *j_env,
                        jobject j_object,
                        jstring j_uri,
                        jlong j_runtime_id,
                        jobject j_callback) {
  TDF_BASE_DLOG(INFO) << "GetDebugDelegateAsync j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "GetDebugDelegateAsync, j_runtime_id invalid";
    return;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "GetDebugDelegateAsync, scope invalid";
    return;
  }

  std::shared_ptr<ADRLoader> loader = std::static_pointer_cast<ADRLoader>(scope->GetUriLoader());
  if (!loader) {
    TDF_BASE_DLOG(WARNING) << "GetDebugDelegateAsync, loader invalid";
    return;
  }
  std::shared_ptr<JavaRef> java_cb = std::make_shared<JavaRef>(j_env, j_callback);
  DebugDelegate::SetRequestCB([java_cb_ = std::move(java_cb)](UriLoader::RetCode ret_code,
      UriLoader::bytes content) {
    JNIEnv* j_env = JNIEnvironment::GetInstance()->AttachCurrentThread();
    jobject uri_resource = JniDelegate::CreateJniUriResource(j_env, ret_code, content);
    jmethodID j_method_id = nullptr; // todo 等max定好
    j_env->CallVoidMethod(java_cb_->GetObj(), j_method_id, uri_resource);
  });
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
             "getDebugDelegateAsync",
             "(Ljava/lang/String;JLcom/tencent/mtt/hippy/bridge/NativeCallback;)V",
             GetDebugDelegateAsync)

void OnNextReady(JNIEnv *j_env,
                     jobject j_object,
                     jobject j_uri_resource,
                     jlong j_runtime_id,
                     jlong j_request_id) {
  TDF_BASE_DLOG(INFO) << "OnNextReady j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "OnNextReady, j_runtime_id invalid";
    return;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "OnNextReady, scope invalid";
    return;
  }

  int64_t request_id = j_request_id;
  TDF_BASE_DLOG(INFO) << "request_id = " << request_id;
  auto cb = JniDelegate::GetRequestCB(request_id);
  if (!cb) {
    TDF_BASE_DLOG(WARNING) << "cb not found" << request_id;
    return;
  }
  if (!j_uri_resource) {
    TDF_BASE_DLOG(INFO) << "OnNextReady, resource null";
    cb(UriLoader::RetCode::Failed, UriLoader::bytes());
    return;
  }

  jclass j_cls = j_env->GetObjectClass(j_uri_resource);
  jfieldID code_field = j_env->GetFieldID(j_cls, "code",
                                          "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  jobject j_ret_code = j_env->GetObjectField(j_uri_resource, code_field);
  UriLoader::RetCode ret_code = JniDelegate::JavaEnumToCEnum(j_ret_code);
  jfieldID bytes_field = j_env->GetFieldID(j_cls, "content", "Ljava/nio/ByteBuffer;");
  jobject j_byte_buffer = j_env->GetObjectField(j_cls, bytes_field);
  if (!j_byte_buffer) {
    cb(ret_code, UriLoader::bytes());
    return;
  }
  int64_t len = (j_env)->GetDirectBufferCapacity(j_byte_buffer);
  if (len == -1) {
    cb(ret_code, UriLoader::bytes());
    return;
  }
  void *buff = (j_env)->GetDirectBufferAddress(j_byte_buffer);
  if (!buff) {
    cb(ret_code, UriLoader::bytes());
    return;
  }
  UriLoader::bytes str(reinterpret_cast<const char *>(buff), len);
  cb(ret_code, std::move(str));
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
             "onNextReady",
             "(Lcom/tencent/mtt/hippy/bridge/HippyUriResource;JJ)V",
             OnNextReady)

void DebugDelegate::NotifyJavaRegisterCoreDebugDelegate() {
  auto instance = JNIEnvironment::GetInstance();
  JNIEnv* j_env = instance->AttachCurrentThread();
  jmethodID j_method_id = instance->GetMethods().j_register_core_debug_delegate_method_id;
  if (j_method_id) {
    j_env->CallVoidMethod(bridge_->GetObj(), j_method_id);
  }
}

void NotifyCoreRegisterJavaDebugDelegate(JNIEnv *j_env,
    jobject j_object,
    jlong j_runtime_id) {
  TDF_BASE_DLOG(INFO) << "NotifyCoreRegisterJavaDebugDelegate j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "NotifyCoreRegisterJavaDebugDelegate, j_runtime_id invalid";
    return;
  }
  std::shared_ptr<DebugDelegate> debug_delegate = runtime->GetDebugDelegate();
  if (debug_delegate) {
    debug_delegate->SetJavaDebugDelegate(true);
  }
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
             "notifyCoreRegisterJavaDebugDelegate",
             "(J)V",
             NotifyCoreRegisterJavaDebugDelegate)

void DebugDelegate::RequestUntrustedContent(
    UriLoader::SyncContext &ctx,
    std::function<void(UriLoader::SyncContext&)> next) {
  if (has_java_debug_delegate_) {
    // 当 java 有注册 delegate 时，任何 c++ 请求都需要先询问 java 是否处理，
    // 同时提供对应 jni 方法可以让 java 获取 c++ 后续 delegate 返回
    auto instance = JNIEnvironment::GetInstance();
    JNIEnv* j_env = instance->AttachCurrentThread();
    jmethodID j_method_id = instance->GetMethods().j_get_next_sync_method_id;
    if (j_method_id) {
      jstring j_uri = JniUtils::StrViewToJString(j_env, ctx.uri);
      jobject j_next_object = j_env->CallObjectMethod(bridge_->GetObj(), j_method_id, j_uri);
      auto uri_resource = JniDelegate::ParseJniUriResource(j_env, j_next_object);
      ctx.ret_code = uri_resource.ret_code;
      ctx.content = std::move(uri_resource.content);
      return;
    }
  }
  if (next) {
    next(ctx);
  } else {
    ctx.ret_code = UriLoader::RetCode::SchemeNotRegister;
  }
}

void DebugDelegate::RequestUntrustedContent(
    UriLoader::ASyncContext &ctx,
    std::function<void(UriLoader::ASyncContext&)> next) {
  if (has_java_debug_delegate_) {
    auto instance = JNIEnvironment::GetInstance();
    JNIEnv* j_env = instance->AttachCurrentThread();
    jmethodID j_method_id = instance->GetMethods().j_get_next_async_method_id;
    if (j_method_id) {
      auto id = JniDelegate::SetRequestCB(ctx.cb);
      jstring j_uri = JniUtils::StrViewToJString(j_env, ctx.uri);
      j_env->CallVoidMethod(bridge_->GetObj(), j_method_id, j_uri, id);
      return;
    }
  }
  if (next) {
    next(ctx);
  } else {
    ctx.cb(UriLoader::RetCode::SchemeNotRegister, UriLoader::bytes());
  }
}

std::function<void(UriLoader::RetCode,
                   UriLoader::bytes)> DebugDelegate::GetRequestCB(int64_t request_id) {
  auto it = request_map_.find(request_id);
  return it != request_map_.end() ? it->second : nullptr;
}

int64_t DebugDelegate::SetRequestCB(std::function<void(UriLoader::RetCode, UriLoader::bytes)> cb) {
  int64_t id = global_request_id.fetch_add(1);
  request_map_.insert({id, cb});
  return id;
}
