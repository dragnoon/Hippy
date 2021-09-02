#include "loader/jni_delegate.h"

#include <utility>
#include "jni/jni_register.h"
#include "bridge/runtime.h"
#include "loader/adr_loader.h"

using unicode_string_view = tdf::base::unicode_string_view;
using UriLoader = hippy::base::UriLoader;
using RetCode = UriLoader::RetCode;

static std::atomic<int64_t> global_request_id{0};
std::unordered_map<int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
    JniDelegate::request_map_ =
    std::unordered_map < int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
{};
JniDelegate::JniUriResourceWrapper JniDelegate::wrapper_ = {};

UriLoader::RetCode JniDelegate::JavaEnumToCEnum(jobject j_ret_code) {
  const JniUriResourceWrapper &wrapper = JniDelegate::GetWrapper();
  if (j_ret_code == wrapper.j_success) {
    return RetCode::Success;
  } else if (j_ret_code == wrapper.j_failed) {
    return RetCode::Failed;
  } else if (j_ret_code == wrapper.j_delegate_error) {
    return RetCode::DelegateError;
  } else if (j_ret_code == wrapper.j_uri_error) {
    return RetCode::UriError;
  } else if (j_ret_code == wrapper.j_scheme_error) {
    return RetCode::SchemeError;
  } else if (j_ret_code == wrapper.j_scheme_not_register) {
    return RetCode::SchemeNotRegister;
  } else if (j_ret_code == wrapper.j_path_not_match) {
    return RetCode::PathNotMatch;
  } else if (j_ret_code == wrapper.j_path_error) {
    return RetCode::PathError;
  } else if (j_ret_code == wrapper.j_resource_not_found) {
    return RetCode::ResourceNotFound;
  } else if (j_ret_code == wrapper.j_timeout) {
    return RetCode::Timeout;
  }
  TDF_BASE_DCHECK(false) << "j_ret_code error";
  return RetCode::Failed;
}

jobject JniDelegate::CEumToJavaEnum(RetCode ret_code) {
  auto wrapper = JniDelegate::GetWrapper();
  if (ret_code == RetCode::Success) {
    return wrapper.j_success;
  } else if (ret_code == RetCode::Failed) {
    return wrapper.j_failed;
  } else if (ret_code == RetCode::DelegateError) {
    return wrapper.j_delegate_error;
  } else if (ret_code == RetCode::UriError) {
    return wrapper.j_uri_error;
  } else if (ret_code == RetCode::SchemeError) {
    return wrapper.j_scheme_error;
  } else if (ret_code == RetCode::SchemeNotRegister) {
    return wrapper.j_scheme_not_register;
  } else if (ret_code == RetCode::PathNotMatch) {
    return wrapper.j_path_not_match;
  } else if (ret_code == RetCode::PathError) {
    return wrapper.j_path_error;
  } else if (ret_code == RetCode::ResourceNotFound) {
    return wrapper.j_resource_not_found;
  } else if (ret_code == RetCode::Timeout) {
    return wrapper.j_timeout;
  }

  return wrapper.j_failed;
}

bool JniDelegate::Init(JNIEnv *j_env) {
  JniUriResourceWrapper wrapper;
  jclass j_cls =
      j_env->FindClass("com/tencent/mtt/hippy/bridge/HippyUriResource$RetCode");
  wrapper.j_clazz = (jclass) j_env->NewGlobalRef(j_cls);
  jfieldID j_success_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "Success",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_success =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_success_method_id));
  jfieldID j_failed_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "Failed",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_failed =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_failed_method_id));
  jfieldID j_delegate_error_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "DelegateError",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_delegate_error =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz,
                                                      j_delegate_error_method_id));
  jfieldID j_uri_error_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "UriError",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_uri_error =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_uri_error_method_id));
  jfieldID j_scheme_error_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "SchemeError",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_scheme_error =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_scheme_error_method_id));
  jfieldID j_scheme_not_register_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "SchemeNotRegister",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_scheme_not_register =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz,
                                                      j_scheme_not_register_method_id));
  jfieldID j_path_not_match_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "PathNotMatch",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_path_not_match =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz,
                                                      j_path_not_match_method_id));
  jfieldID j_path_error_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "PathError",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_path_error =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_path_error_method_id));
  jfieldID j_resource_not_found_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "ResourceNotFound",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_resource_not_found =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz,
                                                      j_resource_not_found_method_id));
  jfieldID j_timeout_method_id =
      j_env->GetStaticFieldID(wrapper.j_clazz, "Timeout",
                              "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  wrapper.j_timeout =
      j_env->NewGlobalRef(j_env->GetStaticObjectField(wrapper.j_clazz, j_timeout_method_id));

  JniDelegate::SetWrapper(wrapper);
  return true;
}

