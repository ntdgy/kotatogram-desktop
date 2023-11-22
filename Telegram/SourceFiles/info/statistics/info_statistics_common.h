/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_statistics.h"

namespace Info::Statistics {

struct RecentPostId final {
	FullMsgId messageId;
	FullStoryId storyId;

	[[nodiscard]] bool valid() const {
		return messageId || storyId;
	}
	explicit operator bool() const {
		return valid();
	}
	friend inline auto operator<=>(RecentPostId, RecentPostId) = default;
	friend inline bool operator==(RecentPostId, RecentPostId) = default;
};

struct SavedState final {
	Data::AnyStatistics stats;
	base::flat_map<RecentPostId, QImage> recentPostPreviews;
	Data::PublicForwardsSlice publicForwardsFirstSlice;
	int recentPostsExpanded = 0;
};

} // namespace Info::Statistics
