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

namespace {

using OptionMap = std::map<std::string, std::string>;

[[nodiscard]] bool
isKnownValueOption(const std::string_view argument) noexcept
{
	constexpr std::array<std::string_view, 48> names{
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
		"--calibration-method", "--identity-collision", "--fsk-id-enabled"};
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
	configuration.pttAddress = requireOption(options, "--ptt-address");
	configuration.pttPort = parseInteger<std::uint16_t>(options, "--ptt-port");
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
		   "      --confirm 'AUTHORIZE M2J discovery SHA256'\n"
		   "  scanline-sstv-cli hil-stage --stage ptt-unkey ...\n"
		   "\n"
		   "This hardware-free Stage 0 command creates no discovery, audio, PTT, or\n"
		   "socket resource. Required groups: build/UTC/worktree, OS/session, exact\n"
		   "audio identity/collision/channel, literal-loopback PTT, operator radio and\n"
		   "safety observations, fixture/hash/timing/gain, and explicit output dir.\n"
		   "See docs/hil/M2J_RUNBOOK.md and M2J_EVIDENCE_TEMPLATE.md.\n";
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
	const std::string digest = requireOption(options, "--digest");
	if (digest.size() != 64U || digest.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
		throw std::invalid_argument("invalid configuration digest");
	}
	if (jsonString(json, "configuration_digest") != digest) {
		throw std::invalid_argument("configuration digest does not match the evidence session");
	}
	const std::string confirmation = requireOption(options, "--confirm");
	if (confirmation != sstv::app::hilStageConfirmationPhrase(
		stage == "discovery" ? sstv::app::HilStage::discovery : sstv::app::HilStage::pttUnkey,
		digest)) {
		throw std::invalid_argument("confirmation phrase does not match the digest");
	}
	if (stage == "ptt-unkey") {
#if !defined(SSTV_BUILD_AUDIO) || !defined(SSTV_BUILD_RIG)
		throw std::runtime_error("Stage 3 is unavailable in this audio/rig-disabled build");
#else
		const std::string providerName = requireOption(options, "--ptt-provider");
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
		} else if (providerName == "rigctld") {
			sstv::rig::RigctldConfiguration configuration{address, port};
			if (const auto error = sstv::rig::validateRigctldConfiguration(configuration))
				throw std::invalid_argument(error->message);
			provider = std::make_shared<sstv::rig::RigctldPttProvider>(configuration, clock,
				sstv::rig::createRigctldPosixTransport(configuration, clock));
		} else {
			throw std::invalid_argument("unsupported PTT provider");
		}
		const sstv::rig::PttOperationResult query = provider->execute({
			sstv::rig::PttAction::query, clock->now() + std::chrono::seconds(2), 1, 1});
		std::cout << "Stage 3 query: " << (query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed
			? "definitely-unkeyed" : "not-definitely-unkeyed") << '\n';
		if (query.certainty == sstv::rig::PttCertainty::definitelyUnkeyed) return 0;
		const sstv::rig::PttOperationResult unkey = provider->execute({
			sstv::rig::PttAction::unkey, clock->now() + std::chrono::seconds(2), 1, 2});
		if (unkey.certainty != sstv::rig::PttCertainty::definitelyUnkeyed)
			throw std::runtime_error("PTT state remains unresolved; later stages are blocked");
		return 0;
#endif
	}
	const auto backend = sstv::audio::parseAudioBackend(requireOption(options, "--backend"));
	if (!backend) throw std::invalid_argument("unknown audio backend");
	const std::string requestedIdentity = requireOption(options, "--playback-id");
	if (requestedIdentity.empty()) throw std::invalid_argument("playback identity is empty");
	sstv::audio::AudioDiscoveryService service(
		sstv::audio::createMiniaudioDiscoveryProvider(),
		sstv::audio::createSystemAudioTransportClassifier());
	const sstv::audio::DiscoveryResult result = service.refresh({{*backend}, false});
	if (const auto* error = std::get_if<sstv::audio::DiscoveryError>(&result)) {
		throw std::runtime_error(error->message);
	}
	const auto snapshot = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result);
	const auto found = std::ranges::find_if(snapshot->backends.front().devices,
		[&](const sstv::audio::AudioDevice& device) {
			return device.identity.direction == sstv::audio::AudioDirection::playback
				&& device.identity.opaque == requestedIdentity;
		});
	if (found == snapshot->backends.front().devices.end()) throw std::runtime_error("requested playback identity was not found");
	const auto second = std::ranges::find_if(std::next(found), snapshot->backends.front().devices.end(),
		[&](const sstv::audio::AudioDevice& device) { return device.identity == found->identity; });
	if (found->hasIdentityCollision || second != snapshot->backends.front().devices.end()) throw std::runtime_error("requested playback identity collides");
	std::cout << "Stage 1 discovery passed for exact identity " << requestedIdentity
		<< " (no audio device was opened)\n";
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
