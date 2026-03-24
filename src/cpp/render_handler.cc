#include "render_handler.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_parser.h"

CefRefPtr<CefV8Value> RenderHandler::on_message_callback_ = nullptr;

void RenderHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();

  CefRefPtr<CefV8Value> global = context->GetGlobal();

  CefRefPtr<CefV8Handler> send_handler = new SendV8Handler();
  CefRefPtr<CefV8Value> send_func =
      CefV8Value::CreateFunction("sendToNative", send_handler);
  global->SetValue("sendToNative", send_func, V8_PROPERTY_ATTRIBUTE_NONE);

  CefRefPtr<CefV8Value> on_message =
      CefV8Value::CreateUndefined();
  global->SetValue("onMessage", on_message, V8_PROPERTY_ATTRIBUTE_NONE);
}

bool RenderHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
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

    CefRefPtr<CefProcessMessage> out_msg =
        CefProcessMessage::Create("from-renderer");
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
