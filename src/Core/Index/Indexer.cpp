//
// Background indexing - see Indexer.h
//

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Indexer.h"
#include "Core/UnicodeHelper.h"
#include "Core/FileSystem/FolderScanner.h"

using namespace gedit;
namespace fs = std::filesystem;

Indexer::Indexer(CodeIndex::Ref index, Languages languages, Options options) : index(std::move(index)), languages(std::move(languages)), options(std::move(options)) {
}

Indexer::~Indexer() {
    Stop();
}

void Indexer::AddRoot(const fs::path &root, const fs::path &priorityPath) {
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        roots.push_back(root);
        queue.push_back({kWorkType::kRoot, root.string(), priorityPath.string()});
        StartThreads();
    }
    workAvailable.notify_one();
}

void Indexer::Enqueue(const std::string &path) {
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        QueueFile(path);
        StartThreads();
    }
    workAvailable.notify_one();
}

void Indexer::EnqueueFolder(const fs::path &folder) {
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        queue.push_back({kWorkType::kFolder, folder.string(), {}});
        StartThreads();
    }
    workAvailable.notify_one();
}

void Indexer::RescanRoots() {
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        for (auto &root : roots) {
            queue.push_back({kWorkType::kRoot, root.string(), {}});
        }
        StartThreads();
    }
    workAvailable.notify_all();
}

void Indexer::ReplaceFromBuffer(const std::string &path, const FileStructure &structure) {
    std::lock_guard guard(bufferLock);
    bufferPaths.insert(path);
    index->ReplaceFile(path, structure);
}

void Indexer::ReleaseFromBuffer(const std::string &path) {
    {
        std::lock_guard guard(bufferLock);
        bufferPaths.erase(path);
    }
    Enqueue(path);
}

// Caller holds the lock
void Indexer::QueueFile(const std::string &path) {
    if (queuedPaths.insert(path).second) {
        queue.push_back({kWorkType::kFile, path, {}});
        numTotal++;
    }
}

// Caller holds the lock
void Indexer::StartThreads() {
    if (!workers.empty()) {
        return;
    }
    auto numThreads = options.numThreads;
    if (numThreads == 0) {
        auto numCores = (size_t)std::thread::hardware_concurrency();
        numThreads = (numCores > 1) ? (numCores - 1) : 1;
    }
    for (size_t i = 0; i < numThreads; i++) {
        workers.emplace_back([this]() {
            WorkerLoop();
        });
    }
}

Indexer::Progress Indexer::GetProgress() {
    std::lock_guard guard(lock);
    return {numDone, numTotal, queue.empty() && (numActive == 0)};
}

void Indexer::WaitIdle() {
    std::unique_lock guard(lock);
    idle.wait(guard, [this]() {
        return queue.empty() && (numActive == 0);
    });
}

void Indexer::Stop() {
    {
        std::lock_guard guard(lock);
        isStopping = true;
        queue.clear();
        queuedPaths.clear();
        requeuedPaths.clear();
    }
    workAvailable.notify_all();
    for (auto &worker : workers) {
        worker.join();
    }
    workers.clear();
    idle.notify_all();
}

void Indexer::WorkerLoop() {
    // One scanner per language, per thread - a scanner has state while it runs
    std::unordered_map<LanguageBase *, StructureScanner::Ptr> scanners;
    WorkItem item;
    while (TakeWork(item)) {
        if (item.type == kWorkType::kRoot) {
            ScanRoot(item);
        } else if (item.type == kWorkType::kFolder) {
            ScanFolder(item);
        } else {
            IndexFile(item.path, scanners);
        }
        FinishWork(item);
    }
}

