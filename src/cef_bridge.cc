#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(_WIN32)
	#include <dlfcn.h>
#endif

#include "cef_app.h"
#include "cef_browser.h"
#include "cef_client.h"
#include "cef_life_span_handler.h"
#include "cef_process_message.h"
#include "cef_render_process_handler.h"
#include "cef_resource_handler.h"
#include "cef_resource_request_handler.h"
#include "cef_scheme.h"
#include "cef_task.h"
#include "cef_v8.h"
#include "cef_task.h"
#include "base/cef_callback.h"
#include "wrapper/cef_closure_task.h"
#include "wrapper/cef_helpers.h"

#if !defined(CEF_HELPER_BINARY)
	#include <node_api.h>

	#if defined(_WIN32)
		#include <windows.h>
	#else
		#include <dlfcn.h>
	#endif
#endif

namespace {

static std::string JsQuote(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 16);
	out.push_back('"');
	for (unsigned char c : s) {
		switch (c) {
			case '\\': out += "\\\\"; break;
			case '"':  out += "\\\""; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				out.push_back(static_cast<char>(c));
				break;
		}
	}
	out.push_back('"');
	return out;
}

static std::string MakeBridgeInstallJs() {
	return R"JS(
(function() {
	const b = window.cef
	if (!b || b.__bridgeInstalled) return

	Object.defineProperty(b, "__bridgeInstalled", {
		value: true,
		enumerable: false,
		configurable: false
	})

	Object.defineProperty(b, "_queue", {
		value: [],
		enumerable: false,
		configurable: false
	})

	let handler = null
	Object.defineProperty(b, "onmessage", {
		enumerable: true,
		configurable: true,
		get() {
			return handler
		},
		set(fn) {
			handler = (typeof fn === "function") ? fn : null
			if (handler && b._queue.length) {
				const queued = b._queue.splice(0, b._queue.length)
				for (const item of queued) handler(item)
			}
		}
	})
})()
)JS";
}

static std::string MakeDispatchJs(const std::string& msg) {
	return "(function(m){"
		"if (window.cef && window.cef.onmessage) window.cef.onmessage(m);"
		"else if (window.onmessage) window.onmessage(m);"
		"else if (window.cef) window.cef._queue.push(m);"
	"})(" + JsQuote(msg) + ");";
}

class NativeSendHandler final : public CefV8Handler {
public:
	bool Execute(
		const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception
	) override {
		CEF_REQUIRE_RENDERER_THREAD();

		if (arguments.size() != 1 || !arguments[0]->IsString()) {
			exception = "cef.send(msg) expects exactly one string argument";
			return true;
		}

		auto context = CefV8Context::GetCurrentContext();
		if (!context) {
			exception = "no V8 context";
			return true;
		}

		auto browser = context->GetBrowser();
		if (!browser || !browser->GetMainFrame()) {
			exception = "no browser frame";
			return true;
		}

		auto msg = CefProcessMessage::Create("bridge.page_to_host");
		auto list = msg->GetArgumentList();
		list->SetSize(1);
		list->SetString(0, arguments[0]->GetStringValue());
		browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);

		retval = CefV8Value::CreateBool(true);
		return true;
	}

private:
	IMPLEMENT_REFCOUNTING(NativeSendHandler);
};

static std::mutex g_render_mutex;
static std::unordered_map<int, std::vector<std::string>> g_pending_host_messages;
static std::unordered_map<int, bool> g_context_ready;

class BridgeRenderProcessHandler final : public CefRenderProcessHandler {
public:
	void OnContextCreated(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context
	) override {
		CEF_REQUIRE_RENDERER_THREAD();
		if (!frame->IsMain()) {
			return;
		}

		const int browser_id = browser->GetIdentifier();

		auto global = context->GetGlobal();

		// Create window.cef object with queue and onmessage
		auto cef_obj = CefV8Value::CreateObject(nullptr, nullptr);

		// _queue array
		auto queue = CefV8Value::CreateArray(0);
		cef_obj->SetValue("_queue", queue, V8_PROPERTY_ATTRIBUTE_NONE);

		// onmessage property with getter/setter
		cef_obj->SetValue("onmessage",
			CefV8Value::CreateUndefined(),
			V8_PROPERTY_ATTRIBUTE_NONE);

		// send function
		auto send_fn = CefV8Value::CreateFunction("send", new NativeSendHandler());
		cef_obj->SetValue("send", send_fn, V8_PROPERTY_ATTRIBUTE_NONE);

		global->SetValue("cef", cef_obj, V8_PROPERTY_ATTRIBUTE_NONE);

		frame->ExecuteJavaScript(MakeBridgeInstallJs(), frame->GetURL(), 0);

		std::vector<std::string> pending;
		{
			std::lock_guard<std::mutex> lock(g_render_mutex);
			g_context_ready[browser_id] = true;
			auto it = g_pending_host_messages.find(browser_id);
			if (it != g_pending_host_messages.end()) {
				pending = std::move(it->second);
				g_pending_host_messages.erase(it);
			}
		}

		for (const auto& msg : pending) {
			frame->ExecuteJavaScript(MakeDispatchJs(msg), frame->GetURL(), 0);
		}
	}

