// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <deque>
#include <set>

#ifdef WIN32
#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <io.h>
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <time.h>
#include <unistd.h>
#endif
#if defined(LEVELDB_PLATFORM_ANDROID)
#include <sys/stat.h>
#endif
#include "leveldb/env.h"
#include "leveldb/slice.h"

#ifdef WIN32
#include "util/win_logger.h"
#else
#include "util/posix_logger.h"
#endif
#include "port/port.h"
#include "util/logging.h"

#ifdef __linux
#include <sys/sysinfo.h>
#include <linux/unistd.h>
#endif

#include <fstream>

// Boost includes - see WINDOWS file to see which modules to install
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

namespace leveldb {
namespace {

// returns the ID of the current process
static boost::uint32_t current_process_id(void) {
#ifdef _WIN32
  return static_cast<boost::uint32_t>(::GetCurrentProcessId());
#else
  return static_cast<boost::uint32_t>(::getpid());
#endif
}

// returns the ID of the current thread
static boost::uint32_t current_thread_id(void) {
#ifdef _WIN32
  return static_cast<boost::uint32_t>(::GetCurrentThreadId());
#else
#ifdef __linux
  return static_cast<boost::uint32_t>(::syscall(__NR_gettid));
#else
  // just return the pid
  return current_process_id();
#endif
#endif
}

static char global_read_only_buf[0x8000];

class PosixSequentialFile: public SequentialFile {
 private:
  std::string filename_;
  FILE* file_;

 public:
  PosixSequentialFile(const std::string& fname, FILE* f)
    : filename_(fname), file_(f) { }
  virtual ~PosixSequentialFile() { fclose(file_); }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
  Status s;
#ifdef BSD
  // fread_unlocked doesn't exist on FreeBSD
  size_t r = fread(scratch, 1, n, file_);
#else
  size_t r = fread_unlocked(scratch, 1, n, file_);
#endif
  *result = Slice(scratch, r);
  if (r < n) {
    if (feof(file_)) {
    // We leave status as ok if we hit the end of the file
    } else {
    // A partial read with an error: return a non-ok status
    s = Status::IOError(filename_, strerror(errno));
    }
  }
  return s;
  }

  virtual Status Skip(uint64_t n) {
  if (fseek(file_, n, SEEK_CUR)) {
    return Status::IOError(filename_, strerror(errno));
  }
  return Status::OK();
  }
};

class PosixRandomAccessFile: public RandomAccessFile {
 private:
  std::string filename_;
  int fd_;
  mutable boost::mutex mu_;

 public:
  PosixRandomAccessFile(const std::string& fname, int fd)
    : filename_(fname), fd_(fd) { }
  virtual ~PosixRandomAccessFile() { close(fd_); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
            char* scratch) const {
    Status s;
#ifdef WIN32
    // no pread on Windows so we emulate it with a mutex
    boost::unique_lock<boost::mutex> lock(mu_);

    if (::_lseeki64(fd_, offset, SEEK_SET) == -1L) {
      return Status::IOError(filename_, strerror(errno));
    }

    int r = ::_read(fd_, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    lock.unlock();
#else
    ssize_t r = pread(fd_, scratch, n, static_cast<off_t>(offset));
    *result = Slice(scratch, (r < 0) ? 0 : r);
#endif
    if (r < 0) {
      // An error: return a non-ok status
      s = Status::IOError(filename_, strerror(errno));
    }
    return s;
  }
};

// We preallocate up to an extra megabyte and use memcpy to append new
// data to the file.  This is safe since we either properly close the
// file before reading from it, or for log files, the reading code
// knows enough to skip zero suffixes.

class BoostFile : public WritableFile {

public:
  explicit BoostFile(std::string path) : path_(path), written_(0) {
    Open();
  }

