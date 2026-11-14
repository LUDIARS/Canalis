#include "canalis/infra/browser_process.hpp"

#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "canalis/base/process.hpp"
#include "canalis/base/time.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace canalis::cdp {
namespace {

/** 受信側: 読んだバイトを NUL で区切ってメッセージの列にする (上限つき)。 */
class Inbox {
public:
    Inbox(std::size_t max_message, std::size_t max_pending) : max_message_(max_message), max_pending_(max_pending) {}

    /** 読んだバイトを足す。上限を超えたら false (以後は切断扱い)。 */
    bool feed(const char* data, std::size_t n) {
        std::lock_guard lock(m_);
        for (std::size_t i = 0; i < n; ++i) {
            if (data[i] == '\0') {
                pending_bytes_ += partial_.size();
                if (pending_bytes_ > max_pending_) return fail_locked();
                queue_.push_back(std::move(partial_));
                partial_.clear();
                cv_.notify_all();
            } else {
                if (partial_.size() >= max_message_) return fail_locked();
                partial_.push_back(data[i]);
            }
        }
        return true;
    }

    void close() {
        std::lock_guard lock(m_);
        closed_ = true;
        cv_.notify_all();
    }

    std::optional<std::string> pop(std::chrono::milliseconds timeout) {
        std::unique_lock lock(m_);
        cv_.wait_for(lock, timeout, [&] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        std::string out = std::move(queue_.front());
        queue_.pop_front();
        pending_bytes_ -= out.size();
        return out;
    }

    bool closed() const {
        std::lock_guard lock(m_);
        return closed_ && queue_.empty();
    }

private:
    bool fail_locked() {
        closed_ = true;
        queue_.clear();
        partial_.clear();
        pending_bytes_ = 0;
        cv_.notify_all();
        return false;
    }

    const std::size_t max_message_;
    const std::size_t max_pending_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    std::string partial_;
    std::size_t pending_bytes_ = 0;
    bool closed_ = false;
};

} // namespace

#ifdef _WIN32

namespace {

std::wstring widen(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

void close_handle(HANDLE& h) {
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = nullptr;
}

// MSVC CRT の fd 継承の旗 (Node / libuv と同じ値)。
constexpr unsigned char kFopen = 0x01;
constexpr unsigned char kFpipe = 0x08;
constexpr unsigned char kFdev = 0x40;

/** STARTUPINFO.lpReserved2 に入れる「fd → HANDLE」の表 (int 個数 + 旗の列 + HANDLE の列、詰めて並べる)。 */
std::vector<unsigned char> crt_fd_block(const std::vector<std::pair<unsigned char, HANDLE>>& fds) {
    const int count = static_cast<int>(fds.size());
    std::vector<unsigned char> block(sizeof(int) + fds.size() + fds.size() * sizeof(HANDLE));
    std::memcpy(block.data(), &count, sizeof(int));
    for (std::size_t i = 0; i < fds.size(); ++i) {
        block[sizeof(int) + i] = fds[i].first;
        std::memcpy(block.data() + sizeof(int) + fds.size() + i * sizeof(HANDLE), &fds[i].second, sizeof(HANDLE));
    }
    return block;
}

} // namespace

struct BrowserProcess::Impl {
    Impl(std::size_t max_message, std::size_t max_pending) : inbox(max_message, max_pending) {}
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE to_child = nullptr;    // 親が書く (子の fd 3)
    HANDLE from_child = nullptr;  // 親が読む (子の fd 4)
    std::mutex write_mutex;
    Inbox inbox;
    std::thread reader;
    bool stopped = false;
};

BrowserProcess::BrowserProcess(const LaunchOptions& options)
    : impl_(std::make_unique<Impl>(options.max_message_bytes, options.max_pending_bytes)) {
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE cmd_r = nullptr, cmd_w = nullptr, resp_r = nullptr, resp_w = nullptr;
    if (!CreatePipe(&cmd_r, &cmd_w, &sa, 0)) throw std::runtime_error("browser: pipe failed");
    if (!CreatePipe(&resp_r, &resp_w, &sa, 0)) {
        close_handle(cmd_r);
        close_handle(cmd_w);
        throw std::runtime_error("browser: pipe failed");
    }
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_EXISTING, 0, nullptr);
    SetHandleInformation(cmd_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(resp_r, HANDLE_FLAG_INHERIT, 0);
    impl_->to_child = cmd_w;
    impl_->from_child = resp_r;
    auto close_child_ends = [&] {
        close_handle(cmd_r);
        close_handle(resp_w);
        close_handle(nul);
    };

    std::string cmdline = proc::quote_windows_arg(options.executable);
    for (const auto& a : options.args) cmdline += " " + proc::quote_windows_arg(a);
    cmdline += " --remote-debugging-pipe";

    const auto block = crt_fd_block({{static_cast<unsigned char>(kFopen | kFdev), nul},
                                     {static_cast<unsigned char>(kFopen | kFdev), nul},
                                     {static_cast<unsigned char>(kFopen | kFdev), nul},
                                     {static_cast<unsigned char>(kFopen | kFpipe), cmd_r},
                                     {static_cast<unsigned char>(kFopen | kFpipe), resp_w}});

    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<unsigned char> attr_buf(attr_size);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
    InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size);
    HANDLE inherit[3] = {nul, cmd_r, resp_w};
    UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof inherit, nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = nul;
    si.StartupInfo.hStdError = nul;
    si.StartupInfo.cbReserved2 = static_cast<WORD>(block.size());
    si.StartupInfo.lpReserved2 = const_cast<LPBYTE>(block.data());
    si.lpAttributeList = attrs;