bool JniDelegate::Destroy(JNIEnv *j_env) {
  auto wrapper = JniDelegate::GetWrapper();
  j_env->DeleteGlobalRef(wrapper.j_success);
  j_env->DeleteGlobalRef(wrapper.j_failed);
  j_env->DeleteGlobalRef(wrapper.j_delegate_error);
  j_env->DeleteGlobalRef(wrapper.j_uri_error);
  j_env->DeleteGlobalRef(wrapper.j_scheme_error);
  j_env->DeleteGlobalRef(wrapper.j_scheme_not_register);
  j_env->DeleteGlobalRef(wrapper.j_path_not_match);
  j_env->DeleteGlobalRef(wrapper.j_path_error);
  j_env->DeleteGlobalRef(wrapper.j_resource_not_found);
  j_env->DeleteGlobalRef(wrapper.j_timeout);
  j_env->DeleteLocalRef(wrapper.j_clazz);

  return true;
}

void JniDelegate::RequestUntrustedContent(
    UriLoader::SyncContext &ctx,
    std::function<void(UriLoader::SyncContext & )> next) {
  if (ctx.src_type != UriLoader::SrcType::Core) {
    ctx.ret_code = UriLoader::RetCode::ResourceNotFound;
    return;
  }
  std::shared_ptr<JNIEnvironment> instance = JNIEnvironment::GetInstance();
  JNIEnv *j_env = instance->AttachCurrentThread();
  if (instance->GetMethods().j_fetch_resource_sync_method_id) {
    jstring j_relative_path = JniUtils::StrViewToJString(j_env, ctx.uri);
    jobject ret = j_env->CallObjectMethod(bridge_->GetObj(),
                                          instance->GetMethods().j_fetch_resource_sync_method_id,
                                          j_relative_path, true);
    j_env->DeleteLocalRef(j_relative_path);
    jclass j_cls = j_env->GetObjectClass(ret);
    jfieldID code_field = j_env->GetFieldID(j_cls, "code",
                                            "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
    jobject j_ret_code = j_env->GetObjectField(ret, code_field);
    RetCode ret_code = JniDelegate::JavaEnumToCEnum(j_ret_code);
    if (ret_code != UriLoader::RetCode::PathNotMatch) {
      jfieldID bytes_field = j_env->GetFieldID(j_cls, "content", "Ljava/nio/ByteBuffer;");
      jobject j_byte_buffer = j_env->GetObjectField(j_cls, bytes_field);
      if (!j_byte_buffer) {
        TDF_BASE_DLOG(INFO) << "RequestUntrustedContent sync, buff null";
        ctx.ret_code = UriLoader::RetCode::Success;
        ctx.content = UriLoader::bytes();
        return;
      }
      int64_t len = (j_env)->GetDirectBufferCapacity(j_byte_buffer);
      if (len == -1) {
        TDF_BASE_DLOG(ERROR) << "RequestUntrustedContent sync, BufferCapacity error";
        ctx.ret_code = UriLoader::RetCode::DelegateError;
        return;
      }
      void *buff = (j_env)->GetDirectBufferAddress(j_byte_buffer);
      if (!buff) {
        TDF_BASE_DLOG(INFO) << "RequestUntrustedContent sync, buff null";
        ctx.ret_code = UriLoader::RetCode::DelegateError;
        return;
      }
      UriLoader::bytes str(reinterpret_cast<const char *>(buff), len);
      ctx.ret_code = UriLoader::RetCode::Success;
      ctx.content = std::move(str);
    } else {
      next(ctx);
    }
  }
  ctx.ret_code = UriLoader::RetCode::DelegateError;
}
void JniDelegate::RequestUntrustedContent(
    UriLoader::ASyncContext &ctx,
    std::function<void(UriLoader::ASyncContext & )> next) {
  if (ctx.src_type != UriLoader::SrcType::Core) {
    ctx.cb(UriLoader::RetCode::ResourceNotFound, UriLoader::bytes());
    return;
  }
  auto new_cb = [&ctx, next](UriLoader::RetCode ret_code, UriLoader::bytes content) {
    if (ret_code != UriLoader::RetCode::PathNotMatch) {
      ctx.cb(ret_code, std::move(content));
    } else {
      next(ctx);
    }
  };
  std::shared_ptr<JNIEnvironment> instance = JNIEnvironment::GetInstance();
  JNIEnv *j_env = instance->AttachCurrentThread();
  if (instance->GetMethods().j_fetch_resource_async_method_id) {
    int64_t id = SetRequestCB(new_cb);
    jstring j_relative_path = JniUtils::StrViewToJString(j_env, ctx.uri);
    j_env->CallVoidMethod(bridge_->GetObj(),
                          instance->GetMethods().j_fetch_resource_async_method_id,
                          j_relative_path, id,
                          true);
    j_env->DeleteLocalRef(j_relative_path);
    return;
  }
  ctx.cb(UriLoader::RetCode::DelegateError, UriLoader::bytes());
}

void OnResourceReady(JNIEnv *j_env,
                     jobject j_object,
                     jobject j_uri_resource,
                     jlong j_runtime_id,
                     jlong j_request_id) {
  TDF_BASE_DLOG(INFO) << "onResourceReady j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "onResourceReady, j_runtime_id invalid";
    return;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "onResourceReady, scope invalid";
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
    TDF_BASE_DLOG(INFO) << "onResourceReady, resource null";
    cb(UriLoader::RetCode::Failed, UriLoader::bytes());
    return;
  }

  jclass j_cls = j_env->GetObjectClass(j_uri_resource);
  jfieldID code_field = j_env->GetFieldID(j_cls, "code",
                                          "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  jobject j_ret_code = j_env->GetObjectField(j_uri_resource, code_field);
  RetCode ret_code = JniDelegate::JavaEnumToCEnum(j_ret_code);
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
"onResourceReady",
"(Lcom/tencent/mtt/hippy/bridge/HippyUriResource;JJ)V",
OnResourceReady)

jobject GetUriContentSync(JNIEnv *j_env,
                          jobject j_object,
                          jstring j_uri,
                          jlong j_runtime_id) {
  TDF_BASE_DLOG(INFO) << "GetUriContentSync j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentSync, j_runtime_id invalid";
    return nullptr;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentSync, scope invalid";
    return nullptr;
  }

  std::shared_ptr<ADRLoader> loader = std::static_pointer_cast<ADRLoader>(scope->GetUriLoader());
  if (!loader) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentSync, loader invalid";
    return nullptr;
  }
  UriLoader::bytes content;
  UriLoader::RetCode ret_code = loader->RequestUntrustedContent(
      JniUtils::ToStrView(j_env, j_uri), content, UriLoader::SrcType::NotCore);
  return JniDelegate::CreateJniUriResource(j_env, ret_code, content);
}

