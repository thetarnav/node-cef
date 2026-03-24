// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_

#include "include/cef_app.h"
#include <string>

void OnRendererMessage(const std::string& window_id, const std::string& msg);
void OnWindowClose(const std::string& window_id);

class SimpleApp : public CefBrowserProcessHandler {
 public:
  SimpleApp();

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() { return this; }
  CefRefPtr<CefClient> GetDefaultClient();

  void OnContextInitialized() override;
  
  void SetAutoCreateBrowser(bool auto_create) { auto_create_browser_ = auto_create; }
  void CreateBrowserWindow(const std::string& url, int width, int height, const std::string& window_id);

 private:
  bool auto_create_browser_ = true;
  IMPLEMENT_REFCOUNTING(SimpleApp);
};

#endif
