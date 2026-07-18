// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2e_flrig_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/transmit_coordinator.hpp>
#include <sstv/rig/flrig.hpp>

#include "flrig_internal.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
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

[[nodiscard]] std::string httpResponse(const std::string& body,
	const std::string& extra = {})
{
	return "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
		+ std::to_string(body.size()) + "\r\n" + extra + "\r\n" + body;
}

[[nodiscard]] std::string stateBody(const bool keyed)
{
	return "<?xml version=\"1.0\"?>\r\n<methodResponse><params><param>\r\n\t"
		"<value><i4>" + std::string(keyed ? "1" : "0")
		+ "</i4></value>\r\n</param></params></methodResponse>\r\n";
}

[[nodiscard]] std::string setBody()
{
	return "<?xml version=\"1.0\"?>\r\n<methodResponse><params><param>\r\n\t"
		"<value></value>\r\n</param></params></methodResponse>\r\n";
}

class ScriptedTransport final : public sstv::rig::FlrigTransport {
public:
	[[nodiscard]] sstv::rig::FlrigTransportResult exchange(
		const std::string& request, const sstv::rig::MonotonicTime) noexcept override
	{
		requests.push_back(request);
		if (responses.empty()) return {sstv::rig::FlrigTransportError::disconnected,
			{}, request.size(), "missing scripted response"};
		auto response = std::move(responses.front());
		responses.pop_front();
		if (response.requestBytesSent == 0) response.requestBytesSent = request.size();
		return response;
	}
	std::deque<sstv::rig::FlrigTransportResult> responses;
	std::vector<std::string> requests;
};

