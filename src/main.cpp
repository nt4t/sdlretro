#include "miniz.h"

#include <logger.h>

#if SDLRETRO_FRONTEND == 1
#include <sdl1_impl.h>
#endif
#if SDLRETRO_FRONTEND == 2
#include <sdl2_impl.h>
#endif
#if SDLRETRO_FRONTEND == 3
#include <fbdev_impl.h>
#endif
#include <i18n.h>
#include <core_manager.h>
#include <helper.h>
#include <ui_host.h>
#include <cfg.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

#include <getopt.h>

static const char* ROM_EXTENSIONS[] = {
    "bin", "img", "iso", "rom",
    "nes", "sfc", "smc", "gba", "gb",
    "md", "sg", "smd", "sms", "gg",
    "z64", "n64", "v64",
    "nds", "nds.gz",
    "pce", "ngc", "ngp",
    "ws", "wsc",
    "pcfx", "sc", "chd",
    nullptr
};

static const char* SKIP_EXTENSIONS[] = {
    "txt", "nfo", "info", "pdf",
    "html", "htm", "xml", "json",
    "cfg", "ini", "log", "md",
    "rtf", "csv", "doc",
    nullptr
};

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static bool is_skip_extension(const std::string& ext) {
    std::string lower_ext = to_lower(ext);
    for (int i = 0; SKIP_EXTENSIONS[i] != nullptr; i++) {
        if (lower_ext == SKIP_EXTENSIONS[i]) return true;
    }
    return false;
}

static bool is_rom_extension(const std::string& ext) {
    std::string lower_ext = to_lower(ext);
    for (int i = 0; ROM_EXTENSIONS[i] != nullptr; i++) {
        if (lower_ext == ROM_EXTENSIONS[i]) return true;
    }
    return false;
}

static std::string get_base_name(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    size_t last_dot = path.find_last_of('.');
    if (last_slash == std::string::npos) last_slash = 0;
    else last_slash++;
    if (last_dot == std::string::npos || last_dot < last_slash) return path.substr(last_slash);
    return path.substr(last_slash, last_dot - last_slash);
}

static int find_best_rom_entry(mz_zip_archive* pZip) {
    mz_uint num_files = mz_zip_reader_get_num_files(pZip);
    int best_idx = -1;
    int best_priority = -1;
    
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(pZip, i, &file_stat)) continue;
        
        const char* ext_ptr = strrchr(file_stat.m_filename, '.');
        if (ext_ptr == nullptr) continue;
        
        std::string ext = ext_ptr + 1;
        
        if (is_skip_extension(ext)) continue;
        
        int priority = is_rom_extension(ext) ? 1 : 0;
        
        if (priority > best_priority) {
            best_priority = priority;
            best_idx = (int)i;
        }
    }
    
    return best_idx;
}

static bool extract_zip_to_file(mz_zip_archive* pZip, int file_index, const std::string& output_path) {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat)) return false;
    
    if (file_stat.m_uncomp_size > 256 * 1024 * 1024) {
        LOG(WARN, "Skipping file >256MB: {}", file_stat.m_filename);
        return false;
    }
    
    FILE* f = fopen(output_path.c_str(), "wb");
    if (!f) return false;
    
    mz_zip_archive_file_stat stat2;
    mz_zip_reader_file_stat(pZip, file_index, &stat2);
    
    mz_uint8* p = (mz_uint8*)malloc(stat2.m_uncomp_size);
    if (!p) {
        fclose(f);
        return false;
    }
    
    if (!mz_zip_reader_extract_to_mem(pZip, file_index, p, stat2.m_uncomp_size, 0)) {
        free(p);
        fclose(f);
        return false;
    }
    
    size_t written = fwrite(p, 1, stat2.m_uncomp_size, f);
    free(p);
    fclose(f);
    
    if (written != stat2.m_uncomp_size) {
        LOG(ERROR, "ZIP: incomplete write {} vs {}", written, stat2.m_uncomp_size);
        return false;
    }
    
    return true;
}

#define DEFAULT_DATA_DIR "."
#ifdef GCW_ZERO
#define DEFAULT_STORE_DIR "/usr/local/home/.sdlretro"
#elif defined(__linux__) || defined(__unix__)
static std::string get_default_store_dir() {
    const char *home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/sdlretro";
    }
    return "./sdlretro";
}
#else
#define DEFAULT_STORE_DIR "./sdlretro"
#endif

int program(int argc, char *argv[]) {
    const char *core_filename = nullptr;
    const char *config_filename = nullptr;
    static struct option long_options[] = {
        {"libretro",     required_argument, 0,  'L' },
        {"config",     required_argument, 0,  'c' },
        {nullptr }
    };
    opterr = 0;
    while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "L:", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
        case 'L':
            core_filename = optarg;
            break;
        case 'c':
            config_filename = optarg;
            break;
        case '?':
            if (optopt)
                LOG(ERROR, "Bad option '-{}'", optopt);
            else
                LOG(ERROR, "Bad option '{}'", argv[optind - 1]);
            return 1;
        default:
            break;
        }
    }
    if (optind >= argc) {
        LOG(ERROR, "ROM filename missing.");
        return 1;
    }
    const char *rom_filename = argv[optind];

