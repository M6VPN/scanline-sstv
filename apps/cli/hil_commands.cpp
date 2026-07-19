// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/hil_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hil_commands.hpp"

#include <sstv/app/hil_evidence.hpp>
#include <sstv/audio/audio_discovery.hpp>
#include <sstv/rig/flrig.hpp>
#include <sstv/rig/rigctld.hpp>

#include <algorithm>
#include <charconv>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace {

using OptionMap = std::map<std::string, std::string>;

[[nodiscard]] bool
isKnownValueOption(const std::string_view argument) noexcept
{
	constexpr std::array<std::string_view, 49> names{
		"--stage", "--evidence-dir", "--digest", "--confirm",
		"--output-dir", "--utc-start", "--git-commit", "--compiler",
		"--compiler-version", "--preset", "--cmake-options", "--worktree",
		"--miniaudio-version", "--os",
		"--kernel", "--arch", "--host-id", "--session", "--backend",
		"--playback-id", "--identity-persistence", "--channel", "--channels",
		"--ptt-provider", "--ptt-address", "--ptt-port", "--radio-manufacturer",
		"--radio-model", "--audio-interface", "--cabling", "--test-arrangement",
		"--radio-mode", "--frequency", "--power", "--vox", "--compressor",
		"--tot", "--antenna", "--mode", "--fixture-sha256", "--flrig-path",
		"--qt-version", "--radio-firmware", "--instrument",
		"--calibration-method", "--identity-collision", "--fsk-id-enabled",
		"--flrig-version"};
	constexpr std::array<std::string_view, 5> numericNames{
		"--duration-ns", "--frame-count", "--gain-dbfs", "--pre-key-ms",
		"--post-audio-ms"};
	return std::ranges::find(names, argument) != names.end()
		|| std::ranges::find(numericNames, argument) != numericNames.end();
}

[[nodiscard]] sstv::app::HilStageResult
makePendingStage(const sstv::app::HilStage stage)
{
	sstv::app::HilStageResult result;
	result.stage = stage;
	return result;
}

[[nodiscard]] OptionMap
parseOptions(const int argc, char* argv[], bool& force)
{
	OptionMap options;
	for (int index = 2; index < argc; ++index) {
		const std::string argument(argv[index]);
		if (!argument.starts_with("--")) {
			throw std::invalid_argument("unexpected argument: " + argument);
		}
		if (argument == "--force") {
			if (force) throw std::invalid_argument("duplicate option: " + argument);
			force = true;
			continue;
		}
		if (!isKnownValueOption(argument)) {
			throw std::invalid_argument("unexpected option: " + argument);
		}
		if (options.contains(argument)) {
			throw std::invalid_argument("duplicate option: " + argument);
		}
		if (++index >= argc || std::string_view(argv[index]).starts_with("--")) {
			throw std::invalid_argument("missing value for " + argument);
		}
		options.emplace(argument, argv[index]);
	}
	return options;
}

[[nodiscard]] const std::string&
requireOption(const OptionMap& options, const std::string& name)
{
	const auto found = options.find(name);
	if (found == options.end() || found->second.empty()) {
		throw std::invalid_argument("required option is missing: " + name);
	}
	return found->second;
}

[[nodiscard]] bool
parseChoice(const OptionMap& options, const std::string& name,
	const std::string_view trueValue, const std::string_view falseValue)
{
	const std::string& value = requireOption(options, name);
	if (value == trueValue) return true;
	if (value == falseValue) return false;
	throw std::invalid_argument("invalid choice for " + name);
}

[[nodiscard]] std::optional<std::string>
optionalOption(const OptionMap& options, const std::string& name)
{
	const auto found = options.find(name);
	if (found == options.end()) return std::nullopt;
	return found->second;
}

template<typename Value>
[[nodiscard]] Value
parseInteger(const OptionMap& options, const std::string& name)
{
	const std::string& text = requireOption(options, name);
	Value value{};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size()) {
		throw std::invalid_argument("invalid integer for " + name);
	}
	return value;
}

[[nodiscard]] double
parseDouble(const OptionMap& options, const std::string& name)
{
	const std::string& text = requireOption(options, name);
	double value = 0.0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size()) {
		throw std::invalid_argument("invalid number for " + name);
	}
	return value;
}