  virtual ~BoostFile() {
    Close();
  }

private:
  void Open() {
    // we truncate the file as implemented in env_posix
     file_.open(path_.generic_string().c_str(), 
         std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
     written_ = 0;
  }

public:
  virtual Status Append(const Slice& data) {
    Status result;
    file_.write(data.data(), data.size());
    if (!file_.good()) {
      result = Status::IOError(
          path_.generic_string() + " Append", "cannot write");
    }
    return result;
  }

  virtual Status Close() {
    Status result;

    try {
      if (file_.is_open()) {
        Sync();
        file_.close();
      }
    } catch (const std::exception & e) {
      result = Status::IOError(path_.generic_string() + " close", e.what());
    }

    return result;
  }

  virtual Status Flush() {
    file_.flush();
    return Status::OK();
  }

  virtual Status Sync() {
    Status result;
    try {
      Flush();
    } catch (const std::exception & e) {
      result = Status::IOError(path_.string() + " sync", e.what());
    }

    return result;
  }

private:
  boost::filesystem::path path_;
  boost::uint64_t written_;
  std::ofstream file_;
};



class BoostFileLock : public FileLock {
 public:
  std::ofstream file_;
  boost::interprocess::file_lock fl_;
  std::string name_;
};

// Set of locked files.  We keep a separate set instead of just
// relying on fcntrl(F_SETLK) since fcntl(F_SETLK) does not provide
// any protection against multiple uses from the same process.
class BoostLockTable {
 private:
  boost::mutex mu_;
  std::set<std::string> locked_files_;
 public:
  bool Insert(const std::string& fname) {
    boost::unique_lock<boost::mutex> l(mu_);
    return locked_files_.insert(fname).second;
  }
  void Remove(const std::string& fname) {
    boost::unique_lock<boost::mutex> l(mu_);
    locked_files_.erase(fname);
  }
};

class PosixEnv : public Env {
 public:
  PosixEnv();
  virtual ~PosixEnv() 
  {
      if (bgthread_)
      {
          bgthread_->interrupt();
          bgthread_->join();
          bgthread_.reset();
      }

      boost::unique_lock<boost::mutex> lock(mu_);
      queue_.clear();
  }

  virtual Status NewSequentialFile(const std::string& fname,
                   SequentialFile** result) {
    FILE* f = fopen(fname.c_str(), "rb");
    if (f == NULL) {
      *result = NULL;
      return Status::IOError(fname, strerror(errno));
    } else {
      *result = new PosixSequentialFile(fname, f);
      return Status::OK();
    }
  }

  virtual Status NewRandomAccessFile(const std::string& fname,
                   RandomAccessFile** result) {
#ifdef WIN32
    int fd = _open(fname.c_str(), _O_RDONLY | _O_RANDOM | _O_BINARY);
#else
    int fd = open(fname.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      *result = NULL;
      return Status::IOError(fname, strerror(errno));
    }
    *result = new PosixRandomAccessFile(fname, fd);
    return Status::OK();
  }

  virtual Status NewWritableFile(const std::string& fname,
                 WritableFile** result) {
    Status s;
    try {
      // will create a new empty file to write to
      *result = new BoostFile(fname);
    }
    catch (const std::exception & e) {
      s = Status::IOError(fname, e.what());
    }

    return s;
  }

  virtual bool FileExists(const std::string& fname) {
    boost::system::error_code ec;
    return boost::filesystem::exists(fname, ec);
  }

  virtual Status GetChildren(const std::string& dir,
               std::vector<std::string>* result) {
    result->clear();

    boost::system::error_code ec;
    boost::filesystem::directory_iterator current(dir, ec);
    if (ec != 0) {
      return Status::IOError(dir, ec.message());
    }

    boost::filesystem::directory_iterator end;

    for(; current != end; ++current) {
      result->push_back(current->path().filename().generic_string());
    }

    return Status::OK();
  }

  virtual Status DeleteFile(const std::string& fname) {
    boost::system::error_code ec;

    boost::filesystem::remove(fname, ec);

    Status result;

    if (ec != 0) {
      result = Status::IOError(fname, ec.message());
    }

    return result;
  }

  virtual Status CreateDir(const std::string& name) {
      Status result;

      boost::system::error_code ec;

      if (boost::filesystem::exists(name, ec) &&
          boost::filesystem::is_directory(name, ec)) {
        return result;
      }
      
      if (!boost::filesystem::create_directories(name, ec)) {
        result = Status::IOError(name, ec.message());
      }

      return result;
    };

    virtual Status DeleteDir(const std::string& name) {
    Status result;

    boost::system::error_code ec;
    if (!boost::filesystem::remove_all(name, ec)) {
      result = Status::IOError(name, ec.message());
    }

    return result;
  };

  virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
    boost::system::error_code ec;

    Status result;

    *size = static_cast<uint64_t>(boost::filesystem::file_size(fname, ec));
    if (ec != 0) {
      *size = 0;
       result = Status::IOError(fname, ec.message());
    }

    return result;
  }

  virtual Status RenameFile(const std::string& src, const std::string& target) {
    boost::system::error_code ec;

    boost::filesystem::rename(src, target, ec);

    Status result;

    if (ec != 0) {
      result = Status::IOError(src, ec.message());
    }

    return result;
  }


  virtual Status LockFile(const std::string& fname, FileLock** lock) {
    *lock = nullptr;
    std::ofstream of(fname.c_str(), std::ios_base::trunc | std::ios_base::out);
    if (of.bad()) {
        return Status::IOError("lock " + fname, "cannot create lock file");
    }
    if (!locks_.Insert(fname)) {
        of.close();
        return Status::IOError("lock " + fname, "already held by process");
    }
    boost::interprocess::file_lock fl(fname.c_str());
    if (!fl.try_lock()) {
        of.close();         
        locks_.Remove(fname);
        return Status::IOError("lock " + fname, "database already in use: could not acquire exclusive lock" );
    }
    BoostFileLock * my_lock = new BoostFileLock();
    my_lock->name_ = fname;
    my_lock->file_ = std::move(of);
    my_lock->fl_ = std::move(fl);
    *lock = my_lock;
    return Status();
  }