jobject JniDelegate::CreateJniUriResource(JNIEnv *j_env,
                                          UriLoader::RetCode ret_code,
                                          const UriLoader::bytes &content) {
  jobject j_ret_code = JniDelegate::CEumToJavaEnum(ret_code);
  JniDelegate::JniUriResourceWrapper wrapper = JniDelegate::GetWrapper();
  jmethodID j_constructor = j_env->GetMethodID(wrapper.j_clazz, "<init>",
                                               "(Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;Ljava/nio/ByteBuffer;)V");
  jobject j_buffer = j_env->NewByteArray(content.length());
  j_env->SetByteArrayRegion(
      reinterpret_cast<jbyteArray>(j_buffer), 0, content.length(),
      reinterpret_cast<const jbyte *>(content.c_str()));
  return j_env->NewObject(wrapper.j_clazz, j_constructor, j_ret_code, j_buffer);
}

JniDelegate::JniUriResource JniDelegate::ParseJniUriResource(JNIEnv* j_env, jobject j_uri_resource) {
  jclass j_cls = j_env->GetObjectClass(j_uri_resource);
  jfieldID code_field = j_env->GetFieldID(j_cls, "code",
                                          "Lcom/tencent/mtt/hippy/bridge/HippyUriResource$RetCode;");
  jobject j_ret_code = j_env->GetObjectField(j_uri_resource, code_field);
  RetCode ret_code = JniDelegate::JavaEnumToCEnum(j_ret_code);
  jfieldID bytes_field = j_env->GetFieldID(j_cls, "content", "Ljava/nio/ByteBuffer;");
  jobject j_byte_buffer = j_env->GetObjectField(j_cls, bytes_field);
  UriLoader::bytes content;
  if (!j_byte_buffer) {
    return {
      ret_code, content
    };
  }
  int64_t len = (j_env)->GetDirectBufferCapacity(j_byte_buffer);
  if (len == -1) {
    return {
        ret_code, content
    };
  }
  void *buff = (j_env)->GetDirectBufferAddress(j_byte_buffer);
  if (!buff) {
    return {
        ret_code, content
    };
  }
  UriLoader::bytes str(reinterpret_cast<const char *>(buff), len);
  return {
    ret_code, std::move(str)
  };
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
"getUriContentSync",
"(Ljava/lang/String;J)Lcom/tencent/mtt/hippy/bridge/HippyUriResource;",
GetUriContentSync)

