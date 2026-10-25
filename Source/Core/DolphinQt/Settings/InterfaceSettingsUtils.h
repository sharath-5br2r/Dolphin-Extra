// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>
#include <vector>

#include <QString>

namespace InterfaceSettings
{
using StringChoice = std::pair<QString, QString>;

std::vector<StringChoice> GetLanguageChoices();
}  // namespace InterfaceSettings
