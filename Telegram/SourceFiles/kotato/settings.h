/*
This file is part of Kotatogram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/kotatogram/kotatogram-desktop/blob/dev/LEGAL
*/
#pragma once

#define DeclareReadSetting(Type, Name) extern Type g##Name; \
inline const Type &c##Name() { \
	return g##Name; \
}

#define DeclareSetting(Type, Name) DeclareReadSetting(Type, Name) \
inline void cSet##Name(const Type &Name) { \
	g##Name = Name; \
}

#define DeclareRefSetting(Type, Name) DeclareSetting(Type, Name) \
inline Type &cRef##Name() { \
	return g##Name; \
}

DeclareSetting(bool, KotatoFirstRun);

DeclareSetting(QString, MainFont);
DeclareSetting(QString, SemiboldFont);
DeclareSetting(bool, SemiboldFontIsBold);
DeclareSetting(QString, MonospaceFont);
DeclareSetting(bool, UseSystemFont);
DeclareSetting(bool, UseOriginalMetrics);

void SetBigEmojiOutline(bool enabled);
[[nodiscard]] bool BigEmojiOutline();
[[nodiscard]] rpl::producer<bool> BigEmojiOutlineChanges();

void SetStickerHeight(int height);
[[nodiscard]] int StickerHeight();
[[nodiscard]] rpl::producer<int> StickerHeightChanges();

void SetAdaptiveBubbles(bool enabled);
[[nodiscard]] bool AdaptiveBubbles();
[[nodiscard]] rpl::producer<bool> AdaptiveBubblesChanges();

DeclareSetting(bool, AlwaysShowScheduled);
DeclareSetting(int, ShowChatId);

DeclareSetting(int, NetSpeedBoost);
DeclareSetting(int, NetRequestsCount);
DeclareSetting(int, NetUploadSessionsCount);
DeclareSetting(int, NetUploadRequestInterval);

inline void SetNetworkBoost(int boost) {
	if (boost < 0) {
		cSetNetSpeedBoost(0);
	} else if (boost > 3) {
		cSetNetSpeedBoost(3);
	} else {
		cSetNetSpeedBoost(boost);
	}

	cSetNetRequestsCount(2 + (2 * cNetSpeedBoost()));
	cSetNetUploadSessionsCount(2 + (2 * cNetSpeedBoost()));
	cSetNetUploadRequestInterval(500 - (100 * cNetSpeedBoost()));
}

DeclareSetting(bool, ShowPhoneInDrawer);

using ScaleVector = std::vector<int>;
DeclareRefSetting(ScaleVector, InterfaceScales);
bool HasCustomScales();
bool AddCustomScale(int scale);
void ClearCustomScales();

void SetDialogListLines(int lines);
[[nodiscard]] int DialogListLines();
[[nodiscard]] rpl::producer<int> DialogListLinesChanges();

DeclareSetting(bool, DisableUpEdit);

using CustomReplacementsMap = QMap<QString, QString>;
DeclareRefSetting(CustomReplacementsMap, CustomReplaces);
bool AddCustomReplace(QString from, QString to);
DeclareSetting(bool, ConfirmBeforeCall);
DeclareSetting(bool, NoTaskbarFlashing);

void SetRecentStickersLimit(int limit);
[[nodiscard]] int RecentStickersLimit();
[[nodiscard]] rpl::producer<int> RecentStickersLimitChanges();

DeclareSetting(int, UserpicCornersType);
DeclareSetting(bool, ShowTopBarUserpic);
DeclareSetting(int, CustomAppIcon);

DeclareSetting(int, DefaultFilterId);
DeclareSetting(bool, UnmutedFilterCounterOnly);
DeclareSetting(bool, HideFilterEditButton);
DeclareSetting(bool, HideFilterNames);
DeclareSetting(bool, HideFilterAllChats);

DeclareSetting(bool, ProfileTopBarNotifications);
