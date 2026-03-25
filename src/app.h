#ifndef BUN_CEF_APP_H_
#define BUN_CEF_APP_H_

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

class RenderHandler : public CefApp, public CefRenderProcessHandler {
 public:
  RenderHandler() = default;

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  static CefRefPtr<CefV8Value> on_message_callback_;
  IMPLEMENT_REFCOUNTING(RenderHandler);
};

class SendV8Handler : public CefV8Handler {
 public:
  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override;
  IMPLEMENT_REFCOUNTING(SendV8Handler);
};

class CombinedApp : public CefApp, public CefBrowserProcessHandler {
 public:
  CombinedApp() : browser_app_(new SimpleApp()), render_handler_(new RenderHandler()) {}

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return browser_app_; }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return render_handler_; }

  void SetAutoCreateBrowser(bool auto_create) { browser_app_->SetAutoCreateBrowser(auto_create); }
  void CreateBrowserWindow(const std::string& url, int width, int height, const std::string& window_id) {
    browser_app_->CreateBrowserWindow(url, width, height, window_id);
  }

 private:
  CefRefPtr<SimpleApp> browser_app_;
  CefRefPtr<RenderHandler> render_handler_;
  IMPLEMENT_REFCOUNTING(CombinedApp);
};

#endif