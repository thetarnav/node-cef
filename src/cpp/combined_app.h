#ifndef CEF_TESTS_CEFSIMPLE_COMBINED_APP_H_
#define CEF_TESTS_CEFSIMPLE_COMBINED_APP_H_

#include "include/cef_app.h"
#include "render_handler.h"
#include "simple_app.h"

class CombinedApp : public CefApp,
                    public CefBrowserProcessHandler {
 public:
  CombinedApp() : browser_app_(new SimpleApp()), render_handler_(new RenderHandler()) {}

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return browser_app_;
  }

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return render_handler_;
  }

  void SetAutoCreateBrowser(bool auto_create) {
    browser_app_->SetAutoCreateBrowser(auto_create);
  }

  void CreateBrowserWindow(const std::string& url, int width, int height, const std::string& window_id) {
    browser_app_->CreateBrowserWindow(url, width, height, window_id);
  }

 private:
  CefRefPtr<SimpleApp> browser_app_;
  CefRefPtr<RenderHandler> render_handler_;

  IMPLEMENT_REFCOUNTING(CombinedApp);
};

#endif
