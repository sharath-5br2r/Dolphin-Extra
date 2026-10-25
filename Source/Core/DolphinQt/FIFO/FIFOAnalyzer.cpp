// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/FIFO/FIFOAnalyzer.h"

#include <algorithm>
#include <bit>
#include <ranges>
#include <utility>

#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "Common/Assert.h"
#include "Common/Swap.h"
#include "Core/FifoPlayer/FifoPlayer.h"

#include "DolphinQt/Settings.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/XFStructs.h"

#include "ui_FIFOAnalyzer.h"

// Values range from 0 to number of frames - 1
constexpr int FRAME_ROLE = Qt::UserRole;
// Values range from 0 to number of parts - 1
constexpr int PART_START_ROLE = Qt::UserRole + 1;
// Values range from 1 to number of parts
constexpr int PART_END_ROLE = Qt::UserRole + 2;

FIFOAnalyzer::FIFOAnalyzer(FifoPlayer& fifo_player)
    : m_fifo_player(fifo_player), m_ui(std::make_unique<Ui::FIFOAnalyzer>())
{
  m_ui->setupUi(this);
  m_ui->treeWidget->header()->hide();
  m_ui->searchBox->setMaximumHeight(m_ui->searchBox->minimumSizeHint().height());

  ConnectWidgets();

  UpdateTree();

  const auto& settings = Settings::GetQSettings();

  m_ui->objectSplitter->restoreState(
      settings.value(QStringLiteral("fifoanalyzer/objectsplitter")).toByteArray());
  m_ui->searchSplitter->restoreState(
      settings.value(QStringLiteral("fifoanalyzer/searchsplitter")).toByteArray());

  OnDebugFontChanged(Settings::Instance().GetDebugFont());
  connect(&Settings::Instance(), &Settings::DebugFontChanged, this,
          &FIFOAnalyzer::OnDebugFontChanged);
}

FIFOAnalyzer::~FIFOAnalyzer()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("fifoanalyzer/objectsplitter"),
                    m_ui->objectSplitter->saveState());
  settings.setValue(QStringLiteral("fifoanalyzer/searchsplitter"),
                    m_ui->searchSplitter->saveState());
}

void FIFOAnalyzer::ConnectWidgets()
{
  connect(m_ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &FIFOAnalyzer::UpdateDetails);
  connect(m_ui->detailList, &QListWidget::currentRowChanged, this,
          &FIFOAnalyzer::UpdateDescription);

  connect(m_ui->searchEdit, &QLineEdit::returnPressed, this, &FIFOAnalyzer::BeginSearch);
  connect(m_ui->searchButton, &QPushButton::clicked, this, &FIFOAnalyzer::BeginSearch);
  connect(m_ui->nextMatchButton, &QPushButton::clicked, this, &FIFOAnalyzer::FindNext);
  connect(m_ui->previousMatchButton, &QPushButton::clicked, this, &FIFOAnalyzer::FindPrevious);
}

void FIFOAnalyzer::Update()
{
  UpdateTree();
  UpdateDetails();
  UpdateDescription();
}

void FIFOAnalyzer::UpdateTree()
{
  m_ui->treeWidget->clear();

  if (!m_fifo_player.IsPlaying())
  {
    m_ui->treeWidget->addTopLevelItem(new QTreeWidgetItem({tr("No recording loaded.")}));
    return;
  }

  auto* recording_item = new QTreeWidgetItem({tr("Recording")});

  m_ui->treeWidget->addTopLevelItem(recording_item);

  const auto* const file = m_fifo_player.GetFile();

  const u32 frame_count = file->GetFrameCount();

  for (u32 frame = 0; frame < frame_count; frame++)
  {
    auto* frame_item = new QTreeWidgetItem({tr("Frame %1").arg(frame)});

    recording_item->addChild(frame_item);

    const auto& [parts, part_type_counts] = m_fifo_player.GetAnalyzedFrameInfo(frame);
    ASSERT(parts.size() != 0);

    Common::EnumMap<u32, FramePartType::EFBCopy> part_counts;
    u32 part_start = 0;

    for (u32 part_nr = 0; part_nr < parts.size(); part_nr++)
    {
      const auto& part = parts[part_nr];

      const u32 part_type_nr = part_counts[part.m_type];
      part_counts[part.m_type]++;

      QTreeWidgetItem* object_item = nullptr;
      if (part.m_type == FramePartType::PrimitiveData)
        object_item = new QTreeWidgetItem({tr("Object %1").arg(part_type_nr)});
      else if (part.m_type == FramePartType::EFBCopy)
        object_item = new QTreeWidgetItem({tr("EFB copy %1").arg(part_type_nr)});
      // We don't create dedicated labels for FramePartType::Command;
      // those are grouped with the primitive

      if (object_item != nullptr)
      {
        frame_item->addChild(object_item);

        object_item->setData(0, FRAME_ROLE, frame);
        object_item->setData(0, PART_START_ROLE, part_start);
        object_item->setData(0, PART_END_ROLE, part_nr);

        part_start = part_nr + 1;
      }
    }

    // We shouldn't end on a Command (it should end with an EFB copy)
    ASSERT(part_start == parts.size());
    // The counts we computed should match the frame's counts
    ASSERT(std::ranges::equal(part_type_counts, part_counts));
  }
}

