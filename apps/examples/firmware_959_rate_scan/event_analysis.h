#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "nalu_event_collector/collector/event_builder.h"
#include "nalu_event_collector/config/event_builder_config.h"
#include "nalu_event_collector/config/packet_parser_config.h"
#include "nalu_event_collector/data/event.h"
#include "nalu_event_collector/parsing/packet_parser.h"

namespace firmware_959_rate_scan {

using json = nlohmann::json;
using nalu_event_collector::Event;

struct EventAnalysisAccumulator {
    uint64_t events = 0;
    uint64_t multi_channel_events = 0;
    uint64_t packets_in_events = 0;
    std::vector<uint32_t> event_timestamps;
    std::map<int, std::vector<uint32_t>> channel_timestamps;
};

inline uint32_t read_u32_le(std::istream& input) {
    uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const int byte = input.get();
        if (byte == EOF) {
            throw std::runtime_error("Truncated NALUUDP1 integer");
        }
        value |= static_cast<uint32_t>(static_cast<uint8_t>(byte)) << shift;
    }
    return value;
}

inline uint64_t read_u64_le(std::istream& input) {
    uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        const int byte = input.get();
        if (byte == EOF) {
            throw std::runtime_error("Truncated NALUUDP1 integer");
        }
        value |= static_cast<uint64_t>(static_cast<uint8_t>(byte)) << shift;
    }
    return value;
}

inline void accumulate_event(const Event& event, EventAnalysisAccumulator& output) {
    std::map<int, uint32_t> observed_channels;
    for (uint16_t index = 0; index < event.header.num_packets; ++index) {
        const auto& packet = event.packets[index];
        observed_channels.emplace(static_cast<int>(packet.channel), packet.trigger_time);
    }
    if (observed_channels.empty()) {
        return;
    }

    ++output.events;
    output.packets_in_events += event.header.num_packets;
    output.event_timestamps.push_back(event.header.reference_time);
    if (observed_channels.size() > 1) {
        ++output.multi_channel_events;
    }
    for (const auto& [channel, timestamp] : observed_channels) {
        output.channel_timestamps[channel].push_back(timestamp);
    }
}

inline void drain_events(nalu_event_collector::EventBuilder& builder,
                         size_t keep,
                         EventAnalysisAccumulator& output) {
    auto& buffer = builder.get_event_buffer();
    const size_t size = buffer.get_events().size();
    if (size <= keep) {
        return;
    }
    auto events = buffer.take_events_before_index_exclusive(size - keep);
    for (const auto& event : events) {
        accumulate_event(*event, output);
    }
}

inline double median(std::vector<uint32_t> values) {
    if (values.empty()) {
        return 0.0;
    }
    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    if (values.size() % 2 != 0) {
        return values[middle];
    }
    const uint32_t upper = values[middle];
    std::nth_element(values.begin(), values.begin() + middle - 1, values.end());
    return (static_cast<double>(values[middle - 1]) + upper) / 2.0;
}

inline json timestamp_statistics(const std::vector<uint32_t>& timestamps,
                                 uint32_t counter_modulus,
                                 double clock_frequency_hz,
                                 double gap_threshold_factor,
                                 bool include_missing_positions = false) {
    std::vector<uint32_t> deltas;
    deltas.reserve(timestamps.size() > 0 ? timestamps.size() - 1 : 0);
    uint64_t span_ticks = 0;
    for (size_t index = 1; index < timestamps.size(); ++index) {
        const uint32_t previous = timestamps[index - 1];
        const uint32_t current = timestamps[index];
        const uint32_t delta = current >= previous
                                   ? current - previous
                                   : counter_modulus - previous + current;
        if (delta > 0) {
            deltas.push_back(delta);
            span_ticks += delta;
        }
    }

    const double typical_delta = median(deltas);
    uint64_t inferred_missing = 0;
    uint64_t large_gaps = 0;
    uint32_t maximum_delta = 0;
    uint64_t elapsed_ticks = 0;
    json missing_positions = json::array();
    for (uint32_t delta : deltas) {
        maximum_delta = std::max(maximum_delta, delta);
        if (typical_delta > 0.0 && delta >= gap_threshold_factor * typical_delta) {
            ++large_gaps;
            const auto intervals = static_cast<uint64_t>(std::llround(delta / typical_delta));
            if (intervals > 1) {
                inferred_missing += intervals - 1;
                if (include_missing_positions && span_ticks > 0) {
                    for (uint64_t missing_index = 1; missing_index < intervals;
                         ++missing_index) {
                        const double position_ticks =
                            static_cast<double>(elapsed_ticks) +
                            typical_delta * static_cast<double>(missing_index);
                        missing_positions.push_back(
                            std::min(1.0, position_ticks / static_cast<double>(span_ticks)));
                    }
                }
            }
        }
        elapsed_ticks += delta;
    }

    const uint64_t inferred_sent = timestamps.size() + inferred_missing;
    const double span_seconds = clock_frequency_hz > 0.0
                                    ? static_cast<double>(span_ticks) / clock_frequency_hz
                                    : 0.0;
    json result = {{"observed", timestamps.size()},
            {"delta_count", deltas.size()},
            {"typical_delta_ticks", typical_delta},
            {"maximum_delta_ticks", maximum_delta},
            {"timestamp_span_ticks", span_ticks},
            {"timestamp_span_seconds", span_seconds},
            {"typical_rate_hz",
             typical_delta > 0.0 ? clock_frequency_hz / typical_delta : 0.0},
            {"span_rate_hz",
             span_seconds > 0.0 ? static_cast<double>(deltas.size()) / span_seconds : 0.0},
            {"large_gap_count", large_gaps},
            {"inferred_missing", inferred_missing},
            {"inferred_sent", inferred_sent},
            {"gap_capture_efficiency",
             inferred_sent > 0 ? static_cast<double>(timestamps.size()) / inferred_sent : 0.0}};
    if (include_missing_positions) {
        result["inferred_missing_normalized_positions"] = std::move(missing_positions);
    }
    return result;
}

