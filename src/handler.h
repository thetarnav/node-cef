#ifndef BUN_CEF_HANDLER_H_
#define BUN_CEF_HANDLER_H_

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

  static SimpleHandler* GetInstance();

  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  void SetWindowId(CefRefPtr<CefBrowser> browser, const std::string& window_id);
  CefRefPtr<CefBrowser> GetBrowserByWindowId(const std::string& window_id);
  void RemoveWindowId(const std::string& window_id);
  void SetPendingWindowId(const std::string& window_id);
  std::string GetAndClearPendingWindowId();

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  void ShowMainWindow();
  void CloseAllBrowsers(bool force_close);

  void SetRendererMessageCallback(std::function<void(const std::string& window_id, const std::string& msg)> callback);
  void SetWindowCloseCallback(std::function<void(const std::string& window_id)> callback);

  bool IsClosing() const { return is_closing_; }

 private:
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);

  std::map<std::string, CefRefPtr<CefBrowser>> window_id_browser_map_;
  std::mutex window_map_mutex_;

  std::string pending_window_id_;
  std::mutex pending_mutex_;

  std::function<void(const std::string& window_id, const std::string& msg)> renderer_message_callback_;
  std::mutex callback_mutex_;
  
  std::function<void(const std::string& window_id)> window_close_callback_;
  std::mutex close_callback_mutex_;

  const bool is_alloy_style_;
  typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
  BrowserList browser_list_;
  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(SimpleHandler);
};

#endif