namespace
{
class DetailCallback : public OpcodeDecoder::Callback
{
public:
  explicit DetailCallback(const CPState& cpmem) : m_cpmem(cpmem) {}

  OPCODE_CALLBACK(void OnCP(const u8 command, const u32 value))
  {
    // Note: No need to update m_cpmem as it already has the final value for this object

    const auto [name, desc] = GetCPRegInfo(command, value);
    ASSERT(!name.empty());

    text = QStringLiteral("CP  %1  %2  %3")
               .arg(command, 2, 16, QLatin1Char('0'))
               .arg(value, 8, 16, QLatin1Char('0'))
               .arg(QString::fromStdString(name));
  }

  OPCODE_CALLBACK(void OnXF(const u16 address, const u8 count, const u8* data))
  {
    const auto [name, desc] = GetXFTransferInfo(address, count, data);
    ASSERT(!name.empty());

    const u32 command = address | ((count - 1) << 16);

    text = QStringLiteral("XF  %1  ").arg(command, 8, 16, QLatin1Char('0'));

    for (u8 i = 0; i < count; i++)
    {
      const u32 value = Common::swap32(&data[i * 4]);

      text += QStringLiteral("%1 ").arg(value, 8, 16, QLatin1Char('0'));
    }

    text += QStringLiteral("  ") + QString::fromStdString(name);
  }

  OPCODE_CALLBACK(void OnBP(const u8 command, const u32 value))
  {
    const auto [name, desc] = GetBPRegInfo(command, value);
    ASSERT(!name.empty());

    text = QStringLiteral("BP  %1  %2  %3")
               .arg(command, 2, 16, QLatin1Char('0'))
               .arg(value, 6, 16, QLatin1Char('0'))
               .arg(QString::fromStdString(name));
  }
  OPCODE_CALLBACK(void OnIndexedLoad(const CPArray array, const u32 index, const u16 address,
                                     const u8 size))
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(array, index, address, size);
    text = QStringLiteral("LOAD INDX %1   %2")
               .arg(QString::fromStdString(fmt::to_string(array)))
               .arg(QString::fromStdString(desc));
  }
  OPCODE_CALLBACK(void OnPrimitiveCommand(const OpcodeDecoder::Primitive primitive, const u8 vat,
                                          const u32 vertex_size, const u16 num_vertices,
                                          const u8* vertex_data))
  {
    const auto name = fmt::to_string(primitive);

    // Note that vertex_count is allowed to be 0, with no special treatment
    // (another command just comes right after the current command, with no vertices in between)
    const u32 object_prim_size = num_vertices * vertex_size;

    const u8 opcode =
        0x80 | std::to_underlying(primitive) << OpcodeDecoder::GX_PRIMITIVE_SHIFT | vat;
    text = QStringLiteral("PRIMITIVE %1 (%2)  %3 vertices %4 bytes/vertex %5 total bytes")
               .arg(QString::fromStdString(name))
               .arg(opcode, 2, 16, QLatin1Char('0'))
               .arg(num_vertices)
               .arg(vertex_size)
               .arg(object_prim_size);

    // It's not really useful to have a massive unreadable hex string for the object primitives.
    // Put it in the description instead.

// #define INCLUDE_HEX_IN_PRIMITIVES
#ifdef INCLUDE_HEX_IN_PRIMITIVES
    text += QStringLiteral("   ");
    for (u32 i = 0; i < object_prim_size; i++)
    {
      text += QStringLiteral("%1").arg(vertex_data[i], 2, 16, QLatin1Char('0'));
    }
#endif
  }