class LoopbackServer final {
public:
	explicit LoopbackServer(const bool isIpv6 = false) : isIpv6_(isIpv6)
	{
		listener_ = ::socket(isIpv6_ ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
		expect(listener_ >= 0, "loopback listener socket opens");
		const int enabled = 1;
		(void)::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
		if (isIpv6_) {
			sockaddr_in6 address{};
			address.sin6_family = AF_INET6;
			address.sin6_port = 0;
			expect(::inet_pton(AF_INET6, "::1", &address.sin6_addr) == 1,
				"IPv6 loopback address parses");
			expect(::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
				"IPv6 loopback listener binds");
		} else {
			sockaddr_in address{};
			address.sin_family = AF_INET;
			address.sin_port = 0;
			expect(::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1,
				"IPv4 loopback address parses");
			expect(::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0,
				"IPv4 loopback listener binds");
		}
		expect(::listen(listener_, 8) == 0, "loopback listener listens");
		if (isIpv6_) {
			sockaddr_in6 bound{};
			socklen_t size = sizeof(bound);
			expect(::getsockname(listener_, reinterpret_cast<sockaddr*>(&bound), &size) == 0,
				"IPv6 bound address is readable");
			expect(IN6_IS_ADDR_LOOPBACK(&bound.sin6_addr),
				"test server is bound only to IPv6 loopback");
			port_ = ntohs(bound.sin6_port);
		} else {
			sockaddr_in bound{};
			socklen_t size = sizeof(bound);
			expect(::getsockname(listener_, reinterpret_cast<sockaddr*>(&bound), &size) == 0,
				"IPv4 bound address is readable");
			expect(ntohl(bound.sin_addr.s_addr) == INADDR_LOOPBACK,
				"test server is bound only to IPv4 loopback");
			port_ = ntohs(bound.sin_port);
		}
		worker_ = std::jthread([this](const std::stop_token token) { run(token); });
	}
	~LoopbackServer()
	{
		worker_.request_stop();
		worker_.join();
		::close(listener_);
		listener_ = -1;
	}
	[[nodiscard]] std::uint16_t port() const noexcept { return port_; }
	void setKeyed(const bool keyed) noexcept { isKeyed_.store(keyed, std::memory_order_release); }
	void setByteAtATime(const bool enabled) noexcept { isByteAtATime_.store(enabled); }
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
			const int ready = ::poll(&state, 1, 100);
			if (ready <= 0) continue;
			const int client = ::accept(listener_, nullptr, nullptr);
			if (client < 0) continue;
			handle(client);
			::close(client);
		}
	}
	void handle(const int client)
	{
		std::string request;
		std::size_t expected = 0;
		char buffer[512];
		while (request.size() < 128 * 1024) {
			const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
			if (count <= 0) return;
			request.append(buffer, static_cast<std::size_t>(count));
			const std::size_t boundary = request.find("\r\n\r\n");
			if (boundary != std::string::npos && expected == 0) {
				const std::size_t field = request.find("Content-Length: ");
				if (field == std::string::npos) return;
				const std::size_t start = field + 16;
				const std::size_t end = request.find("\r\n", start);
				expected = boundary + 4 + static_cast<std::size_t>(std::stoul(request.substr(start, end - start)));
			}
			if (expected != 0 && request.size() >= expected) break;
		}
		{
			std::lock_guard lock(mutex_);
			requests_.push_back(request);
		}
		std::string response;
		if (request.find("<methodName>rig.set_ptt</methodName>") != std::string::npos) {
			isKeyed_.store(request.find("<i4>1</i4>") != std::string::npos,
				std::memory_order_release);
			response = httpResponse(setBody());
		} else {
			response = httpResponse(stateBody(isKeyed_.load(std::memory_order_acquire)));
		}
		if (isByteAtATime_.load(std::memory_order_acquire)) {
			for (const char character : response) {
				if (::send(client, &character, 1, 0) != 1) return;
			}
		} else {
			std::size_t offset = 0;
			while (offset < response.size()) {
				const ssize_t count = ::send(client, response.data() + offset, response.size() - offset, 0);
				if (count <= 0) return;
				offset += static_cast<std::size_t>(count);
			}
		}
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
	[[nodiscard]] sstv::app::SampleReadResult read(std::span<float> destination) noexcept override
	{
		if (offset_ >= 2 || destination.empty()) return std::size_t{0};
		const std::size_t count = std::min<std::size_t>(destination.size(), 2 - offset_);
		for (std::size_t index = 0; index < count; ++index) destination[index] = 0.125F;
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
		return {sstv::app::TransmitAudioFault::none, 0, queued_ > 0, isOpen_, isPrimed_, isRunning_};
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

[[nodiscard]] sstv::rig::FlrigConfiguration configuration(const std::uint16_t port)
{
	return {"127.0.0.1", port, "/RPC2", 500ms, 500ms, 1s, {}};
}

[[nodiscard]] sstv::rig::FlrigConfiguration ipv6Configuration(const std::uint16_t port)
{
	return {"::1", port, "/RPC2", 500ms, 500ms, 1s, {}};
}

void testConfigurationAndCodec()
{
	for (const std::string invalid : {"localhost", "127.0.0.2", "::ffff:127.0.0.1",
		"::ffff:192.0.2.1", "fe80::1%lo"}) {
		auto value = configuration(12345);
		value.address = invalid;
		expect(sstv::rig::validateFlrigConfiguration(value).has_value(),
			"non-literal-loopback address is rejected");
	}
	auto invalidPath = configuration(12345);
	invalidPath.path = "/RPC2?query";
	expect(sstv::rig::validateFlrigConfiguration(invalidPath).has_value(), "query path is rejected");
	const auto valid = configuration(12345);
	const std::string key = sstv::rig::flrigInternal::buildRequest(valid, sstv::rig::PttAction::key);
	expect(key.find("POST /RPC2 HTTP/1.1\r\n") == 0, "key request uses explicit path");
	expect(key.find("<methodName>rig.set_ptt</methodName>") != std::string::npos
		&& key.find("<i4>1</i4>") != std::string::npos, "key request is exact integer set RPC");
	const std::string query = sstv::rig::flrigInternal::buildRequest(valid, sstv::rig::PttAction::query);
	expect(query.find("<methodName>rig.get_ptt</methodName>") != std::string::npos
		&& query.find("<params>") == std::string::npos, "query request has no parameters");
	sstv::rig::flrigInternal::HttpResponse parsedHttp;
	const std::string response = httpResponse(stateBody(true), "X-Test: harmless\r\n");
	expect(!sstv::rig::flrigInternal::parseHttpResponse(response, valid.limits, parsedHttp),
		"valid HTTP response parses");
	sstv::rig::flrigInternal::XmlResponse parsedXml;
	expect(!sstv::rig::flrigInternal::parseXmlResponse(parsedHttp.body,
		sstv::rig::PttAction::query, valid.limits, parsedXml)
		&& parsedXml.state == sstv::rig::PttObservedState::keyed, "integer keyed readback parses");
	for (std::size_t split = 0; split < response.size(); ++split) {
		std::size_t total = 0;
		bool header = false;
		const auto error = sstv::rig::flrigInternal::findHttpMessageLength(
			std::string_view(response).substr(0, split), valid.limits, total, header);
		expect(!error.has_value(), "fragmented bounded response remains parseable");
	}
	expect(sstv::rig::flrigInternal::parseHttpResponse(
		"HTTP/1.1 302 Found\r\nContent-Type: text/xml\r\nContent-Length: 0\r\n\r\n",
		valid.limits, parsedHttp).has_value(), "redirect is rejected");
	expect(sstv::rig::flrigInternal::parseHttpResponse(
		"HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nTransfer-Encoding: chunked\r\n\r\n",
		valid.limits, parsedHttp).has_value(), "chunked response is rejected");
	expect(sstv::rig::flrigInternal::parseXmlResponse(
		"<!DOCTYPE x [<!ENTITY e SYSTEM \"file:///etc/passwd\">]><methodResponse/>",
		sstv::rig::PttAction::query, valid.limits, parsedXml).has_value(), "DTD is rejected");
	expect(sstv::rig::flrigInternal::parseXmlResponse(
		"<methodResponse><params><param><value><i4>2</i4></value></param></params></methodResponse>",
		sstv::rig::PttAction::query, valid.limits, parsedXml).has_value(), "invalid PTT value is rejected");
	const std::string fault = "<methodResponse><fault><value><struct><member>"
		"<name>faultCode</name><value><i4>-1</i4></value></member><member>"
		"<name>faultString</name><value>rejected &amp; safe</value></member>"
		"</struct></value></fault></methodResponse>";
	expect(!sstv::rig::flrigInternal::parseXmlResponse(fault,
		sstv::rig::PttAction::query, valid.limits, parsedXml)
		&& parsedXml.kind == sstv::rig::flrigInternal::XmlResponseKind::fault,
		"bounded standard XML-RPC fault parses");
	const std::string overflowFault = "<methodResponse><fault><value><struct><member>"
		"<name>faultCode</name><value><i4>999999999999</i4></value></member><member>"
		"<name>faultString</name><value>bad</value></member></struct></value>"
		"</fault></methodResponse>";
	expect(sstv::rig::flrigInternal::parseXmlResponse(overflowFault,
		sstv::rig::PttAction::query, valid.limits, parsedXml).has_value(),
		"overflowing XML-RPC fault code is rejected");
}

void testInjectedProviderCertainty()
{
	auto clock = sstv::rig::createSteadyMonotonicClock();
	auto transport = std::make_unique<ScriptedTransport>();
	ScriptedTransport* observer = transport.get();
	observer->responses.push_back({sstv::rig::FlrigTransportError::none,
		httpResponse(setBody()), 1, {}});
	observer->responses.push_back({sstv::rig::FlrigTransportError::none,
		httpResponse(stateBody(true)), 1, {}});
	sstv::rig::FlrigPttProvider provider(configuration(12345), clock, std::move(transport));
	const auto keyed = provider.execute({sstv::rig::PttAction::key, clock->now() + 1s, 1, 1});
	expect(keyed.error == sstv::rig::PttErrorCategory::none
		&& keyed.certainty == sstv::rig::PttCertainty::definitelyKeyed
		&& keyed.mayHaveKeyed, "set plus readback confirms key certainty");
	expect(observer->requests.size() == 2, "key uses a set RPC and independent readback RPC");

	auto lostTransport = std::make_unique<ScriptedTransport>();
	lostTransport->responses.push_back({sstv::rig::FlrigTransportError::disconnected,
		{}, 20, "lost key response"});
	lostTransport->responses.push_back({sstv::rig::FlrigTransportError::disconnected,
		{}, 20, "lost query response"});
	sstv::rig::FlrigPttProvider lost(configuration(12345), clock, std::move(lostTransport));
	const auto ambiguous = lost.execute({sstv::rig::PttAction::key, clock->now() + 1s, 1, 2});
	expect(ambiguous.certainty == sstv::rig::PttCertainty::indeterminate
		&& ambiguous.mayHaveKeyed, "lost key response remains possibly keyed");

	auto expiredTransport = std::make_unique<ScriptedTransport>();
	ScriptedTransport* expiredObserver = expiredTransport.get();
	sstv::rig::FlrigPttProvider expired(configuration(12345), clock, std::move(expiredTransport));
	const auto expiredResult = expired.execute({sstv::rig::PttAction::query, clock->now(), 1, 3});
	expect(expiredResult.error == sstv::rig::PttErrorCategory::timeout
		&& expiredObserver->requests.empty(), "expired deadline performs no transport operation");
}

void testRealLoopbackAndCoordinator()
{
	{
		LoopbackServer ipv6Server(true);
		auto ipv6Clock = sstv::rig::createSteadyMonotonicClock();
		sstv::rig::FlrigPttProvider ipv6Provider(
			ipv6Configuration(ipv6Server.port()), ipv6Clock);
		const auto ipv6Query = ipv6Provider.execute(
			{sstv::rig::PttAction::query, ipv6Clock->now() + 2s, 1, 1});
		expect(ipv6Query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
			"real IPv6 loopback query confirms unkeyed");
	}
	LoopbackServer server;
	server.setByteAtATime(true);
	auto clock = sstv::rig::createSteadyMonotonicClock();
	auto provider = std::make_shared<sstv::rig::FlrigPttProvider>(
		configuration(server.port()), clock);
	const auto queried = provider->execute({sstv::rig::PttAction::query, clock->now() + 2s, 1, 1});
	expect(queried.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		"real loopback query confirms unkeyed");
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
	expect(result->outcome == sstv::app::TransmitOutcome::completed,
		"loopback flrig coordinator completes finite mock session");
	expect(result->keyWasAttempted && result->nonSilentAudioWasReleased,
		"mock signal is released only after confirmed loopback keying");
	expect(result->ptt.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
		&& !result->ptt.hasHazard, "loopback session finishes definitely unkeyed");
	const auto requests = server.requests();
	expect(requests.size() >= 8, "coordinator performs preflight, cleanup, key, and unkey readbacks");
	expect(requests[1].find("rig.get_ptt") != std::string::npos,
		"coordinator preflight queries PTT before audio");
}

} // namespace

int main()
{
	testConfigurationAndCodec();
	testInjectedProviderCertainty();
	testRealLoopbackAndCoordinator();
	std::cout << "M2E flrig loopback tests passed\n";
	return 0;
}
