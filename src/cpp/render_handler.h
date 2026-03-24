#ifndef CEF_TESTS_CEFSIMPLE_RENDER_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_RENDER_HANDLER_H_

#include "include/cef_app.h"
#include "include/cef_v8.h"

class RenderHandler : public CefApp,
                      public CefRenderProcessHandler {
 public:
  RenderHandler() = default;

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

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

#endif