  OPCODE_CALLBACK(void OnDisplayList(const u32 address, const u32 size))
  {
    text = QObject::tr("Call display list at %1 with size %2")
               .arg(address, 8, 16, QLatin1Char('0'))
               .arg(size, 8, 16, QLatin1Char('0'));
  }

  OPCODE_CALLBACK(void OnNop(const u32 count))
  {
    if (count > 1)
      text = QStringLiteral("NOP (%1x)").arg(count);
    else
      text = QStringLiteral("NOP");
  }

  OPCODE_CALLBACK(void OnUnknown(u8 opcode, const u8* data))
  {
    using OpcodeDecoder::Opcode;
    if (static_cast<Opcode>(opcode) == Opcode::GX_CMD_UNKNOWN_METRICS)
      text = QStringLiteral("GX_CMD_UNKNOWN_METRICS");
    else if (static_cast<Opcode>(opcode) == Opcode::GX_CMD_INVL_VC)
      text = QStringLiteral("GX_CMD_INVL_VC");
    else
      text = QStringLiteral("Unknown opcode %1").arg(opcode, 2, 16);
  }

  OPCODE_CALLBACK(void OnCommand(const u8* data, u32 size)) {}

  OPCODE_CALLBACK(CPState& GetCPState()) { return m_cpmem; }

  QString text;
  CPState m_cpmem;
};
}  // namespace

void FIFOAnalyzer::UpdateDetails()
{
  // Clearing the detail list can update the selection, which causes UpdateDescription to be called
  // immediately.  However, the object data offsets have not been recalculated yet, which can cause
  // the wrong data to be used, potentially leading to out of bounds data or other bad things.
  // Clear m_object_data_offsets first, so that UpdateDescription exits immediately.
  m_object_data_offsets.clear();
  m_ui->detailList->clear();
  m_search_results.clear();
  m_ui->nextMatchButton->setEnabled(false);
  m_ui->previousMatchButton->setEnabled(false);
  m_ui->searchLabel->clear();

  if (!m_fifo_player.IsPlaying())
    return;

  const auto items = m_ui->treeWidget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, PART_START_ROLE).isNull())
    return;

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 start_part_nr = items[0]->data(0, PART_START_ROLE).toUInt();
  const u32 end_part_nr = items[0]->data(0, PART_END_ROLE).toUInt();

  const auto& [parts, _part_type_counts] = m_fifo_player.GetAnalyzedFrameInfo(frame_nr);
  const auto& fifo_frame = m_fifo_player.GetFile()->GetFrame(frame_nr);

  const u32 object_start = parts[start_part_nr].m_start;
  const u32 object_end = parts[end_part_nr].m_end;
  const u32 object_size = object_end - object_start;

  u32 object_offset = 0;
  // NOTE: object_info.m_cpmem is the state of cpmem _after_ all of the commands in this object.
  // However, it doesn't matter that it doesn't match the start, since it will match by the time
  // primitives are reached.
  auto callback = DetailCallback(parts[end_part_nr].m_cpmem);

  while (object_offset < object_size)
  {
    const u32 start_offset = object_offset;
    m_object_data_offsets.push_back(start_offset);

    object_offset += OpcodeDecoder::RunCommand(&fifo_frame.fifoData[object_start + start_offset],
                                               object_size - start_offset, callback);

    QString new_label =
        QStringLiteral("%1:  ").arg(object_start + start_offset, 8, 16, QLatin1Char('0')) +
        callback.text;
    m_ui->detailList->addItem(new_label);
  }

  // Needed to ensure the description updates when changing objects
  m_ui->detailList->setCurrentRow(0);
}

