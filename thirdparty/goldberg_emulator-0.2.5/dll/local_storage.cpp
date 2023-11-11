/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "local_storage.h"

#include <fstream>
#include <algorithm>
#include <iterator>

struct File_Data {
    std::string name;
};

#ifdef NO_DISK_WRITES
std::string Local_Storage::get_program_path()
{
    return " ";
}


std::string Local_Storage::get_user_appdata_path()
{
    return " ";
}

std::string Local_Storage::get_game_settings_path()
{
    return " ";
}

std::string Local_Storage::get_path(std::string folder)
{
    return "";
}

std::string Local_Storage::get_global_settings_path()
{
    return "";
}

Local_Storage::Local_Storage(std::string save_directory)
{

}

void Local_Storage::setAppId(uint32 appid)
{

}

int Local_Storage::store_file_data(std::string folder, std::string file, char *data, unsigned int length)
{
    return -1;
}

int Local_Storage::store_data(std::string folder, std::string file, char *data, unsigned int length)
{
    return -1;
}

int Local_Storage::store_data_settings(std::string file, char *data, unsigned int length)
{
    return -1;
}

int Local_Storage::get_file_data(std::string full_path, char *data, unsigned int max_length, unsigned int offset)
{
    return -1;
}

int Local_Storage::get_data(std::string folder, std::string file, char *data, unsigned int max_length, unsigned int offset)
{
    return -1;
}

int Local_Storage::get_data_settings(std::string file, char *data, unsigned int max_length)
{
    return 0;
}

int Local_Storage::count_files(std::string folder)
{
    return 0;
}

bool Local_Storage::file_exists(std::string folder, std::string file)
{
    return false;
}

unsigned int Local_Storage::file_size(std::string folder, std::string file)
{
    return 0;
}

bool Local_Storage::file_delete(std::string folder, std::string file)
{
    return false;
}

uint64_t Local_Storage::file_timestamp(std::string folder, std::string file)
{
    return 0;
}

bool Local_Storage::iterate_file(std::string folder, int index, char *output_filename, int32 *output_size)
{
    return false;
}

bool Local_Storage::update_save_filenames(std::string folder)
{
    return true;
}

std::vector<std::string> Local_Storage::get_filenames_path(std::string path)
{
    return std::vector<std::string>();
}

#else
#if defined(WIN32) || defined(_WIN32)
#include <windows.h>

