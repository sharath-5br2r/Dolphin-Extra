// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include <QWidget>

#include "Common/CommonTypes.h"

class FifoPlayer;
class QFont;

namespace Ui
{
class FIFOAnalyzer;
}

class FIFOAnalyzer final : public QWidget
{
  Q_OBJECT

public:
  explicit FIFOAnalyzer(FifoPlayer& fifo_player);
  ~FIFOAnalyzer() override;

  void Update();

private:
  void ConnectWidgets();

  void BeginSearch();
  void FindNext();
  void FindPrevious();

  void ShowSearchResult(size_t index);

  void UpdateTree();
  void UpdateDetails();
  void UpdateDescription();

  void OnDebugFontChanged(const QFont& font);

  FifoPlayer& m_fifo_player;
  std::unique_ptr<Ui::FIFOAnalyzer> m_ui;

  struct SearchResult
  {
    constexpr SearchResult(u32 frame, u32 object_idx, u32 cmd)
        : m_frame(frame), m_object_idx(object_idx), m_cmd(cmd)
    {
    }
    const u32 m_frame;
    // Index in tree view.  Does not correspond with object numbers or part numbers.
    const u32 m_object_idx;
    const u32 m_cmd;
  };

  // Offsets from the start of the first part in an object for each command within the currently
  // selected object.
  std::vector<int> m_object_data_offsets;

  std::vector<SearchResult> m_search_results;
};
