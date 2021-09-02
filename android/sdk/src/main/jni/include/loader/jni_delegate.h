/*
 *
 * Tencent is pleased to support the open source community by making
 * Hippy available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include <android/asset_manager.h>

#include "core/core.h"
#include "jni/scoped_java_ref.h"


class JniDelegate : public hippy::base::UriLoader::Delegate {
 public:
  using unicode_string_view = tdf::base::unicode_string_view;
  using UriLoader = hippy::base::UriLoader;
  struct JniUriResourceWrapper {
    jclass j_clazz = nullptr;
    jobject j_success = nullptr;
    jobject j_failed = nullptr;
    jobject j_delegate_error = nullptr;
    jobject j_uri_error = nullptr;
    jobject j_scheme_error = nullptr;
    jobject j_scheme_not_register = nullptr;
    jobject j_path_not_match = nullptr;
    jobject j_path_error = nullptr;
    jobject j_resource_not_found = nullptr;
    jobject j_timeout = nullptr;
  };
  struct JniUriResource {
    UriLoader::RetCode ret_code;
    UriLoader::bytes content;
  };

  JniDelegate() = default;
  virtual ~JniDelegate() = default;

  static std::function<void(UriLoader::RetCode, UriLoader::bytes)> GetRequestCB(int64_t request_id);
  static int64_t SetRequestCB(std::function<void(UriLoader::RetCode, UriLoader::bytes)> cb);

  inline void SetBridge(std::shared_ptr<JavaRef> bridge) { bridge_ = bridge; }
  virtual void RequestUntrustedContent(
      UriLoader::SyncContext& ctx,
      std::function<void(UriLoader::SyncContext&)> next);
  virtual void RequestUntrustedContent(
      UriLoader::ASyncContext& ctx,
      std::function<void(UriLoader::ASyncContext&)> next);

  static inline const JniUriResourceWrapper& GetWrapper() {
    return wrapper_;
  }
  static inline void SetWrapper(const JniUriResourceWrapper& wrapper) {
    wrapper_ = wrapper;
  }
  static UriLoader::RetCode JavaEnumToCEnum(jobject j_ret_code);
  static jobject CEumToJavaEnum(UriLoader::RetCode ret_code);
  static bool Init(JNIEnv* j_env);
  static bool Destroy(JNIEnv* j_env);
  static jobject CreateJniUriResource(JNIEnv* j_env, UriLoader::RetCode ret_code,
                                      const UriLoader::bytes& content);
  static JniUriResource ParseJniUriResource(JNIEnv* j_env, jobject j_uri_resource);

 private:
  std::shared_ptr<JavaRef> bridge_;
  static JniUriResourceWrapper wrapper_;
  static std::unordered_map<int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
      request_map_;
};
