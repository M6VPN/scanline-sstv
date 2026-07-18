// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2f_rigctld_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/transmit_coordinator.hpp>
#include <sstv/rig/flrig.hpp>
#include <sstv/rig/rigctld.hpp>

#include "rigctld_internal.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using namespace std::chrono_literals;

namespace {

void expect(const bool condition, const std::string& message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

[[nodiscard]] sstv::rig::RigctldConfiguration configuration(
	const std::uint16_t port, const std::string& address = "127.0.0.1")
{
	return {address, port, 500ms, 500ms, 1s, {}};
}

class ScriptedRigctldTransport final : public sstv::rig::RigctldTransport {
public:
	[[nodiscard]] sstv::rig::RigctldTransportResult exchange(
		const std::string& request, const sstv::rig::MonotonicTime) noexcept override
	{
		requests.push_back(request);
		if (responses.empty()) return {sstv::rig::RigctldTransportError::disconnected,
			{}, request.size(), "missing scripted response"};
		auto response = std::move(responses.front());
		responses.pop_front();
		if (response.requestBytesSent == 0) response.requestBytesSent = request.size();
		return response;
	}
	std::deque<sstv::rig::RigctldTransportResult> responses;
	std::vector<std::string> requests;
};

class ScriptedFlrigTransport final : public sstv::rig::FlrigTransport {
public:
	[[nodiscard]] sstv::rig::FlrigTransportResult exchange(
		const std::string& request, const sstv::rig::MonotonicTime) noexcept override
	{
		if (responses.empty()) return {sstv::rig::FlrigTransportError::disconnected,
			{}, request.size(), "missing scripted response"};
		auto response = std::move(responses.front());
		responses.pop_front();
		if (response.requestBytesSent == 0) response.requestBytesSent = request.size();
		return response;
	}
	std::deque<sstv::rig::FlrigTransportResult> responses;
};

[[nodiscard]] std::string flrigHttpResponse(const std::string& body)
{
	return "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
		+ std::to_string(body.size()) + "\r\n\r\n" + body;
}

[[nodiscard]] std::string flrigStateResponse(const bool keyed)
{
	return flrigHttpResponse("<methodResponse><params><param><value><i4>"
		+ std::string(keyed ? "1" : "0")
		+ "</i4></value></param></params></methodResponse>");
}

[[nodiscard]] std::string flrigSetResponse()
{
	return flrigHttpResponse(
		"<methodResponse><params><param><value></value></param></params></methodResponse>");
}

class FixedPttProvider final : public sstv::rig::PttProvider {
public:
	explicit FixedPttProvider(const bool isAmbiguous = false) : isAmbiguous_(isAmbiguous) {}
	[[nodiscard]] sstv::rig::PttOperationResult execute(
		const sstv::rig::PttRequest& request) noexcept override
	{
		sstv::rig::PttOperationResult result{};
		result.action = request.action;
		result.attempt = request.attempt;
		result.operationId = request.operationId;
		result.started = request.deadline - 2ns;
		result.completed = request.deadline - 1ns;
		if (isAmbiguous_ && request.action == sstv::rig::PttAction::key) {
			result.error = sstv::rig::PttErrorCategory::disconnected;
			result.mayHaveKeyed = true;
			result.message = "lost response";
			return result;
		}
		result.readback = sstv::rig::PttReadback::available;
		result.observed = request.action == sstv::rig::PttAction::unkey
			? sstv::rig::PttObservedState::unkeyed : sstv::rig::PttObservedState::keyed;
		if (request.action == sstv::rig::PttAction::query) {
			result.observed = sstv::rig::PttObservedState::unkeyed;
		}
		result.certainty = result.observed == sstv::rig::PttObservedState::keyed
			? sstv::rig::PttCertainty::definitelyKeyed
			: sstv::rig::PttCertainty::definitelyUnkeyed;
		result.mayHaveKeyed = request.action == sstv::rig::PttAction::key;
		return result;
	}
private:
	bool isAmbiguous_;
};

void runProviderConformance(const std::string& name,
	sstv::rig::PttProvider& confirmed, sstv::rig::PttProvider& ambiguous,
	const std::shared_ptr<sstv::rig::MonotonicClock>& clock)
{
	const auto deadline = clock->now() + 2s;
	const auto query = confirmed.execute({sstv::rig::PttAction::query, deadline, 2, 101});
	expect(query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		name + " confirms unkeyed query");
	const auto key = confirmed.execute({sstv::rig::PttAction::key, deadline, 3, 102});
	expect(key.certainty == sstv::rig::PttCertainty::definitelyKeyed && key.mayHaveKeyed,
		name + " confirms key with conservative side-effect flag");
	const auto unkey = confirmed.execute({sstv::rig::PttAction::unkey, deadline, 4, 103});
	expect(unkey.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		name + " confirms unkey");
	const auto lost = ambiguous.execute({sstv::rig::PttAction::key, deadline, 5, 104});
	expect(lost.certainty == sstv::rig::PttCertainty::indeterminate && lost.mayHaveKeyed,
		name + " preserves ambiguous key certainty");
	expect(query.attempt == 2 && query.operationId == 101
		&& query.completed >= query.started, name + " preserves request metadata");
	expect(query.message.size() <= sstv::rig::maximumRigctldDiagnosticBytes,
		name + " diagnostics remain bounded");
}

class LoopbackRigctldServer final {
public:
	explicit LoopbackRigctldServer(const bool isIpv6 = false) : isIpv6_(isIpv6)
	{
		listener_ = ::socket(isIpv6_ ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
		expect(listener_ >= 0, "rigctld loopback listener opens");
		if (isIpv6_) {
			sockaddr_in6 address{};
			address.sin6_family = AF_INET6;
			address.sin6_port = 0;
			expect(::inet_pton(AF_INET6, "::1", &address.sin6_addr) == 1,
				"IPv6 loopback parses");
			expect(::bind(listener_, reinterpret_cast<const sockaddr*>(&address),
				sizeof(address)) == 0, "IPv6 loopback binds");
			sockaddr_in6 bound{};
			socklen_t size = sizeof(bound);
			expect(::getsockname(listener_, reinterpret_cast<sockaddr*>(&bound), &size) == 0
				&& IN6_IS_ADDR_LOOPBACK(&bound.sin6_addr), "server remains IPv6 loopback-only");
			port_ = ntohs(bound.sin6_port);
		} else {
			sockaddr_in address{};
			address.sin_family = AF_INET;
			address.sin_port = 0;
			address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			expect(::bind(listener_, reinterpret_cast<const sockaddr*>(&address),
				sizeof(address)) == 0, "IPv4 loopback binds");
			sockaddr_in bound{};
			socklen_t size = sizeof(bound);
			expect(::getsockname(listener_, reinterpret_cast<sockaddr*>(&bound), &size) == 0
				&& ntohl(bound.sin_addr.s_addr) == INADDR_LOOPBACK,
				"server remains IPv4 loopback-only");
			port_ = ntohs(bound.sin_port);
		}
		expect(::listen(listener_, 8) == 0, "rigctld loopback listens");
		worker_ = std::jthread([this](const std::stop_token token) { run(token); });
	}
	~LoopbackRigctldServer()
	{
		worker_.request_stop();
		worker_.join();
		::close(listener_);
	}
	[[nodiscard]] std::uint16_t port() const noexcept { return port_; }
	void setByteAtATime(const bool enabled) noexcept { isByteAtATime_.store(enabled); }
	void setKeyed(const bool keyed) noexcept { isKeyed_.store(keyed); }
	[[nodiscard]] std::vector<std::string> requests() const
	{
		std::lock_guard lock(mutex_);
		return requests_;
	}
private:
	void run(const std::stop_token token)
	{
		while (!token.stop_requested()) {
			pollfd state{listener_, POLLIN, 0};
			if (::poll(&state, 1, 50) <= 0) continue;
			const int client = ::accept(listener_, nullptr, nullptr);
			if (client < 0) continue;
			handle(client);
			::close(client);
		}
	}
	void handle(const int client)
	{
		std::string request;
		char value = 0;
		while (request.size() <= 64 && request.find('\n') == std::string::npos) {
			const ssize_t count = ::recv(client, &value, 1, 0);
			if (count <= 0) return;
			request.push_back(value);
		}
		{
			std::lock_guard lock(mutex_);
			requests_.push_back(request);
		}
		std::string response;
		if (request == "+T 1\n") {
			isKeyed_.store(true);
			response = "set_ptt: 1\nRPRT 0\n";
		} else if (request == "+T 0\n") {
			isKeyed_.store(false);
			response = "set_ptt: 0\nRPRT 0\n";
		} else if (request == "+t\n") {
			response = "get_ptt:\nPTT: "
				+ std::string(isKeyed_.load() ? "1" : "0") + "\nRPRT 0\n";
		} else {
			response = "RPRT -1\n";
		}
		if (isByteAtATime_.load()) {
			for (const char character : response) {
				if (::send(client, &character, 1, 0) != 1) return;
			}
			return;
		}
		(void)::send(client, response.data(), response.size(), 0);
	}
	int listener_ = -1;
	std::uint16_t port_ = 0;
	bool isIpv6_ = false;
	std::atomic<bool> isKeyed_{false};
	std::atomic<bool> isByteAtATime_{false};
	mutable std::mutex mutex_;
	std::vector<std::string> requests_;
	std::jthread worker_;
};

class TinySource final : public sstv::app::FiniteSampleSource {
public:
	[[nodiscard]] sstv::app::FiniteSourceFacts facts() const noexcept override
	{
		return {48'000, 2, true};
	}
	[[nodiscard]] sstv::app::SampleReadResult read(
		const std::span<float> destination) noexcept override
	{
		if (offset_ >= 2 || destination.empty()) return std::size_t{0};
		const std::size_t count = std::min<std::size_t>(destination.size(), 2 - offset_);
		std::fill_n(destination.begin(), count, 0.125F);
		offset_ += count;
		return count;
	}
	[[nodiscard]] bool isExhausted() const noexcept override { return offset_ == 2; }
	void cancel() noexcept override { offset_ = 2; }
private:
	std::size_t offset_ = 0;
};

class MockAudio final : public sstv::app::TransmitAudioEndpoint {
public:
	[[nodiscard]] sstv::app::TransmitAudioResult open() noexcept override { isOpen_ = true; return {}; }
	[[nodiscard]] std::optional<sstv::audio::NegotiatedStreamFacts> negotiated() const noexcept override
	{
		return sstv::audio::NegotiatedStreamFacts{};
	}
	[[nodiscard]] sstv::app::TransmitAudioResult prefillSilence(std::size_t) noexcept override { return {}; }
	[[nodiscard]] sstv::app::TransmitAudioResult prime() noexcept override { isPrimed_ = true; return {}; }
	[[nodiscard]] sstv::app::TransmitAudioResult start() noexcept override { isRunning_ = true; return {}; }
	[[nodiscard]] std::size_t queue(const std::span<const float> samples) noexcept override
	{
		queued_ += samples.size();
		return samples.size();
	}
	void gateSignal() noexcept override {}
	[[nodiscard]] sstv::app::TransmitAudioStatus status() const noexcept override
	{
		return {sstv::app::TransmitAudioFault::none, 0, queued_ > 0,
			isOpen_, isPrimed_, isRunning_};
	}
	[[nodiscard]] sstv::app::TransmitAudioResult requestStop() noexcept override { return {}; }
	[[nodiscard]] sstv::app::TransmitAudioResult stop() noexcept override { isRunning_ = false; return {}; }
	[[nodiscard]] sstv::app::TransmitAudioResult close() noexcept override { isOpen_ = false; return {}; }
private:
	std::uint64_t queued_ = 0;
	bool isOpen_ = false;
	bool isPrimed_ = false;
	bool isRunning_ = false;
};

void testConfigurationAndCodec()
{
	for (const std::string invalid : {"localhost", "127.0.0.2", "::ffff:127.0.0.1",
		"::ffff:192.0.2.1", "fe80::1%lo", "http://127.0.0.1"}) {
		auto value = configuration(12345, invalid);
		expect(sstv::rig::validateRigctldConfiguration(value).has_value(),
			"unsafe rigctld address is rejected before transport");
	}
	auto invalidPort = configuration(0);
	expect(sstv::rig::validateRigctldConfiguration(invalidPort).has_value(),
		"zero rigctld port is rejected");
	expect(sstv::rig::rigctldInternal::buildCommand(sstv::rig::PttAction::key) == "+T 1\n"
		&& sstv::rig::rigctldInternal::buildCommand(sstv::rig::PttAction::unkey) == "+T 0\n"
		&& sstv::rig::rigctldInternal::buildCommand(sstv::rig::PttAction::query) == "+t\n",
		"rigctld command bytes match pinned extended grammar");
	for (std::int32_t value = 0; value <= 3; ++value) {
		const std::string response = "get_ptt:\nPTT: " + std::to_string(value) + "\nRPRT 0\n";
		sstv::rig::rigctldInternal::Response parsed;
		expect(!sstv::rig::rigctldInternal::parseResponse(response,
			sstv::rig::PttAction::query, {}, parsed), "documented PTT value parses");
		expect(parsed.state == (value == 0 ? sstv::rig::PttObservedState::unkeyed
			: sstv::rig::PttObservedState::keyed), "PTT values map deliberately");
		for (std::size_t split = 0; split < response.size(); ++split) {
			const auto status = sstv::rig::rigctldInternal::inspectResponse(
				std::string_view(response).substr(0, split), sstv::rig::PttAction::query, {});
			expect(status.error.empty() && !status.isComplete,
				"fragmented rigctld response remains incomplete and valid");
		}
	}
	sstv::rig::rigctldInternal::Response parsed;
	expect(!sstv::rig::rigctldInternal::parseResponse(
		"get_ptt:\nRPRT -5\n", sstv::rig::PttAction::query, {}, parsed)
		&& parsed.resultCode == -5, "negative query RPRT parses without a value");
	for (const std::string invalid : {"0\n", "get_ptt:\r\nPTT: 0\r\nRPRT 0\r\n",
		"get_ptt:\nPTT: 4\nRPRT 0\n", "get_ptt:\nPTT: 0\nRPRT 0\nextra\n",
		"get_ptt:\nPTT: 0\nRPRT 2147483648\n", "get_ptt:\nPTT: 0\nRPRT 1\n"}) {
		expect(sstv::rig::rigctldInternal::parseResponse(
			invalid, sstv::rig::PttAction::query, {}, parsed).has_value(),
			"malformed or unsupported rigctld response is rejected");
	}
}

void testInjectedProviderAndConformance()
{
	auto clock = sstv::rig::createSteadyMonotonicClock();
	auto transport = std::make_unique<ScriptedRigctldTransport>();
	ScriptedRigctldTransport* observer = transport.get();
	observer->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 0\nRPRT 0\n", 3, {}});
	observer->responses.push_back({sstv::rig::RigctldTransportError::none,
		"set_ptt: 1\nRPRT 0\n", 5, {}});
	observer->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 1\nRPRT 0\n", 3, {}});
	observer->responses.push_back({sstv::rig::RigctldTransportError::none,
		"set_ptt: 0\nRPRT 0\n", 5, {}});
	observer->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 0\nRPRT 0\n", 3, {}});
	sstv::rig::RigctldPttProvider confirmed(configuration(12345), clock, std::move(transport));
	auto lostTransport = std::make_unique<ScriptedRigctldTransport>();
	lostTransport->responses.push_back({sstv::rig::RigctldTransportError::disconnected,
		{}, 5, "lost key response"});
	lostTransport->responses.push_back({sstv::rig::RigctldTransportError::disconnected,
		{}, 3, "lost query response"});
	sstv::rig::RigctldPttProvider ambiguous(configuration(12345), clock,
		std::move(lostTransport));
	runProviderConformance("rigctld", confirmed, ambiguous, clock);
	expect(observer->requests == std::vector<std::string>{"+t\n", "+T 1\n", "+t\n",
		"+T 0\n", "+t\n"}, "rigctld provider uses exact set and readback sequence");

	FixedPttProvider mockConfirmed;
	FixedPttProvider mockAmbiguous(true);
	runProviderConformance("mock", mockConfirmed, mockAmbiguous, clock);

	auto flrigTransport = std::make_unique<ScriptedFlrigTransport>();
	flrigTransport->responses.push_back({sstv::rig::FlrigTransportError::none,
		flrigStateResponse(false), 1, {}});
	flrigTransport->responses.push_back({sstv::rig::FlrigTransportError::none,
		flrigSetResponse(), 1, {}});
	flrigTransport->responses.push_back({sstv::rig::FlrigTransportError::none,
		flrigStateResponse(true), 1, {}});
	flrigTransport->responses.push_back({sstv::rig::FlrigTransportError::none,
		flrigSetResponse(), 1, {}});
	flrigTransport->responses.push_back({sstv::rig::FlrigTransportError::none,
		flrigStateResponse(false), 1, {}});
	sstv::rig::FlrigConfiguration flrigConfiguration{
		"127.0.0.1", 12345, "/RPC2", 500ms, 500ms, 1s, {}};
	sstv::rig::FlrigPttProvider flrigConfirmed(
		flrigConfiguration, clock, std::move(flrigTransport));
	auto lostFlrigTransport = std::make_unique<ScriptedFlrigTransport>();
	lostFlrigTransport->responses.push_back({sstv::rig::FlrigTransportError::disconnected,
		{}, 1, "lost key response"});
	lostFlrigTransport->responses.push_back({sstv::rig::FlrigTransportError::disconnected,
		{}, 1, "lost query response"});
	sstv::rig::FlrigPttProvider flrigAmbiguous(
		flrigConfiguration, clock, std::move(lostFlrigTransport));
	runProviderConformance("flrig", flrigConfirmed, flrigAmbiguous, clock);
}

void testInjectedFailureSemantics()
{
	auto clock = sstv::rig::createSteadyMonotonicClock();
	auto rejectedTransport = std::make_unique<ScriptedRigctldTransport>();
	rejectedTransport->responses.push_back({sstv::rig::RigctldTransportError::none,
		"set_ptt: 1\nRPRT -9\n", 5, {}});
	rejectedTransport->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 0\nRPRT 0\n", 3, {}});
	sstv::rig::RigctldPttProvider rejected(configuration(12345), clock,
		std::move(rejectedTransport));
	const auto rejectedResult = rejected.execute({sstv::rig::PttAction::key,
		clock->now() + 1s, 2, 201});
	expect(rejectedResult.error == sstv::rig::PttErrorCategory::rejected
		&& rejectedResult.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
		&& rejectedResult.providerCode == -9 && rejectedResult.mayHaveKeyed,
		"negative set RPRT retains code and confirmed safe readback");

	auto mismatchTransport = std::make_unique<ScriptedRigctldTransport>();
	mismatchTransport->responses.push_back({sstv::rig::RigctldTransportError::none,
		"set_ptt: 1\nRPRT 0\n", 5, {}});
	mismatchTransport->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 0\nRPRT 0\n", 3, {}});
	sstv::rig::RigctldPttProvider mismatch(configuration(12345), clock,
		std::move(mismatchTransport));
	const auto mismatchResult = mismatch.execute({sstv::rig::PttAction::key,
		clock->now() + 1s, 1, 202});
	expect(mismatchResult.error == sstv::rig::PttErrorCategory::rejected
		&& mismatchResult.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		"set success with mismatched readback is rejected conservatively");

	auto timeoutTransport = std::make_unique<ScriptedRigctldTransport>();
	timeoutTransport->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nRPRT -5\n", 3, {}});
	sstv::rig::RigctldPttProvider timeout(configuration(12345), clock,
		std::move(timeoutTransport));
	const auto timeoutResult = timeout.execute({sstv::rig::PttAction::query,
		clock->now() + 1s, 1, 203});
	expect(timeoutResult.error == sstv::rig::PttErrorCategory::timeout
		&& timeoutResult.providerCode == -5
		&& timeoutResult.certainty == sstv::rig::PttCertainty::indeterminate,
		"Hamlib timeout RPRT remains typed and indeterminate");

	auto expiredTransport = std::make_unique<ScriptedRigctldTransport>();
	ScriptedRigctldTransport* expiredObserver = expiredTransport.get();
	sstv::rig::RigctldPttProvider expired(configuration(12345), clock,
		std::move(expiredTransport));
	const auto expiredResult = expired.execute({sstv::rig::PttAction::query,
		clock->now(), 1, 204});
	expect(expiredResult.error == sstv::rig::PttErrorCategory::timeout
		&& expiredObserver->requests.empty(),
		"already-expired request performs no transport operation");

	auto invalidTransport = std::make_unique<ScriptedRigctldTransport>();
	ScriptedRigctldTransport* invalidObserver = invalidTransport.get();
	sstv::rig::RigctldPttProvider invalid(configuration(12345, "localhost"), clock,
		std::move(invalidTransport));
	const auto invalidResult = invalid.execute({sstv::rig::PttAction::query,
		clock->now() + 1s, 1, 205});
	expect(invalidResult.error == sstv::rig::PttErrorCategory::invalidRequest
		&& invalidObserver->requests.empty(),
		"invalid endpoint fails before injected transport use");

	auto unkeyTransport = std::make_unique<ScriptedRigctldTransport>();
	ScriptedRigctldTransport* unkeyObserver = unkeyTransport.get();
	unkeyObserver->responses.push_back({sstv::rig::RigctldTransportError::connectionRefused,
		{}, 0, "set connection refused"});
	unkeyObserver->responses.push_back({sstv::rig::RigctldTransportError::none,
		"get_ptt:\nPTT: 0\nRPRT 0\n", 3, {}});
	sstv::rig::RigctldPttProvider unkey(configuration(12345), clock,
		std::move(unkeyTransport));
	const auto unkeyResult = unkey.execute({sstv::rig::PttAction::unkey,
		clock->now() + 1s, 1, 206});
	expect(unkeyResult.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
		&& unkeyResult.error == sstv::rig::PttErrorCategory::disconnected
		&& unkeyObserver->requests == std::vector<std::string>{"+T 0\n", "+t\n"},
		"failed unkey set still queries readback under the original deadline");
}

void testRealLoopbackAndCoordinator()
{
	{
		LoopbackRigctldServer ipv6Server(true);
		auto clock = sstv::rig::createSteadyMonotonicClock();
		sstv::rig::RigctldPttProvider provider(configuration(ipv6Server.port(), "::1"), clock);
		const auto result = provider.execute({sstv::rig::PttAction::query,
			clock->now() + 2s, 1, 1});
		expect(result.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
			"IPv6 loopback query confirms unkeyed");
	}
	LoopbackRigctldServer server;
	server.setByteAtATime(true);
	auto clock = sstv::rig::createSteadyMonotonicClock();
	auto provider = std::make_shared<sstv::rig::RigctldPttProvider>(
		configuration(server.port()), clock);
	const auto query = provider->execute({sstv::rig::PttAction::query,
		clock->now() + 2s, 1, 1});
	expect(query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		"byte-fragmented loopback query succeeds");
	server.setByteAtATime(false);
	server.setKeyed(true);
	auto supervisor = std::make_shared<sstv::rig::PttSupervisor>(provider, clock);
	auto scheduler = sstv::rig::createSteadyMonotonicScheduler(clock);
	sstv::app::TransmitCoordinator coordinator(supervisor, clock, scheduler);
	sstv::app::TransmitRequest request;
	request.prefillFrames = 8;
	request.blockFrames = 2;
	request.policy.keyTimeout = 1s;
	request.policy.readbackTimeout = 1s;
	request.policy.preKeyDelay = 0ms;
	request.policy.heartbeatInterval = 20ms;
	request.policy.watchdogLease = 250ms;
	request.policy.audioDrainTimeout = 200ms;
	request.policy.postAudioTail = 0ms;
	request.policy.unkeyTimeout = 1s;
	request.policy.unkeyAttempts = 2;
	request.policy.retryDelay = 10ms;
	request.policy.shutdownDeadline = 3s;
	const auto result = coordinator.run(request, std::make_unique<TinySource>(),
		std::make_unique<MockAudio>());
	expect(result->outcome == sstv::app::TransmitOutcome::completed
		&& result->keyWasAttempted && result->nonSilentAudioWasReleased,
		"rigctld coordinator completes finite mock transmission");
	expect(result->ptt.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
		&& !result->ptt.hasHazard, "rigctld coordinator finishes definitely unkeyed");
	const auto requests = server.requests();
	expect(requests.size() >= 8 && requests.front() == "+t\n",
		"coordinator preflights PTT before mock audio acquisition");
}

} // namespace

int main()
{
	testConfigurationAndCodec();
	testInjectedProviderAndConformance();
	testInjectedFailureSemantics();
	testRealLoopbackAndCoordinator();
	std::cout << "M2F rigctld loopback tests passed\n";
	return 0;
}