	void OnContextReleased(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context
	) override {
		CEF_REQUIRE_RENDERER_THREAD();
		if (!frame->IsMain()) {
			return;
		}

		const int browser_id = browser->GetIdentifier();
		std::lock_guard<std::mutex> lock(g_render_mutex);
		g_context_ready[browser_id] = false;
	}

	void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
		CEF_REQUIRE_RENDERER_THREAD();
		const int browser_id = browser->GetIdentifier();

		std::lock_guard<std::mutex> lock(g_render_mutex);
		g_context_ready.erase(browser_id);
		g_pending_host_messages.erase(browser_id);
	}

	bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message
	) override {
		CEF_REQUIRE_RENDERER_THREAD();
		if (!frame->IsMain()) {
			return false;
		}

		if (message->GetName() != "bridge.host_to_page") {
			return false;
		}

		auto list = message->GetArgumentList();
		if (list->GetSize() < 1) {
			return true;
		}

		const int browser_id = browser->GetIdentifier();
		const std::string text = list->GetString(0).ToString();

		bool ready = false;
		{
			std::lock_guard<std::mutex> lock(g_render_mutex);
			auto it = g_context_ready.find(browser_id);
			ready = (it != g_context_ready.end() && it->second);
			if (!ready) {
				g_pending_host_messages[browser_id].push_back(text);
				return true;
			}
		}

		frame->ExecuteJavaScript(MakeDispatchJs(text), frame->GetURL(), 0);
		return true;
	}

private:
	IMPLEMENT_REFCOUNTING(BridgeRenderProcessHandler);
};

class BridgeApp final : public CefApp, public CefRenderProcessHandler {
public:
	CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
		return this;
	}

	void OnContextCreated(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context
	) override {
		CEF_REQUIRE_RENDERER_THREAD();
		if (!frame->IsMain()) {
			return;
		}

		render_handler_.OnContextCreated(browser, frame, context);
	}

	void OnContextReleased(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context
	) override {
		render_handler_.OnContextReleased(browser, frame, context);
	}

	void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
		render_handler_.OnBrowserDestroyed(browser);
	}

	bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message
	) override {
		return render_handler_.OnProcessMessageReceived(browser, frame, source_process, message);
	}

private:
	BridgeRenderProcessHandler render_handler_;

	IMPLEMENT_REFCOUNTING(BridgeApp);
};

} // namespace

#if !defined(CEF_HELPER_BINARY)

namespace {

static std::mutex g_mutex;
static std::condition_variable g_cv;

struct BrowserState {
	int token = 0;
	int browser_id = -1;
	CefRefPtr<CefBrowser> browser;
	napi_threadsafe_function tsfn = nullptr;
	bool close_requested = false;
	std::string url;
};

static std::unordered_map<int, std::vector<std::string>> g_pending_to_renderer;

static std::unordered_map<int, std::shared_ptr<BrowserState>> g_states_by_token;
static std::unordered_map<int, int> g_token_by_browser_id;
static std::atomic<int> g_next_token{1};

static std::atomic<bool> g_cef_initialized{false};
static std::atomic<bool> g_shutdown_requested{false};
static std::atomic<bool> g_atexit_registered{false};
static std::string g_cef_root;
static std::string g_browser_subprocess_path;

static CefRefPtr<BridgeApp> g_app;
static int g_active_browser_count = 0;

static std::string NapiGetString(napi_env env, napi_value value) {
	size_t len = 0;
	napi_get_value_string_utf8(env, value, nullptr, 0, &len);
	std::string out(len + 1, '\0');
	napi_get_value_string_utf8(env, value, out.data(), out.size(), &len);
	out.resize(len);
	return out;
}

static napi_value NapiMakeUndefined(napi_env env) {
	napi_value v;
	napi_get_undefined(env, &v);
	return v;
}

static std::string ModulePath() {
#if defined(_WIN32)
	HMODULE module = nullptr;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(&ModulePath),
		&module
	);