void GetUriContentASync(JNIEnv *j_env,
                          jobject j_object,
                          jstring j_uri,
                          jlong j_runtime_id,
                          jobject j_callback) {
  TDF_BASE_DLOG(INFO) << "GetUriContentASync j_runtime_id = "
                      << j_runtime_id;
  std::shared_ptr<Runtime> runtime = Runtime::Find(j_runtime_id);
  if (!runtime) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentASync, j_runtime_id invalid";
    return;
  }
  std::shared_ptr<Scope> scope = runtime->GetScope();
  if (!scope) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentASync, scope invalid";
    return;
  }

  std::shared_ptr<ADRLoader> loader = std::static_pointer_cast<ADRLoader>(scope->GetUriLoader());
  if (!loader) {
    TDF_BASE_DLOG(WARNING) << "GetUriContentASync, loader invalid";
    return;
  }
  std::shared_ptr<JavaRef> java_cb = std::make_shared<JavaRef>(j_env, j_callback);
  std::function<void(RetCode, UriLoader::bytes)>
      cb = [java_cb_ = std::move(java_cb)](RetCode ret_code,
                                           UriLoader::bytes content) {
    auto instance = JNIEnvironment::GetInstance();
    JNIEnv *j_env = instance->AttachCurrentThread();
    jobject j_uri_resource = JniDelegate::CreateJniUriResource(j_env, ret_code, content);
    jclass j_class = j_env->GetObjectClass(java_cb_->GetObj());
    if (!j_class) {
      TDF_BASE_LOG(ERROR) << "OnGetUriContentASync j_class error";
      return;
    }
    jmethodID j_cb_id =
        j_env->GetMethodID(j_class, "Callback",
                           "(Lcom/tencent/mtt/hippy/bridge/HippyUriResource;)V");
    if (!j_cb_id) {
      TDF_BASE_LOG(ERROR) << "OnGetUriContentASync j_cb_id error";
      return;
    }
    j_env->CallVoidMethod(java_cb_->GetObj(), j_cb_id, j_uri_resource);
    JNIEnvironment::ClearJEnvException(j_env);
    if (j_class) {
      j_env->DeleteLocalRef(j_class);
    }
  };
  loader->RequestUntrustedContent(JniUtils::ToStrView(j_env, j_uri), cb,
                                  UriLoader::SrcType::NotCore);
}

REGISTER_JNI("com/tencent/mtt/hippy/bridge/HippyBridgeImpl",
"getUriContentASync",
"(Ljava/lang/String;JLcom/tencent/mtt/hippy/bridge/NativeCallback;)V",
GetUriContentASync)

std::function<void(UriLoader::RetCode,
                   UriLoader::bytes)> JniDelegate::GetRequestCB(int64_t request_id) {
  auto it = request_map_.find(request_id);
  return it != request_map_.end() ? it->second : nullptr;
}

int64_t JniDelegate::SetRequestCB(std::function<void(UriLoader::RetCode, UriLoader::bytes)> cb) {
  int64_t id = global_request_id.fetch_add(1);
  request_map_.insert({id, cb});
  return id;
}
