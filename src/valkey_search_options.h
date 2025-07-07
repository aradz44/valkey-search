/*
 * Copyright (c) 2025, valkey-search contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD 3-Clause
 *
 */
#pragma once

#include "vmsdk/src/module_config.h"

namespace valkey_search {
namespace options {

namespace config = vmsdk::config;

/// Return a mutable reference to the HNSW resize configuration parameter
config::Number& GetHNSWBlockSize();

/// Return the configuration entry that allows the caller to control the
/// number of reader threads
config::Number& GetReaderThreadCount();

/// Return the configuration entry that allows the caller to control the
/// number of writer threads
config::Number& GetWriterThreadCount();

/// Return the maximum number of indexes allowed to create.
config::Number& GetMaxIndexes();

/// Return the maximum number of prefixes allowed per index.
config::Number& GetMaxPrefixes();

/// Return the maximum length of a tag field.
config::Number& GetMaxTagFieldLen();

/// Return the maximum length of a numeric field.
config::Number& GetMaxNumericFieldLen();

/// Return an immutable reference to the "use-coordinator" flag
const config::Boolean& GetUseCoordinator();

/// Return the log level
config::Enum& GetLogLevel();

/// Reset the state of the options (mainly needed for testing)
absl::Status Reset();
}  // namespace options
}  // namespace valkey_search
