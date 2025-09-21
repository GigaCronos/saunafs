/*
   Copyright 2005-2010 Jakub Kruszona-Zawadzki, Gemius SA
   Copyright 2013-2016 Skytechnology sp. z o.o.
   Copyright 2023      Leil Storage OÜ

   This file is part of SaunaFS.

   SaunaFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   SaunaFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with SaunaFS. If not, see <http://www.gnu.org/licenses/>.
*/

#include "mount/pichardo.h"

#include <string.h>

PichardoInfo::PichardoInfo() { openCount = 0; }

// Needs Protection
void PichardoInfo::open() { openCount++; }

instancePichardoInfo *PichardoInfo::getInfoCopy() {
	instancePichardoInfo *ins = new instancePichardoInfo();
	ins->buff = strdup(("Number of times opened: " + std::to_string(openCount) + "\n").c_str());
	ins->len = ("Number of times opened: " + std::to_string(openCount) + "\n").size();
	return ins;
}
