// SPDX-License-Identifier: MIT

#include <cstdlib>

#include <iostream>
#include <limits>
#include <memory>
#include <string_view>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/operation.h>
#include <pulse/subscribe.h>

namespace {

constexpr auto PaClientName = "PaPersistD";

[[nodiscard]] auto PaServer() noexcept {
	return ::getenv("PULSE_SERVER");
}

[[nodiscard]] auto PaDefaultSink() noexcept {
	return ::getenv("PULSE_DEFAULT_SINK");
}

void LogConfig() {
	auto server = PaServer();
	auto default_sink = PaDefaultSink();
	if (server) {
		std::clog << "server: " << server << std::endl;
	} else {
		std::clog << "default server" << std::endl;
	}
	if (default_sink) {
		std::clog << "default sink: " << default_sink << std::endl;
	} else {
		std::clog << "default sink is not configured" << std::endl;
	}
}

[[nodiscard]] auto MainLoopPtr(pa_mainloop* m) noexcept {
	return std::unique_ptr<pa_mainloop, decltype(pa_mainloop_free)*>{m, pa_mainloop_free};
}

[[nodiscard]] auto ContextPtr(pa_context* c) noexcept {
	return std::unique_ptr<pa_context, decltype(pa_context_unref)*>{c, pa_context_unref};
}

auto OperationPtr(pa_operation* o) noexcept {
	return std::unique_ptr<pa_operation, decltype(pa_operation_unref)*>{o, pa_operation_unref};
}

[[nodiscard]] constexpr auto NameOf(pa_context_state_t state) noexcept {
	switch (state) {
	case PA_CONTEXT_UNCONNECTED: return "unconnected";
	case PA_CONTEXT_CONNECTING: return "connecting";
	case PA_CONTEXT_AUTHORIZING: return "authorizing";
	case PA_CONTEXT_SETTING_NAME: return "setting_name";
	case PA_CONTEXT_READY: return "ready";
	case PA_CONTEXT_FAILED: return "failed";
	case PA_CONTEXT_TERMINATED: return "terminated";
	}
	return "unknown";
}

struct PaPersistState {
	pa_mainloop_api* api{};
	uint32_t currentDefaultIdx{std::numeric_limits<uint32_t>::max()};
};

struct DefaultSinkArgs {
	PaPersistState* paPersistState{};
	uint32_t defaultIdx{std::numeric_limits<uint32_t>::max()};
};

void SuccessCb(const char* name, pa_context*, int success, void* userdata) noexcept {
	if (success) {
		std::clog << name << " succeed" << std::endl;
	} else {
		std::cerr << name << " failed" << std::endl;
		auto api = static_cast<pa_mainloop_api*>(userdata);
		api->quit(api, EXIT_FAILURE);
	}
}

void DefaultSinkSuccessCb(pa_context* ctx, int success, void* userdata) noexcept {
	auto args = std::unique_ptr<DefaultSinkArgs>{static_cast<DefaultSinkArgs*>(userdata)};
	if (success) {
		args->paPersistState->currentDefaultIdx = args->defaultIdx;
		std::clog << "current default sink index: " << args->defaultIdx << std::endl;
	}
	SuccessCb("default sink set", ctx, success, args->paPersistState->api);
}

constexpr bool StartsWith(std::string_view full, std::string_view prefix) noexcept
{
	return full.compare(0, prefix.size(), prefix) == 0;
}

void HandleSink(pa_context* ctx, const pa_sink_info* info, int, void* userdata) {
	if (!info) {
		return;
	}
	std::cout << "Handle " << info->index << ": " << info->name << std::endl;
	auto default_sink = PaDefaultSink();
	if (default_sink && StartsWith(info->name, default_sink)) {
		auto paPersistState = static_cast<PaPersistState*>(userdata);
		auto args = std::make_unique<DefaultSinkArgs>(DefaultSinkArgs{paPersistState, info->index});
		OperationPtr(pa_context_set_default_sink(
			ctx, info->name, DefaultSinkSuccessCb, args.release()));
	}
}

void SubSuccessCb(pa_context* ctx, int success, void* userdata) noexcept {
	SuccessCb("event subscription", ctx, success, userdata);
}

void CtxStateChanged(pa_context* ctx, void* userdata) {
	auto state = pa_context_get_state(ctx);
	std::clog << "context state changed: " << NameOf(state) << std::endl;
	if (state == PA_CONTEXT_READY) {
		auto paPersistState = static_cast<PaPersistState*>(userdata);
		OperationPtr(pa_context_subscribe(
			ctx, PA_SUBSCRIPTION_MASK_SINK, SubSuccessCb, paPersistState->api));
		OperationPtr(pa_context_get_sink_info_list(ctx, HandleSink, userdata));
	}
}

void SubEventCb(pa_context* ctx, pa_subscription_event_type_t t, uint32_t idx, void* userdata) {
	switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
	case PA_SUBSCRIPTION_EVENT_SINK:
		switch (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
		case PA_SUBSCRIPTION_EVENT_NEW:
			std::clog << "new sink: " << idx << std::endl;
			OperationPtr(pa_context_get_sink_info_by_index(ctx, idx, HandleSink, userdata));
			break;
		case PA_SUBSCRIPTION_EVENT_REMOVE:
			std::clog << "removed sink: " << idx << std::endl;
			auto paPersistState = static_cast<PaPersistState*>(userdata);
			if (idx == paPersistState->currentDefaultIdx) {
				paPersistState->currentDefaultIdx = std::numeric_limits<uint32_t>::max();
				std::clog << "current default sink removed" << std::endl;
				std::clog << "searching for new sink" << std::endl;
				OperationPtr(pa_context_get_sink_info_list(ctx, HandleSink, userdata));
			}
			break;
		}
		break;
	default:
		std::cerr << "unknown event facility" << std::endl;
	}
}

}  // namespace

int main() {
	LogConfig();

	auto main_loop = MainLoopPtr(pa_mainloop_new());
	if (!main_loop) {
		std::cerr << "pa_mainloop_new failed" << std::endl;
		return EXIT_FAILURE;
	}

	auto api = pa_mainloop_get_api(main_loop.get());
	auto ctx = ContextPtr(pa_context_new(api, PaClientName));
	if (!ctx) {
		std::cerr << "pa_context_new failed" << std::endl;
		return EXIT_FAILURE;
	}
	PaPersistState paPersistState{api};
	pa_context_set_state_callback(ctx.get(), CtxStateChanged, &paPersistState);
	pa_context_set_subscribe_callback(ctx.get(), SubEventCb, &paPersistState);
	auto err = pa_context_connect(ctx.get(), PaServer(),
		static_cast<pa_context_flags>(PA_CONTEXT_NOAUTOSPAWN | PA_CONTEXT_NOFAIL), nullptr);
	if (err) {
		std::cerr << "pa_context_connect failed" << std::endl;
		return EXIT_FAILURE;
	}

	int exit_code{};
	err = pa_mainloop_run(main_loop.get(), &exit_code);
	if (err) {
		std::cerr << "pa_mainloop_run failed" << std::endl;
		return EXIT_FAILURE;
	}
	return exit_code;
}