static BOOL DirectoryExists(LPCSTR szPath)
{
  DWORD dwAttrib = GetFileAttributesA(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
    (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static void createDirectoryRecursively(std::string path)
{
  unsigned long long pos = 0;
  do
  {
    pos = path.find_first_of("\\/", pos + 1);
    CreateDirectoryA(path.substr(0, pos).c_str(), NULL);
  } while (pos != std::string::npos);
}

static void create_directory(std::string strPath)
{
    if (DirectoryExists(strPath.c_str()) == FALSE)
        createDirectoryRecursively(strPath);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


static std::vector<struct File_Data> get_filenames(std::string strPath)
{
    std::vector<struct File_Data> output;
    strPath = strPath.append("\\*");
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    // Start iterating over the files in the path directory.
    hFind = ::FindFirstFileA (strPath.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do // Managed to locate and create an handle to that folder.
        {
            if (strcmp(".", ffd.cFileName) == 0) continue;
            if (strcmp("..", ffd.cFileName) == 0) continue;
            struct File_Data f_data;
            f_data.name = ffd.cFileName;
            output.push_back(f_data);
        } while (::FindNextFileA(hFind, &ffd) == TRUE);
        ::FindClose(hFind);
    } else {
        //printf("Failed to find path: %s", strPath.c_str());
    }

    return output;
}

static std::vector<struct File_Data> get_filenames_recursive(std::string base_path)
{
    if (base_path.back() == *PATH_SEPARATOR)
        base_path.pop_back();
    std::vector<struct File_Data> output;
    std::string strPath = base_path;
    strPath = strPath.append("\\*");
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    // Start iterating over the files in the path directory.
    hFind = ::FindFirstFileA (strPath.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do // Managed to locate and create an handle to that folder.
        {
            if (strcmp(".", ffd.cFileName) == 0) continue;
            if (strcmp("..", ffd.cFileName) == 0) continue;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Construct new path from our base path
                std::string dir_name = ffd.cFileName;

                std::string path = base_path;
                path += PATH_SEPARATOR;
                path += dir_name;

                std::vector<struct File_Data> lower = get_filenames_recursive(path);
                std::transform(lower.begin(), lower.end(), std::back_inserter(output), [&dir_name](File_Data f) {f.name = dir_name + "\\" + f.name; return f;});
            } else {
                File_Data f;
                f.name = ffd.cFileName;
                output.push_back(f);
            }
        } while (::FindNextFileA(hFind, &ffd) == TRUE);
        ::FindClose(hFind);
    } else {
        //printf("Failed to find path: %s", strPath.c_str());
    }

    return output;
}

#else 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>

#define PATH_MAX_STRING_SIZE 512

/* recursive mkdir */
static int mkdir_p(const char *dir, const mode_t mode) {
    char tmp[PATH_MAX_STRING_SIZE];
    char *p = NULL;
    struct stat sb;
    size_t len;
    
    /* copy path */
    len = strnlen (dir, PATH_MAX_STRING_SIZE);
    if (len == 0 || len == PATH_MAX_STRING_SIZE) {
        return -1;
    }
    memcpy (tmp, dir, len);
    tmp[len] = '\0';

    /* remove trailing slash */
    if(tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    /* check if path exists and is a directory */
    if (stat (tmp, &sb) == 0) {
        if (S_ISDIR (sb.st_mode)) {
            return 0;
        }
    }
    
    /* recursive mkdir */
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            /* test path */
            if (stat(tmp, &sb) != 0) {
                /* path does not exist - create directory */
                if (mkdir(tmp, mode) < 0) {
                    return -1;
                }
            } else if (!S_ISDIR(sb.st_mode)) {
                /* not a directory */
                return -1;
            }
            *p = '/';
        }
    }
    /* test path */
    if (stat(tmp, &sb) != 0) {
        /* path does not exist - create directory */
        if (mkdir(tmp, mode) < 0) {
            return -1;
        }
    } else if (!S_ISDIR(sb.st_mode)) {
        /* not a directory */
        return -1;
    }
    return 0;
}

static void create_directory(std::string strPath)
{
    mkdir_p(strPath.c_str(), 0777);
}

static std::vector<struct File_Data> get_filenames(std::string strPath)
{
  DIR *dp;
  int i = 0;
  struct dirent *ep;
  std::vector<struct File_Data> output;
  dp = opendir (strPath.c_str());

  if (dp != NULL)
  {
    while ((ep = readdir (dp))) {
      if (memcmp(ep->d_name, ".", 2) != 0 && memcmp(ep->d_name, "..", 3) != 0) {
        struct File_Data f_data;
        f_data.name = ep->d_name;
        output.push_back(f_data);
        i++;
      }
    }

    (void) closedir (dp);
  }

  return output;
}

static std::vector<struct File_Data> get_filenames_recursive(std::string base_path)
{
    std::vector<struct File_Data> output;
    std::string path;
    struct dirent *dp;
    DIR *dir = opendir(base_path.c_str());

    // Unable to open directory stream
    if (!dir)
        return output;

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            if (dp->d_type == DT_REG) {
                File_Data f;
                f.name = dp->d_name;
                output.push_back(f);
            } else if (dp->d_type == DT_DIR) {
                // Construct new path from our base path
                std::string dir_name = dp->d_name;

                path = base_path;
                path += "/";
                path += dir_name;

                std::vector<struct File_Data> lower = get_filenames_recursive(path);
                std::transform(lower.begin(), lower.end(), std::back_inserter(output), [&dir_name](File_Data f) {f.name = dir_name + "/" + f.name; return f;});
            }
        }
    }

    closedir(dir);

    return output;
}


#endif 

std::string Local_Storage::get_program_path()
{
    return get_full_program_path();
}

std::string Local_Storage::get_game_settings_path()
{
    return get_program_path().append(GAME_SETTINGS_FOLDER).append(PATH_SEPARATOR);
}

#if defined(STEAM_WIN32)
#include <shlobj.h>
#include <sstream>
#endif