	char buffer[MAX_PATH];
	DWORD len = GetModuleFileNameA(module, buffer, MAX_PATH);
	return std::string(buffer, buffer + len);
#else
	Dl_info info{};
	if (dladdr(reinterpret_cast<void*>(&ModulePath), &info) && info.dli_fname) {
		return info.dli_fname;
	}
	return {};
#endif
}

static std::string DirName(const std::string& path) {
	const size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos) {
		return ".";
	}
	return path.substr(0, pos);
}

static std::string JoinPath(const std::string& dir, const std::string& file) {
	if (dir.empty()) {
		return file;
	}
#if defined(_WIN32)
	return dir + "\\" + file;
#else
	return dir + "/" + file;
#endif
}

static std::string ResolveCefRoot() {
	if (!g_cef_root.empty()) {
		return g_cef_root;
	}
	// Try to find where libcef.so was loaded from using a symbol that lives in it
	// Use a CEF function that exists in libcef.so
	void* handle = dlopen("libcef.so", RTLD_NOLOAD);
	if (handle) {
		// Now find where libcef.so is loaded
		Dl_info info;
		// Try to get info on any CEF symbol - use CefInitialize as it's definitely in libcef
		// We can't call it without initializing, but we can look it up
		// Instead, just use the handle directly - but dladdr needs a function address
		// Let's use dlsym to find a CEF function first
		void* cef_sym = dlsym(handle, "CefInitialize");
		if (cef_sym && dladdr(cef_sym, &info) && info.dli_fname) {
			// libcef.so is loaded here - use this directory
			std::string libcef_dir = DirName(info.dli_fname);
			dlclose(handle);
			return libcef_dir;
		}
		dlclose(handle);
	}
	// Fallback to addon location
	return DirName(ModulePath());
}

static void SendToPageOnUi(CefRefPtr<CefBrowser> browser, std::string msg) {
	CEF_REQUIRE_UI_THREAD();

	if (!browser || !browser->GetMainFrame()) {
		return;
	}

	auto message = CefProcessMessage::Create("bridge.host_to_page");
	auto list = message->GetArgumentList();
	list->SetSize(1);
	list->SetString(0, msg);
	browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
}

static void CreateBrowserOnUi(std::shared_ptr<BrowserState> state);

class BridgeClient final : public CefClient, public CefLifeSpanHandler {
public:
	explicit BridgeClient(std::shared_ptr<BrowserState> state)
		: state_(std::move(state)) {}

	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
		return this;
	}

	void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
		CEF_REQUIRE_UI_THREAD();

		std::vector<std::string> queued;
		bool close_now = false;

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			state_->browser = browser;
			state_->browser_id = browser->GetIdentifier();
			g_token_by_browser_id[state_->browser_id] = state_->token;
			++g_active_browser_count;
			auto it = g_pending_to_renderer.find(state_->token);
			if (it != g_pending_to_renderer.end()) {
				queued = std::move(it->second);
				g_pending_to_renderer.erase(it);
			}
			close_now = state_->close_requested;
		}

		for (const auto& msg : queued) {
			SendToPageOnUi(browser, msg);
		}

		if (close_now) {
			browser->GetHost()->CloseBrowser(false);
		}
	}

	bool DoClose(CefRefPtr<CefBrowser> browser) override {
		CEF_REQUIRE_UI_THREAD();
		return false;
	}

	void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
		CEF_REQUIRE_UI_THREAD();

		std::shared_ptr<BrowserState> state;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			const int browser_id = browser->GetIdentifier();
			auto it_token = g_token_by_browser_id.find(browser_id);
			if (it_token != g_token_by_browser_id.end()) {
				auto it_state = g_states_by_token.find(it_token->second);
				if (it_state != g_states_by_token.end()) {
					state = it_state->second;
					g_states_by_token.erase(it_state);
				}
				g_token_by_browser_id.erase(it_token);
			}

			if (g_active_browser_count > 0) {
				--g_active_browser_count;
			}

			if (g_shutdown_requested.load() && g_active_browser_count == 0) {
				g_cv.notify_all();
			}
		}

		if (state && state->tsfn) {
			napi_release_threadsafe_function(state->tsfn, napi_tsfn_release);
			state->tsfn = nullptr;
		}
	}

	bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message
	) override {
		CEF_REQUIRE_UI_THREAD();

		if (message->GetName() != "bridge.page_to_host") {
			return false;
		}

		auto list = message->GetArgumentList();
		if (list->GetSize() < 1) {
			return true;
		}

		const int browser_id = browser->GetIdentifier();
		const std::string text = list->GetString(0).ToString();

		std::shared_ptr<BrowserState> state;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			auto it_token = g_token_by_browser_id.find(browser_id);
			if (it_token != g_token_by_browser_id.end()) {
				auto it_state = g_states_by_token.find(it_token->second);
				if (it_state != g_states_by_token.end()) {
					state = it_state->second;
				}
			}
		}

		if (!state || !state->tsfn) {
			return true;
		}

		auto* payload = new std::string(text);
		const napi_status st = napi_call_threadsafe_function(
			state->tsfn,
			payload,
			napi_tsfn_nonblocking
		);

		if (st != napi_ok) {
			delete payload;
		}

		return true;
	}

