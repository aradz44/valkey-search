/*
 * Copyright (c) 2025, valkey-search contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD 3-Clause
 *
 */
#include "valkey_search_options.h"

#include "valkey_search.h"
#include "vmsdk/src/concurrency.h"
#include "vmsdk/src/module_config.h"
#include "vmsdk/src/thread_pool.h"

namespace valkey_search {
namespace options {

constexpr uint32_t kHNSWDefaultBlockSize{10240};
constexpr uint32_t kHNSWMinimumBlockSize{0};
constexpr uint32_t kMaxThreadsCount{1024};
constexpr uint32_t kMaxIndexesDefault{10};
constexpr uint32_t kMaxPrefixesDefault{16};
constexpr uint32_t kMaxTagFieldLenDefault{10000};
constexpr uint32_t kMaxNumericFieldLenDefault{256};

constexpr absl::string_view kHNSWBlockSizeConfig{"hnsw-block-size"};
constexpr absl::string_view kReaderThreadsConfig{"reader-threads"};
constexpr absl::string_view kWriterThreadsConfig{"writer-threads"};
constexpr absl::string_view kUseCoordinator{"use-coordinator"};
constexpr absl::string_view kLogLevel{"log-level"};
constexpr absl::string_view kMaxIndexesConfig{"max-indexes"};
constexpr absl::string_view kMaxPrefixesConfig{"max-prefixes"};
constexpr absl::string_view kMaxTagFieldLenConfig{"max-tag-field-length"};
constexpr absl::string_view kMaxNumericFieldLenConfig{
    "max-numeric-field-length"};

static const int64_t kDefaultThreadsCount = vmsdk::GetPhysicalCPUCoresCount();

namespace {

/// Check that the new value for configuration item `hnsw-block-size` confirms
/// to the allowed values.
absl::Status ValidateHNSWBlockSize(long long new_value) {
  if (new_value < kHNSWMinimumBlockSize || new_value > UINT32_MAX) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Block size must be between %u and %u",
                        kHNSWMinimumBlockSize, UINT32_MAX));
  }
  return absl::OkStatus();
}

/// Resize `pool` to match its new value
void UpdateThreadPoolCount(vmsdk::ThreadPool* pool, long long new_value) {
  if (!pool) {
    return;
  }
  pool->Resize(new_value);
}

absl::Status ValidateLogLevel(const int value) {
  if (value >= static_cast<int>(LogLevel::kWarning) &&
      value <= static_cast<int>(LogLevel::kDebug)) {
    return absl::OkStatus();
  }
  return absl::OutOfRangeError(
      absl::StrFormat("Log level of: %d is out of range", value));
}

}  // namespace

// Configuration entries
namespace config = vmsdk::config;

// Register an enumerator for the log level
static const std::vector<std::string_view> kLogLevelNames = {
    VALKEYMODULE_LOGLEVEL_WARNING,
    VALKEYMODULE_LOGLEVEL_NOTICE,
    VALKEYMODULE_LOGLEVEL_VERBOSE,
    VALKEYMODULE_LOGLEVEL_DEBUG,
};

static const std::vector<int> kLogLevelValues = {
    static_cast<int>(LogLevel::kWarning), static_cast<int>(LogLevel::kNotice),
    static_cast<int>(LogLevel::kVerbose), static_cast<int>(LogLevel::kDebug)};

static auto hnsw_block_size =
    config::NumberBuilder(kHNSWBlockSizeConfig,   // name
                          kHNSWDefaultBlockSize,  // default size
                          kHNSWMinimumBlockSize,  // min size
                          UINT_MAX)               // max size
        .WithValidationCallback(ValidateHNSWBlockSize)
        .Build();

/// Register the "--reader-threads" flag. Controls the readers thread pool
static auto reader_threads_count =
    config::NumberBuilder(kReaderThreadsConfig,  // name
                          kDefaultThreadsCount,  // default size
                          1,                     // min size
                          kMaxThreadsCount)      // max size
        .WithModifyCallback(                     // set an "On-Modify" callback
            [](auto new_value) {
              UpdateThreadPoolCount(
                  ValkeySearch::Instance().GetReaderThreadPool(), new_value);
            })
        .Build();