bool Indexer::TakeWork(WorkItem &item) {
    std::unique_lock guard(lock);
    while (true) {
        workAvailable.wait(guard, [this]() {
            return isStopping || !queue.empty();
        });
        if (isStopping) {
            return false;
        }
        item = std::move(queue.front());
        queue.pop_front();
        if (item.type != kWorkType::kFile) {
            numActive++;
            return true;
        }
        queuedPaths.erase(item.path);
        if (activePaths.contains(item.path)) {
            // Another thread is reading it - that thread reads it once more when done
            requeuedPaths.insert(item.path);
            numDone++;
            continue;
        }
        activePaths.insert(item.path);
        numActive++;
        return true;
    }
}

void Indexer::FinishWork(const WorkItem &item) {
    bool isIdle = false;
    {
        std::lock_guard guard(lock);
        numActive--;
        if (item.type == kWorkType::kFile) {
            numDone++;
            activePaths.erase(item.path);
            if (requeuedPaths.erase(item.path) > 0) {
                QueueFile(item.path);
                workAvailable.notify_one();
            }
        }
        isIdle = queue.empty() && (numActive == 0);
    }
    if (isIdle) {
        idle.notify_all();
    }
}

// How many leading path components two paths share
static size_t CommonDepth(const fs::path &a, const fs::path &b) {
    size_t depth = 0;
    auto itA = a.begin();
    auto itB = b.begin();
    while ((itA != a.end()) && (itB != b.end()) && (*itA == *itB)) {
        depth++;
        ++itA;
        ++itB;
    }
    return depth;
}

// 'folder' is 'root' or somewhere below it
static bool IsBelow(const std::string &folder, const std::string &root) {
    return (folder.compare(0, root.size(), root) == 0) && ((folder.size() == root.size()) || (folder[root.size()] == '/'));
}

void Indexer::ScanRoot(const WorkItem &item) {
    std::vector<std::string> paths;
    FolderScanner scanner;
    scanner.onFile = [this, &paths](const fs::path &path, int) {
        if (languages.contains(path.extension().string())) {
            paths.push_back(path.string());
        }
    };
    FolderScanner::Options scanOptions;
    scanOptions.exclude = options.exclude;
    scanOptions.maxDepth = options.maxDepth;
    scanner.Scan(item.path, scanOptions);

    if (!item.priorityPath.empty()) {
        fs::path priorityFolder = fs::path(item.priorityPath).parent_path();
        std::vector<std::pair<size_t, std::string>> ranked;
        ranked.reserve(paths.size());
        for (auto &path : paths) {
            ranked.emplace_back(CommonDepth(fs::path(path).parent_path(), priorityFolder), std::move(path));
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
            return a.first > b.first;
        });
        paths.clear();
        for (auto &entry : ranked) {
            paths.push_back(std::move(entry.second));
        }
    }

    std::unordered_set<std::string> found(paths.begin(), paths.end());
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        for (auto &path : paths) {
            if (IsStale(path)) {
                QueueFile(path);
            }
        }
        // Read before, not there now
        for (auto &[folder, files] : stamps) {
            if (!IsBelow(folder, item.path)) {
                continue;
            }
            for (auto &[path, stamp] : files) {
                if (!found.contains(path)) {
                    QueueFile(path);
                }
            }
        }
    }
    workAvailable.notify_all();
}

void Indexer::ScanFolder(const WorkItem &item) {
    std::unordered_set<std::string> found;
    std::error_code error;
    for (auto &entry : fs::directory_iterator(item.path, fs::directory_options::skip_permission_denied, error)) {
        if (entry.is_regular_file(error) && languages.contains(entry.path().extension().string())) {
            found.insert(entry.path().string());
        }
    }
    bool isGone = !fs::is_directory(item.path, error);
    {
        std::lock_guard guard(lock);
        if (isStopping) {
            return;
        }
        for (auto &path : found) {
            if (IsStale(path)) {
                QueueFile(path);
            }
        }
        for (auto &[folder, files] : stamps) {
            if ((folder != item.path) && !(isGone && IsBelow(folder, item.path))) {
                continue;
            }
            for (auto &[path, stamp] : files) {
                if (!found.contains(path)) {
                    QueueFile(path);
                }
            }
        }
    }
    workAvailable.notify_all();
}

