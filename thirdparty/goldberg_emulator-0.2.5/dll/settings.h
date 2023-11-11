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

#include "base.h"
#include <set>
#include <string>

#ifndef SETTINGS_INCLUDE
#define SETTINGS_INCLUDE

struct DLC_entry {
    AppId_t appID;
    std::string name;
    bool available;
};

struct Mod_entry {
    PublishedFileId_t id;
    std::string title;
    std::string path;
};

struct Leaderboard_config {
    enum ELeaderboardSortMethod sort_method;
    enum ELeaderboardDisplayType display_type;
};

class Settings {
    CSteamID steam_id;
    CGameID game_id;
    std::string name, language;
    CSteamID lobby_id;

    bool unlockAllDLCs;
    bool offline;
    std::vector<struct DLC_entry> DLCs;
    std::vector<struct Mod_entry> mods;
    std::map<AppId_t, std::string> app_paths;
    std::map<std::string, Leaderboard_config> leaderboards;
    bool create_unknown_leaderboards;

public:
#ifdef LOBBY_CONNECT
    static const bool is_lobby_connect = true;
#else
    static const bool is_lobby_connect = false;
#endif
    static std::string sanitize(std::string name);
    Settings(CSteamID steam_id, CGameID game_id, std::string name, std::string language, bool offline);
    CSteamID get_local_steam_id();
    CGameID get_local_game_id();
    const char *get_local_name();
    const char *get_language();
    void set_game_id(CGameID game_id);
    void set_lobby(CSteamID lobby_id);
    CSteamID get_lobby();
    bool is_offline() {return offline; }

    //DLC stuff
    void unlockAllDLC(bool value);
    void addDLC(AppId_t appID, std::string name, bool available);
    unsigned int DLCCount();
    bool hasDLC(AppId_t appID);
    bool getDLC(unsigned int index, AppId_t &appID, bool &available, std::string &name);

    //App Install paths
    void setAppInstallPath(AppId_t appID, std::string path);
    std::string getAppInstallPath(AppId_t appID);

    //mod stuff
    void addMod(PublishedFileId_t id, std::string title, std::string path);
    Mod_entry getMod(PublishedFileId_t id);
    bool isModInstalled(PublishedFileId_t id);
    std::set<PublishedFileId_t> modSet();

    //leaderboards
    void setLeaderboard(std::string leaderboard, enum ELeaderboardSortMethod sort_method, enum ELeaderboardDisplayType display_type);
    std::map<std::string, Leaderboard_config> getLeaderboards() { return leaderboards; }
    void setCreateUnknownLeaderboards(bool enable) {create_unknown_leaderboards = enable;}
    bool createUnknownLeaderboards() { return create_unknown_leaderboards; }
};

#endif