void FIFOAnalyzer::BeginSearch()
{
  const QString search_str = m_ui->searchEdit->text();

  if (!m_fifo_player.IsPlaying())
    return;

  const auto items = m_ui->treeWidget->selectedItems();

  if (items.isEmpty() || items[0]->data(0, FRAME_ROLE).isNull() ||
      items[0]->data(0, PART_START_ROLE).isNull())
  {
    m_ui->searchLabel->setText(tr("Invalid search parameters (no object selected)"));
    return;
  }

  // Having PART_START_ROLE indicates that this is valid
  const int object_idx = items[0]->parent()->indexOfChild(items[0]);

  // TODO: Remove even string length limit
  if (search_str.length() % 2)
  {
    m_ui->searchLabel->setText(tr("Invalid search string (only even string lengths supported)"));
    return;
  }

  const size_t length = search_str.length() / 2;

  std::vector<u8> search_val;

  for (size_t i = 0; i < length; i++)
  {
    const QString byte_str = search_str.mid(static_cast<int>(i * 2), 2);

    bool good;
    u8 value = byte_str.toUInt(&good, 16);

    if (!good)
    {
      m_ui->searchLabel->setText(tr("Invalid search string (couldn't convert to number)"));
      return;
    }

    search_val.push_back(value);
  }

  m_search_results.clear();

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 start_part_nr = items[0]->data(0, PART_START_ROLE).toUInt();
  const u32 end_part_nr = items[0]->data(0, PART_END_ROLE).toUInt();

  const auto& [parts, _part_type_counts] = m_fifo_player.GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = m_fifo_player.GetFile()->GetFrame(frame_nr);

  const u32 object_start = parts[start_part_nr].m_start;
  const u32 object_end = parts[end_part_nr].m_end;
  const u32 object_size = object_end - object_start;

  const u8* const object = &fifo_frame.fifoData[object_start];

  // TODO: Support searching for bit patterns
  for (u32 cmd_nr = 0; cmd_nr < m_object_data_offsets.size(); cmd_nr++)
  {
    const u32 cmd_start = m_object_data_offsets[cmd_nr];
    const u32 cmd_end = (cmd_nr + 1 == m_object_data_offsets.size()) ?
                            object_size :
                            m_object_data_offsets[cmd_nr + 1];

    const u8* const cmd_start_ptr = &object[cmd_start];
    const u8* const cmd_end_ptr = &object[cmd_end];

    for (const u8* ptr = cmd_start_ptr; ptr < cmd_end_ptr - length + 1; ptr++)
    {
      if (std::equal(search_val.begin(), search_val.end(), ptr))
      {
        m_search_results.emplace_back(frame_nr, object_idx, cmd_nr);
        break;
      }
    }
  }

  ShowSearchResult(0);

  m_ui->searchLabel->setText(
      tr("Found %1 results for \"%2\"").arg(m_search_results.size()).arg(search_str));
}

void FIFOAnalyzer::FindNext()
{
  const int index = m_ui->detailList->currentRow();
  ASSERT(index >= 0);

  const auto next_result = std::ranges::find_if(
      m_search_results, [index](auto& result) { return result.m_cmd > static_cast<u32>(index); });
  if (next_result != m_search_results.end())
  {
    ShowSearchResult(next_result - m_search_results.begin());
  }
}

void FIFOAnalyzer::FindPrevious()
{
  const int index = m_ui->detailList->currentRow();
  ASSERT(index >= 0);

  const auto prev_result =
      std::ranges::find_if(m_search_results | std::views::reverse, [index](auto& result) {
        return result.m_cmd < static_cast<u32>(index);
      });
  if (prev_result != m_search_results.rend())
  {
    ShowSearchResult((m_search_results.rend() - prev_result) - 1);
  }
}

void FIFOAnalyzer::ShowSearchResult(const size_t index)
{
  if (m_search_results.empty())
    return;

  if (index >= m_search_results.size())
  {
    ShowSearchResult(m_search_results.size() - 1);
    return;
  }

  const auto& result = m_search_results[index];

  QTreeWidgetItem* object_item =
      m_ui->treeWidget->topLevelItem(0)->child(result.m_frame)->child(result.m_object_idx);

  m_ui->treeWidget->setCurrentItem(object_item);
  m_ui->detailList->setCurrentRow(result.m_cmd);

  m_ui->nextMatchButton->setEnabled(index + 1 < m_search_results.size());
  m_ui->previousMatchButton->setEnabled(index > 0);
}

namespace
{
// TODO: Not sure whether we should bother translating the descriptions
class DescriptionCallback : public OpcodeDecoder::Callback
{
public:
  explicit DescriptionCallback(const CPState& cpmem) : m_cpmem(cpmem) {}