std::string Local_Storage::get_user_appdata_path()
{
    std::string user_appdata_path = "SAVE";
#if defined(STEAM_WIN32)
    CHAR szPath[MAX_PATH] = {};

    HRESULT hr = SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath);

    if (SUCCEEDED(hr)) {
        user_appdata_path = szPath;
    }

#else
    /* $XDG_DATA_HOME defines the base directory relative to which user specific data files should be stored. 
    If $XDG_DATA_HOME is either not set or empty, a default equal to $HOME/.local/share should be used. */
    char *datadir = getenv("XDG_DATA_HOME");
    if (datadir) {
        user_appdata_path = datadir;
    } else {
        char *homedir = getenv("HOME");
        if (homedir) {
            user_appdata_path = std::string(homedir) + "/.local/share";
        }
    }
#endif
    return user_appdata_path.append(PATH_SEPARATOR).append(PROGRAM_NAME).append(" Saves");
}

static std::string replace_with(std::string s, std::string const &old, const char *new_str)
{
    int pos;
    while ((pos = s.find(old)) != std::string::npos)
        s.replace(pos, old.length(), new_str);

    return s;
}

static std::string sanitize_file_name(std::string name)
{
    //I'm not sure all of these are necessary but just to be sure
    if (name[0] == '.' && name.size() > 2 && (name[1] == '\\' || name[1] == '/')) name.erase(0, 2);

#if defined(STEAM_WIN32)
    name = replace_with(name, "/", PATH_SEPARATOR);
#else
    //On linux does using "\\" in a remote storage file name create a directory?
    //I didn't test but I'm going to say yes
    name = replace_with(name, "\\", PATH_SEPARATOR);
#endif
    name = replace_with(name, "|", ".V_SLASH.");
    name = replace_with(name, ":", ".COLON.");
    name = replace_with(name, "*", ".ASTERISK.");
    name = replace_with(name, "\"", ".QUOTE.");
    name = replace_with(name, "?", ".Q_MARK.");

    return name;
}

static std::string desanitize_file_name(std::string name)
{
    //I'm not sure all of these are necessary but just to be sure
    name = replace_with(name, ".SLASH.", "/");
    name = replace_with(name, ".B_SLASH.", "\\");
    name = replace_with(name, ".F_SLASH.", "/");
    name = replace_with(name, ".V_SLASH.", "|");
    name = replace_with(name, ".COLON.", ":");
    name = replace_with(name, ".ASTERISK.", "*");
    name = replace_with(name, ".QUOTE.", "\"");
    name = replace_with(name, ".Q_MARK.", "?");

    return name;
}

Local_Storage::Local_Storage(std::string save_directory)
{
    this->save_directory = save_directory;

    if (this->save_directory.back() != *PATH_SEPARATOR) {
        this->save_directory.append(PATH_SEPARATOR);
    }
}

void Local_Storage::setAppId(uint32 appid)
{
    this->appid = std::to_string(appid) + PATH_SEPARATOR;
}

int Local_Storage::store_file_data(std::string folder, std::string file, char *data, unsigned int length)
{
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    file = sanitize_file_name(file);
    std::string::size_type pos = file.rfind(PATH_SEPARATOR);

    std::string file_folder;
    if (pos == 0 || pos == std::string::npos) {
        file_folder = "";
    } else {
        file_folder = file.substr(0,pos);
    }

    create_directory(folder + file_folder);
    std::ofstream myfile;
    myfile.open(folder + file, std::ios::binary | std::ios::out);
    if (!myfile.is_open()) return -1;
    myfile.write(data, length);
    int position = myfile.tellp();
    myfile.close();
    return position;
}

std::string Local_Storage::get_path(std::string folder)
{
    std::string path = save_directory + appid + folder;
    create_directory(path);
    return path;
}

std::string Local_Storage::get_global_settings_path()
{
    return save_directory + SETTINGS_STORAGE_FOLDER + PATH_SEPARATOR;
}

std::vector<std::string> Local_Storage::get_filenames_path(std::string path)
{
    if (path.back() != *PATH_SEPARATOR) {
        path.append(PATH_SEPARATOR);
    }

    std::vector<struct File_Data> filenames = get_filenames(path);
    std::vector<std::string> output;
    std::transform(filenames.begin(), filenames.end(), std::back_inserter(output), [](struct File_Data d) { return d.name;});
    return output;
}