/// Register the "--reader-threads" flag. Controls the writer thread pool
static auto writer_threads_count =
    config::NumberBuilder(kWriterThreadsConfig,  // name
                          kDefaultThreadsCount,  // default size
                          1,                     // min size
                          kMaxThreadsCount)      // max size
        .WithModifyCallback(                     // set an "On-Modify" callback
            [](auto new_value) {
              UpdateThreadPoolCount(
                  ValkeySearch::Instance().GetWriterThreadPool(), new_value);
            })
        .Build();

/// Should this instance use coordinator?
static auto use_coordinator =
    config::BooleanBuilder(kUseCoordinator, false)
        .WithFlags(VALKEYMODULE_CONFIG_HIDDEN)  // can only be set during
                                                // start-up
        .Build();

/// Control the modules log level verbosity
static auto log_level =
    config::EnumBuilder(kLogLevel, static_cast<int>(LogLevel::kNotice),
                        kLogLevelNames, kLogLevelValues)
        .WithModifyCallback([](int value) {
          auto res = ValidateLogLevel(value);
          if (!res.ok()) {
            VMSDK_LOG(WARNING, nullptr)
                << "Invalid value: '" << value << "' provided to enum: '"
                << kLogLevel << "'. " << res.message();
            return;
          }
          // "value" is already validated using "ValidateLogLevel" callback
          // below
          auto log_level_str = kLogLevelNames[value];
          res = vmsdk::InitLogging(nullptr, log_level_str.data());
          if (!res.ok()) {
            VMSDK_LOG(WARNING, nullptr)
                << "Failed to initialize log with new value: " << log_level_str
                << ". " << res.message();
          }
        })
        .WithValidationCallback(ValidateLogLevel)
        .Build();

/// Register the "--max-indexes" flag. Controls the max number of indexes we can
/// have.
static auto max_indexes =
    config::NumberBuilder(kMaxIndexesConfig,   // name
                          kMaxIndexesDefault,  // default size
                          1,                   // min size
                          UINT_MAX)            // max size
        .Build();

/// Register the "--max-prefixes" flag. Controls the max number of prefixes per
/// index.
static auto max_prefixes =
    config::NumberBuilder(kMaxPrefixesConfig,   // name
                          kMaxPrefixesDefault,  // default size
                          1,                    // min size
                          UINT_MAX)             // max size
        .Build();

/// Register the "--max-tag-field-length" flag. Controls the max length of a tag
/// field.
static auto max_tag_field_len =
    config::NumberBuilder(kMaxTagFieldLenConfig,   // name
                          kMaxTagFieldLenDefault,  // default size
                          1,                       // min size
                          UINT_MAX)                // max size
        .Build();

/// Register the "--max-numeric-field-length" flag. Controls the max length of a
/// numeric field.
static auto max_numeric_field_len =
    config::NumberBuilder(kMaxNumericFieldLenConfig,   // name
                          kMaxNumericFieldLenDefault,  // default size
                          1,                           // min size
                          UINT_MAX)                    // max size
        .Build();

vmsdk::config::Number& GetHNSWBlockSize() {
  return dynamic_cast<vmsdk::config::Number&>(*hnsw_block_size);
}

vmsdk::config::Number& GetReaderThreadCount() {
  return dynamic_cast<vmsdk::config::Number&>(*reader_threads_count);
}

vmsdk::config::Number& GetWriterThreadCount() {
  return dynamic_cast<vmsdk::config::Number&>(*writer_threads_count);
}

vmsdk::config::Number& GetMaxIndexes() {
  return dynamic_cast<vmsdk::config::Number&>(*max_indexes);
}

vmsdk::config::Number& GetMaxPrefixes() {
  return dynamic_cast<vmsdk::config::Number&>(*max_prefixes);
}

vmsdk::config::Number& GetMaxTagFieldLen() {
  return dynamic_cast<vmsdk::config::Number&>(*max_tag_field_len);
}

vmsdk::config::Number& GetMaxNumericFieldLen() {
  return dynamic_cast<vmsdk::config::Number&>(*max_numeric_field_len);
}

const vmsdk::config::Boolean& GetUseCoordinator() {
  return dynamic_cast<const vmsdk::config::Boolean&>(*use_coordinator);
}

vmsdk::config::Enum& GetLogLevel() {
  return dynamic_cast<vmsdk::config::Enum&>(*log_level);
}

absl::Status Reset() {
  VMSDK_RETURN_IF_ERROR(use_coordinator->SetValue(false));
  return absl::OkStatus();
}

}  // namespace options
}  // namespace valkey_search
