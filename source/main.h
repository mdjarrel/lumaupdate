#pragma once

#include "libs.h"

#include "release.h"
#include "version.h"

/* States */

enum UpdateState {
	UpdateConfirmationScreen,
	Updating,
	UpdateComplete,
	UpdateFailed,
	Restoring,
	RestoreComplete,
	RestoreFailed
};

enum class ChoiceType {
	NoChoice,
	UpdatePayload,
	RestoreBackup
};

enum class SelfUpdateChoice {
	NoChoice,
	SelfUpdate,
	IgnoreUpdate,
};

struct UpdateChoice {
	ChoiceType type          = ChoiceType::NoChoice;
	ReleaseVer chosenVersion = ReleaseVer{};
	bool       isHourly      = false;

	explicit UpdateChoice(const ChoiceType type)
		:type(type) {}
	UpdateChoice(const ChoiceType type, const ReleaseVer& ver, const bool hourly)
		:type(type), chosenVersion(ver), isHourly(hourly) {}
};

struct UpdateInfo {
	// Detected options
	LumaVersion  currentVersion;
	LumaVersion  backupVersion;
	bool         migrateARN     = false;
	bool         backupExists   = false;

	// Configuration options
	PayloadType  payloadType    = PayloadType::SIGHAX;
	std::string  payloadPath    = "/boot.firm";
	bool         backupExisting = true;
	bool         selfUpdate     = true;
	bool         writeLog       = true;

	// Available data
	ReleaseInfo* stable = nullptr;
	ReleaseInfo* hourly = nullptr;

	// Chosen settings
	UpdateChoice choice = UpdateChoice(ChoiceType::NoChoice);

  ReleaseVer chosenVersion = choice.chosenVersion;
  bool isHourly = choice.isHourly;
};

struct PromptStatus {
	// Redraw queries
	bool redrawTop = false;
	bool redrawBottom = false;
	bool partialredraw = false;
	bool redrawRequired() { return redrawTop || redrawBottom || partialredraw; }
	void resetRedraw() { redrawTop = redrawBottom = partialredraw = false; }

	// Selection and paging
	int  selected = 0;
	int  currentPage = 0;
	int  pageCount = 0;

	// Prompt choice taken?
	bool optionChosen = false;
};