int Local_Storage::store_data(std::string folder, std::string file, char *data, unsigned int length)
{
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    return store_file_data(save_directory + appid + folder, file, data, length);
}

int Local_Storage::store_data_settings(std::string file, char *data, unsigned int length)
{
    return store_file_data(get_global_settings_path(), file, data, length);
}

int Local_Storage::get_file_data(std::string full_path, char *data, unsigned int max_length, unsigned int offset)
{
    std::ifstream myfile;
    myfile.open(full_path, std::ios::binary | std::ios::in);
    if (!myfile.is_open()) return -1;

    myfile.seekg (offset, std::ios::beg);
    myfile.read (data, max_length);
    myfile.close();
    return myfile.gcount();
}

int Local_Storage::get_data(std::string folder, std::string file, char *data, unsigned int max_length, unsigned int offset)
{
    file = sanitize_file_name(file);
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::string full_path = save_directory + appid + folder + file;
    return get_file_data(full_path, data, max_length, offset);
}

int Local_Storage::get_data_settings(std::string file, char *data, unsigned int max_length)
{
    file = sanitize_file_name(file);
    std::string full_path = get_global_settings_path() + file;
    return get_file_data(full_path, data, max_length);
}

int Local_Storage::count_files(std::string folder)
{
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    return get_filenames_recursive(save_directory + appid + folder).size();
}

bool Local_Storage::file_exists(std::string folder, std::string file)
{
    file = sanitize_file_name(file);
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::string full_path = save_directory + appid + folder + file;
    struct stat buffer;   

    if (stat(full_path.c_str(), &buffer) != 0)
        return false;

#if defined(STEAM_WIN32)
    if ( buffer.st_mode & _S_IFDIR)
        return false;
#else
    if (S_ISDIR(buffer.st_mode))
        return false;
#endif

    return true;
}

unsigned int Local_Storage::file_size(std::string folder, std::string file)
{
    file = sanitize_file_name(file);
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::string full_path = save_directory + appid + folder + file;
    struct stat buffer = {};   
    if (stat (full_path.c_str(), &buffer) != 0) return 0;
    return buffer.st_size;
}

bool Local_Storage::file_delete(std::string folder, std::string file)
{
    file = sanitize_file_name(file);
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::string full_path = save_directory + appid + folder + file;
    return remove(full_path.c_str()) == 0;
}

uint64_t Local_Storage::file_timestamp(std::string folder, std::string file)
{
    file = sanitize_file_name(file);
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::string full_path = save_directory + appid + folder + file;
    struct stat buffer = {};
    if (stat (full_path.c_str(), &buffer) != 0) return 0;
    return buffer.st_mtime;
}

bool Local_Storage::iterate_file(std::string folder, int index, char *output_filename, int32 *output_size)
{
    if (folder.back() != *PATH_SEPARATOR) {
        folder.append(PATH_SEPARATOR);
    }

    std::vector<struct File_Data> files = get_filenames_recursive(save_directory + appid + folder);
    if (index < 0 || index >= files.size()) return false;

    std::string name = desanitize_file_name(files[index].name);
    if (output_size) *output_size = file_size(folder, name);
#if defined(STEAM_WIN32)
    name = replace_with(name, PATH_SEPARATOR, "/");
#endif
    strcpy(output_filename, name.c_str());
    return true;
}

bool Local_Storage::update_save_filenames(std::string folder)
{
    std::vector<struct File_Data> files = get_filenames_recursive(save_directory + appid + folder);

    for (auto &f : files) {
        std::string path = f.name;
        PRINT_DEBUG("Local_Storage:: remote file %s\n", path.c_str());
        std::string to = sanitize_file_name(desanitize_file_name(path));
        if (path != to && !file_exists(folder, to)) {
            //create the folder
            store_data(folder, to, (char *)"", 0);
            file_delete(folder, to);

            std::string from = (save_directory + appid + folder + PATH_SEPARATOR + path);
            to = (save_directory + appid + folder + PATH_SEPARATOR + to);
            PRINT_DEBUG("Local_Storage::update_save_filenames renaming %s to %s\n", from.c_str(), to.c_str());
            if (std::rename(from.c_str(), to.c_str()) < 0) {
                PRINT_DEBUG("ERROR RENAMING\n");
            }
        }
    }

    return true;
}

#endif