inline json analyze_raw_events(const std::string& raw_path,
                               size_t udp_header_bytes,
                               uint64_t measurement_start_ns,
                               uint64_t measurement_end_ns,
                               int windows,
                               const std::vector<int>& channels,
                               const json& config) {
    std::ifstream input(raw_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open raw capture for event analysis: " + raw_path);
    }
    char magic[8]{};
    input.read(magic, sizeof(magic));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(magic)) ||
        std::string(magic, sizeof(magic)) != "NALUUDP1") {
        throw std::runtime_error("Invalid raw capture header: " + raw_path);
    }

    const uint32_t time_threshold = config.value("time_threshold_ticks", 5000U);
    const uint32_t counter_modulus = config.value("timestamp_counter_modulus", 16777216U);
    const double clock_frequency_hz = config.value("timestamp_clock_frequency_hz", 31250000.0);
    const double gap_threshold_factor = config.value("gap_threshold_factor", 1.5);
    const size_t max_lookback = config.value("max_event_lookback", size_t{2});

    nalu_event_collector::PacketParserConfig parser_config;
    parser_config.check_packet_integrity = true;
    nalu_event_collector::PacketParser parser(parser_config);

    nalu_event_collector::EventBuilderConfig builder_config;
    builder_config.channels = channels;
    builder_config.windows = windows;
    // Strict completion keeps recent events open for other channel groups while
    // decoding faster than real time. Group membership remains the collector's
    // normal channel-group/time-threshold algorithm.
    builder_config.trigger_type = "ext";
    builder_config.wlc_mode = false;
    builder_config.time_threshold = time_threshold;
    builder_config.max_trigger_time = counter_modulus;
    builder_config.max_lookback = max_lookback;
    builder_config.max_events_in_buffer = std::max<size_t>(1024, max_lookback + 64);
    nalu_event_collector::EventBuilder builder(builder_config);

    EventAnalysisAccumulator accumulator;
    uint64_t datagrams = 0;
    uint64_t parsed_packets = 0;
    while (input.peek() != EOF) {
        const uint64_t elapsed_ns = read_u64_le(input);
        const uint32_t datagram_size = read_u32_le(input);
        std::vector<uint8_t> datagram(datagram_size);
        input.read(reinterpret_cast<char*>(datagram.data()), datagram_size);
        if (input.gcount() != static_cast<std::streamsize>(datagram_size)) {
            throw std::runtime_error("Truncated NALUUDP1 datagram: " + raw_path);
        }
        if (elapsed_ns < measurement_start_ns) {
            continue;
        }
        if (elapsed_ns > measurement_end_ns) {
            break;
        }
        ++datagrams;
        const size_t payload_start = std::min(udp_header_bytes, datagram.size());
        std::vector<uint8_t> payload(datagram.begin() + payload_start, datagram.end());
        auto packets = parser.process_stream(payload);
        parsed_packets += packets.size();
        builder.collect_events(packets);
        drain_events(builder, max_lookback, accumulator);
    }
    drain_events(builder, 0, accumulator);

    json channel_statistics = json::object();
    for (int channel : channels) {
        const auto found = accumulator.channel_timestamps.find(channel);
        const std::vector<uint32_t> empty;
        const auto& timestamps =
            found == accumulator.channel_timestamps.end() ? empty : found->second;
        channel_statistics[std::to_string(channel)] = timestamp_statistics(
            timestamps, counter_modulus, clock_frequency_hz, gap_threshold_factor);
    }

    return {{"algorithm", "nalu_event_collector_channel_groups_v1"},
            {"measurement_datagrams", datagrams},
            {"parsed_packets", parsed_packets},
            {"packets_in_complete_channel_groups", accumulator.packets_in_events},
            {"events_total", accumulator.events},
            {"multi_channel_events", accumulator.multi_channel_events},
            {"time_threshold_ticks", time_threshold},
            {"timestamp_counter_modulus", counter_modulus},
            {"timestamp_clock_frequency_hz", clock_frequency_hz},
            {"gap_threshold_factor", gap_threshold_factor},
            {"event_timestamps",
             timestamp_statistics(accumulator.event_timestamps,
                                  counter_modulus,
                                  clock_frequency_hz,
                                  gap_threshold_factor,
                                  true)},
            {"channels", channel_statistics}};
}

}  // namespace firmware_959_rate_scan