    impl_->job = CreateJobObjectW(nullptr, nullptr);
    if (impl_->job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(impl_->job, JobObjectExtendedLimitInformation, &info, sizeof info);
    }

    std::wstring wexe = widen(options.executable);
    std::wstring wcmd = widen(cmdline);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(wexe.c_str(), wcmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr,
                                   &si.StartupInfo, &pi);
    const DWORD error = GetLastError();
    DeleteProcThreadAttributeList(attrs);
    close_child_ends();
    if (!ok) {
        close_handle(impl_->to_child);
        close_handle(impl_->from_child);
        close_handle(impl_->job);
        throw std::runtime_error("browser: CreateProcess failed: " + std::to_string(error));
    }
    impl_->process = pi.hProcess;
    if (impl_->job) AssignProcessToJobObject(impl_->job, impl_->process);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    impl_->reader = std::thread([impl = impl_.get()] {
        char buf[65536];
        DWORD n = 0;
        while (ReadFile(impl->from_child, buf, sizeof buf, &n, nullptr) && n > 0)
            if (!impl->inbox.feed(buf, n)) break;
        impl->inbox.close();
    });
}

bool BrowserProcess::send(std::string_view message) {
    std::lock_guard lock(impl_->write_mutex);
    if (!impl_->to_child || impl_->inbox.closed()) return false;
    std::string data(message);
    data.push_back('\0');
    std::size_t offset = 0;
    while (offset < data.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 1u << 20));
        if (!WriteFile(impl_->to_child, data.data() + offset, chunk, &written, nullptr) || written == 0) return false;
        offset += written;
    }
    return true;
}

void BrowserProcess::close(std::chrono::milliseconds grace) {
    if (impl_->stopped) return;
    impl_->stopped = true;
    if (impl_->process && WaitForSingleObject(impl_->process, static_cast<DWORD>(grace.count())) != WAIT_OBJECT_0) {
        if (impl_->job) TerminateJobObject(impl_->job, 1);
        else TerminateProcess(impl_->process, 1);
        WaitForSingleObject(impl_->process, 5000);
    }
    if (impl_->job) TerminateJobObject(impl_->job, 0);  // ブラウザが残した子プロセスも止める
    {
        std::lock_guard lock(impl_->write_mutex);
        close_handle(impl_->to_child);
    }
    // 子の書き側がすべて閉じると ReadFile が終わる。念のため読み取りを取り消す。
    if (impl_->reader.joinable()) {
        CancelSynchronousIo(impl_->reader.native_handle());
        impl_->reader.join();
    }
    close_handle(impl_->from_child);
    close_handle(impl_->process);
    close_handle(impl_->job);
    impl_->inbox.close();
}

#else  // POSIX

