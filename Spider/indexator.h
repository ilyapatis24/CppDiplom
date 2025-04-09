#pragma once

#include <string.h>
#include <set>
#include "database.h"



std::set<std::string> indexator(database& DB, std::string inLink);