  OPCODE_CALLBACK(void OnBP(const u8 command, const u32 value))
  {
    const auto [name, desc] = GetBPRegInfo(command, value);
    ASSERT(!name.empty());

    text = QObject::tr("BP register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += QObject::tr("No description available");
    else
      text += QString::fromStdString(desc);
  }

  OPCODE_CALLBACK(void OnCP(const u8 command, const u32 value))
  {
    // Note: No need to update m_cpmem as it already has the final value for this object

    const auto [name, desc] = GetCPRegInfo(command, value);
    ASSERT(!name.empty());

    text = QObject::tr("CP register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += QObject::tr("No description available");
    else
      text += QString::fromStdString(desc);
  }

  OPCODE_CALLBACK(void OnXF(const u16 address, const u8 count, const u8* data))
  {
    const auto [name, desc] = GetXFTransferInfo(address, count, data);
    ASSERT(!name.empty());

    text = QObject::tr("XF register ");
    text += QString::fromStdString(name);
    text += QLatin1Char{'\n'};

    if (desc.empty())
      text += QObject::tr("No description available");
    else
      text += QString::fromStdString(desc);
  }

  OPCODE_CALLBACK(void OnIndexedLoad(const CPArray array, const u32 index, const u16 address,
                                     const u8 size))
  {
    const auto [desc, written] = GetXFIndexedLoadInfo(array, index, address, size);

    text = QString::fromStdString(desc);
    text += QLatin1Char{'\n'};
    switch (array)
    {
    case CPArray::XF_A:
      text += QObject::tr("Usually used for position matrices");
      break;
    case CPArray::XF_B:
      // i18n: A normal matrix is a matrix used for transforming normal vectors. The word "normal"
      // does not have its usual meaning here, but rather the meaning of "perpendicular to a
      // surface".
      text += QObject::tr("Usually used for normal matrices");
      break;
    case CPArray::XF_C:
      // i18n: Tex coord is short for texture coordinate
      text += QObject::tr("Usually used for tex coord matrices");
      break;
    case CPArray::XF_D:
      text += QObject::tr("Usually used for light objects");
      break;
    default:
      break;
    }
    text += QLatin1Char{'\n'};
    text += QString::fromStdString(written);
  }

  OPCODE_CALLBACK(void OnPrimitiveCommand(OpcodeDecoder::Primitive primitive, u8 vat,
                                          const u32 vertex_size, const u16 num_vertices,
                                          const u8* vertex_data))
  {
    const auto name = fmt::format("{} VAT {}", primitive, vat);

    // i18n: In this context, a primitive means a point, line, triangle or rectangle.
    // Do not translate the word primitive as if it was an adjective.
    text = QObject::tr("Primitive %1").arg(QString::fromStdString(name));
    text += QLatin1Char{'\n'};

    const auto& [low, high] = m_cpmem.vtx_desc;
    const auto& vtx_attr = m_cpmem.vtx_attr[vat];

    u32 i = 0;
    const auto process_component = [&](const VertexComponentFormat cformat, ComponentFormat format,
                                       const u32 non_indexed_count, const u32 indexed_count = 1) {
      u32 count;
      if (cformat == VertexComponentFormat::NotPresent)
        return;
      else if (cformat == VertexComponentFormat::Index8)
      {
        format = ComponentFormat::UByte;
        count = indexed_count;
      }
      else if (cformat == VertexComponentFormat::Index16)
      {
        format = ComponentFormat::UShort;
        count = indexed_count;
      }
      else
      {
        count = non_indexed_count;
      }

      const u32 component_size = GetElementSize(format);
      for (u32 j = 0; j < count; j++)
      {
        for (u32 component_off = 0; component_off < component_size; component_off++)
        {
          text += QStringLiteral("%1").arg(vertex_data[i + component_off], 2, 16, QLatin1Char('0'));
        }
        if (format == ComponentFormat::Float)
        {
          const float value = std::bit_cast<float>(Common::swap32(&vertex_data[i]));
          text += QStringLiteral(" (%1)").arg(value);
        }
        i += component_size;
        text += QLatin1Char{' '};
      }
      text += QLatin1Char{' '};
    };
    const auto process_simple_component = [&](const u32 size) {
      for (u32 component_off = 0; component_off < size; component_off++)
      {
        text += QStringLiteral("%1").arg(vertex_data[i + component_off], 2, 16, QLatin1Char('0'));
      }
      i += size;
      text += QLatin1Char{' '};
      text += QLatin1Char{' '};
    };

    for (u32 vertex_num = 0; vertex_num < num_vertices; vertex_num++)
    {
      ASSERT(i == vertex_num * vertex_size);

      text += QLatin1Char{'\n'};
      if (low.PosMatIdx)
        process_simple_component(1);
      for (auto texmtxidx : low.TexMatIdx)
      {
        if (texmtxidx)
          process_simple_component(1);
      }
      process_component(low.Position, vtx_attr.g0.PosFormat,
                        vtx_attr.g0.PosElements == CoordComponentCount::XY ? 2 : 3);
      const u32 normal_component_count = low.Normal == VertexComponentFormat::Direct ? 3 : 1;
      const u32 normal_elements = vtx_attr.g0.NormalElements == NormalComponentCount::NTB ? 3 : 1;
      process_component(low.Normal, vtx_attr.g0.NormalFormat,
                        normal_component_count * normal_elements,
                        vtx_attr.g0.NormalIndex3 ? normal_elements : 1);
      for (u32 c = 0; c < low.Color.Size(); c++)
      {
        static constexpr Common::EnumMap<u32, ColorFormat::RGBA8888> component_sizes = {
            2,  // RGB565
            3,  // RGB888
            4,  // RGB888x
            2,  // RGBA4444
            3,  // RGBA6666
            4,  // RGBA8888
        };
        switch (low.Color[c])
        {
        case VertexComponentFormat::Index8:
          process_simple_component(1);
          break;
        case VertexComponentFormat::Index16:
          process_simple_component(2);
          break;
        case VertexComponentFormat::Direct:
          process_simple_component(component_sizes[vtx_attr.GetColorFormat(c)]);
          break;
        case VertexComponentFormat::NotPresent:
          break;
        }
      }
      for (u32 t = 0; t < high.TexCoord.Size(); t++)
      {
        process_component(high.TexCoord[t], vtx_attr.GetTexFormat(t),
                          vtx_attr.GetTexElements(t) == TexComponentCount::ST ? 2 : 1);
      }
    }
  }

  OPCODE_CALLBACK(void OnDisplayList(u32 address, u32 size))
  {
    text = QObject::tr("No description available");
  }

  OPCODE_CALLBACK(void OnNop(u32 count)) { text = QObject::tr("No description available"); }
  OPCODE_CALLBACK(void OnUnknown(u8 opcode, const u8* data))
  {
    text = QObject::tr("No description available");
  }

  OPCODE_CALLBACK(void OnCommand(const u8* data, u32 size)) {}

  OPCODE_CALLBACK(CPState& GetCPState()) { return m_cpmem; }

  QString text;
  CPState m_cpmem;
};
}  // namespace

void FIFOAnalyzer::UpdateDescription()
{
  m_ui->entryDetailBrowser->clear();

  if (!m_fifo_player.IsPlaying())
    return;

  const auto items = m_ui->treeWidget->selectedItems();

  if (items.isEmpty() || m_object_data_offsets.empty())
    return;

  if (items[0]->data(0, FRAME_ROLE).isNull() || items[0]->data(0, PART_START_ROLE).isNull())
    return;

  const u32 frame_nr = items[0]->data(0, FRAME_ROLE).toUInt();
  const u32 start_part_nr = items[0]->data(0, PART_START_ROLE).toUInt();
  const u32 end_part_nr = items[0]->data(0, PART_END_ROLE).toUInt();
  const u32 entry_nr = m_ui->detailList->currentRow();

  const auto& [parts, _part_type_counts] = m_fifo_player.GetAnalyzedFrameInfo(frame_nr);
  const FifoFrameInfo& fifo_frame = m_fifo_player.GetFile()->GetFrame(frame_nr);

  const u32 object_start = parts[start_part_nr].m_start;
  const u32 object_end = parts[end_part_nr].m_end;
  const u32 object_size = object_end - object_start;
  const u32 entry_start = m_object_data_offsets[entry_nr];

  auto callback = DescriptionCallback(parts[end_part_nr].m_cpmem);
  OpcodeDecoder::RunCommand(&fifo_frame.fifoData[object_start + entry_start],
                            object_size - entry_start, callback);
  m_ui->entryDetailBrowser->setText(callback.text);
}

void FIFOAnalyzer::OnDebugFontChanged(const QFont& font)
{
  m_ui->detailList->setFont(font);
  m_ui->entryDetailBrowser->setFont(font);
}