// Caller holds the lock
bool Indexer::IsStale(const fs::path &path) {
    auto itFolder = stamps.find(path.parent_path().string());
    if (itFolder == stamps.end()) {
        return true;
    }
    auto itFile = itFolder->second.find(path.string());
    if (itFile == itFolder->second.end()) {
        return true;
    }
    std::error_code error;
    auto timeModified = fs::last_write_time(path, error);
    auto size = fs::file_size(path, error);
    return error || (timeModified != itFile->second.timeModified) || (size != itFile->second.size);
}

// A file that isn't UTF-8 (Latin-1 comments are common in old C) would stop the decoder at the first bad
// byte and lose the rest of the file. Identifiers are ASCII, so each bad byte just becomes a '?'.
static void ReplaceInvalidUtf8(std::string &data) {
    size_t idx = 0;
    while (idx < data.size()) {
        auto ch = (unsigned char)data[idx];
        size_t length = (ch < 0x80) ? 1 : ((ch >= 0xc2) && (ch <= 0xdf)) ? 2 : ((ch & 0xf0) == 0xe0) ? 3 : ((ch >= 0xf0) && (ch <= 0xf4)) ? 4 : 0;
        bool isValid = (length > 0) && ((idx + length) <= data.size());
        for (size_t i = 1; isValid && (i < length); i++) {
            isValid = ((unsigned char)data[idx + i] & 0xc0) == 0x80;
        }
        // Overlong forms, surrogates and past U+10FFFF
        if (isValid && (length > 2)) {
            auto second = (unsigned char)data[idx + 1];
            isValid = !(((ch == 0xe0) && (second < 0xa0)) || ((ch == 0xed) && (second > 0x9f)) ||
                        ((ch == 0xf0) && (second < 0x90)) || ((ch == 0xf4) && (second > 0x8f)));
        }
        if (isValid) {
            idx += length;
        } else {
            data[idx] = '?';
            idx++;
        }
    }
}

void Indexer::IndexFile(const std::string &path, std::unordered_map<LanguageBase *, StructureScanner::Ptr> &scanners) {
    fs::path filePath(path);
    auto folder = filePath.parent_path().string();
    auto itLanguage = languages.find(filePath.extension().string());
    std::error_code error;
    auto fileSize = fs::file_size(filePath, error);
    auto timeModified = fs::last_write_time(filePath, error);
    std::ifstream file(filePath, std::ios::binary);

    // Stamped before the read: a change while reading then looks like a change, not like the same file
    {
        std::lock_guard guard(lock);
        if (error || !file.is_open()) {
            auto itFolder = stamps.find(folder);
            if (itFolder != stamps.end()) {
                itFolder->second.erase(path);
                if (itFolder->second.empty()) {
                    stamps.erase(itFolder);
                }
            }
        } else {
            stamps[folder][path] = {timeModified, fileSize};
        }
    }

    if ((itLanguage == languages.end()) || error || !file.is_open() || (fileSize > options.maxFileSize)) {
        RemoveUnlessFromBuffer(path);
        return;
    }
    std::ostringstream content;
    content << file.rdbuf();
    auto data = std::move(content).str();
    ReplaceInvalidUtf8(data);

    auto *language = itLanguage->second.get();
    auto &scanner = scanners[language];
    if (scanner == nullptr) {
        scanner = StructureScanner::CreateForLanguage(*language);
    }
    if (scanner == nullptr) {
        return;
    }
    auto structure = scanner->Scan(UnicodeHelper::utf8to32(data));
    std::lock_guard guard(bufferLock);
    if (!bufferPaths.contains(path)) {
        index->ReplaceFile(path, structure);
    }
}

void Indexer::RemoveUnlessFromBuffer(const std::string &path) {
    std::lock_guard guard(bufferLock);
    if (!bufferPaths.contains(path)) {
        index->RemoveFile(path);
    }
}
