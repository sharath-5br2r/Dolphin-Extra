// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/BigPictureLibrary.h"

#include <algorithm>
#include <cstring>
#include <tuple>
#include <utility>

#include <QBuffer>
#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QThread>

#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "UICommon/UICommon.h"

namespace
{
constexpr std::size_t MAX_COVERS = 24;
constexpr std::size_t MAX_COVER_JOBS = 4;
constexpr std::size_t MAX_ENCODED_COVER = 16 * 1024 * 1024;
constexpr std::size_t MAX_FILES = 4096;
QString Q(const std::string& text)
{
  return QString::fromStdString(text);
}
std::string S(const QString& text)
{
  return text.toStdString();
}
bool IsGameFile(const QFileInfo& info, bool disc)
{
  const auto suffix = info.suffix().toLower();
  static const QStringList discs{
      QStringLiteral("iso"),  QStringLiteral("gcm"),  QStringLiteral("rvz"), QStringLiteral("gcz"),
      QStringLiteral("wbfs"), QStringLiteral("ciso"), QStringLiteral("wia")};
  return discs.contains(suffix) ||
         (!disc && (suffix == QStringLiteral("dol") || suffix == QStringLiteral("elf") ||
                    suffix == QStringLiteral("wad") || suffix == QStringLiteral("json") ||
                    suffix == QStringLiteral("dff") || suffix == QStringLiteral("m3u")));
}
void Cycle(std::string& current, std::vector<std::string> values)
{
  std::ranges::sort(values);
  values.erase(std::unique(values.begin(), values.end()), values.end());
  std::erase(values, std::string{});
  values.insert(values.begin(), std::string{});
  const auto it = std::ranges::find(values, current);
  current = it == values.end() || std::next(it) == values.end() ? values.front() : *std::next(it);
}
}  // namespace

BigPictureLibrary::BigPictureLibrary(QObject* parent, Callbacks callbacks)
    : QObject(parent), m_callbacks(std::move(callbacks)),
      m_worker("BigPictureLibrary", [this](Job job) { RunJob(std::move(job)); })
{
  Publish(true);
}

BigPictureLibrary::~BigPictureLibrary()
{
  m_destroying = true;
  m_worker.StopAndCancel();
}

