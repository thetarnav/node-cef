#include "app.h"
#include "handler.h"

#include <string>
#include <sstream>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

// SimpleApp implementation

namespace {
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                       cef_runtime_style_t runtime_style,
                       cef_show_state_t initial_show_state)
      : browser_view_(browser_view),
        runtime_style_(runtime_style),
        initial_show_state_(initial_show_state) {}

  SimpleWindowDelegate(const SimpleWindowDelegate&) = delete;
  SimpleWindowDelegate& operator=(const SimpleWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    if (initial_show_state_ != CEF_SHOW_STATE_HIDDEN) {
      window->Show();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return initial_show_state_;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  const cef_runtime_style_t runtime_style_;
  const cef_show_state_t initial_show_state_;
  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
};

class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit SimpleBrowserViewDelegate(cef_runtime_style_t runtime_style)
      : runtime_style_(runtime_style) {}

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        popup_browser_view, runtime_style_, CEF_SHOW_STATE_NORMAL));
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  const cef_runtime_style_t runtime_style_;
  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
};
}  // namespace

SimpleApp::SimpleApp() = default;

void SimpleApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  if (!auto_create_browser_) {
    return;
  }

  CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
  bool use_alloy_style = command_line->HasSwitch("use-alloy-style");
  if (use_alloy_style) {
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  CefRefPtr<SimpleHandler> handler(new SimpleHandler(use_alloy_style));
  CefBrowserSettings browser_settings;

  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://www.google.com";
  }

  const bool use_views = !command_line->HasSwitch("use-native");

  if (use_views) {
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        handler, url, browser_settings, nullptr, nullptr,
        new SimpleBrowserViewDelegate(runtime_style));

    cef_show_state_t initial_show_state = CEF_SHOW_STATE_NORMAL;
    const std::string& show_state_value = command_line->GetSwitchValue("initial-show-state");
    if (show_state_value == "minimized") {
      initial_show_state = CEF_SHOW_STATE_MINIMIZED;
    } else if (show_state_value == "maximized") {
      initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
    }

    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        browser_view, runtime_style, initial_show_state));
  } else {
    CefWindowInfo window_info;
    window_info.runtime_style = runtime_style;
    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);
  }
}

CefRefPtr<CefClient> SimpleApp::GetDefaultClient() {
  return SimpleHandler::GetInstance();
}

void SimpleApp::CreateBrowserWindow(const std::string& url_or_html,
                                    int width,
                                    int height,
                                    const std::string& window_id) {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
  bool use_alloy_style = command_line->HasSwitch("use-alloy-style");
  cef_runtime_style_t runtime_style =
      use_alloy_style ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;

  CefRefPtr<SimpleHandler> handler(new SimpleHandler(use_alloy_style));
  handler->SetPendingWindowId(window_id);
  handler->SetRendererMessageCallback(&OnRendererMessage);
  handler->SetWindowCloseCallback(&OnWindowClose);

  CefBrowserSettings browser_settings;

  std::string url = url_or_html;
  if (url_or_html.find("<") != std::string::npos) {
    url = "data:text/html;charset=utf-8," + CefURIEncode(url_or_html, false).ToString();
  }

  const bool use_views = !command_line->HasSwitch("use-native");

  if (use_views) {
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        handler, url, browser_settings, nullptr, nullptr,
        new SimpleBrowserViewDelegate(runtime_style));

    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        browser_view, runtime_style, CEF_SHOW_STATE_NORMAL));
  } else {
    CefWindowInfo window_info;
    window_info.runtime_style = runtime_style;
    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);
  }
}

// RenderHandler implementation

CefRefPtr<CefV8Value> RenderHandler::on_message_callback_ = nullptr;

void RenderHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();

  CefRefPtr<CefV8Value> global = context->GetGlobal();

  CefRefPtr<CefV8Handler> send_handler = new SendV8Handler();
  CefRefPtr<CefV8Value> send_func = CefV8Value::CreateFunction("sendToNative", send_handler);
  global->SetValue("sendToNative", send_func, V8_PROPERTY_ATTRIBUTE_NONE);

  CefRefPtr<CefV8Value> on_message = CefV8Value::CreateUndefined();
  global->SetValue("onMessage", on_message, V8_PROPERTY_ATTRIBUTE_NONE);
}

bool RenderHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefProcessId source_process,
                                            CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RENDERER_THREAD();

  if (message->GetName() == "from-backend") {
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    std::string json_str = args->GetString(0).ToString();

    CefRefPtr<CefV8Context> context = frame->GetV8Context();
    context->Enter();

    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<CefV8Value> callback = global->GetValue("onMessage");

    if (callback && callback->IsFunction()) {
      CefV8ValueList argv;
      argv.push_back(CefV8Value::CreateString(json_str));
      callback->ExecuteFunction(nullptr, argv);
    }

    context->Exit();
    return true;
  }

  return false;
}

bool SendV8Handler::Execute(const CefString& name,
                            CefRefPtr<CefV8Value> object,
                            const CefV8ValueList& arguments,
                            CefRefPtr<CefV8Value>& retval,
                            CefString& exception) {
  CEF_REQUIRE_RENDERER_THREAD();

  if (arguments.size() > 0 && arguments[0]->IsString()) {
    std::string msg = arguments[0]->GetStringValue().ToString();
    CefRefPtr<CefProcessMessage> out_msg = CefProcessMessage::Create("from-renderer");
    out_msg->GetArgumentList()->SetString(0, msg);
    CefV8Context::GetCurrentContext()->GetBrowser()->GetMainFrame()->SendProcessMessage(
        PID_BROWSER, out_msg);
  } else if (arguments.size() > 0) {
    exception = "Argument must be a string";
  } else {
    exception = "Missing argument";
  }

  return true;
}