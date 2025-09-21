/*
   Copyright 2005-2010 Jakub Kruszona-Zawadzki, Gemius SA
   Copyright 2013-2014 EditShare
   Copyright 2013-2015 Skytechnology sp. z o.o.
   Copyright 2023      Leil Storage OÜ


   SaunaFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   SaunaFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with SaunaFS  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <mutex>
#include <sstream>
#include <string>
#include "common/platform.h"

struct instancePichardoInfo {
	int32_t len;
	char *buff;
};

class PichardoInfo {
public:
	PichardoInfo();
	void open();
	instancePichardoInfo *getInfoCopy();

private:
	uint64_t openCount;
};

inline PichardoInfo gPichardoInfo;
inline std::mutex gPichardoMtx;