[[nodiscard]] sstv::app::HilEvidenceRecord
makeManifest(const OptionMap& options)
{
	using namespace sstv::app;
	HilEvidenceRecord record;
	record.build = {requireOption(options, "--utc-start"), std::nullopt,
		requireOption(options, "--git-commit"),
		parseChoice(options, "--worktree", "dirty", "clean"),
		requireOption(options, "--compiler"),
		requireOption(options, "--compiler-version"),
		requireOption(options, "--preset"),
		{{"configured", requireOption(options, "--cmake-options")}},
		requireOption(options, "--miniaudio-version"),
		optionalOption(options, "--qt-version"),
		requireOption(options, "--os"), requireOption(options, "--kernel"),
		requireOption(options, "--arch"), requireOption(options, "--host-id"),
		requireOption(options, "--session")};
	HilConfiguration& configuration = record.configuration;
	configuration.audioBackend = requireOption(options, "--backend");
	configuration.playbackIdentity = requireOption(options, "--playback-id");
	configuration.identityPersistence = requireOption(options, "--identity-persistence");
	configuration.hasIdentityCollision = parseChoice(options,
		"--identity-collision", "yes", "no");
	configuration.selectedChannel = parseInteger<std::uint32_t>(options, "--channel");
	configuration.channelCount = parseInteger<std::uint32_t>(options, "--channels");
	configuration.pttProvider = requireOption(options, "--ptt-provider");
	configuration.pttProviderVersion = optionalOption(options, "--flrig-version");
	configuration.pttAddress = requireOption(options, "--ptt-address");
	if (options.contains("--ptt-port")) {
		configuration.pttPort = parseInteger<std::uint16_t>(options, "--ptt-port");
	}
	configuration.flrigPath = optionalOption(options, "--flrig-path");
	configuration.radioManufacturer = requireOption(options, "--radio-manufacturer");
	configuration.radioModel = requireOption(options, "--radio-model");
	configuration.radioFirmware = optionalOption(options, "--radio-firmware");
	configuration.audioInterface = requireOption(options, "--audio-interface");
	configuration.cabling = requireOption(options, "--cabling");
	configuration.testArrangement = requireOption(options, "--test-arrangement");
	configuration.radioMode = requireOption(options, "--radio-mode");
	configuration.frequency = requireOption(options, "--frequency");
	configuration.power = requireOption(options, "--power");
	configuration.voxState = requireOption(options, "--vox");
	configuration.compressorState = requireOption(options, "--compressor");
	configuration.timeoutState = requireOption(options, "--tot");
	configuration.antennaState = requireOption(options, "--antenna");
	configuration.sstvMode = requireOption(options, "--mode");
	configuration.fixtureSha256 = requireOption(options, "--fixture-sha256");
	configuration.hasFskIdentifier = parseChoice(options,
		"--fsk-id-enabled", "yes", "no");
	configuration.durationNanoseconds
		= parseInteger<std::uint64_t>(options, "--duration-ns");
	configuration.frameCount = parseInteger<std::uint64_t>(options, "--frame-count");
	configuration.gainDbfs = parseDouble(options, "--gain-dbfs");
	configuration.preKeyMilliseconds
		= parseInteger<std::uint64_t>(options, "--pre-key-ms");
	configuration.postAudioMilliseconds
		= parseInteger<std::uint64_t>(options, "--post-audio-ms");
	configuration.instrumentDescription = optionalOption(options, "--instrument");
	configuration.calibrationMethod = optionalOption(options, "--calibration-method");
	record.stages = {
		{HilStage::manifest, HilResultState::passed,
			HilEvidenceSource::automaticallyMeasured, record.build.utcStarted,
			record.build.utcStarted, std::nullopt, std::nullopt, {}, std::nullopt,
			{{"resource_acquisitions", std::string("0")}}, std::nullopt},
		makePendingStage(HilStage::discovery),
		makePendingStage(HilStage::audioCalibration),
		makePendingStage(HilStage::pttUnkey),
		makePendingStage(HilStage::keyedSilence),
		makePendingStage(HilStage::fullSstv),
		makePendingStage(HilStage::controlledFault),
		makePendingStage(HilStage::guiCompositor)};
	record.artifacts = {{"robot-36-reference.png",
		"1676228514e96b46f144f05875ec7c74e3e16dca4d98291ea7f5d93658c1a1b7",
		48'521U, false}};
	record.limitations = {"No physical HIL evidence is present in this manifest.",
		"SIGKILL, power loss, OS failure, daemon failure, cables, and radio hardware "
		"remain outside in-process cleanup guarantees."};
	return record;
}

} // namespace

bool
isHilCommand(const std::string_view argument) noexcept
{
	return argument == "hil-manifest" || argument == "hil-stage";
}

void
printHilCommandHelp()
{
	std::cout
		<< "\nM2J hardware-in-loop evidence preparation:\n"
		   "  scanline-sstv-cli hil-manifest --output-dir DIR [metadata options]\n"
		   "  scanline-sstv-cli hil-stage --stage discovery --evidence-dir DIR\n"
		   "      --backend BACKEND --playback-id ID --digest SHA256\n"
		   "      (type the confirmation phrase at the interactive prompt)\n"
		   "  scanline-sstv-cli hil-stage --stage ptt-unkey --evidence-dir DIR\n"
		   "      --ptt-provider flrig --ptt-address 127.0.0.1 --ptt-port PORT\n"
		   "      --flrig-path PATH --flrig-version VERSION --digest SHA256\n"
		   "\n"
		   "This hardware-free Stage 0 command creates no discovery, audio, PTT, or\n"
		   "socket resource. Required groups: build/UTC/worktree, OS/session, exact\n"
		   "audio identity/collision/channel, literal-loopback PTT, operator radio and\n"
		   "safety observations, fixture/hash/timing/gain, and explicit output dir.\n"
		   "See docs/hil/M2J_RUNBOOK.md and M2J_EVIDENCE_TEMPLATE.md.\n";
}

[[nodiscard]] bool
confirmInteractive(const sstv::app::HilStage stage, const std::string_view digest)
{
	if (!::isatty(STDIN_FILENO)) {
		throw std::invalid_argument("a foreground interactive terminal is required");
	}
	const std::string phrase = sstv::app::hilStageConfirmationPhrase(stage, digest);
	std::cout << "Type exactly: " << phrase << "\n> " << std::flush;
	std::string entered;
	if (!std::getline(std::cin, entered) || entered != phrase) {
		throw std::invalid_argument("confirmation phrase was rejected");
	}
	sstv::app::HilStageAuthorizer authorizer;
	const auto permit = authorizer.authorize(stage, digest, entered, true);
	if (const auto* error = std::get_if<sstv::app::HilError>(&permit)) {
		throw std::invalid_argument(error->message);
	}
	if (const auto error = authorizer.consume(std::get<sstv::app::HilStagePermit>(permit),
		stage, digest)) {
		throw std::invalid_argument(error->message);
	}
	return true;
}

[[nodiscard]] std::string
readBoundedFile(const std::filesystem::path& path)
{
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error) || error) {
		throw std::invalid_argument("evidence JSON is not a regular file");
	}
	if (std::filesystem::file_size(path, error) > 2U * 1024U * 1024U || error) {
		throw std::invalid_argument("evidence JSON exceeds the configured limit");
	}
	std::ifstream input(path);
	if (!input) throw std::runtime_error("cannot open evidence JSON");
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::string
jsonString(const std::string& json, const std::string_view key)
{
	const std::string marker = "\"" + std::string(key) + "\":";
	const std::size_t start = json.find(marker);
	if (start == std::string::npos) throw std::invalid_argument("missing evidence field: " + std::string(key));
	const std::size_t quote = json.find('"', start + marker.size());
	const std::size_t end = json.find('"', quote + 1U);
	if (quote == std::string::npos || end == std::string::npos) throw std::invalid_argument("invalid evidence field: " + std::string(key));
	return json.substr(quote + 1U, end - quote - 1U);
}