private:
	std::shared_ptr<BrowserState> state_;

	IMPLEMENT_REFCOUNTING(BridgeClient);
};

static void CreateBrowserOnUi(std::shared_ptr<BrowserState> state) {
	CEF_REQUIRE_UI_THREAD();

	if (g_shutdown_requested.load()) {
		return;
	}

	CefWindowInfo window_info;
	window_info.SetAsChild(0, CefRect(0, 0, 800, 600));

	CefBrowserSettings browser_settings;

	auto client = new BridgeClient(state);
	CefBrowserHost::CreateBrowser(
		window_info,
		client,
		state->url.empty() ? "about:blank" : state->url,
		browser_settings,
		nullptr,
		nullptr
	);
}

static void RegisterAtexitOnce() {
	if (g_atexit_registered.exchange(true)) {
		return;
	}

	std::atexit([]() {
		if (g_cef_initialized.load()) {
			std::vector<std::shared_ptr<BrowserState>> no_browser_states;
			std::vector<CefRefPtr<CefBrowser>> browsers_to_close;

			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_shutdown_requested = true;

				for (const auto& kv : g_states_by_token) {
					auto state = kv.second;
					if (state->browser) {
						browsers_to_close.push_back(state->browser);
					} else {
						no_browser_states.push_back(state);
					}
					state->close_requested = true;
				}

				for (const auto& state : no_browser_states) {
					g_states_by_token.erase(state->token);
				}
			}

			for (const auto& state : no_browser_states) {
				if (state->tsfn) {
					napi_release_threadsafe_function(state->tsfn, napi_tsfn_release);
					state->tsfn = nullptr;
				}
			}

			for (const auto& browser : browsers_to_close) {
				CefPostTask(TID_UI, base::BindOnce(
					[](CefRefPtr<CefBrowser> b) {
						if (b) {
							b->GetHost()->CloseBrowser(false);
						}
					},
					browser
				));
			}

			std::unique_lock<std::mutex> lock(g_mutex);
			g_cv.wait(lock, [] {
				return g_active_browser_count == 0;
			});

			lock.unlock();
			CefShutdown();
			g_cef_initialized = false;
			g_shutdown_requested = false;
		}
	});
}

static void EnsureCefInitialized() {
	if (g_cef_initialized.load()) {
		return;
	}

	RegisterAtexitOnce();

	CefMainArgs main_args;
	CefSettings settings;
	settings.multi_threaded_message_loop = true;
	settings.no_sandbox = true;

	char cwd[4096];
	if (!getcwd(cwd, sizeof(cwd))) {
		cwd[0] = '\0';
	}

	// Set resources directory from g_cef_root or detect from module path
	std::string cef_root = ResolveCefRoot();
	char abs_cef_root[4096];
	if (!realpath(cef_root.c_str(), abs_cef_root)) {
		strcpy(abs_cef_root, cef_root.c_str());
	}
	// Resources are in a "Resources" subdirectory (CEF distribution structure)
	const std::string resources_dir = JoinPath(abs_cef_root, "Resources");
	const std::string locales_dir = JoinPath(resources_dir, "locales");
	const std::string helper = JoinPath(abs_cef_root, "cef_bridge_helper");
	fprintf(stderr, "DEBUG: cef_root=%s, resources_dir=%s, locales_dir=%s, helper=%s\n", abs_cef_root, resources_dir.c_str(), locales_dir.c_str(), helper.c_str());
	CefString(&settings.resources_dir_path) = resources_dir;
	CefString(&settings.locales_dir_path) = locales_dir;
	CefString(&settings.browser_subprocess_path) = helper;

	g_app = new BridgeApp();

	if (!CefInitialize(main_args, settings, g_app.get(), nullptr)) {
		throw std::runtime_error("CefInitialize failed");
	}

	g_cef_initialized = true;
}