void BigPictureLibrary::Publish(bool filter)
{
  Q_ASSERT(QThread::currentThread() == thread());
  if (m_callbacks.recursive)
    m_state.recursive = m_callbacks.recursive();
  if (filter)
  {
    ++m_state.view_revision;
    m_state.games.clear();
    const auto query = Q(m_state.query).toCaseFolded();
    for (const auto& game : m_games)
    {
      const bool text_match =
          query.isEmpty() ||
          Q(game.title + "\n" + game.game_id + "\n" + game.path).toCaseFolded().contains(query);
      if (text_match && (m_state.platform.empty() || game.platform == m_state.platform) &&
          (m_state.region.empty() || game.region == m_state.region) &&
          (m_state.tag.empty() || std::ranges::find(game.tags, m_state.tag) != game.tags.end()))
        m_state.games.push_back(game);
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::ranges::sort(m_state.games, [&](const auto& a, const auto& b) {
      if (m_sort == 1 && a.played_ms != b.played_ms)
        return a.played_ms > b.played_ms;
      if (m_sort == 2 && a.size != b.size)
        return a.size > b.size;
      const int order = collator.compare(Q(a.title), Q(b.title));
      return order != 0 ? order < 0 : a.path < b.path;
    });
    m_state.total_games = m_games.size();
    m_state.sort = S(m_sort == 1 ? tr("Time Played") : m_sort == 2 ? tr("File Size") : tr("Title"));
  }
  m_snapshot = std::make_shared<const BigPicture::LibrarySnapshot>(m_state);
  if (m_callbacks.changed && !m_destroying)
    m_callbacks.changed();
}

void BigPictureLibrary::SetGames(std::vector<BigPicture::LibraryGame> games)
{
  if (m_state.scanning)
  {
    m_pending_games = std::move(games);
    return;
  }
  for (auto& game : games)
  {
    const auto played =
        tr("%1h %2m").arg(game.played_ms / 3600000).arg(game.played_ms / 60000 % 60);
    QStringList details{tr("Platform: %1").arg(Q(game.platform)),
                        tr("Region: %1").arg(Q(game.region)),
                        tr("Game ID: %1").arg(Q(game.game_id)),
                        tr("Time Played: %1").arg(played),
                        tr("File Size: %1").arg(Q(UICommon::FormatSize(game.size))),
                        tr("File: %1").arg(QDir::toNativeSeparators(Q(game.path)))};
    QStringList tags;
    for (const auto& tag : game.tags)
      tags.push_back(Q(tag));
    if (!tags.empty())
      details.push_back(tr("Tags: %1").arg(tags.join(QStringLiteral(", "))));
    game.details = S(details.join(QLatin1Char('\n')));
  }
  m_games = std::move(games);
  ++m_state.catalog_revision;
  m_state.covers.clear();
  m_cover_use.clear();
  m_state.missing_covers.clear();
  if (!m_state.selected_path.empty() &&
      std::ranges::find(m_games, m_state.selected_path, &BigPicture::LibraryGame::path) ==
          m_games.end())
    m_state.selected_path.clear();
  Publish(true);
}

void BigPictureLibrary::SetInitialSelection(std::string path)
{
  if (m_state.selected_path.empty())
  {
    m_state.selected_path = std::move(path);
    Publish();
  }
}

void BigPictureLibrary::SetScanning(bool scanning)
{
  m_state.scanning = scanning;
  if (!scanning && m_pending_games)
  {
    auto games = std::move(*m_pending_games);
    m_pending_games.reset();
    SetGames(std::move(games));
    return;
  }
  Publish();
}

void BigPictureLibrary::Browse(const std::string& path)
{
  m_state.picker_library = false;
  // At most one queued directory request. A new location cancels obsolete queued work;
  // completion is always guarded by picker revision, including cancellation/renderer handoff.
  ++m_state.picker_revision;
  m_state.directory = path;
  m_state.files.clear();
  m_state.parent_directory.clear();
  m_state.picker_error.clear();
  m_state.picker_loading = true;
  Job job;
  job.revision = m_state.picker_revision;
  job.path = path;
  job.folders_only = m_state.picker_folders_only;
  job.disc = m_state.picker_disc;
  if (m_directory_pending)
    m_next_directory = std::move(job);
  else
  {
    m_directory_pending = true;
    m_worker.Push(std::move(job));
  }
  Publish();
}

void BigPictureLibrary::SetDiscContext(std::string path, std::string game_id, std::string title,
                                       std::vector<std::string> folders)
{
  m_disc_path = std::move(path);
  m_disc_game_id = std::move(game_id);
  m_disc_title = std::move(title);
  m_disc_folders = std::move(folders);
}

std::string BigPictureLibrary::DiscDirectory() const
{
  if (!m_disc_path.empty() && QFileInfo(Q(m_disc_path)).absoluteDir().exists())
    return S(QFileInfo(Q(m_disc_path)).absolutePath());
  for (const auto& folder : m_disc_folders)
    if (QFileInfo(Q(folder)).isDir() && QFileInfo(Q(folder)).isReadable())
      return folder;
  return S(QDir::homePath());
}

void BigPictureLibrary::OpenDiscLibrary()
{
  std::vector<const BigPicture::LibraryGame*> games;
  for (const auto& game : m_games)
    if (IsGameFile(QFileInfo(Q(game.path)), true))
      games.push_back(&game);
  if (games.empty())
  {
    Browse(DiscDirectory());
    return;
  }
  const auto rank = [&](const auto* game) {
    const bool same_id = !m_disc_game_id.empty() && game->game_id == m_disc_game_id;
    // Also group alternate-region releases and multi-disc filenames without shared IDs.
    const auto title = Q(game->title).toCaseFolded();
    const auto running = Q(m_disc_title).toCaseFolded();
    qsizetype prefix = 0;
    while (prefix < std::min(title.size(), running.size()) && title[prefix] == running[prefix])
      ++prefix;
    return std::tuple{same_id, prefix >= std::min<qsizetype>(8, running.size()) && prefix >= 3,
                      game->path != m_disc_path, prefix};
  };
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::ranges::sort(games, [&](const auto* a, const auto* b) {
    if (rank(a) != rank(b))
      return rank(a) > rank(b);
    const int order = collator.compare(Q(a->title), Q(b->title));
    return order != 0 ? order < 0 : a->path < b->path;
  });
  ++m_state.picker_revision;
  m_state.picker_library = true;
  m_state.picker_loading = false;
  m_state.directory.clear();
  m_state.parent_directory.clear();
  m_state.picker_error.clear();
  m_state.files.clear();
  for (const auto* game : games)
    m_state.files.push_back({game->path, game->title, false});
  Publish();
}

void BigPictureLibrary::RequestCover(const std::string& path, std::uint64_t revision)
{
  const auto key = std::pair{revision, path};
  if (revision == m_state.catalog_revision && m_state.covers.contains(path))
  {
    m_cover_use[path] = ++m_cover_clock;
    return;
  }
  if (revision != m_state.catalog_revision || m_pending_covers.size() >= MAX_COVER_JOBS ||
      m_pending_covers.contains(key) || m_state.covers.contains(path) ||
      m_state.missing_covers.contains(path) ||
      std::ranges::find(m_games, path, &BigPicture::LibraryGame::path) == m_games.end())
    return;
  auto bytes = m_callbacks.read_cover ? m_callbacks.read_cover(path) : std::vector<unsigned char>{};
  if (m_state.scanning && bytes.empty())
    return;
  if (bytes.empty() || bytes.size() > MAX_ENCODED_COVER)
  {
    m_state.missing_covers.insert(path);
    Publish();
    return;
  }
  m_pending_covers.insert(key);
  Job job;
  job.cover = true;
  job.revision = revision;
  job.path = path;
  job.bytes = std::move(bytes);
  m_worker.Push(std::move(job));
}

void BigPictureLibrary::RunJob(Job job)
{
  if (job.cover)
  {
    auto result = std::make_shared<BigPicture::LibraryImage>();
    QByteArray bytes(reinterpret_cast<const char*>(job.bytes.data()),
                     static_cast<qsizetype>(job.bytes.size()));
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto size = reader.size();
    if (size.isValid() && size.width() <= 8192 && size.height() <= 8192 &&
        static_cast<std::uint64_t>(size.width()) * size.height() <= 16 * 1024 * 1024)
    {
      reader.setScaledSize(size.scaled(256, 384, Qt::KeepAspectRatio));
      const auto image = reader.read().convertToFormat(QImage::Format_RGBA8888);
      if (!image.isNull())
      {
        result->revision = job.revision;
        result->width = image.width();
        result->height = image.height();
        result->rgba.resize(static_cast<std::size_t>(image.width()) * image.height() * 4);
        for (int y = 0; y < image.height(); ++y)
          std::memcpy(result->rgba.data() + static_cast<std::size_t>(y) * image.width() * 4,
                      image.constScanLine(y), static_cast<std::size_t>(image.width()) * 4);
      }
    }
    QueueOnObject(this, [this, job = std::move(job), result] {
      m_pending_covers.erase({job.revision, job.path});
      if (job.revision != m_state.catalog_revision || m_destroying)
        return;
      if (result->rgba.empty())
        m_state.missing_covers.insert(job.path);
      else
      {
        if (m_state.covers.size() >= MAX_COVERS)
        {
          const auto oldest = std::ranges::min_element(
              m_cover_use, {}, [](const auto& item) { return item.second; });
          m_state.covers.erase(oldest->first);
          m_cover_use.erase(oldest);
        }
        m_state.covers[job.path] = result;
        m_cover_use[job.path] = ++m_cover_clock;
      }
      Publish();
    });
    return;
  }
  std::vector<BigPicture::LibraryFile> files;
  std::string parent, error;
  if (job.path.empty())
  {
    files.push_back({S(QDir::homePath()), S(tr("Home")), true});
    for (const auto& drive : QDir::drives())
      files.push_back({S(drive.absoluteFilePath()), S(drive.absoluteFilePath()), true});
  }
  else
  {
    QFileInfo info(Q(job.path));
    if (!info.isDir() || !info.isReadable())
      error = S(tr("This folder is unavailable or unreadable."));
    else
    {
      QDir dir(info.absoluteFilePath());
      if (!dir.isRoot())
      {
        dir.cdUp();
        parent = S(dir.absolutePath());
      }
      QDirIterator it(info.absoluteFilePath(),
                      QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
      std::size_t examined = 0;
      while (it.hasNext() && files.size() < MAX_FILES && examined++ < MAX_FILES * 4 &&
             !m_destroying)
      {
        it.next();
        const auto entry = it.fileInfo();
        if (entry.isDir() || (!job.folders_only && IsGameFile(entry, job.disc)))
          files.push_back({S(entry.absoluteFilePath()), S(entry.fileName()), entry.isDir()});
      }
      if (it.hasNext())
        error = S(tr("This folder is too large. Enter a more specific path."));
      QCollator collator;
      collator.setNumericMode(true);
      collator.setCaseSensitivity(Qt::CaseInsensitive);
      std::ranges::sort(files, [&](const auto& a, const auto& b) {
        if (a.directory != b.directory)
          return a.directory;
        return collator.compare(Q(a.name), Q(b.name)) < 0;
      });
    }
  }
  QueueOnObject(this, [this, revision = job.revision, files = std::move(files),
                       parent = std::move(parent), error = std::move(error)] {
    m_directory_pending = false;
    if (m_next_directory && !m_destroying)
    {
      m_directory_pending = true;
      m_worker.Push(std::move(*m_next_directory));
      m_next_directory.reset();
    }
    if (revision != m_state.picker_revision || m_destroying)
      return;
    m_state.files = files;
    m_state.parent_directory = parent;
    m_state.picker_error = error;
    m_state.picker_loading = false;
    Publish();
  });
}

std::string BigPictureLibrary::Execute(BigPicture::LibraryOperation operation,
                                       const std::string& value, std::uint64_t revision)
{
  using O = BigPicture::LibraryOperation;
  bool filter = false;
  switch (operation)
  {
  case O::Select:
    if (std::ranges::find(m_state.games, value, &BigPicture::LibraryGame::path) !=
        m_state.games.end())
      m_state.selected_path = value;
    else
      return S(tr("This game is no longer in the current library view."));
    break;
  case O::Options:
    break;
  case O::Search:
    m_state.editing_path = false;
    m_state.draft = m_state.query;
    m_state.native_text_input = m_callbacks.native_text_input && m_callbacks.native_text_input();
    break;
  case O::EditPath:
    m_state.editing_path = true;
    m_state.draft = m_state.directory;
    m_state.native_text_input = m_callbacks.native_text_input && m_callbacks.native_text_input();
    break;
  case O::AddFolder:
  case O::OpenFile:
  case O::OpenDisc:
    m_state.picker_folders_only = operation == O::AddFolder;
    m_state.picker_disc = operation == O::OpenDisc;
    if (operation == O::OpenDisc)
      OpenDiscLibrary();
    else
      Browse({});
    return {};
  case O::BrowseDiscs:
    Browse(DiscDirectory());
    return {};
  case O::EnterDirectory:
    if (revision != m_state.picker_revision || m_state.picker_loading)
      return S(tr("The folder listing changed. Choose the location again."));
    Browse(value);
    return {};
  case O::ChooseFolder:
    if (revision != m_state.picker_revision || m_state.picker_loading ||
        value != m_state.directory || !QFileInfo(Q(value)).isDir() ||
        !QFileInfo(Q(value)).isReadable())
      return S(tr("This folder is no longer available."));
    if (m_callbacks.add_folder)
      m_callbacks.add_folder(S(QFileInfo(Q(value)).canonicalFilePath()));
    break;
  case O::Refresh:
    if (m_callbacks.refresh)
      m_callbacks.refresh();
    break;
  case O::View:
    m_state.grid = !m_state.grid;
    ++m_state.view_revision;
    break;
  case O::Recursive:
    if (m_callbacks.set_recursive)
      m_callbacks.set_recursive(!m_state.recursive);
    break;
  case O::Sort:
    m_sort = (m_sort + 1) % 3;
    filter = true;
    break;
  case O::Platform:
  case O::Region:
  case O::Tag:
  {
    std::vector<std::string> values;
    for (const auto& game : m_games)
      if (operation == O::Tag)
        values.insert(values.end(), game.tags.begin(), game.tags.end());
      else
        values.push_back(operation == O::Platform ? game.platform : game.region);
    Cycle(operation == O::Tag      ? m_state.tag :
          operation == O::Platform ? m_state.platform :
                                     m_state.region,
          std::move(values));
    filter = true;
    break;
  }
  case O::ClearFilters:
    m_state.query.clear();
    m_state.platform.clear();
    m_state.region.clear();
    m_state.tag.clear();
    filter = true;
    break;
  case O::ClearSearch:
    m_state.query.clear();
    filter = true;
    break;
  case O::TextClear:
    m_state.draft.clear();
    break;
  case O::ControllerKeyboard:
    m_state.native_text_input = false;
    break;
  case O::TextReplace:
    if (Q(value).size() > 1024)
      return S(tr("The text is too long."));
    m_state.draft = value;
    break;
  case O::TextKey:
    if (Q(value).size() > 8 || Q(m_state.draft).size() + Q(value).size() > 1024)
      return S(tr("The text is too long."));
    m_state.draft += value;
    break;
  case O::TextErase:
  {
    auto text = Q(m_state.draft);
    if (!text.isEmpty())
    {
      const int length = text.size() >= 2 && text.back().isLowSurrogate() &&
                                 text.at(text.size() - 2).isHighSurrogate() ?
                             2 :
                             1;
      text.chop(length);
    }
    m_state.draft = S(text);
    break;
  }
  case O::TextDone:
    if (m_state.editing_path)
    {
      const QFileInfo info(Q(m_state.draft));
      if (!info.isDir() || !info.isReadable())
        return S(tr("Enter an existing, readable folder path."));
      Browse(S(info.absoluteFilePath()));
      return {};
    }
    m_state.query = m_state.draft;
    filter = true;
    break;
  case O::Cover:
    RequestCover(value, revision);
    return {};
  default:
    return S(tr("Invalid library operation."));
  }
  Publish(filter);
  return {};
}
