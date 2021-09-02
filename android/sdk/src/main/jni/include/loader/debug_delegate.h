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

class DebugDelegate : public hippy::base::UriLoader::Delegate {
 public:
  using unicode_string_view = tdf::base::unicode_string_view;
  using UriLoader = hippy::base::UriLoader;

  DebugDelegate(std::shared_ptr<JavaRef> bridge);
  virtual ~DebugDelegate() = default;

  virtual void RequestUntrustedContent(
      UriLoader::SyncContext &ctx,
      std::function<void(UriLoader::SyncContext&)> next);
  virtual void RequestUntrustedContent(
      UriLoader::ASyncContext &ctx,
      std::function<void(UriLoader::ASyncContext&)> next);

  inline bool HasJavaDebugDelegate() {
    return has_java_debug_delegate_;
  }
  inline void SetJavaDebugDelegate(bool has_java_debug_delegate) {
    has_java_debug_delegate_ = has_java_debug_delegate;
  }

  void NotifyJavaRegisterCoreDebugDelegate();
  static std::function<void(UriLoader::RetCode, UriLoader::bytes)> GetRequestCB(int64_t request_id);
  static int64_t SetRequestCB(std::function<void(UriLoader::RetCode, UriLoader::bytes)> cb);
 private:
  bool has_java_debug_delegate_;
  std::shared_ptr<JavaRef> bridge_;
  static std::unordered_map<int64_t, std::function<void(UriLoader::RetCode, UriLoader::bytes)>>
      request_map_;
};
