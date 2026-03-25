#include "handler.h"

#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace {

SimpleHandler* g_instance = nullptr;

std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false).ToString();
}

}  // namespace

SimpleHandler::SimpleHandler(bool is_alloy_style) : is_alloy_style_(is_alloy_style) {
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  } else if (is_alloy_style_) {
    PlatformTitleChange(browser, title);
  }
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  browser_list_.push_back(browser);

  std::string pending_id = GetAndClearPendingWindowId();
  if (!pending_id.empty()) {
    SetWindowId(browser, pending_id);
  }
}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (browser_list_.size() == 1) {
    is_closing_ = true;
  }
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  std::string window_id;
  {
    std::lock_guard<std::mutex> lock(window_map_mutex_);
    for (const auto& pair : window_id_browser_map_) {
      if (pair.second->IsSame(browser)) {
        window_id = pair.first;
        break;
      }
    }
  }

  BrowserList::iterator bit = browser_list_.begin();
  for (; bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (!window_id.empty()) {
    std::lock_guard<std::mutex> lock(close_callback_mutex_);
    if (window_close_callback_) {
      window_close_callback_(window_id);
    }
    RemoveWindowId(window_id);
  }

  if (browser_list_.empty()) {
    CefQuitMessageLoop();
  }
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  if (!is_alloy_style_) return;
  if (errorCode == ERR_ABORTED) return;

  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void SimpleHandler::ShowMainWindow() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::ShowMainWindow, this));
    return;
  }

  if (browser_list_.empty()) return;

  auto main_browser = browser_list_.front();

  if (auto browser_view = CefBrowserView::GetForBrowser(main_browser)) {
    if (auto window = browser_view->GetWindow()) {
      window->Show();
    }
  } else if (is_alloy_style_) {
    PlatformShowWindow(main_browser);
  }
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this, force_close));
    return;
  }

  if (browser_list_.empty()) return;

  for (const auto& browser : browser_list_) {
    browser->GetHost()->CloseBrowser(force_close);
  }
}

#if !defined(OS_MAC)
void SimpleHandler::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  // Not implemented on Linux
}
#endif

void SimpleHandler::SetWindowId(CefRefPtr<CefBrowser> browser, const std::string& window_id) {
  std::lock_guard<std::mutex> lock(window_map_mutex_);
  window_id_browser_map_[window_id] = browser;
}

CefRefPtr<CefBrowser> SimpleHandler::GetBrowserByWindowId(const std::string& window_id) {
  std::lock_guard<std::mutex> lock(window_map_mutex_);
  auto it = window_id_browser_map_.find(window_id);
  if (it != window_id_browser_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void SimpleHandler::RemoveWindowId(const std::string& window_id) {
  std::lock_guard<std::mutex> lock(window_map_mutex_);
  window_id_browser_map_.erase(window_id);
}

void SimpleHandler::SetPendingWindowId(const std::string& window_id) {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  pending_window_id_ = window_id;
}

std::string SimpleHandler::GetAndClearPendingWindowId() {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  std::string id = pending_window_id_;
  pending_window_id_.clear();
  return id;
}

void SimpleHandler::SetRendererMessageCallback(std::function<void(const std::string& window_id, const std::string& msg)> callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  renderer_message_callback_ = callback;
}

void SimpleHandler::SetWindowCloseCallback(std::function<void(const std::string& window_id)> callback) {
  std::lock_guard<std::mutex> lock(close_callback_mutex_);
  window_close_callback_ = callback;
}

bool SimpleHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                             CefRefPtr<CefFrame> frame,
                                             CefProcessId source_process,
                                             CefRefPtr<CefProcessMessage> message) {
  if (source_process != PID_RENDERER) return false;

  std::string msg_name = message->GetName().ToString();
  if (msg_name == "from-renderer") {
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    if (args && args->GetSize() >= 1) {
      std::string msg_content = args->GetString(0).ToString();

      std::string window_id;
      {
        std::lock_guard<std::mutex> lock(window_map_mutex_);
        for (const auto& pair : window_id_browser_map_) {
          if (pair.second->IsSame(browser)) {
            window_id = pair.first;
            break;
          }
        }
      }

      if (!window_id.empty()) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (renderer_message_callback_) {
          renderer_message_callback_(window_id, msg_content);
        }
      }
    }
    return true;
  }

  return false;
}