  virtual Status UnlockFile(FileLock* lock) {
    BoostFileLock * my_lock = static_cast<BoostFileLock *>(lock);
    Status result;
    try {      
      my_lock->fl_.unlock();      
    } catch (const std::exception & e) {
      result = Status::IOError("unlock " + my_lock->name_, e.what());
    }
    locks_.Remove(my_lock->name_);
    my_lock->file_.close();
    delete my_lock;
    return result;
  }

  virtual void Schedule(void (*function)(void*), void* arg);

  virtual void StartThread(void (*function)(void* arg), void* arg);

  virtual Status GetTestDirectory(std::string* result) {
    boost::system::error_code ec;
    boost::filesystem::path temp_dir = 
        boost::filesystem::temp_directory_path(ec);
    if (ec != 0) {
      temp_dir = "tmp";
    }

    temp_dir /= "leveldb_tests";
    temp_dir /= boost::lexical_cast<std::string>(current_process_id());

    // Directory may already exist
    CreateDir(temp_dir.generic_string());

    *result = temp_dir.generic_string();

    return Status::OK();
  }

#ifndef WIN32
  static uint64_t gettid() {
    pthread_t tid = pthread_self();
    uint64_t thread_id = 0;
    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
    return thread_id;
  }
#endif

  virtual Status NewLogger(const std::string& fname, Logger** result) {
  FILE* f = fopen(fname.c_str(), "wt");
  if (f == NULL) {
    *result = NULL;
    return Status::IOError(fname, strerror(errno));
  } else {
#ifdef WIN32
    *result = new WinLogger(f);
#else
    *result = new PosixLogger(f, &PosixEnv::gettid);
#endif
    return Status::OK();
  }
  }

  virtual uint64_t NowMicros() {
    return static_cast<uint64_t>(
        boost::posix_time::microsec_clock::universal_time()
        .time_of_day().total_microseconds());
  }

  virtual void SleepForMicroseconds(int micros) {
  boost::this_thread::sleep(boost::posix_time::microseconds(micros));
  }

 private:
  void PthreadCall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    exit(1);
  }
  }

  // BGThread() is the body of the background thread
  void BGThread();

  static void* BGThreadWrapper(void* arg) {
    reinterpret_cast<PosixEnv*>(arg)->BGThread();
    return NULL;
  }

  boost::mutex mu_;
  boost::condition_variable bgsignal_;
  boost::scoped_ptr<boost::thread> bgthread_;

  // Entry per Schedule() call
  struct BGItem { void* arg; void (*function)(void*); };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;

  BoostLockTable locks_;
};

PosixEnv::PosixEnv() { }

void PosixEnv::Schedule(void (*function)(void*), void* arg) {
  boost::unique_lock<boost::mutex> lock(mu_);

  // Start background thread if necessary
  if (!bgthread_) {
     bgthread_.reset(
         new boost::thread(boost::bind(&PosixEnv::BGThreadWrapper, this)));
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  lock.unlock();

  bgsignal_.notify_one();

}

void PosixEnv::BGThread() {
    try
    {
        while (true) {
            // Wait until there is an item that is ready to run
            boost::unique_lock<boost::mutex> lock(mu_);

            while (queue_.empty()) {
                bgsignal_.wait(lock);
            }

            void (*function)(void*) = queue_.front().function;
            void* arg = queue_.front().arg;
            queue_.pop_front();

            lock.unlock();
            (*function)(arg);
        }
    }
    catch (const boost::thread_interrupted &) {}

}

namespace {
struct StartThreadState {
  void (*user_function)(void*);
  void* arg;
};
}

static void* StartThreadWrapper(void* arg) {
  StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
  state->user_function(state->arg);
  delete state;
  return NULL;
}

void PosixEnv::StartThread(void (*function)(void* arg), void* arg) {
  StartThreadState* state = new StartThreadState;
  state->user_function = function;
  state->arg = arg;

  boost::thread t(boost::bind(&StartThreadWrapper, state));
}

}

static const boost::once_flag init_value = BOOST_ONCE_INIT;
static boost::once_flag once = BOOST_ONCE_INIT;
static Env* default_env = nullptr;
static void InitDefaultEnv() { 
  ::memset(global_read_only_buf, 0, sizeof(global_read_only_buf));
  default_env = new PosixEnv;
}

Env* Env::Default() {
  boost::call_once(once, InitDefaultEnv);
  return default_env;
}

void Env::UnsafeDeallocate() {
    once = init_value;
    delete default_env;
    default_env = nullptr;
}

}