[[nodiscard]] int
runHilStage(const int argc, char* argv[])
{
	bool force = false;
	const OptionMap options = parseOptions(argc, argv, force);
	const std::string stage = requireOption(options, "--stage");
	const std::filesystem::path directory = requireOption(options, "--evidence-dir");
	if (stage != "discovery" && stage != "ptt-unkey") {
		throw std::invalid_argument("unsupported executable HIL stage");
	}
	const std::string json = readBoundedFile(directory / "m2j-evidence-v1.json");
	if (json.find("\"stage\":\"manifest\",\"state\":\"passed\"") == std::string::npos) {
		throw std::invalid_argument("a passed Stage 0 manifest is required");
	}
	if (json.find("\"stage\":\"" + stage + "\",\"state\":\"passed\"")
			!= std::string::npos) {
		throw std::invalid_argument("this HIL stage has already passed");
	}
	if (stage == "ptt-unkey"
			&& json.find("\"stage\":\"discovery\",\"state\":\"passed\"")
			== std::string::npos) {
		throw std::invalid_argument("Stage 1 discovery must pass before Stage 3");
	}
	const std::string digest = requireOption(options, "--digest");
	if (digest.size() != 64U || digest.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
		throw std::invalid_argument("invalid configuration digest");
	}
	if (jsonString(json, "configuration_digest") != digest) {
		throw std::invalid_argument("configuration digest does not match the evidence session");
	}
	const sstv::app::HilStage requestedStage = stage == "discovery"
		? sstv::app::HilStage::discovery : sstv::app::HilStage::pttUnkey;
	(void)confirmInteractive(requestedStage, digest);
	if (stage == "ptt-unkey") {
#if !defined(SSTV_ENABLE_LIVE_TX) || !defined(SSTV_ENABLE_TX_HARDWARE_TESTS) \
		|| !defined(SSTV_ARM_TX_HARDWARE_TESTS)
		throw std::runtime_error("Stage 3 is unavailable in this audio/rig-disabled build");
#else
		const std::string providerName = requireOption(options, "--ptt-provider");
		if (providerName != "flrig") throw std::invalid_argument("Stage 3 requires flrig");
		const std::string flrigVersion = requireOption(options, "--flrig-version");
		if (json.find("\"ptt_provider_version\":\"" + flrigVersion + "\"")
				== std::string::npos) {
			throw std::invalid_argument("flrig version does not match the evidence session");
		}
		const std::string address = requireOption(options, "--ptt-address");
		const auto port = parseInteger<std::uint16_t>(options, "--ptt-port");
		const auto clock = sstv::rig::createSteadyMonotonicClock();
		std::shared_ptr<sstv::rig::PttProvider> provider;
		if (providerName == "flrig") {
			sstv::rig::FlrigConfiguration configuration{address, port,
				requireOption(options, "--flrig-path")};
			if (const auto error = sstv::rig::validateFlrigConfiguration(configuration))
				throw std::invalid_argument(error->message);
			provider = std::make_shared<sstv::rig::FlrigPttProvider>(configuration, clock,
				sstv::rig::createFlrigPosixTransport(configuration, clock));
		}
		const sstv::rig::PttOperationResult query = provider->execute({
			sstv::rig::PttAction::query, clock->now() + std::chrono::seconds(2), 1, 1});
		std::cout << "Stage 3 query: " << (query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
			? "definitely-unkeyed" : "not-definitely-unkeyed") << '\n';
		const auto measurements = std::map<std::string, std::optional<std::string>>{
			{"provider", providerName}, {"provider_version", flrigVersion},
			{"operation", "query-unkey-query"},
			{"query_certainty", query.certainty == sstv::rig::PttCertainty::definitelyKeyed
				? "definitely-keyed" : query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
					? "definitely-unkeyed" : "indeterminate"}};
		if (query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed) {
			const auto publication = sstv::app::publishHilStageResult(directory,
				sstv::app::HilStage::pttUnkey, sstv::app::HilResultState::passed,
				sstv::app::HilEvidenceSource::automaticallyMeasured, measurements,
				std::nullopt, "definitely-unkeyed");
			if (std::holds_alternative<sstv::app::HilError>(publication))
				throw std::runtime_error(std::get<sstv::app::HilError>(publication).message);
			std::cout << "Stage 3 passed: definitely-unkeyed (query only)\n";
			return 0;
		}
		const auto scheduler = sstv::rig::createSteadyMonotonicScheduler(clock);
		sstv::rig::PttSupervisor supervisor(provider, clock);
		const sstv::rig::PttCleanupResult cleanup = supervisor.unkey(
			{std::chrono::milliseconds(500), 3U, std::chrono::milliseconds(100)}, *scheduler);
		if (cleanup.certainty != sstv::rig::PttCertainty::definitelyUnkeyed)
			throw std::runtime_error("PTT state remains unresolved; later stages are blocked");
		const auto publication = sstv::app::publishHilStageResult(directory,
			sstv::app::HilStage::pttUnkey, sstv::app::HilResultState::passed,
			sstv::app::HilEvidenceSource::automaticallyMeasured, measurements,
			std::nullopt, "definitely-unkeyed");
		if (std::holds_alternative<sstv::app::HilError>(publication))
			throw std::runtime_error(std::get<sstv::app::HilError>(publication).message);
		return 0;
#endif
	}
	const auto failDiscovery = [&](const std::string& message) -> void {
		const auto publication = sstv::app::publishHilStageResult(directory,
			sstv::app::HilStage::discovery, sstv::app::HilResultState::failed,
			sstv::app::HilEvidenceSource::automaticallyMeasured, {}, message);
		if (std::holds_alternative<sstv::app::HilError>(publication))
			throw std::runtime_error(std::get<sstv::app::HilError>(publication).message);
		throw std::runtime_error(message);
	};
	const auto backend = sstv::audio::parseAudioBackend(requireOption(options, "--backend"));
	if (!backend) failDiscovery("unknown audio backend");
	const std::string requestedIdentity = requireOption(options, "--playback-id");
	const std::string identityPrefix = std::string(sstv::audio::audioBackendApiName(*backend))
		+ ":playback:";
	if (!requestedIdentity.starts_with(identityPrefix)
			|| requestedIdentity.size() == identityPrefix.size()) {
		throw std::invalid_argument("playback identity must be the complete backend:direction:selector value");
	}
	const std::string requestedOpaque = requestedIdentity.substr(identityPrefix.size());
	sstv::audio::AudioDiscoveryService service(
		sstv::audio::createMiniaudioDiscoveryProvider(),
		sstv::audio::createSystemAudioTransportClassifier());
	const sstv::audio::DiscoveryResult result = service.refresh({{*backend}, false});
	if (const auto* error = std::get_if<sstv::audio::DiscoveryError>(&result)) {
		failDiscovery(error->message);
	}
	const auto snapshot = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result);
	const auto found = std::ranges::find_if(snapshot->backends.front().devices,
		[&](const sstv::audio::AudioDevice& device) {
			return device.identity.direction == sstv::audio::AudioDirection::playback
				&& device.identity.opaque == requestedOpaque;
		});
	if (found == snapshot->backends.front().devices.end()) failDiscovery("requested playback identity was not found");
	if (found->identity.stability != sstv::audio::IdentityStability::persistent
			|| found->isDefault || found->hasIdentityCollision) {
		failDiscovery("requested identity is not one persistent, non-default, non-colliding match");
	}
	const auto second = std::ranges::find_if(std::next(found), snapshot->backends.front().devices.end(),
		[&](const sstv::audio::AudioDevice& device) { return device.identity == found->identity; });
	if (found->hasIdentityCollision || second != snapshot->backends.front().devices.end()) failDiscovery("requested playback identity collides");
	std::string formats;
	for (const auto& format : found->capabilities.nativeFormats) {
		if (!formats.empty()) formats += ",";
		formats += std::to_string(static_cast<int>(format.format));
		formats += "/" + std::to_string(format.channels.value_or(0U)) + "ch/"
			+ std::to_string(format.sampleRate.value_or(0U)) + "Hz";
	}
	const auto measurements = std::map<std::string, std::optional<std::string>>{
		{"backend_api", std::string(snapshot->backends.front().apiName)},
		{"backend_status", "available"}, {"device_name", found->name},
		{"identity", requestedIdentity}, {"direction", "playback"},
		{"identity_persistence", "persistent"}, {"is_default", "no"},
		{"identity_collision", "no"}, {"transport", "unknown"},
		{"native_formats", formats.empty() ? "unknown" : formats},
		{"native_capabilities_known", found->capabilities.detailsKnown ? "yes" : "no"},
		{"discovery_generation", std::to_string(snapshot->generation)},
		{"negotiated_facts", "not-measured"}};
	const auto publication = sstv::app::publishHilStageResult(directory,
		sstv::app::HilStage::discovery, sstv::app::HilResultState::passed,
		sstv::app::HilEvidenceSource::automaticallyMeasured, measurements,
		std::nullopt);
	if (std::holds_alternative<sstv::app::HilError>(publication))
		throw std::runtime_error(std::get<sstv::app::HilError>(publication).message);
	std::cout << "Stage 1 discovery passed for exact identity " << requestedIdentity
		<< " (no audio device was opened)\n"
		<< "Evidence updated: " << directory << "\n";
	return 0;
}

int
runHilCommand(const int argc, char* argv[])
{
	try {
		if (argc > 1 && std::string_view(argv[1]) == "hil-stage") return runHilStage(argc, argv);
		bool force = false;
		const OptionMap options = parseOptions(argc, argv, force);
		const std::filesystem::path output
			= requireOption(options, "--output-dir");
		const sstv::app::HilEvidenceRecord record = makeManifest(options);
		const sstv::app::HilPublicationResult result
			= sstv::app::publishHilEvidence(record, output, force);
		if (const auto* error = std::get_if<sstv::app::HilError>(&result)) {
			std::cerr << "Error: " << error->message << '\n';
			return error->code == sstv::app::HilErrorCode::destinationExists ? 2 : 1;
		}
		const sstv::app::HilPublication& publication
			= std::get<sstv::app::HilPublication>(result);
		std::cout << "M2J stage: manifest\n"
			<< "Resources acquired: 0\n"
			<< "Configuration digest: "
			<< sstv::app::calculateHilConfigurationDigest(record.configuration) << '\n'
			<< "JSON: " << publication.jsonPath << '\n'
			<< "Markdown: " << publication.markdownPath << '\n';
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