namespace {

/** fd を 10 以上の番号へ CLOEXEC 付きで移す (子で dup2 する前に 3 / 4 と重ならないように)。 */
int move_high(int fd) {
    const int moved = fcntl(fd, F_DUPFD_CLOEXEC, 10);
    ::close(fd);
    if (moved < 0) throw std::runtime_error("browser: fcntl failed");
    return moved;
}

} // namespace

struct BrowserProcess::Impl {
    Impl(std::size_t max_message, std::size_t max_pending) : inbox(max_message, max_pending) {}
    pid_t pid = -1;
    int to_child = -1;
    int from_child = -1;
    std::mutex write_mutex;
    Inbox inbox;
    std::thread reader;
    bool stopped = false;
};

BrowserProcess::BrowserProcess(const LaunchOptions& options)
    : impl_(std::make_unique<Impl>(options.max_message_bytes, options.max_pending_bytes)) {
    // 閉じた pipe へ書いたときにプロセスごと落ちないようにする (send は false を返す)。
    std::signal(SIGPIPE, SIG_IGN);
    int cmd[2];
    int resp[2];
    if (pipe(cmd) != 0) throw std::runtime_error("browser: pipe failed");
    if (pipe(resp) != 0) {
        ::close(cmd[0]);
        ::close(cmd[1]);
        throw std::runtime_error("browser: pipe failed");
    }
    const int child_read = move_high(cmd[0]);
    const int parent_write = move_high(cmd[1]);
    const int parent_read = move_high(resp[0]);
    const int child_write = move_high(resp[1]);

    std::vector<std::string> argv_store{options.executable};
    for (const auto& a : options.args) argv_store.push_back(a);
    argv_store.emplace_back("--remote-debugging-pipe");
    std::vector<char*> argv;
    for (auto& a : argv_store) argv.push_back(a.data());
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, child_read, 3);
    posix_spawn_file_actions_adddup2(&actions, child_write, 4);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setpgroup(&attr, 0);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    pid_t pid = -1;
    const int rc = posix_spawn(&pid, options.executable.c_str(), &actions, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    ::close(child_read);
    ::close(child_write);
    if (rc != 0) {
        ::close(parent_write);
        ::close(parent_read);
        throw std::runtime_error("browser: spawn failed: " + std::string(std::strerror(rc)));
    }
    impl_->pid = pid;
    impl_->to_child = parent_write;
    impl_->from_child = parent_read;
    impl_->reader = std::thread([impl = impl_.get()] {
        char buf[65536];
        while (true) {
            const ssize_t n = ::read(impl->from_child, buf, sizeof buf);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            if (!impl->inbox.feed(buf, static_cast<std::size_t>(n))) break;
        }
        impl->inbox.close();
    });
}

bool BrowserProcess::send(std::string_view message) {
    std::lock_guard lock(impl_->write_mutex);
    if (impl_->to_child < 0 || impl_->inbox.closed()) return false;
    std::string data(message);
    data.push_back('\0');
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t n = ::write(impl_->to_child, data.data() + offset, data.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

void BrowserProcess::close(std::chrono::milliseconds grace) {
    if (impl_->stopped) return;
    impl_->stopped = true;
    if (impl_->pid > 0) {
        const auto deadline = time::steady_ms() + grace.count();
        int status = 0;
        bool exited = false;
        while (time::steady_ms() < deadline) {
            const pid_t r = waitpid(impl_->pid, &status, WNOHANG);
            if (r == impl_->pid || r < 0) {
                exited = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        kill(-impl_->pid, SIGKILL);  // ブラウザが残した子プロセスも止める (プロセスグループごと)
        if (!exited) waitpid(impl_->pid, &status, 0);
        impl_->pid = -1;
    }
    {
        std::lock_guard lock(impl_->write_mutex);
        if (impl_->to_child >= 0) ::close(impl_->to_child);
        impl_->to_child = -1;
    }
    if (impl_->reader.joinable()) impl_->reader.join();
    if (impl_->from_child >= 0) ::close(impl_->from_child);
    impl_->from_child = -1;
    impl_->inbox.close();
}

#endif

BrowserProcess::~BrowserProcess() { close(std::chrono::milliseconds(0)); }

std::optional<std::string> BrowserProcess::receive(std::chrono::milliseconds timeout) { return impl_->inbox.pop(timeout); }

bool BrowserProcess::closed() const { return impl_->inbox.closed(); }

} // namespace canalis::cdp