static void TsfnCallJs(
	napi_env env,
	napi_value js_cb,
	void* /*context*/,
	void* data
) {
	auto* msg = static_cast<std::string*>(data);

	if (!env || !js_cb || !msg) {
		delete msg;
		return;
	}

	napi_handle_scope scope;
	napi_open_handle_scope(env, &scope);

	napi_value argv[1];
	argv[0] = NapiMakeUndefined(env);
	napi_create_string_utf8(env, msg->c_str(), msg->size(), &argv[0]);

	napi_value global;
	napi_get_global(env, &global);

	napi_value result;
	napi_call_function(env, global, js_cb, 1, argv, &result);

	napi_close_handle_scope(env, scope);
	delete msg;
}

static void ShutdownCore() {
	if (!g_cef_initialized.load()) {
		return;
	}

	std::vector<std::shared_ptr<BrowserState>> no_browser_states;
	std::vector<CefRefPtr<CefBrowser>> browsers_to_close;

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_shutdown_requested = true;

		for (const auto& kv : g_states_by_token) {
			auto state = kv.second;
			state->close_requested = true;

			if (state->browser) {
				browsers_to_close.push_back(state->browser);
			} else {
				no_browser_states.push_back(state);
			}
		}

		for (const auto& state : no_browser_states) {
			g_states_by_token.erase(state->token);
		}
	}

	for (const auto& state : no_browser_states) {
		if (state->tsfn) {
			napi_release_threadsafe_function(state->tsfn, napi_tsfn_release);
			state->tsfn = nullptr;
		}
	}

	for (const auto& browser : browsers_to_close) {
		CefPostTask(TID_UI, base::BindOnce(
			[](CefRefPtr<CefBrowser> b) {
				if (b) {
					b->GetHost()->CloseBrowser(false);
				}
			},
			browser
		));
	}

	std::unique_lock<std::mutex> lock(g_mutex);
	g_cv.wait(lock, [] {
		return g_active_browser_count == 0;
	});

	lock.unlock();
	CefShutdown();
	g_cef_initialized = false;
	g_shutdown_requested = false;
}

static napi_value Init(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	if (argc >= 1) {
		bool is_object = false;
		napi_valuetype type;
		napi_typeof(env, args[0], &type);
		is_object = (type == napi_object);

		if (is_object) {
			napi_value path_value;
			bool has_subprocess = false;
			napi_has_named_property(env, args[0], "browserSubprocessPath", &has_subprocess);
			if (has_subprocess) {
				napi_get_named_property(env, args[0], "browserSubprocessPath", &path_value);
				g_browser_subprocess_path = NapiGetString(env, path_value);
			}
		}
	}

	EnsureCefInitialized();
	return NapiMakeUndefined(env);
}

static napi_value CreateWindow(napi_env env, napi_callback_info info) {
	EnsureCefInitialized();

	size_t argc = 2;
	napi_value args[2];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	if (argc < 2) {
		napi_throw_type_error(env, nullptr, "createWindow(url, onMessage) expects 2 arguments");
		return nullptr;
	}

	if (g_shutdown_requested.load()) {
		napi_throw_error(env, nullptr, "CEF is shutting down");
		return nullptr;
	}

	std::string url = NapiGetString(env, args[0]);

	napi_valuetype type;
	napi_typeof(env, args[1], &type);
	if (type != napi_function) {
		napi_throw_type_error(env, nullptr, "onMessage must be a function");
		return nullptr;
	}

	auto state = std::make_shared<BrowserState>();
	state->token = g_next_token.fetch_add(1);
	state->url = url;

	napi_value resource_name;
	napi_create_string_utf8(env, "cef-bridge", NAPI_AUTO_LENGTH, &resource_name);

	napi_status st = napi_create_threadsafe_function(
		env,
		args[1],
		nullptr,
		resource_name,
		0,
		1,
		nullptr,
		nullptr,
		nullptr,
		TsfnCallJs,
		&state->tsfn
	);

	if (st != napi_ok) {
		napi_throw_error(env, nullptr, "failed to create thread-safe function");
		return nullptr;
	}

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_states_by_token[state->token] = state;
	}

	CefPostTask(TID_UI, base::BindOnce(&CreateBrowserOnUi, state));

	napi_value out;
	napi_create_int32(env, state->token, &out);
	return out;
}

