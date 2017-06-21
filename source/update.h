#pragma once

#include "libs.h"

#include "main.h"
#include "release.h"

#define MAXPATHLEN 37

struct UpdateResult {
	bool        success; /*!< Wether the operation was a success */
	std::string errcode; /*!< Error code if success is false     */
};

UpdateResult update(const UpdateInfo& args);
UpdateResult restore(const UpdateInfo& args);