#include "app.h"
#include "handler.h"
#include "ipc.h"

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>

#if defined(CEF_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#include "include/base/cef_logging.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_closure_task.h"

#if defined(CEF_X11)
namespace {
int XErrorHandlerImpl(Display*, XErrorEvent*) { return 0; }
int XIOErrorHandlerImpl(Display*) { return 0; }
}
#endif

CefRefPtr<CombinedApp> g_app = nullptr;
IPCClient* g_ipc = nullptr;
std::mutex g_pending_mutex;
std::string g_pending_url;
std::string g_pending_id;
std::atomic<bool> g_window_creating{false};

void SendToBun(const std::string& msg) {
    if (g_ipc) g_ipc->send(msg);
}

void OnRendererMessage(const std::string& window_id, const std::string& msg) {
    SendToBun("ui:event:" + window_id + ":" + msg);
}

void OnWindowClose(const std::string& window_id) {
    SendToBun("window:closed:" + window_id);
}

void CreateWindowOnUIThread() {
    std::string id, url;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        if (g_pending_id.empty() || g_pending_url.empty()) {
            g_window_creating = false;
            return;
        }
        id = g_pending_id;
        url = g_pending_url;
        g_pending_id.clear();
        g_pending_url.clear();
    }
    
    g_app->CreateBrowserWindow(url, 800, 600, id);
    SendToBun("window:created:" + id);
    g_window_creating = false;
}

void PostMessageToRenderer(const std::string& window_id, const std::string& content) {
    SimpleHandler* handler = SimpleHandler::GetInstance();
    if (!handler) return;
    
    CefRefPtr<CefBrowser> browser = handler->GetBrowserByWindowId(window_id);
    if (!browser) {
        LOG(WARNING) << "No browser found for window_id: " << window_id;
        return;
    }
    
    CefRefPtr<CefProcessMessage> cef_msg = CefProcessMessage::Create("from-backend");
    cef_msg->GetArgumentList()->SetString(0, content);
    browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, cef_msg);
}

class ClosureTask : public CefTask {
public:
    explicit ClosureTask(std::function<void()> fn) : fn_(std::move(fn)) {}
    void Execute() override { fn_(); }
private:
    std::function<void()> fn_;
    IMPLEMENT_REFCOUNTING(ClosureTask);
};

void PostClosureToUI(std::function<void()> fn) {
    CefPostTask(TID_UI, new ClosureTask(fn));
}

void OnIPCMessage(const std::string& msg);

#if defined(CEF_X11)
void SimpleHandler::PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    std::string titleStr(title);
    ::Display* display = cef_get_xdisplay();
    if (!display) return;

    ::Window window = browser->GetHost()->GetWindowHandle();
    if (window == kNullWindowHandle) return;

    const char* kAtoms[] = {"_NET_WM_NAME", "UTF8_STRING"};
    Atom atoms[2];
    int result = XInternAtoms(display, const_cast<char**>(kAtoms), 2, false, atoms);
    if (!result) return;

    XChangeProperty(display, window, atoms[0], atoms[1], 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(titleStr.c_str()),
                    titleStr.size());
    XStoreName(display, window, titleStr.c_str());
}
#endif

NO_STACK_PROTECTOR
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    CefRefPtr<CombinedApp> app(new CombinedApp());
    
    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0) return exit_code;

#if defined(CEF_X11)
    XSetErrorHandler(XErrorHandlerImpl);
    XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

    CefRefPtr<CefCommandLine> cmd = CefCommandLine::CreateCommandLine();
    cmd->InitFromArgv(argc, argv);

    std::string socket_path = cmd->GetSwitchValue("socket-path").ToString();
    
    g_app = app;
    app->SetAutoCreateBrowser(!cmd->HasSwitch("no-auto-browser"));

    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;

    if (!CefInitialize(main_args, settings, app.get(), nullptr))
        return CefGetExitCode();

    std::unique_ptr<IPCClient> ipc;
    if (!socket_path.empty()) {
        ipc = std::make_unique<IPCClient>(socket_path);
        if (ipc->connect()) {
            g_ipc = ipc.get();
            ipc->setMessageCallback(&OnIPCMessage);
            SendToBun("hello");
        }
    }

    CefRunMessageLoop();
    
    g_ipc = nullptr;
    g_app = nullptr;
    CefShutdown();
    return 0;
}

void OnIPCMessage(const std::string& msg) {
    LOG(INFO) << "IPC: " << msg;
    
    if (msg == "app:quit") {
        PostClosureToUI([]() {
            SimpleHandler::GetInstance()->CloseAllBrowsers(false);
        });
        return;
    }
    
    if (msg.rfind("window:create:", 0) == 0) {
        size_t first = 14;
        size_t second = msg.find(':', first);
        if (second == std::string::npos) return;
        
        std::string id = msg.substr(first, second - first);
        std::string url = msg.substr(second + 1);
        
        {
            std::lock_guard<std::mutex> lock(g_pending_mutex);
            g_pending_url = url;
            g_pending_id = id;
        }
        
        PostClosureToUI(&CreateWindowOnUIThread);
        return;
    }
    
    if (msg.rfind("window:post:", 0) == 0) {
        size_t first = 12;
        size_t second = msg.find(':', first);
        if (second == std::string::npos) return;
        
        std::string id = msg.substr(first, second - first);
        std::string content = msg.substr(second + 1);
        LOG(INFO) << "Forward to renderer [" << id << "]: " << content;
        
        std::string captured_id = id;
        std::string captured_content = content;
        PostClosureToUI([captured_id, captured_content]() {
            PostMessageToRenderer(captured_id, captured_content);
        });
        return;
    }
    
    if (msg.rfind("window:close:", 0) == 0) {
        std::string id = msg.substr(13);
        LOG(INFO) << "Close window [" << id << "]";
        
        std::string captured_id = id;
        PostClosureToUI([captured_id]() {
            SimpleHandler* handler = SimpleHandler::GetInstance();
            if (!handler) return;
            
            CefRefPtr<CefBrowser> browser = handler->GetBrowserByWindowId(captured_id);
            if (browser) {
                browser->GetHost()->CloseBrowser(false);
            }
        });
        return;
    }
    
    LOG(WARNING) << "Unknown message: " << msg;
}