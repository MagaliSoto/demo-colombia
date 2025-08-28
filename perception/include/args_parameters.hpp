#pragma once

#include <string>
#include <iostream>
#include <thread>

#include <gflags/gflags.h>

static const char help_message[] = "Print an usage message.";
static const char streams_file_message[] = "YAML file containing source streams. Default: /app/cfg-yml/sources.yml";
static const char n_threads_message[] = "Number of threads to create in the pool. Default: hardware_concurrency()-1";
static const char use_nvurisrcbin_message[] = "Use nvurisrcbin instead of uridecodebin. Default: false";
static const char save_frames_message[] = "Save frames to disk. Default: false.";
static const char show_sink_message[] = "Show Rendered Output. Default: false.";


DEFINE_bool(h, false, help_message);
DEFINE_string(streams, "/app/cfg/sources.yml", streams_file_message);
DEFINE_uint32(n_threads, std::thread::hardware_concurrency()-1, n_threads_message);
DEFINE_bool(use_nvurisrcbin, false, use_nvurisrcbin_message);
DEFINE_bool(save_frames, false, save_frames_message);
DEFINE_bool(show_sink, false, show_sink_message);


void showUsage() {
    std::cout << std::endl;
    std::cout << "perception [OPTION] <config.yml file>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << std::endl;
    std::cout << "    -h                       " << help_message << std::endl;
    std::cout << "    -streams \"<file>\"        " << streams_file_message << std::endl;
    std::cout << "    -n_threads <number>      " << n_threads_message << std::endl;
    std::cout << "    -use_nvurisrcbin         " << use_nvurisrcbin_message << std::endl;
    std::cout << "    -save_frames             " << save_frames_message << std::endl;
    std::cout << "    -show_sink               " << show_sink_message << std::endl;
}
