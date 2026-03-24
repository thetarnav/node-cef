// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include <list>
#include <map>
#include <mutex>
#include <functional>

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  explicit SimpleHandler(bool is_alloy_style);
  ~SimpleHandler() override;

  // Provide access to the single global instance of this object.
  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // Window ID management
  void SetWindowId(CefRefPtr<CefBrowser> browser, const std::string& window_id);
  CefRefPtr<CefBrowser> GetBrowserByWindowId(const std::string& window_id);
  void RemoveWindowId(const std::string& window_id);
  void SetPendingWindowId(const std::string& window_id);
  std::string GetAndClearPendingWindowId();

  // CefClient method - handle process messages from renderer
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  void ShowMainWindow();

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  // Set callback for messages from renderer
  void SetRendererMessageCallback(std::function<void(const std::string& window_id, const std::string& msg)> callback);
  
  // Set callback for window close events
  void SetWindowCloseCallback(std::function<void(const std::string& window_id)> callback);

  bool IsClosing() const { return is_closing_; }

 private:
  // Platform-specific implementation.
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);

  // Window ID to browser mapping
  std::map<std::string, CefRefPtr<CefBrowser>> window_id_browser_map_;
  std::mutex window_map_mutex_;

  // Pending window ID for next browser creation
  std::string pending_window_id_;
  std::mutex pending_mutex_;

  // Callback for messages from renderer
  std::function<void(const std::string& window_id, const std::string& msg)> renderer_message_callback_;
  std::mutex callback_mutex_;
  
  // Callback for window close events
  std::function<void(const std::string& window_id)> window_close_callback_;
  std::mutex close_callback_mutex_;

  // True if this client is Alloy style, otherwise Chrome style.
  const bool is_alloy_style_;

  // List of existing browser windows. Only accessed on the CEF UI thread.
  typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
  BrowserList browser_list_;

  bool is_closing_ = false;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