#ifdef __linux__
    std::string default_store_dir = get_default_store_dir();
#else
    const char *default_store_dir = DEFAULT_STORE_DIR;
#endif

    if (config_filename) {
        g_cfg.load(config_filename);
        if (g_cfg.get_data_dir().empty())
            g_cfg.set_data_dir(DEFAULT_DATA_DIR);
        if (g_cfg.get_store_dir().empty())
            g_cfg.set_store_dir(default_store_dir);
    } else {
        g_cfg.set_data_dir(DEFAULT_DATA_DIR);
        g_cfg.set_store_dir(default_store_dir);
        g_cfg.load("");
    }

    libretro::i18n_obj.set_language(g_cfg.get_language());

    libretro::core_manager coreman;

#if SDLRETRO_FRONTEND == 1
    auto impl = drivers::create_driver<drivers::sdl1_impl>();
#endif
#if SDLRETRO_FRONTEND == 2
    auto impl = drivers::create_driver<drivers::sdl2_impl>();
#endif
#if SDLRETRO_FRONTEND == 3
    auto impl = drivers::create_driver<drivers::fbdev_impl>();
#endif
    if (!impl) {
        LOG(ERROR, "Unable to create driver!");
        return 1;
    }

    gui::ui_host ui(impl);

    std::string rom_ext;
    std::vector<const libretro::core_info *> core_list;
    std::string extracted_file;

    // ZIP extraction (before core selection, works with or without -L flag)
    const char *ptr = strrchr(rom_filename, '.');
    if (ptr != nullptr && strcasecmp(ptr, ".zip") == 0) {
        LOG(INFO, "ZIP: Opening {}", rom_filename);
        mz_zip_archive arc = {};
        if (!mz_zip_reader_init_file(&arc, rom_filename, 0)) {
            LOG(ERROR, "Failed to open ZIP file!");
            return 1;
        }
        
        int num_files = mz_zip_reader_get_num_files(&arc);
        LOG(INFO, "ZIP: {} files in archive", num_files);
        
        for (mz_uint i = 0; i < num_files; i++) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&arc, i, &st)) {
                LOG(INFO, "ZIP: entry[{}] = {} ({})", i, st.m_filename, st.m_uncomp_size);
            }
        }
        
        int best_idx = find_best_rom_entry(&arc);
        if (best_idx < 0) {
            LOG(ERROR, "No valid ROM file found in ZIP!");
            mz_zip_reader_end(&arc);
            return 1;
        }
        
        mz_zip_archive_file_stat file_stat;
        mz_zip_reader_file_stat(&arc, best_idx, &file_stat);
        LOG(INFO, "ZIP: selected {} (idx={})", file_stat.m_filename, best_idx);
        
        rom_ext = strrchr(file_stat.m_filename, '.') + 1;
        LOG(INFO, "ZIP: extension={}", rom_ext);
        
        core_list = coreman.match_cores_by_extension(rom_ext);
        LOG(INFO, "ZIP: matched {} cores", core_list.size());
        
        mz_zip_reader_end(&arc);
    }

    std::string core_filepath;
    if (core_filename) {
        core_filepath = core_filename;
        if (!helper::file_exists(core_filepath)) {
            std::vector<std::string> dirs;
            g_cfg.get_core_dirs(dirs);
            bool found = false;
            for (auto &d: dirs) {
                core_filepath = d + PATH_SEPARATOR_CHAR + core_filename;
                if (helper::file_exists(core_filepath)) {
                    found = true;
                    break;
                }
                core_filepath = d + PATH_SEPARATOR_CHAR + core_filename + "." DYNLIB_EXTENSION;
                if (helper::file_exists(core_filepath)) {
                    found = true;
                    break;
                }
                core_filepath = d + PATH_SEPARATOR_CHAR + core_filename + "_libretro." DYNLIB_EXTENSION;
                if (helper::file_exists(core_filepath)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG(ERROR, "Unable to load core '{}'!", core_filename);
                return 1;
            }
        }
    } else {
        if (core_list.empty() && ptr != nullptr) {
            rom_ext = ptr + 1;
            core_list = coreman.match_cores_by_extension(rom_ext);
            if (core_list.empty()) {
                LOG(ERROR, "Cannot find core for file extension {}!", rom_ext.c_str());
                return 1;
            }
        }
        int index = 0;
        if (core_list.size() > 1) {
            index = ui.select_core_menu(core_list);
            if (index < 0 || index >= core_list.size()) {
                return 1;
            }
        }
        core_filepath = core_list[index]->filepath;
    }
    if (!impl->load_core(core_filepath)) {
        LOG(ERROR, "Unable to load core from '{}'!", core_filepath);
        return 1;
    }
    LOG(INFO, "Loading game: {} (exists={})", rom_filename, helper::file_exists(rom_filename) ? "yes" : "no");
    impl->load_game(rom_filename);
    impl->run([&ui] { ui.in_game_menu(); });
    impl->unload_game();

    return 0;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

#ifdef NDEBUG
    util::logInit(stdout, util::LogLevel::INFO);
#else
    util::logInit(stdout, util::LogLevel::TRACE);
#endif
    int res = program(argc, argv);
    util::logUninit();
    return res;
}

#ifdef _MSC_VER

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}

#endif