static napi_value Send(napi_env env, napi_callback_info info) {
	EnsureCefInitialized();

	size_t argc = 2;
	napi_value args[2];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	if (argc < 2) {
		napi_throw_type_error(env, nullptr, "send(token, message) expects 2 arguments");
		return nullptr;
	}

	if (g_shutdown_requested.load()) {
		napi_throw_error(env, nullptr, "CEF is shutting down");
		return nullptr;
	}

	int32_t token = 0;
	napi_get_value_int32(env, args[0], &token);
	std::string msg = NapiGetString(env, args[1]);

	std::shared_ptr<BrowserState> state;
	CefRefPtr<CefBrowser> browser;

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		auto it = g_states_by_token.find(token);
		if (it != g_states_by_token.end()) {
			state = it->second;
			browser = state->browser;
			if (!browser) {
				g_pending_to_renderer[token].push_back(msg);
			}
		}
	}

	if (browser) {
		CefPostTask(TID_UI, base::BindOnce(
			[](CefRefPtr<CefBrowser> b, std::string m) {
				if (b && b->GetMainFrame()) {
					auto message = CefProcessMessage::Create("bridge.host_to_page");
					auto list = message->GetArgumentList();
					list->SetSize(1);
					list->SetString(0, m);
					b->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
				}
			},
			browser,
			std::move(msg)
		));
	}

	return NapiMakeUndefined(env);
}

static napi_value Close(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	if (argc < 1) {
		napi_throw_type_error(env, nullptr, "close(token) expects 1 argument");
		return nullptr;
	}

	int32_t token = 0;
	napi_get_value_int32(env, args[0], &token);

	std::shared_ptr<BrowserState> state;
	CefRefPtr<CefBrowser> browser;

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		auto it = g_states_by_token.find(token);
		if (it != g_states_by_token.end()) {
			state = it->second;
			state->close_requested = true;
			browser = state->browser;
		}
	}

	if (browser) {
		CefPostTask(TID_UI, base::BindOnce(
			[](CefRefPtr<CefBrowser> b) {
				if (b) {
					b->GetHost()->CloseBrowser(false);
				}
			},
			browser
		));
	}

	return NapiMakeUndefined(env);
}

static napi_value Shutdown(napi_env env, napi_callback_info info) {
	ShutdownCore();
	return NapiMakeUndefined(env);
}

static napi_value InitExports(napi_env env, napi_value exports) {
	napi_value fn;

	napi_create_function(env, "init", NAPI_AUTO_LENGTH, Init, nullptr, &fn);
	napi_set_named_property(env, exports, "init", fn);

	napi_create_function(env, "createWindow", NAPI_AUTO_LENGTH, CreateWindow, nullptr, &fn);
	napi_set_named_property(env, exports, "createWindow", fn);

	napi_create_function(env, "send", NAPI_AUTO_LENGTH, Send, nullptr, &fn);
	napi_set_named_property(env, exports, "send", fn);

	napi_create_function(env, "close", NAPI_AUTO_LENGTH, Close, nullptr, &fn);
	napi_set_named_property(env, exports, "close", fn);

	napi_create_function(env, "shutdown", NAPI_AUTO_LENGTH, Shutdown, nullptr, &fn);
	napi_set_named_property(env, exports, "shutdown", fn);

	return exports;
}

} // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, InitExports)

#else

int main(int argc, char* argv[]) {
	CefMainArgs main_args(argc, argv);
	CefRefPtr<CefApp> app = new BridgeApp();

	const int exit_code = CefExecuteProcess(main_args, app, nullptr);
	if (exit_code >= 0) {
		return exit_code;
	}

	return 0;
}

#endif